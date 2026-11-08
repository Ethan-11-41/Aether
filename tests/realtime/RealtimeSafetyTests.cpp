// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
//
// Real-time safety tests: drive the audio-thread code the way a sound card would.
//
// In a normal build these simply check that nothing goes wrong. In the RealtimeSanitizer build
// (AETHER_RTSAN=ON, run by the Linux job in CI) any memory allocation, lock or blocking system
// call inside an AETHER_NONBLOCKING function aborts the test run with a report showing where.

#include <algorithm>
#include <array>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "dsp/DrumKit.h"
#include "dsp/Effect.h"
#include "dsp/Sampler.h"
#include "dsp/Synth.h"
#include "engine/AudioEngine.h"
#include "engine/TimelineSync.h"
#include "engine/rt/SpscQueue.h"
#include "model/ClipEdits.h"
#include "model/EditCommands.h"
#include "model/MidiClips.h"
#include "model/Mixer.h"
#include "model/Recording.h"
#include "model/TimelineEdits.h"

TEST_CASE ("AudioEngine::process is real-time safe in every state", "[realtime]")
{
    aether::AudioEngine engine;

    for (const double sampleRate : { 44100.0, 48000.0, 96000.0 })
    {
        for (const int blockSize : { 1, 32, 64, 256, 1024, 2048 })
        {
            engine.prepare (sampleRate, blockSize);

            std::vector<float> in1 ((size_t) blockSize, 0.1f), in2 ((size_t) blockSize, -0.1f);
            std::vector<float> out1 ((size_t) blockSize), out2 ((size_t) blockSize);
            const float* inputs[] = { in1.data(), in2.data() };
            float* outputs[] = { out1.data(), out2.data() };

            // Tone off, switching on, playing, switching off, and silent again.
            for (int step = 0; step < 200; ++step)
            {
                engine.setTestToneEnabled (step >= 20 && step < 120);
                engine.process (inputs, 2, outputs, 2, blockSize);
            }

            engine.takeOutputPeak (0);
            engine.takeInputPeak (0);
        }
    }

    SUCCEED ("no real-time violations");
}

TEST_CASE ("SpscQueue push and pop are real-time safe", "[realtime]")
{
    aether::rt::SpscQueue<int> queue (16);
    int value = 0;

    for (int i = 0; i < 1000; ++i)
    {
        queue.tryPush (i);
        queue.tryPop (value);
    }

    SUCCEED ("no real-time violations");
}

TEST_CASE ("The transport and metronome are real-time safe", "[realtime]")
{
    constexpr aether::Tick q = aether::ticksPerQuarter;
    aether::AudioEngine engine;
    engine.prepare (48000.0, 256);
    engine.setMetronomeEnabled (true);
    engine.setCountInBars (1);

    // Tempo ramps, a jump, a signature change and a short loop: every code path of the transport.
    auto timeline = std::make_unique<aether::TimelineSnapshot>();
    timeline->tempo = aether::TempoMap ({ { 0, 90.0, aether::TempoCurve::ramp }, { 8 * q, 180.0, aether::TempoCurve::jump } },
                                        { { 0, 4, 4 }, { 2, 7, 8 } });
    timeline->loopEnabled = true;
    timeline->loopStart = 4 * q;
    timeline->loopEnd = 6 * q;
    engine.setTimeline (std::move (timeline));
    engine.play();

    std::vector<float> left (256), right (256);
    float* outputs[] = { left.data(), right.data() };

    for (int block = 0; block < 2000; ++block)
    {
        // The UI thread keeps publishing new timelines while audio runs; the swap must be safe.
        if (block % 100 == 50)
        {
            engine.setTimeline (std::make_unique<aether::TimelineSnapshot>());
            engine.collectGarbage();
        }

        if (block == 1500) engine.stop();
        if (block == 1600) { engine.locate (2 * q); engine.play(); }

        engine.process (nullptr, 0, outputs, 2, 256);
    }

    engine.collectGarbage();
    SUCCEED ("no real-time violations");
}

TEST_CASE ("Clip playback is real-time safe", "[realtime]")
{
    constexpr aether::Tick q = aether::ticksPerQuarter;
    aether::AudioEngine engine;
    engine.prepare (48000.0, 256);

    auto audio = std::make_shared<aether::AudioData>();
    audio->sampleRate = 48000.0;
    audio->samples.setSize (2, 48000);
    audio->samples.clear();

    auto timeline = std::make_unique<aether::TimelineSnapshot>();
    timeline->sampleRate = 48000.0;
    timeline->loopEnabled = true;
    timeline->loopStart = 0;
    timeline->loopEnd = 4 * q;

    aether::TrackPlayback track;
    for (int i = 0; i < 8; ++i)
    {
        aether::ClipPlayback clip;
        clip.audio = audio.get();
        clip.start = i * 12000;
        clip.duration = 30000;
        clip.contentLength = 7000;
        clip.loop = (i % 2) == 0;
        clip.fadeIn = 1000;
        clip.fadeOut = 2000;
        clip.gain = 0.5f;
        track.clips.push_back (clip);
    }

    timeline->tracks.push_back (track);
    timeline->keepAlive.push_back (audio);
    engine.setTimeline (std::move (timeline));
    engine.play();

    std::vector<float> left (256), right (256), mono (256);
    float* stereo[] = { left.data(), right.data() };
    float* single[] = { mono.data() };

    for (int block = 0; block < 1000; ++block)
        engine.process (nullptr, 0, block % 2 == 0 ? stereo : single, block % 2 == 0 ? 2 : 1, 256);

    SUCCEED ("no real-time violations");
}

TEST_CASE ("The mixer, buses, sends and effects are real-time safe", "[realtime]")
{
    using namespace aether;
    Project project;
    const auto a = edit::addTrack (project, TrackKind::audio).getId();
    const auto b = edit::addTrack (project, TrackKind::audio).getId();
    const auto group = edit::addTrack (project, TrackKind::bus).getId();
    const auto fx = edit::addTrack (project, TrackKind::bus).getId();

    edit::addAudioClip (project, a, { "Audio/a.wav", "a", 48000.0, 96000 }, 0);
    edit::addAudioClip (project, b, { "Audio/b.wav", "b", 48000.0, 96000 }, 0);
    edit::setOutput (project, { a, b }, group);
    edit::addSend (project, a, fx);
    edit::setSendPreFader (project, edit::addSend (project, b, fx), true);
    edit::addInsert (project, a, "delay");
    edit::addInsert (project, fx, "reverb");
    edit::addInsert (project, mixer::masterId, "delay");
    edit::setPan (project, a, -0.3);

    auto audio = std::make_shared<AudioData>();
    audio->sampleRate = 48000.0;
    audio->samples.setSize (2, 96000);
    audio->samples.clear();
    audio->samples.setSample (0, 100, 1.0f);

    AudioEngine engine;
    engine.prepare (48000.0, 2048);
    MixerResources resources;
    auto resolver = [audio] (const juce::String&) { return audio; };
    engine.setTimeline (buildTimelineSnapshot (project, 48000.0, resolver, &resources));
    engine.play();

    std::vector<float> left (2048), right (2048);
    float* outputs[] = { left.data(), right.data() };

    for (int block = 0; block < 600; ++block)
    {
        // The UI keeps editing (volume, mute, solo, effect settings) while audio runs.
        if (block % 40 == 20)
        {
            edit::setVolume (project, a, -(double) (block % 12));
            edit::setMuted (project, { b }, block % 80 == 20);
            edit::setSoloed (project, { group }, block % 120 == 20);
            engine.setTimeline (buildTimelineSnapshot (project, 48000.0, resolver, &resources));
            engine.collectGarbage();
        }

        const auto blockSize = 1 + (block * 37) % 2048;   // odd sizes, and blocks bigger than a mixer chunk
        engine.process (nullptr, 0, outputs, block % 3 == 0 ? 1 : 2, blockSize);
    }

    engine.collectGarbage();
    SUCCEED ("no real-time violations");
}

TEST_CASE ("Recording, monitoring and latency measurement are real-time safe", "[realtime]")
{
    using namespace aether;
    Project project;
    const auto a = edit::addTrack (project, TrackKind::audio).getId();
    const auto b = edit::addTrack (project, TrackKind::audio).getId();
    edit::setMonitoring (project, { a, b }, true);
    edit::setInput (project, { b }, 0, true);
    edit::setLoopRange (project, 0, ticksPerQuarter);
    edit::setLoopEnabled (project, true);

    AudioEngine engine;
    engine.prepare (48000.0, 2048);
    engine.setCountInBars (1);

    auto ring = std::make_shared<RecordingRing> (std::vector<int> { 0, 1, 5 }, 4096);   // small: it will overflow
    auto timeline = buildTimelineSnapshot (project, 48000.0);
    timeline->recordRing = ring;
    engine.setTimeline (std::move (timeline));
    engine.record();

    std::vector<float> in1 (2048, 0.1f), in2 (2048, -0.1f), left (2048), right (2048);
    const float* inputs[] = { in1.data(), in2.data() };
    float* outputs[] = { left.data(), right.data() };
    juce::AudioBuffer<float> drained (3, 4096);

    for (int blockNumber = 0; blockNumber < 400; ++blockNumber)
    {
        if (blockNumber == 200)
        {
            engine.stop();
            engine.startLatencyCapture (std::make_shared<LatencyCapture> (48000.0, 0));
        }

        if (blockNumber == 100)
            ring->pop (drained, 4096);   // the "disk writer" catches up once

        const auto blockSize = 1 + (blockNumber * 53) % 2048;
        engine.process (inputs, blockNumber % 4 == 0 ? 1 : 2, outputs, 2, blockSize);
    }

    engine.collectGarbage();
    CHECK (ring->getDroppedFrames() > 0);
    SUCCEED ("no real-time violations");
}

TEST_CASE ("MIDI playback, the synth, live MIDI and MIDI recording are real-time safe", "[realtime]")
{
    using namespace aether;
    Project project;
    const auto keys = edit::addTrack (project, TrackKind::midi).getId();
    const auto pad = edit::addTrack (project, TrackKind::midi).getId();
    edit::setInstrumentPreset (project, pad, "Soft pad");

    std::vector<NoteData> notes;
    for (int i = 0; i < 64; ++i)
        notes.push_back ({ 40 + i % 40, 100, i * ticksPerQuarter / 4, ticksPerQuarter });
    edit::addMidiClip (project, keys, "a", 0, 16 * ticksPerQuarter, notes);
    edit::addMidiClip (project, pad, "b", 0, 16 * ticksPerQuarter, notes);
    edit::setArmed (project, { keys }, true);
    edit::setLoopRange (project, 0, 2 * ticksPerQuarter);
    edit::setLoopEnabled (project, true);

    AudioEngine engine;
    engine.prepare (48000.0, 2048);
    MixerResources resources;
    auto timeline = buildTimelineSnapshot (project, 48000.0, {}, &resources, pad);
    timeline->recordRing = std::make_shared<RecordingRing> (std::vector<int> {}, 4096);
    engine.setTimeline (std::move (timeline));
    engine.record();

    std::vector<float> left (2048), right (2048);
    float* outputs[] = { left.data(), right.data() };
    RecordedMidiMessage drained;

    for (int block = 0; block < 400; ++block)
    {
        // A keyboard playing all the while (from "MIDI threads" and the on-screen keyboard).
        engine.handleIncomingMidiMessage (nullptr, juce::MidiMessage::noteOn (1, 40 + block % 50, (juce::uint8) 90));
        engine.handleIncomingMidiMessage (nullptr, juce::MidiMessage::controllerEvent (1, 64, block % 2 == 0 ? 127 : 0));
        engine.handleIncomingMidiMessage (nullptr, juce::MidiMessage::pitchWheel (1, 8192 + block));
        engine.sendLiveMidi (juce::MidiMessage::noteOff (1, 40 + (block + 25) % 50));
        engine.sendPreviewNote (pad, 50 + block % 30, 100, block % 3 != 0);   // the piano roll

        if (block == 200) engine.stop();
        if (block == 250) { engine.locate (0); engine.play(); }
        if (block % 50 == 0)
            while (engine.takeRecordedMidi (drained)) {}

        engine.process (nullptr, 0, outputs, 2, 1 + (block * 71) % 2048);
    }

    engine.collectGarbage();
    SUCCEED ("no real-time violations");
}

TEST_CASE ("The Synth is real-time safe with every preset", "[realtime]")
{
    using namespace aether::dsp;
    const auto* type = findEffectType (synth::typeId);
    REQUIRE (type != nullptr);

    auto instrument = createInstrument (synth::typeId);
    instrument->prepare (48000.0, 2048);
    std::vector<float> left (2048), right (2048);

    int block = 0;
    for (const auto& preset : type->presets)
    {
        EffectParameterValues values {};
        for (size_t i = 0; i < preset.values.size(); ++i)
            values[i] = preset.values[i];

        for (int b = 0; b < 12; ++b, ++block)
        {
            std::array<NoteEvent, 6> events {};
            for (int i = 0; i < 4; ++i)
            {
                events[(size_t) i].type = b % 4 == 3 ? NoteEvent::Type::noteOff : NoteEvent::Type::noteOn;
                events[(size_t) i].pitch = (std::uint8_t) (40 + (block * 7 + i * 5) % 50);
                events[(size_t) i].velocity = 90;
                events[(size_t) i].offset = i * 37;
            }
            events[4].type = NoteEvent::Type::modWheel;
            events[4].value = (float) (b % 5) / 4.0f;
            events[4].offset = 300;
            events[5].type = b == 11 ? NoteEvent::Type::allNotesOff : NoteEvent::Type::pitchBend;
            events[5].value = 0.3f;
            events[5].offset = 400;

            instrument->process (left.data(), right.data(), 1 + (block * 131) % 2048, values, events.data(), (int) events.size());
        }
    }

    CHECK (std::all_of (left.begin(), left.end(), [] (float x) { return std::isfinite (x); }));
}

TEST_CASE ("The drum kit is real-time safe", "[realtime]")
{
    using namespace aether::dsp;
    auto instrument = createInstrument (drumkit::typeId);
    instrument->prepare (48000.0, 2048);

    // Sounds made beforehand (as the engine's snapshot holds them).
    std::vector<juce::AudioBuffer<float>> buffers;
    for (int pad = 0; pad < drums::numPads; ++pad)
        buffers.push_back (drums::render (drums::kits().front().pads[(size_t) pad].sound, 48000.0));

    InstrumentSounds sounds;
    for (int pad = 0; pad < drums::numPads; ++pad)
        sounds.sounds[(size_t) pad] = &buffers[(size_t) pad];

    EffectParameterValues values {};
    const auto preset = drumkit::presets().front();
    for (size_t i = 0; i < preset.values.size(); ++i)
        values[i] = preset.values[i];

    std::vector<float> left (2048), right (2048);
    for (int block = 0; block < 200; ++block)
    {
        std::array<NoteEvent, 8> events {};
        for (int i = 0; i < 8; ++i)
        {
            events[(size_t) i].type = NoteEvent::Type::noteOn;
            events[(size_t) i].pitch = (std::uint8_t) (36 + (block * 3 + i * 5) % 16);
            events[(size_t) i].velocity = (std::uint8_t) (40 + i * 10);
            events[(size_t) i].offset = i * 61;
        }
        if (block % 50 == 49)
            events[7].type = NoteEvent::Type::allNotesOff;

        instrument->process (left.data(), right.data(), 1 + (block * 131) % 2048, values, events.data(), 8,
                             block % 37 == 0 ? nullptr : &sounds);
    }

    CHECK (std::all_of (left.begin(), left.end(), [] (float x) { return std::isfinite (x); }));
}

TEST_CASE ("The sampler is real-time safe in every voice mode", "[realtime]")
{
    using namespace aether::dsp;

    // Recordings made beforehand (as the engine's snapshot holds them): a few zones over the
    // keyboard, two velocity layers, round robin, a loop and a one-shot.
    std::vector<juce::AudioBuffer<float>> buffers;
    for (int i = 0; i < 8; ++i)
    {
        juce::AudioBuffer<float> b (1 + i % 2, 4000 + i * 1000);
        for (int c = 0; c < b.getNumChannels(); ++c)
            for (int s = 0; s < b.getNumSamples(); ++s)
                b.setSample (c, s, 0.3f * (float) std::sin (0.05 * s * (i + 1)));
        buffers.push_back (std::move (b));
    }

    SampleMap map;
    for (int i = 0; i < 8; ++i)
    {
        SampleZone z;
        z.sample = &buffers[(size_t) i];
        z.lowKey = 30 + (i / 2) * 20;
        z.highKey = z.lowKey + 19;
        z.rootKey = z.lowKey + 10;
        z.lowVelocity = i % 2 == 0 ? 1 : 64;
        z.highVelocity = i % 2 == 0 ? 63 : 127;
        z.sequenceLength = i == 2 ? 2 : 1;
        z.loopStart = i == 3 ? 500 : -1;
        z.loopEnd = i == 3 ? 3000 : -1;
        z.oneShot = i == 5;
        map.zones.push_back (z);
    }

    InstrumentSounds sounds;
    sounds.map = &map;
    SampleMap empty;
    InstrumentSounds none;
    none.map = &empty;

    for (const auto mode : { sampler::poly, sampler::mono, sampler::slide })
    {
        auto instrument = createInstrument (sampler::typeId);
        instrument->prepare (48000.0, 2048);

        EffectParameterValues values {};
        const auto params = sampler::parameters();
        for (size_t i = 0; i < params.size(); ++i)
            values[i] = params[i].defaultValue;
        values[(size_t) sampler::voiceMode] = (float) mode;
        values[(size_t) sampler::tone] = 3000.0f;

        std::vector<float> left (2048), right (2048);
        for (int block = 0; block < 200; ++block)
        {
            std::array<NoteEvent, 8> events {};
            for (int i = 0; i < 8; ++i)
            {
                auto& e = events[(size_t) i];
                e.type = i % 3 == 2 ? NoteEvent::Type::noteOff : NoteEvent::Type::noteOn;
                e.pitch = (std::uint8_t) (30 + (block * 7 + i * 11) % 80);
                e.velocity = (std::uint8_t) (20 + i * 13);
                e.offset = i * 61;
                e.fromClip = i % 2 == 0;
                e.length = 3000;
            }
            if (block % 50 == 49) events[7].type = NoteEvent::Type::allNotesOff;
            if (block % 20 == 10) { events[6].type = NoteEvent::Type::sustain; events[6].value = (float) (block % 40 == 10); }
            if (block % 30 == 15) { events[5].type = NoteEvent::Type::pitchBend; events[5].value = 0.5f; }

            instrument->process (left.data(), right.data(), 1 + (block * 131) % 2048, values, events.data(), 8,
                                 block % 37 == 0 ? &none : &sounds);
        }

        CHECK (std::all_of (left.begin(), left.end(), [] (float x) { return std::isfinite (x); }));
    }
}

TEST_CASE ("Every effect is real-time safe with every preset, while its settings move", "[realtime]")
{
    using namespace aether::dsp;

    for (const auto& type : effectTypes())
    {
        if (type.isInstrument)
            continue;

        auto effect = createEffect (type.id);
        REQUIRE (effect != nullptr);
        effect->prepare (48000.0, 2048);

        std::vector<float> left (2048), right (2048);
        EffectContext context;
        context.sampleRate = 48000.0;

        for (size_t p = 0; p < type.presets.size(); ++p)
        {
            EffectParameterValues values {};
            for (size_t i = 0; i < type.presets[p].values.size(); ++i)
                values[i] = type.presets[p].values[i];

            for (int block = 0; block < 40; ++block)
            {
                // Something to work on, loud and quiet; settings moving between blocks.
                for (size_t i = 0; i < left.size(); ++i)
                {
                    left[i] = (float) std::sin (0.01 * (double) (i + (size_t) block * 7)) * (block % 3 == 0 ? 1.5f : 0.05f);
                    right[i] = -left[i];
                }

                const auto& parameter = type.parameters[(size_t) block % type.parameters.size()];
                values[(size_t) block % type.parameters.size()] = parameter.clamp (parameter.minimum + (parameter.maximum - parameter.minimum)
                                                                                                       * (float) ((block * 37) % 100) / 100.0f);
                context.bpm = 60.0 + block * 3.0;
                effect->process (left.data(), right.data(), 1 + (block * 131) % 2048, values, context);
                juce::ignoreUnused (effect->getMeter());
            }
        }

        INFO (type.name);
        CHECK (std::all_of (left.begin(), left.end(), [] (float x) { return std::isfinite (x); }));
    }
}
