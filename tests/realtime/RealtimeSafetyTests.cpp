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
