// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
//
// Real-time safety tests: drive the audio-thread code the way a sound card would.
//
// In a normal build these simply check that nothing goes wrong. In the RealtimeSanitizer build
// (AETHER_RTSAN=ON, run by the Linux job in CI) any memory allocation, lock or blocking system
// call inside an AETHER_NONBLOCKING function aborts the test run with a report showing where.

#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "engine/AudioEngine.h"
#include "engine/rt/SpscQueue.h"

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
