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
        "<global> ampeg_release=0.6 ampeg_attack=0.005\n"
        "<region> sample=Bell/Bell C4.wav lokey=0 hikey=127 pitch_keycenter=60 loop_mode=loop_continuous loop_start=1000 loop_end=20000\n");
    sounds.getChildFile ("library.json").replaceWithText (R"({ "version": 1, "instruments": [
        { "category": "Keys", "name": "Bell", "file": "Keys/Bell.sfz", "description": "A bell" },
        { "category": "Keys", "name": "Missing", "file": "Keys/Missing.sfz" } ] })");

    samplerlibrary::setBuiltInFolder (sounds);
    const auto entries = samplerlibrary::list();
    REQUIRE (entries.size() == 1);   // the missing one isn't offered
    CHECK (entries[0].category == "Keys");
    CHECK (entries[0].name == "Bell");
    CHECK (entries[0].reference == samplersound::builtIn ("Keys/Bell"));

    // Its suggested settings come from the SFZ's envelope; everything else back to the default.
    const auto settings = samplerlibrary::suggestedSettings (entries[0].reference, {});
    CHECK (std::abs (settings.at ("release") - 600.0f) < 0.01f);
    CHECK (std::abs (settings.at ("attack") - 5.0f) < 0.01f);
    CHECK (settings.at ("voiceMode") == (float) sm::poly);

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
