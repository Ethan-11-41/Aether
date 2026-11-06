// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
//
// MIDI in the engine: notes from clips at exactly the right sample, playing live (with the
// sustain pedal), and no notes left hanging when playback stops or loops.

#include <cmath>

#include <catch2/catch_test_macros.hpp>

#include "dsp/Instrument.h"
#include "engine/AudioEngine.h"
#include "engine/Recorder.h"
#include "engine/TimelineSync.h"
#include "model/ClipEdits.h"
#include "model/EditCommands.h"
#include "model/MidiClips.h"
#include "model/Recording.h"
#include "model/TimelineEdits.h"

using namespace aether;

namespace
{
    constexpr double rate = 48000.0;
    constexpr Tick q = ticksPerQuarter;   // 24000 samples at 120 BPM

    struct Fixture
    {
        Fixture()
        {
            keys = edit::addTrack (project, TrackKind::midi, -1, true, "Keys").getId();
            engine.prepare (rate, 256);
        }

        void update (const juce::String& focused = {})
        {
            engine.setTimeline (buildTimelineSnapshot (project, rate, {}, &resources, focused));
        }

        std::vector<float> render (int total)
        {
            std::vector<float> out;
            std::vector<float> l (256), r (256);

            for (int done = 0; done < total; done += 256)
            {
                std::fill (l.begin(), l.end(), 0.0f);
                float* outputs[] = { l.data(), r.data() };
                engine.process (nullptr, 0, outputs, 2, 256);
                engine.collectGarbage();
                out.insert (out.end(), l.begin(), l.end());
            }

            return out;
        }

        static float loudest (const std::vector<float>& v, size_t from, size_t to)
        {
            float m = 0.0f;
            for (auto i = from; i < std::min (to, v.size()); ++i)
                m = std::max (m, std::abs (v[i]));
            return m;
        }

        Project project;
        juce::String keys;
        AudioEngine engine;
        MixerResources resources;
    };
}

TEST_CASE ("Notes in a MIDI clip sound from exactly their sample", "[midi][engine]")
{
    Fixture f;
    edit::setInstrumentPreset (f.project, f.keys, "Organ");
    edit::addMidiClip (f.project, f.keys, "x", 0, 4 * q, { { 69, 100, q, q / 2 } });   // A4 on beat 2, an eighth note
    f.update();
    f.engine.play();

    const auto out = f.render (48000);
    CHECK (Fixture::loudest (out, 0, 24000) == 0.0f);          // silent until the note
    CHECK (Fixture::loudest (out, 24000, 24010) > 0.0f);       // sounding straight away
    CHECK (Fixture::loudest (out, 24000, 36000) > 0.01f);
    CHECK (Fixture::loudest (out, 44000, 48000) == 0.0f);      // over (an eighth = 12000 samples, then the release)
}

TEST_CASE ("Every preset makes a sound", "[midi][engine]")
{
    const auto* synth = dsp::findEffectType ("synth");
    REQUIRE (synth != nullptr);

    for (const auto& preset : synth->presets)
    {
        INFO (preset.name);
        Fixture f;
        edit::setInstrumentPreset (f.project, f.keys, preset.name);
        edit::addMidiClip (f.project, f.keys, "x", 0, 4 * q, { { 48, 100, 0, q }, { 72, 60, 0, q } });
        f.update();
        f.engine.play();
        const auto out = f.render (24000);
        CHECK (Fixture::loudest (out, 0, 24000) > 0.01f);
        CHECK (Fixture::loudest (out, 0, 24000) < 1.0f);   // and not too loud
        for (const auto v : out)
            REQUIRE (std::isfinite (v));
    }
}

TEST_CASE ("Playing live: the selected MIDI track plays, with the sustain pedal", "[midi][engine][live]")
{
    Fixture f;
    f.update();
    f.engine.sendLiveMidi (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100));
    CHECK (Fixture::loudest (f.render (2048), 0, 2048) == 0.0f);   // not selected, armed or monitoring: silent

    f.update (f.keys);   // the track is selected
    f.engine.sendLiveMidi (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100));
    CHECK (Fixture::loudest (f.render (4096), 0, 4096) > 0.01f);

    // With the pedal down, letting go of the key keeps it sounding...
    f.engine.sendLiveMidi (juce::MidiMessage::controllerEvent (1, 64, 127));
    f.engine.sendLiveMidi (juce::MidiMessage::noteOff (1, 60));
    auto out = f.render (24000);
    CHECK (Fixture::loudest (out, 20000, 24000) > 0.001f);

    // ...until the pedal comes up.
    f.engine.sendLiveMidi (juce::MidiMessage::controllerEvent (1, 64, 0));
    out = f.render (48000);
    CHECK (Fixture::loudest (out, 40000, 48000) == 0.0f);

    // Armed tracks play too, selected or not.
    edit::setArmed (f.project, { f.keys }, true);
    f.update();
    f.engine.sendLiveMidi (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 90));
    CHECK (Fixture::loudest (f.render (4096), 0, 4096) > 0.01f);
}

TEST_CASE ("Stopping playback doesn't leave notes hanging", "[midi][engine]")
{
    Fixture f;
    edit::setInstrumentPreset (f.project, f.keys, "Organ");
    edit::addMidiClip (f.project, f.keys, "x", 0, 16 * q, { { 60, 100, 0, 16 * q } });   // four bars long
    f.update();
    f.engine.play();
    CHECK (Fixture::loudest (f.render (24000), 12000, 24000) > 0.01f);

    f.engine.stop();
    const auto out = f.render (24000);
    CHECK (Fixture::loudest (out, 12000, 24000) == 0.0f);
}

TEST_CASE ("Looping releases notes at the jump and plays them again", "[midi][engine][loop]")
{
    Fixture f;
    edit::setInstrumentPreset (f.project, f.keys, "Organ");
    edit::addMidiClip (f.project, f.keys, "x", 0, 8 * q, { { 60, 100, 0, q / 2 }, { 67, 100, q, 7 * q } });   // a long G
    edit::setLoopRange (f.project, 0, 2 * q);
    edit::setLoopEnabled (f.project, true);
    f.update();
    f.engine.play();

    // Three times round a two-beat loop: the C plays at the start of every pass, and the long G
    // is cut at each jump instead of piling up.
    const auto out = f.render (3 * 48000);
    for (int pass = 0; pass < 3; ++pass)
        CHECK (Fixture::loudest (out, (size_t) (pass * 48000), (size_t) (pass * 48000 + 1000)) > 0.01f);

    CHECK (Fixture::loudest (out, 0, out.size()) < 1.0f);   // the G doesn't pile up into a roar
}

TEST_CASE ("The synth steals the quietest voice when all sixteen are busy", "[midi][synth]")
{
    auto synth = dsp::createInstrument ("synth");
    synth->prepare (rate, 512);
    dsp::EffectParameterValues p {};
    const auto* type = dsp::findEffectType ("synth");
    for (size_t i = 0; i < type->parameters.size(); ++i)
        p[i] = type->presets[0].values[i];

    std::vector<dsp::NoteEvent> events;
    for (int n = 0; n < 20; ++n)
    {
        dsp::NoteEvent e;
        e.pitch = (std::uint8_t) (40 + n);
        e.offset = n;
        events.push_back (e);
    }

    std::vector<float> l (512), r (512);
    synth->process (l.data(), r.data(), 512, p, events.data(), (int) events.size());
    CHECK (synth->getNumSounding() == 16);

    dsp::NoteEvent off;
    off.type = dsp::NoteEvent::Type::allNotesOff;
    for (int i = 0; i < 200; ++i)
        synth->process (l.data(), r.data(), 512, p, i == 0 ? &off : nullptr, i == 0 ? 1 : 0);
    CHECK (synth->getNumSounding() == 0);
}

//==============================================================================
TEST_CASE ("Recorded MIDI lands where it was played, corrected for latency", "[midi][recording]")
{
    recording::Facts f;
    f.startSample = 48000;
    f.lengthInSamples = 48000;
    f.latencySamples = 100;

    const auto placed = recording::placeMidi (f, { { 60, 100, 1100, 2000 } }, rate);
    REQUIRE (placed.has_value());
    CHECK (placed->timelineStart == 48000);
    CHECK (placed->duration == 48000);
    REQUIRE (placed->notes.size() == 1);
    CHECK (placed->notes[0] == NoteData { 60, 100, 1000, 2000 });

    // Nothing played: no clip.
    CHECK_FALSE (recording::placeMidi (f, {}, rate).has_value());
}

TEST_CASE ("Loop recording MIDI layers every pass into one clip", "[midi][recording][loop]")
{
    recording::Facts f;
    f.loopEnabled = true;
    f.loopStart = 24000;
    f.loopEnd = 48000;
    f.startSample = 0;
    f.lengthInSamples = 96000;

    // Played at the same point of the loop in two passes, and once near its start.
    const auto placed = recording::placeMidi (f, { { 60, 100, 30000, 1000 }, { 64, 100, 54000, 1000 }, { 67, 100, 74000, 30000 } }, rate);
    REQUIRE (placed.has_value());
    CHECK (placed->timelineStart == 24000);
    CHECK (placed->duration == 24000);
    REQUIRE (placed->notes.size() == 3);
    CHECK (placed->notes[0].start == 2000);
    CHECK (placed->notes[1].start == 6000);
    CHECK (placed->notes[2].start == 6000);
    CHECK (placed->notes[0].length == 22000);   // the long note stops at the loop end
}

TEST_CASE ("Recording MIDI from a keyboard makes a MIDI clip", "[midi][recording]")
{
    Fixture f;
    const auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("aether-midi-rec-" + juce::Uuid().toString());
    folder.createDirectory();
    const ProjectLocation location (folder.getChildFile ("Song.aether"));

    AudioFileCache cache;
    TimelineSync sync (f.project, f.engine, cache, [location] { return location; });
    Recorder recorder (f.project, f.engine, sync, [location] { return location; }, [] { return 0; }, [] { return 0; }, [] { return 0; });
    edit::setArmed (f.project, { f.keys }, true);

    juce::StringArray clipIds;
    recorder.onFinished = [&] (const juce::StringArray& ids) { clipIds = ids; };
    REQUIRE (recorder.start().isEmpty());

    std::vector<float> l (256), r (256);
    float* outputs[] = { l.data(), r.data() };

    for (int block = 0; block < 200; ++block)
    {
        // The keyboard: a note from block 75 (0.4 s) to block 150 (0.8 s).
        if (block == 75)  f.engine.sendLiveMidi (juce::MidiMessage::noteOn (1, 62, (juce::uint8) 80));
        if (block == 150) f.engine.sendLiveMidi (juce::MidiMessage::noteOff (1, 62));

        f.engine.process (nullptr, 0, outputs, 2, 256);
        f.engine.collectGarbage();

        if (block % 20 == 0)
            recorder.poll();
    }

    recorder.stop();
    f.engine.process (nullptr, 0, outputs, 2, 256);
    recorder.poll();

    REQUIRE (clipIds.size() == 1);
    const auto clip = clips::findClip (f.project, clipIds[0]);
    REQUIRE (clip.isMidi());
    CHECK (clip.getStart() == 0);
    const auto notes = midi::audibleNotes (clip);
    REQUIRE (notes.size() == 1);
    CHECK (notes[0] == NoteData { 62, 80, 4 * q / 5, 4 * q / 5 });   // 0.4 s = 0.8 beats at 120 BPM, for 0.8 beats

    // No audio files for a MIDI recording.
    CHECK (folder.getChildFile ("Audio").findChildFiles (juce::File::findFiles, false).isEmpty());
    folder.deleteRecursively();
}
