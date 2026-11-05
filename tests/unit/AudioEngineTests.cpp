// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include <algorithm>
#include <cmath>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "engine/AudioEngine.h"

using aether::AudioEngine;
using Catch::Matchers::WithinAbs;

namespace
{
    /** A fake sound card: input and output buffers for the engine to work on. */
    struct FakeDevice
    {
        FakeDevice (int numInputs, int numOutputs, int blockSize)
            : inputData ((size_t) numInputs, std::vector<float> ((size_t) blockSize, 0.0f)),
              outputData ((size_t) numOutputs, std::vector<float> ((size_t) blockSize, 99.0f))
        {
            for (auto& ch : inputData)  inputs.push_back (ch.data());
            for (auto& ch : outputData) outputs.push_back (ch.data());
        }

        void run (AudioEngine& engine, int numSamples)
        {
            engine.process (inputs.data(), (int) inputs.size(), outputs.data(), (int) outputs.size(), numSamples);
        }

        float outputPeak (size_t channel) const
        {
            float peak = 0.0f;
            for (auto s : outputData[channel])
                peak = std::max (peak, std::abs (s));
            return peak;
        }

        std::vector<std::vector<float>> inputData, outputData;
        std::vector<const float*> inputs;
        std::vector<float*> outputs;
    };
}

TEST_CASE ("The engine outputs silence when the test tone is off", "[engine]")
{
    AudioEngine engine;
    engine.prepare (48000.0, 256);

    FakeDevice device (2, 2, 256);
    device.run (engine, 256);

    // The output buffers started full of junk (99.0); the engine must clear them.
    CHECK (device.outputPeak (0) == 0.0f);
    CHECK (device.outputPeak (1) == 0.0f);
    CHECK (engine.takeOutputPeak (0) == 0.0f);
}

TEST_CASE ("The engine plays the test tone and meters it", "[engine]")
{
    AudioEngine engine;
    engine.prepare (48000.0, 512);
    engine.setTestToneEnabled (true);
    CHECK (engine.isTestToneEnabled());

    FakeDevice device (0, 2, 512);

    for (int block = 0; block < 10; ++block)   // past the fade-in
        device.run (engine, 512);

    const auto expected = std::pow (10.0f, -18.0f / 20.0f);
    CHECK_THAT (device.outputPeak (0), WithinAbs (expected, 0.002));
    CHECK_THAT (device.outputPeak (1), WithinAbs (expected, 0.002));

    CHECK_THAT (engine.takeOutputPeak (0), WithinAbs (expected, 0.002));
    // Reading a meter resets it.
    CHECK (engine.takeOutputPeak (0) == 0.0f);
}

TEST_CASE ("The engine measures the input level", "[engine]")
{
    AudioEngine engine;
    engine.prepare (48000.0, 128);

    FakeDevice device (2, 2, 128);
    device.inputData[0][40] = -0.5f;   // a single loud sample on the left input
    device.inputData[1][10] = 0.25f;
    device.run (engine, 128);

    CHECK (engine.takeInputPeak (0) == 0.5f);
    CHECK (engine.takeInputPeak (1) == 0.25f);
    CHECK (engine.takeInputPeak (0) == 0.0f);
}

TEST_CASE ("Meters keep the highest level until they are read", "[engine]")
{
    AudioEngine engine;
    engine.prepare (48000.0, 64);

    FakeDevice device (1, 1, 64);
    device.inputData[0][0] = 0.8f;
    device.run (engine, 64);
    device.inputData[0][0] = 0.1f;
    device.run (engine, 64);

    CHECK (engine.takeInputPeak (0) == 0.8f);
}

TEST_CASE ("The engine copes with unusual device layouts", "[engine]")
{
    AudioEngine engine;
    engine.prepare (44100.0, 256);
    engine.setTestToneEnabled (true);

    SECTION ("mono output")
    {
        FakeDevice device (1, 1, 256);
        device.run (engine, 256);
        CHECK (device.outputPeak (0) > 0.0f);
    }

    SECTION ("more than two channels")
    {
        FakeDevice device (8, 8, 256);
        device.run (engine, 256);
        CHECK (device.outputPeak (7) > 0.0f);
    }

    SECTION ("no inputs and an empty block")
    {
        FakeDevice device (0, 2, 256);
        device.run (engine, 0);
        SUCCEED();
    }

    SECTION ("out-of-range meter channels read as silent")
    {
        CHECK (engine.takeOutputPeak (-1) == 0.0f);
        CHECK (engine.takeOutputPeak (2) == 0.0f);
    }
}
