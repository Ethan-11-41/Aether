// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
//
// Sampler tracks: choosing what they play (undoably), the built-in library, and playing
// recordings, SFZ instruments and drum sounds through the engine.

#include <cmath>

#include <catch2/catch_test_macros.hpp>

#include "dsp/PitchDetect.h"
#include "dsp/Sampler.h"
#include "engine/AudioEngine.h"
#include "engine/SamplerLibrary.h"
#include "engine/TimelineSync.h"
#include "io/SamplerImport.h"
#include "model/EditCommands.h"
#include "model/MidiClips.h"
#include "model/Mixer.h"

using namespace aether;
namespace sm = aether::dsp::sampler;

namespace
{
    constexpr double rate = 48000.0;
    constexpr Tick q = ticksPerQuarter;

    std::shared_ptr<AudioData> sineData (double hz, double seconds, double sourceRate = rate)
    {
        auto data = std::make_shared<AudioData>();
        data->sampleRate = rate;
        data->sourceRate = sourceRate;
        data->samples.setSize (1, (int) (seconds * rate));
        for (int i = 0; i < data->samples.getNumSamples(); ++i)
            data->samples.setSample (0, i, 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * hz * i / rate));
        return data;
    }

    std::vector<float> render (Project& project, const AudioResolver& resolver, int samples, MixerResources& resources,
                               const juce::File& folder = {})
    {
        AudioEngine engine;
        engine.prepare (rate, 512);
        engine.setTimeline (buildTimelineSnapshot (project, rate, resolver, &resources, {}, {}, folder));
        engine.play();

        std::vector<float> l, bl (512), br (512);
        for (int done = 0; done < samples; done += 512)
        {
            float* outputs[] = { bl.data(), br.data() };
            engine.process (nullptr, 0, outputs, 2, 512);
            l.insert (l.end(), bl.begin(), bl.end());
        }
        engine.collectGarbage();
        return l;
    }

    /** A temporary folder that's removed afterwards. */
    struct TempFolder
    {
        TempFolder()  { folder.createDirectory(); }
        ~TempFolder() { folder.deleteRecursively(); }
        juce::File folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getNonexistentChildFile ("AetherSamplerTest", "");
    };

    void writeWav (const juce::File& file, double hz, double seconds, double fileRate = 48000.0)
    {
        file.getParentDirectory().createDirectory();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (file);
        auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate (fileRate).withNumChannels (1).withBitsPerSample (16));
        REQUIRE (writer != nullptr);
        juce::AudioBuffer<float> b (1, (int) (seconds * fileRate));
        for (int i = 0; i < b.getNumSamples(); ++i)
            b.setSample (0, i, 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * hz * i / fileRate));
        writer->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
    }
}

TEST_CASE ("Choosing a sampler's sound switches the track to the sampler, in one undo step", "[sampler][model]")
{
    Project project;
    const auto track = edit::addTrack (project, TrackKind::midi).getId();
    project.getUndoManager().clearUndoHistory();
    const auto before = project.getState().toXmlString();

    REQUIRE (edit::setSamplerSound (project, track, samplersound::recording ("Audio/My 808.wav"), {}, 31,
                                    { { "voiceMode", (float) sm::slide }, { "release", 120.0f }, { "nonsense", 1.0f } }));
    CHECK (project.getUndoManager().getUndoDescription() == "Use My 808");

    const auto instrument = midi::instrumentOf (project, track);
    CHECK (instrument[ids::type].toString() == sm::typeId);
    CHECK (samplersound::of (project, track) == samplersound::recording ("Audio/My 808.wav"));
    CHECK (samplersound::nameOf (project, track) == "My 808");
    CHECK (samplersound::rootKeyOf (project, track) == 31);
    CHECK (mixer::getInsertParameter (instrument, "voiceMode") == (float) sm::slide);
    CHECK (mixer::getInsertParameter (instrument, "release") == 120.0f);

    REQUIRE (edit::setSamplerRootKey (project, track, 33));
    CHECK (samplersound::rootKeyOf (project, track) == 33);
    CHECK_FALSE (edit::setSamplerRootKey (project, track, 33));

    // Another sound on the same sampler keeps the instrument.
    const auto id = instrument[ids::id].toString();
    REQUIRE (edit::setSamplerSound (project, track, samplersound::builtIn ("Pianos/Grand piano")));
    CHECK (midi::instrumentOf (project, track)[ids::id].toString() == id);
    CHECK (samplersound::nameOf (project, track) == "Grand piano");

    while (project.getUndoManager().canUndo())
        project.getUndoManager().undo();
    CHECK (project.getState().toXmlString() == before);

    CHECK (samplersound::parse ("drum:kick-808") == samplersound::drum ("kick-808"));
    CHECK (samplersound::parse ("nonsense").kind == samplersound::Kind::none);
    CHECK (samplersound::defaultName (samplersound::drum ("kick-808")) == "Kick 808");
    CHECK (samplersound::defaultName (samplersound::sfzFile ("Audio/Strings/Strings.sfz")) == "Strings");
}

TEST_CASE ("A recording plays across the keyboard through the engine", "[sampler][engine]")
{
    Project project;
    const auto track = edit::addTrack (project, TrackKind::midi).getId();
    edit::setSamplerSound (project, track, samplersound::recording ("Audio/A.wav"), {}, 69);
    edit::addMidiClip (project, track, "Notes", 0, 4 * q, { { 81, 100, 0, 2 * q } });   // A5: an octave up

    const auto a440 = sineData (440.0, 2.0);
    const AudioResolver resolver = [&] (const juce::String& path) { return path == "Audio/A.wav" ? std::shared_ptr<const AudioData> (a440) : nullptr; };

    MixerResources resources;
    const auto out = render (project, resolver, 24000, resources);
    juce::AudioBuffer<float> part (1, 19200);
    for (int i = 0; i < 19200; ++i)
        part.setSample (0, i, out[(size_t) (4800 + i)]);

    const auto hz = dsp::pitch::detectFrequency (part, rate);
    REQUIRE (hz.has_value());
    CHECK (std::abs (*hz - 880.0) < 3.0);

    // The map is the same object from one build to the next, until something in it changes.
    const auto first = buildTimelineSnapshot (project, rate, resolver, &resources);
    const auto second = buildTimelineSnapshot (project, rate, resolver, &resources);
    REQUIRE (first->tracks[0].sampleMap != nullptr);
    CHECK (first->tracks[0].sampleMap == second->tracks[0].sampleMap);
    CHECK (first->tracks[0].sounds.map == first->tracks[0].sampleMap.get());

    edit::setSamplerRootKey (project, track, 57);
    const auto third = buildTimelineSnapshot (project, rate, resolver, &resources);
    CHECK (third->tracks[0].sampleMap != first->tracks[0].sampleMap);

    // Until its audio is loaded, the sampler has nothing to play (and doesn't crash).
    const auto empty = buildTimelineSnapshot (project, rate, [] (const juce::String&) { return std::shared_ptr<const AudioData>(); }, &resources);
    CHECK (empty->tracks[0].sampleMap->isEmpty());
}

TEST_CASE ("An 808 from the drum kit plays as a bass line", "[sampler][engine]")
{
    Project project;
    const auto track = edit::addTrack (project, TrackKind::midi).getId();
    edit::setSamplerSound (project, track, samplersound::drum ("kick-808"), {}, 31,
                           samplerlibrary::suggestedSettings (samplersound::drum ("kick-808"), {}));
    CHECK (mixer::getInsertParameter (midi::instrumentOf (project, track), "voiceMode") == (float) sm::slide);

    edit::addMidiClip (project, track, "Bass", 0, 4 * q, { { 31, 100, 0, q }, { 36, 100, q, q } });
    MixerResources resources;
    const auto out = render (project, {}, 30000, resources);
    CHECK (*std::max_element (out.begin(), out.end()) > 0.3f);
}

TEST_CASE ("SFZ instruments and the built-in library", "[sampler][engine]")
{
    TempFolder temp;
    const auto sounds = temp.folder.getChildFile ("Sounds");
    writeWav (sounds.getChildFile ("Keys/Bell/Bell C4.wav"), 261.63, 1.0, 24000.0);
    sounds.getChildFile ("Keys/Bell.sfz").replaceWithText (
        "<global> ampeg_release=0.6 ampeg_attack=0.005 amp_veltrack=0\n"
        "<region> sample=Bell/Bell C4.wav lokey=0 hikey=127 pitch_keycenter=60 loop_mode=loop_continuous loop_start=1000 loop_end=20000\n");
    sounds.getChildFile ("Keys/Horn.sfz").replaceWithText ("<region> sample=Bell/Bell C4.wav pitch_keycenter=60\n");
    sounds.getChildFile ("library.json").replaceWithText (R"({ "version": 1, "instruments": [
        { "category": "Keys", "name": "Bell", "file": "Keys/Bell.sfz", "description": "A bell" },
        { "category": "Keys", "name": "Horn", "file": "Keys/Horn.sfz", "description": "A horn", "solo": true },
        { "category": "Keys", "name": "Missing", "file": "Keys/Missing.sfz" } ] })");

    samplerlibrary::setBuiltInFolder (sounds);
    const auto entries = samplerlibrary::list();
    REQUIRE (entries.size() == 2);   // the missing one isn't offered
    CHECK (entries[0].category == "Keys");
    CHECK (entries[0].name == "Bell");
    CHECK (entries[0].reference == samplersound::builtIn ("Keys/Bell"));
    CHECK_FALSE (entries[0].solo);

    // A solo instrument (one note at a time) starts in Legato.
    CHECK (entries[1].solo);
    CHECK (samplerlibrary::suggestedSettings (entries[1].reference, {}).at ("voiceMode") == (float) sm::legato);

    // Its suggested settings come from the SFZ's envelope; everything else back to the default.
    const auto settings = samplerlibrary::suggestedSettings (entries[0].reference, {});
    CHECK (std::abs (settings.at ("release") - 600.0f) < 0.01f);
    CHECK (std::abs (settings.at ("attack") - 5.0f) < 0.01f);
    CHECK (settings.at ("voiceMode") == (float) sm::poly);
    CHECK (settings.at ("touch") == 0.0f);   // amp_veltrack=0

    // Regions point at the files themselves; loops are converted from the file's rate.
    const auto regions = samplerlibrary::regionsOf (entries[0].reference, 60, {});
    REQUIRE (regions.size() == 1);
    CHECK (juce::File (regions[0].sample) == sounds.getChildFile ("Keys/Bell/Bell C4.wav"));

    const auto data = sineData (261.63, 2.0, 24000.0);   // at 48 kHz, from a 24 kHz file
    const auto map = samplerlibrary::buildMap (regions, { data });
    REQUIRE (map.zones.size() == 1);
    CHECK (map.zones[0].loopStart == 2000);
    CHECK (map.zones[0].loopEnd == 40000);

    // Played in a project, through the audio file cache.
    Project project;
    const auto track = edit::addTrack (project, TrackKind::midi).getId();
    edit::setSamplerSound (project, track, entries[0].reference, "Bell", -1, settings);
    edit::addMidiClip (project, track, "Notes", 0, 4 * q, { { 60, 100, 0, q } });
    CHECK (samplerlibrary::filesUsed (project, {}).contains (sounds.getChildFile ("Keys/Bell/Bell C4.wav")));

    AudioFileCache cache;
    cache.setTargetSampleRate (rate);
    const AudioResolver resolver = [&] (const juce::String& path) { return cache.getAudio (juce::File (path)); };
    resolver (regions[0].sample);
    cache.waitUntilIdle();

    MixerResources resources;
    const auto out = render (project, resolver, 12000, resources);
    CHECK (*std::max_element (out.begin(), out.end()) > 0.2f);

    samplerlibrary::setBuiltInFolder ({});
}

TEST_CASE ("The recorded instrument pack is complete and readable", "[sampler][pack]")
{
    // Only where the pack is present (the Windows build downloads it; elsewhere set AETHER_SOUNDS).
    const auto folder = samplerlibrary::builtInFolder();
    if (! folder.getChildFile ("library.json").existsAsFile())
        SKIP ("The sound pack isn't here (set AETHER_SOUNDS to its folder to check it).");

    const auto entries = samplerlibrary::list();
    CHECK (entries.size() >= 20);

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    juce::StringArray categories;

    for (const auto& e : entries)
    {
        INFO (e.name);
        categories.addIfNotAlreadyThere (e.category);
        CHECK (e.description.isNotEmpty());

        const auto regions = samplerlibrary::regionsOf (e.reference, 60, {});
        REQUIRE (! regions.empty());

        // Every recording is there; the first can be decoded, and its loop is inside it.
        for (const auto& r : regions)
            CHECK (juce::File (r.sample).existsAsFile());

        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (juce::File (regions.front().sample)));
        REQUIRE (reader != nullptr);
        CHECK (reader->lengthInSamples > 1000);
        if (regions.front().loop)
            CHECK (regions.front().loopEnd <= reader->lengthInSamples);

        CHECK (sfz::read (samplerlibrary::sfzFileOf (e.reference, {})).ignored.isEmpty());
    }

    CHECK (categories.size() == 5);
}

TEST_CASE ("Your own sounds become sampler sounds", "[sampler][io]")
{
    TempFolder temp;
    ProjectLocation location;
    REQUIRE (projectfiles::createProjectFolder (temp.folder, "Song", location).wasOk());
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    // One recording, named after its note.
    const auto named = temp.folder.getChildFile ("Elsewhere/Bass C2.wav");
    writeWav (named, 65.4, 1.0);
    auto result = importSamplerSound (location, formats, { named });
    CHECK (result.problem.isEmpty());
    CHECK (result.sound == samplersound::recording ("Audio/Bass C2.wav"));
    CHECK (result.name == "Bass C2");
    CHECK (result.rootKey == 36);
    CHECK (result.keyFound);
    CHECK (location.getAudioFolder().getChildFile ("Bass C2.wav").existsAsFile());

    // One without a note in its name: its pitch says (110 Hz is A2).
    const auto plain = temp.folder.getChildFile ("Elsewhere/My bass.wav");
    writeWav (plain, 110.0, 1.0);
    result = importSamplerSound (location, formats, { plain });
    CHECK (result.rootKey == 45);
    CHECK (result.keyFound);

    // A folder of notes: copied together, laid out, with an SFZ beside them.
    const auto folder = temp.folder.getChildFile ("Elsewhere/Glass");
    for (const auto* name : { "Glass C4.wav", "Glass E4.wav", "Glass G4.wav" })
        writeWav (folder.getChildFile (name), 300.0, 0.5);
    folder.getChildFile ("notes.txt").replaceWithText ("not audio");

    result = importSamplerSound (location, formats, { folder });
    CHECK (result.problem.isEmpty());
    CHECK (result.name == "Glass");
    CHECK (result.sound == samplersound::sfzFile ("Audio/Glass/Glass.sfz"));
    const auto instrument = sfz::read (location.getFolder().getChildFile ("Audio/Glass/Glass.sfz"));
    REQUIRE (instrument.regions.size() == 3);
    CHECK (instrument.regions[0].sample == "Glass C4.wav");
    CHECK (instrument.regions[0].rootKey == 60);

    // An SFZ elsewhere is used where it is.
    const auto outside = temp.folder.getChildFile ("Elsewhere/Glass/Copy.sfz");
    outside.replaceWithText ("<region> sample=Glass C4.wav key=60\n");
    result = importSamplerSound (location, formats, { outside });
    CHECK (result.sound == samplersound::sfzFile (outside.getFullPathName().replaceCharacter ('\\', '/')));

    // Nothing playable.
    result = importSamplerSound (location, formats, { folder.getChildFile ("notes.txt") });
    CHECK (result.problem.isNotEmpty());
    CHECK (result.sound.kind == samplersound::Kind::none);

    // Aether's 808 is about G1.
    const auto key = findDrumRootKey ("kick-808");
    CHECK ((key == 30 || key == 31));
}

TEST_CASE ("A sampler track can be added in one step", "[sampler][model]")
{
    Project project;
    edit::addTrack (project, TrackKind::audio);
    project.getUndoManager().clearUndoHistory();

    const auto track = edit::addSamplerTrack (project, samplersound::drum ("kick-808"), "808", 31, { { "voiceMode", (float) sm::slide } }, 0);
    CHECK (project.getTrack (0).getId() == track.getId());
    CHECK (track.getName() == "808");
    CHECK (samplersound::of (project, track.getId()) == samplersound::drum ("kick-808"));
    CHECK (project.getUndoManager().getUndoDescription() == "Add 808 track");

    project.getUndoManager().undo();
    CHECK (project.getNumTracks() == 1);
}

TEST_CASE ("Recordings no track uses any more are let go after a while", "[sampler][engine]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;   // the sync listens for changes on the message thread
    TempFolder temp;
    ProjectLocation location;
    REQUIRE (projectfiles::createProjectFolder (temp.folder, "Song", location).wasOk());
    writeWav (location.getAudioFolder().getChildFile ("Keys/C4.wav"), 261.6, 0.5);
    writeWav (location.getAudioFolder().getChildFile ("Keys/G4.wav"), 392.0, 0.5);
    location.getAudioFolder().getChildFile ("Keys/Keys.sfz").replaceWithText ("<region> sample=C4.wav key=60\n<region> sample=G4.wav key=67\n");

    Project project;
    const auto track = edit::addTrack (project, TrackKind::midi).getId();
    edit::setSamplerSound (project, track, samplersound::sfzFile ("Audio/Keys/Keys.sfz"));

    AudioEngine engine;
    AudioFileCache cache;
    cache.setTargetSampleRate (rate);
    TimelineSync sync (project, engine, cache, [location] { return location; });
    sync.update();
    cache.waitUntilIdle();
    CHECK (cache.getNumEntries() == 2);
    CHECK (samplerlibrary::loadingProgress (project, track, location.getFolder(), cache) == std::pair<int, int> (2, 2));

    // Another sound: the old recordings stay a little while (in case you go back), then go.
    edit::setSamplerSound (project, track, samplersound::drum ("kick-808"));
    sync.update();
    const auto now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    sync.releaseUnusedAudio (now + 1.0);
    CHECK (cache.getNumEntries() == 2);
    sync.releaseUnusedAudio (now + 30.0);
    CHECK (cache.getNumEntries() == 0);

    // Back again: they load again.
    project.getUndoManager().undo();
    sync.update();
    cache.waitUntilIdle();
    CHECK (cache.getNumEntries() == 2);
}

TEST_CASE ("Held notes on the recorded strings, woodwinds and organ keep sounding", "[sampler][pack]")
{
    const auto folder = samplerlibrary::builtInFolder();
    if (! folder.getChildFile ("library.json").existsAsFile())
        SKIP ("The sound pack isn't here (set AETHER_SOUNDS to its folder to check it).");

    for (const auto engineRate : { 44100.0, 48000.0 })
    {
        for (const auto* name : { "Strings/String ensemble", "Woodwinds/Oboe", "Brass/Brass section", "Mallets harp organ/Pipe organ" })
        {
            INFO (name << " at " << engineRate);
            Project project;
            const auto track = edit::addTrack (project, TrackKind::midi).getId();
            const auto sound = samplersound::builtIn (name);
            edit::setSamplerSound (project, track, sound, {}, -1, samplerlibrary::suggestedSettings (sound, {}));
            edit::addMidiClip (project, track, "Chord", 0, 40 * q, { { 60, 100, 0, 36 * q }, { 64, 100, 0, 36 * q }, { 67, 100, 0, 36 * q } });

            AudioFileCache cache;
            cache.setTargetSampleRate (engineRate);
            const AudioResolver resolver = [&] (const juce::String& path) { return cache.getAudio (juce::File (path)); };
            buildTimelineSnapshot (project, engineRate, resolver);   // starts the loading
            cache.waitUntilIdle();

            AudioEngine engine;
            MixerResources resources;
            engine.prepare (engineRate, 512);
            engine.setTimeline (buildTimelineSnapshot (project, engineRate, resolver, &resources));
            engine.play();

            // Eight seconds of a held chord: the last second is about as loud as the second one.
            std::vector<float> l, bl (512), br (512);
            for (int done = 0; done < (int) (engineRate * 8.0); done += 512)
            {
                float* outputs[] = { bl.data(), br.data() };
                engine.process (nullptr, 0, outputs, 2, 512);
                l.insert (l.end(), bl.begin(), bl.end());
            }
            engine.collectGarbage();

            auto rms = [&] (double from, double to)
            {
                double sum = 0.0;
                const auto a = (size_t) (from * engineRate), b = (size_t) (to * engineRate);
                for (auto i = a; i < b; ++i)
                    sum += (double) l[i] * l[i];
                return std::sqrt (sum / (double) (b - a));
            };

            const auto early = rms (1.0, 2.0), late = rms (7.0, 8.0);
            CHECK (early > 0.005);
            CHECK (late > early * 0.4);
        }
    }
}

TEST_CASE ("No recorded instrument goes over the maximum: hard notes and firm chords stay below 0 dB", "[sampler][pack]")
{
    // Until 0.16.1 the source libraries' own volumes made a trumpet chord peak at +5 dB (the
    // sound card cuts that off: the crunch). The pack builder now sets every instrument's level:
    // the loudest note, played as hard as possible, peaks at -8 dB.
    const auto folder = samplerlibrary::builtInFolder();
    if (! folder.getChildFile ("library.json").existsAsFile())
        SKIP ("The sound pack isn't here (set AETHER_SOUNDS to its folder to check it).");

    const auto peakOf = [] (const samplerlibrary::Entry& entry, const std::vector<NoteData>& notes)
    {
        Project project;
        const auto track = edit::addTrack (project, TrackKind::midi).getId();
        auto settings = samplerlibrary::suggestedSettings (entry.reference, {});
        settings["voiceMode"] = (float) sm::poly;   // chords, even on a solo instrument
        edit::setSamplerSound (project, track, entry.reference, {}, -1, settings);
        edit::addMidiClip (project, track, "Notes", 0, 16 * q, notes);

        AudioFileCache cache;
        cache.setTargetSampleRate (rate);
        const AudioResolver resolver = [&] (const juce::String& path) { return cache.getAudio (juce::File (path)); };
        buildTimelineSnapshot (project, rate, resolver);   // starts the loading
        cache.waitUntilIdle();

        MixerResources resources;
        const auto out = render (project, resolver, (int) (rate * 8.0), resources);
        return std::max (*std::max_element (out.begin(), out.end()), -*std::min_element (out.begin(), out.end()));
    };

    for (const auto& entry : samplerlibrary::list())
    {
        INFO (entry.name);

        // Single notes as hard as possible, low to high: none above -6 dB.
        std::vector<NoteData> singles;
        for (int i = 0; i < 6; ++i)
            singles.push_back ({ 48 + i * 8, 127, i * 2 * q, q });

        const auto single = peakOf (entry, singles);
        CHECK (single > 0.02f);   // it plays
        CHECK (single < 0.5f);

        // Chords played firmly (velocity 100), in the middle and near the top: below 0 dB.
        const auto chords = peakOf (entry, { { 60, 100, 0, 3 * q }, { 64, 100, 0, 3 * q }, { 67, 100, 0, 3 * q },
                                             { 79, 100, 4 * q, 3 * q }, { 84, 100, 4 * q, 3 * q }, { 88, 100, 4 * q, 3 * q } });
        CHECK (chords < 1.0f);
    }
}
