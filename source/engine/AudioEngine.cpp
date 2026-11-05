// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "AudioEngine.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace aether
{

namespace
{
    float blockPeak (const float* data, int numSamples) noexcept AETHER_NONBLOCKING
    {
        float peak = 0.0f;

        for (int i = 0; i < numSamples; ++i)
            peak = std::max (peak, std::abs (data[i]));

        return peak;
    }
}

void AudioEngine::prepare (double sampleRate, int /*maximumBlockSize*/)
{
    testTone.prepare (sampleRate);
}

void AudioEngine::process (const float* const* inputs, int numInputs,
                           float* const* outputs, int numOutputs,
                           int numSamples) noexcept AETHER_NONBLOCKING
{
    // Measure the inputs (e.g. the microphone) so the UI can show they are working.
    for (int ch = 0; ch < std::min (numInputs, numMeteredChannels); ++ch)
        if (inputs != nullptr && inputs[ch] != nullptr)
            updatePeak (inputPeaks[(size_t) ch], blockPeak (inputs[ch], numSamples));

    // Produce the output: the test tone, or silence.
    testTone.setActive (testToneRequested.load (std::memory_order_relaxed));

    if (testTone.isSounding())
    {
        testTone.render (outputs, numOutputs, numSamples);
    }
    else
    {
        for (int ch = 0; ch < numOutputs; ++ch)
            if (outputs[ch] != nullptr)
                std::memset (outputs[ch], 0, sizeof (float) * (size_t) numSamples);
    }

    // Measure the outputs.
    for (int ch = 0; ch < std::min (numOutputs, numMeteredChannels); ++ch)
        if (outputs[ch] != nullptr)
            updatePeak (outputPeaks[(size_t) ch], blockPeak (outputs[ch], numSamples));
}

void AudioEngine::updatePeak (std::atomic<float>& meter, float value) noexcept AETHER_NONBLOCKING
{
    // Keep the highest value until the UI collects it. Lock-free compare-and-swap loop.
    auto previous = meter.load (std::memory_order_relaxed);

    while (value > previous && ! meter.compare_exchange_weak (previous, value, std::memory_order_relaxed))
    {
    }
}

float AudioEngine::takePeak (std::array<std::atomic<float>, numMeteredChannels>& meters, int channel) noexcept
{
    if (channel < 0 || channel >= numMeteredChannels)
        return 0.0f;

    return meters[(size_t) channel].exchange (0.0f, std::memory_order_relaxed);
}

float AudioEngine::takeOutputPeak (int channel) noexcept   { return takePeak (outputPeaks, channel); }
float AudioEngine::takeInputPeak (int channel) noexcept    { return takePeak (inputPeaks, channel); }

//==============================================================================
void AudioEngine::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    if (device != nullptr)
        prepare (device->getCurrentSampleRate(), device->getCurrentBufferSizeSamples());
}

void AudioEngine::audioDeviceStopped()
{
}

void AudioEngine::audioDeviceIOCallbackWithContext (const float* const* inputChannelData, int numInputChannels,
                                                    float* const* outputChannelData, int numOutputChannels,
                                                    int numSamples,
                                                    const juce::AudioIODeviceCallbackContext&)
{
    process (inputChannelData, numInputChannels, outputChannelData, numOutputChannels, numSamples);
}

} // namespace aether
