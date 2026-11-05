// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "TestToneGenerator.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace aether
{

void TestToneGenerator::prepare (double newSampleRate) noexcept
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    fadeStep = static_cast<float> (1.0 / std::max (1.0, fadeTimeSeconds * sampleRate));
    updatePhaseIncrement();

    if (levelGain == 0.0f)
        setLevelDb (defaultLevelDb);
}

void TestToneGenerator::setFrequency (double hz) noexcept
{
    frequencyHz = std::clamp (hz, 1.0, 20000.0);
    updatePhaseIncrement();
}

void TestToneGenerator::setLevelDb (float decibels) noexcept
{
    levelGain = std::pow (10.0f, std::min (decibels, 0.0f) / 20.0f);
}

void TestToneGenerator::setActive (bool shouldBeActive) noexcept AETHER_NONBLOCKING
{
    targetGain = shouldBeActive ? 1.0f : 0.0f;
}

void TestToneGenerator::updatePhaseIncrement() noexcept
{
    phaseIncrement = 2.0 * std::numbers::pi * frequencyHz / sampleRate;
}

void TestToneGenerator::render (float* const* channels, int numChannels, int numSamples) noexcept AETHER_NONBLOCKING
{
    constexpr double twoPi = 2.0 * std::numbers::pi;

    for (int i = 0; i < numSamples; ++i)
    {
        // Move the fade gain one step towards its target.
        if (currentGain < targetGain)
            currentGain = std::min (targetGain, currentGain + fadeStep);
        else if (currentGain > targetGain)
            currentGain = std::max (targetGain, currentGain - fadeStep);

        float sample = 0.0f;

        if (currentGain > 0.0f)
        {
            sample = static_cast<float> (std::sin (phase)) * levelGain * currentGain;
            phase += phaseIncrement;

            if (phase >= twoPi)
                phase -= twoPi;
        }
        else
        {
            phase = 0.0;   // restart from zero so the next fade-in starts cleanly
        }

        for (int ch = 0; ch < numChannels; ++ch)
            if (channels[ch] != nullptr)
                channels[ch][i] = sample;
    }
}

} // namespace aether
