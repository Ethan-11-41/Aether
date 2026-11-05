// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "engine/TestToneGenerator.h"

using aether::TestToneGenerator;
using Catch::Matchers::WithinAbs;

namespace
{
    std::vector<float> renderMono (TestToneGenerator& tone, int numSamples)
    {
        std::vector<float> buffer ((size_t) numSamples, 123.0f);   // garbage, must be overwritten
        float* channels[] = { buffer.data() };
        tone.render (channels, 1, numSamples);
        return buffer;
    }

    int countUpwardZeroCrossings (const std::vector<float>& samples)
    {
        int count = 0;
        for (size_t i = 1; i < samples.size(); ++i)
            if (samples[i - 1] < 0.0f && samples[i] >= 0.0f)
                ++count;
        return count;
    }

    float peakOf (const std::vector<float>& samples, size_t from = 0)
    {
        float peak = 0.0f;
        for (size_t i = from; i < samples.size(); ++i)
            peak = std::max (peak, std::abs (samples[i]));
        return peak;
    }
}

TEST_CASE ("The tone is silent until it is switched on", "[tone]")
{
    TestToneGenerator tone;
    tone.prepare (48000.0);

    auto output = renderMono (tone, 512);
    CHECK (peakOf (output) == 0.0f);
    CHECK_FALSE (tone.isSounding());
}

TEST_CASE ("The tone plays at the requested frequency", "[tone]")
{
    for (const double sampleRate : { 44100.0, 48000.0, 96000.0 })
    {
        TestToneGenerator tone;
        tone.prepare (sampleRate);
        tone.setFrequency (1000.0);
        tone.setActive (true);

        auto output = renderMono (tone, (int) sampleRate);   // one second
        const auto cycles = countUpwardZeroCrossings (output);

        INFO ("sample rate " << sampleRate);
        CHECK (std::abs (cycles - 1000) <= 1);
    }
}

TEST_CASE ("The tone plays at the default level of -18 dBFS", "[tone]")
{
    TestToneGenerator tone;
    tone.prepare (48000.0);
    tone.setActive (true);

    auto output = renderMono (tone, 48000);
    const auto expectedPeak = std::pow (10.0f, -18.0f / 20.0f);

    // Ignore the 20 ms fade-in at the start.
    CHECK_THAT (peakOf (output, 2000), WithinAbs (expectedPeak, 0.001));
}

TEST_CASE ("Switching the tone on and off fades instead of clicking", "[tone]")
{
    constexpr double sampleRate = 48000.0;
    TestToneGenerator tone;
    tone.prepare (sampleRate);
    tone.setActive (true);

    auto fadeIn = renderMono (tone, 4800);

    // The very first samples must be tiny: no sudden jump from silence.
    CHECK (std::abs (fadeIn[0]) < 0.001f);

    // The biggest possible step between neighbouring samples of a clean sine wave.
    const auto maxStep = static_cast<float> (2.0 * std::numbers::pi * tone.getFrequency() / sampleRate) * tone.getLevelGain() * 1.05f;

    for (size_t i = 1; i < fadeIn.size(); ++i)
        REQUIRE (std::abs (fadeIn[i] - fadeIn[i - 1]) <= maxStep);

    tone.setActive (false);
    auto fadeOut = renderMono (tone, 4800);
    REQUIRE (std::abs (fadeOut[0] - fadeIn.back()) <= maxStep);

    for (size_t i = 1; i < fadeOut.size(); ++i)
        REQUIRE (std::abs (fadeOut[i] - fadeOut[i - 1]) <= maxStep);

    // After the fade (20 ms = 960 samples) it is completely silent.
    CHECK (peakOf (fadeOut, 1000) == 0.0f);
    CHECK_FALSE (tone.isSounding());
}

TEST_CASE ("The tone is written to every channel", "[tone]")
{
    TestToneGenerator tone;
    tone.prepare (48000.0);
    tone.setActive (true);

    std::vector<float> left (256), right (256);
    float* channels[] = { left.data(), right.data() };
    tone.render (channels, 2, 256);

    CHECK (left == right);
    CHECK (peakOf (left) > 0.0f);
}

TEST_CASE ("Odd frequencies and levels are kept within safe limits", "[tone]")
{
    TestToneGenerator tone;
    tone.prepare (48000.0);

    tone.setFrequency (-5.0);
    CHECK (tone.getFrequency() >= 1.0);
    tone.setFrequency (1.0e6);
    CHECK (tone.getFrequency() <= 20000.0);

    tone.setLevelDb (+12.0f);   // never louder than full scale
    CHECK (tone.getLevelGain() <= 1.0f);
}
