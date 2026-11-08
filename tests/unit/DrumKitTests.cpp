// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
//
// The drum kit instrument: kits and pad sounds in the project, and the sound it makes.

#include <cmath>

#include <catch2/catch_test_macros.hpp>

#include "dsp/DrumKit.h"
#include "engine/AudioEngine.h"
#include "engine/DrumSoundCache.h"
#include "engine/TimelineSync.h"
#include "model/ClipEdits.h"
#include "model/EditCommands.h"
#include "model/MidiClips.h"
#include "model/Mixer.h"

using namespace aether;
namespace kit = aether::dsp::drumkit;

namespace
{
    constexpr double rate = 48000.0;
    constexpr Tick q = ticksPerQuarter;   // 24000 samples at 120 BPM

    /** A drum kit on its own, fed sounds directly. */
    struct KitFixture
    {
        KitFixture()
        {
            instrument = dsp::createInstrument (kit::typeId);
            REQUIRE (instrument != nullptr);
            instrument->prepare (rate, 512);

            const auto* type = dsp::findEffectType (kit::typeId);
            for (size_t i = 0; i < type->parameters.size(); ++i)
                params[i] = type->parameters[i].defaultValue;

            // Every pad plays a steady tone of 0.5, one second long, so levels are easy to check.
            tone.setSize (1, 48000);
            for (int i = 0; i < 48000; ++i)
                tone.setSample (0, i, 0.5f);

            for (auto& s : sounds.sounds)
                s = &tone;
        }

        void set (int pad, kit::PadSetting setting, float value)   { params[(size_t) kit::parameterIndex (pad, setting)] = value; }

        static dsp::NoteEvent hit (int note, int offset = 0, int velocity = 127)
        {
            dsp::NoteEvent e;
            e.type = dsp::NoteEvent::Type::noteOn;
            e.pitch = (std::uint8_t) note;
            e.velocity = (std::uint8_t) velocity;
            e.offset = offset;
            return e;
        }

        std::pair<std::vector<float>, std::vector<float>> run (int samples, std::vector<dsp::NoteEvent> events = {})
        {
            std::vector<float> l ((size_t) samples, 0.0f), r ((size_t) samples, 0.0f);
            instrument->process (l.data(), r.data(), samples, params, events.data(), (int) events.size(), &sounds);
            return { l, r };
        }

        std::unique_ptr<dsp::Instrument> instrument;
        dsp::EffectParameterValues params {};
        juce::AudioBuffer<float> tone;
        dsp::InstrumentSounds sounds;
    };
}

TEST_CASE ("A drum track gets a kit's sounds, and kits and pad sounds are undoable", "[drums][model]")
{
    Project project;
    const auto track = edit::addTrack (project, TrackKind::midi).getId();
    REQUIRE (edit::setInstrumentType (project, track, kit::typeId, "808"));

    auto instrument = midi::instrumentOf (project, track);
    CHECK (instrument[ids::preset].toString() == "808");
    CHECK (instrument[kit::soundProperty (0)].toString() == "builtin:kick-808");
    CHECK (kit::describeSound (instrument[kit::soundProperty (0)].toString()) == "Kick 808");

    REQUIRE (edit::setInstrumentPreset (project, track, "Lo-fi"));
    CHECK (instrument[kit::soundProperty (0)].toString() == "builtin:kick-lofi");
    project.getUndoManager().undo();
    CHECK (instrument[kit::soundProperty (0)].toString() == "builtin:kick-808");

    REQUIRE (edit::setPadSound (project, track, 2, "Audio/My snare.wav"));
    CHECK (kit::describeSound (instrument[kit::soundProperty (2)].toString()) == "My snare");
    CHECK (project.getUndoManager().getUndoDescription() == "Pad 3: My snare");
    project.getUndoManager().undo();
    CHECK (instrument[kit::soundProperty (2)].toString() == "builtin:snare-808");

    CHECK_FALSE (edit::setPadSound (project, track, 16, "builtin:clap"));
    CHECK (kit::describeSound ({}) == "(empty)");
}

TEST_CASE ("Drum hits sound from exactly their sample, at their level and pan", "[drums][engine]")
{
    KitFixture f;
    auto [l, r] = f.run (2000, { KitFixture::hit (36, 100) });
    CHECK (l[99] == 0.0f);
    CHECK (std::abs (l[100] - 0.5f) < 1.0e-5f);   // full velocity, 0 dB, centre: as recorded
    CHECK (std::abs (r[100] - 0.5f) < 1.0e-5f);

    // Volume and pan.
    KitFixture g;
    g.set (1, kit::volume, -6.0206f);
    g.set (1, kit::pan, -100.0f);
    std::tie (l, r) = g.run (100, { KitFixture::hit (37) });
    CHECK (std::abs (l[10] - 0.25f * std::sqrt (2.0f)) < 1.0e-3f);   // half as loud, all on the left (equal power)
    CHECK (std::abs (r[10]) < 1.0e-6f);

    // Notes outside the 16 pads, and pads with no sound, are silent.
    KitFixture h;
    h.sounds.sounds[2] = nullptr;
    std::tie (l, r) = h.run (100, { KitFixture::hit (35), KitFixture::hit (52), KitFixture::hit (38) });
    CHECK (*std::max_element (l.begin(), l.end()) == 0.0f);
}

TEST_CASE ("A drum pad's tune and length change how long it plays", "[drums][engine]")
{
    KitFixture f;
    f.set (0, kit::tune, 12.0f);   // an octave up: twice as fast, half as long
    f.run (100, { KitFixture::hit (36) });
    f.run (23000);
    CHECK (f.instrument->getNumSounding() == 1);
    f.run (2000);
    CHECK (f.instrument->getNumSounding() == 0);

    KitFixture g;
    g.set (0, kit::decay, 25.0f);   // a quarter of the sound, fading out
    auto [l, r] = g.run (13000, { KitFixture::hit (36) });
    CHECK (l[5000] > 0.4f);
    CHECK (l[11000] < l[9500]);     // fading
    CHECK (l[12500] == 0.0f);       // over
}

TEST_CASE ("A closed hi-hat cuts off an open one (choke group)", "[drums][engine]")
{
    KitFixture f;
    f.set (10, kit::choke, 1.0f);   // open hat (46)
    f.set (6, kit::choke, 1.0f);    // closed hat (42)
    juce::AudioBuffer<float> blip (1, 100);   // the closed hat itself is very short here, to hear the cut
    blip.clear();
    f.sounds.sounds[6] = &blip;

    auto [l, r] = f.run (24000, { KitFixture::hit (46), KitFixture::hit (42, 4800) });
    CHECK (l[4000] > 0.4f);
    CHECK (l[6000] == 0.0f);        // cut within 5 ms

    // Without choke groups, both ring.
    KitFixture g;
    std::tie (l, r) = g.run (24000, { KitFixture::hit (46), KitFixture::hit (42, 4800) });
    CHECK (l[6000] > 0.9f);
}

TEST_CASE ("A pad hit again cuts off its previous hit, so long sounds don't pile up", "[drums][engine]")
{
    // Every pad here is a steady 0.5 for a second. Two hits 0.1 s apart on the same pad: one sound
    // at a time (like a real drum), not two on top of each other.
    KitFixture f;
    auto [l, r] = f.run (24000, { KitFixture::hit (36), KitFixture::hit (36, 4800) });
    CHECK (std::abs (l[4000] - 0.5f) < 1.0e-4f);
    CHECK (std::abs (l[10000] - 0.5f) < 1.0e-4f);   // not 1.0
    CHECK (*std::max_element (l.begin() + 4800 + 48, l.end()) <= 0.5f + 1.0e-4f);   // after a 1 ms crossfade

    // Different pads still play together.
    KitFixture g;
    std::tie (l, r) = g.run (24000, { KitFixture::hit (36), KitFixture::hit (38, 4800) });
    CHECK (std::abs (l[10000] - 1.0f) < 1.0e-4f);
}

TEST_CASE ("A fast 808 pattern doesn't get louder and louder", "[drums][engine]")
{
    Project project;
    const auto track = edit::addTrack (project, TrackKind::midi).getId();
    edit::setInstrumentType (project, track, kit::typeId, "808");

    std::vector<NoteData> notes;   // the 808 on every 1/8 note for four bars
    for (int i = 0; i < 32; ++i)
        notes.push_back ({ 36, 127, i * (q / 2), q / 4 });
    edit::addMidiClip (project, track, "808s", 0, 16 * q, notes);

    AudioEngine engine;
    MixerResources resources;
    engine.prepare (rate, 512);
    engine.setTimeline (buildTimelineSnapshot (project, rate, {}, &resources));
    engine.play();

    const auto kick = builtInDrumSound ("kick-808", rate);
    REQUIRE (kick != nullptr);
    const auto onePeak = kick->samples.getMagnitude (0, 0, kick->samples.getNumSamples());

    float peak = 0.0f;
    std::vector<float> bl (512), br (512);
    for (int done = 0; done < 8 * 24000; done += 512)
    {
        float* outputs[] = { bl.data(), br.data() };
        engine.process (nullptr, 0, outputs, 2, 512);
        for (auto x : bl)
            peak = std::max (peak, std::abs (x));
    }
    engine.collectGarbage();

    CHECK (peak <= onePeak * 1.05f);   // no louder than a single hit
}

TEST_CASE ("A drum track plays its pattern through the engine, with built-in and imported sounds", "[drums][engine]")
{
    Project project;
    const auto track = edit::addTrack (project, TrackKind::midi).getId();
    edit::setInstrumentType (project, track, kit::typeId, "808");
    edit::setPadSound (project, track, 2, "Audio/snare.wav");   // an "imported" snare on pad 3
    edit::addMidiClip (project, track, "Beat", 0, 4 * q, { { 36, 127, 0, q / 4 }, { 38, 127, q, q / 4 } });

    auto imported = std::make_shared<AudioData>();
    imported->samples.setSize (2, 4800);
    imported->samples.clear();
    for (int i = 0; i < 4800; ++i)
        imported->samples.setSample (0, i, 0.3f);   // left only

    AudioEngine engine;
    MixerResources resources;
    engine.prepare (rate, 512);
    engine.setTimeline (buildTimelineSnapshot (project, rate, [&] (const juce::String& file)
    {
        return file == "Audio/snare.wav" ? std::shared_ptr<const AudioData> (imported) : nullptr;
    }, &resources));
    engine.play();

    std::vector<float> l, r, bl (512), br (512);
    for (int done = 0; done < 30000; done += 512)
    {
        float* outputs[] = { bl.data(), br.data() };
        engine.process (nullptr, 0, outputs, 2, 512);
        l.insert (l.end(), bl.begin(), bl.end());
        r.insert (r.end(), br.begin(), br.end());
    }
    engine.collectGarbage();

    // The kick, from the very start, is the built-in 808 kick.
    const auto kick = builtInDrumSound ("kick-808", rate);
    REQUIRE (kick != nullptr);
    CHECK (std::abs (l[200] - kick->samples.getSample (0, 200)) < 1.0e-4f);

    // The snare on beat 2 is the imported file: left only, 0.3 (the mixer's 10 ms fade-in is
    // long past).
    CHECK (std::abs (l[24000 + 100] - (0.3f + kick->samples.getSample (0, 24100))) < 1.0e-3f);
    CHECK (std::abs (r[24000 + 100] - kick->samples.getSample (0, 24100)) < 1.0e-3f);
}

TEST_CASE ("A part can loop on its own (the drum editor's Space) without changing the project", "[drums][engine]")
{
    Project project;
    const auto drums = edit::addTrack (project, TrackKind::midi).getId();
    const auto other = edit::addTrack (project, TrackKind::midi).getId();
    edit::setInstrumentType (project, drums, kit::typeId, "808");
    edit::setInstrumentType (project, other, kit::typeId, "808");
    edit::addMidiClip (project, drums, "Kick", 4 * q, 4 * q, { { 36, 127, 0, q / 4 } });
    edit::addMidiClip (project, other, "Snare", 4 * q, 4 * q, { { 38, 127, 0, q / 4 } });
    edit::setMuted (project, { drums }, true);   // even a muted track is heard while you edit it
    const auto before = project.getState().toXmlString();

    auto render = [&] (const PartLoop& loop, int samples)
    {
        AudioEngine engine;
        MixerResources resources;
        engine.prepare (rate, 512);
        auto snapshot = buildTimelineSnapshot (project, rate, {}, &resources, {}, loop);
        CHECK (snapshot->loopEnabled);
        CHECK (snapshot->loopStart == loop.start);
        CHECK (snapshot->loopEnd == loop.end);
        engine.setTimeline (std::move (snapshot));
        engine.locate (loop.start);
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
    };

    const auto kick = builtInDrumSound ("kick-808", rate);
    const auto snare = builtInDrumSound ("snare-808", rate);
    REQUIRE ((kick != nullptr && snare != nullptr));

    // Alone: only the kick, and the bar repeats (4 beats = 96000 samples at 120 BPM).
    auto l = render ({ drums, 4 * q, 8 * q, true }, 100000);
    CHECK (std::abs (l[300] - kick->samples.getSample (0, 300)) < 1.0e-3f);
    CHECK (std::abs (l[96000 + 300] - kick->samples.getSample (0, 300)) < 1.0e-3f);   // again, from the loop

    // With the song: the other track too.
    l = render ({ drums, 4 * q, 8 * q, false }, 2048);
    CHECK (std::abs (l[300] - (kick->samples.getSample (0, 300) + snare->samples.getSample (0, 300))) < 1.0e-3f);

    CHECK (project.getState().toXmlString() == before);
}
