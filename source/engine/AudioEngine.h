// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <array>
#include <atomic>

#include <juce_audio_devices/juce_audio_devices.h>

#include "TestToneGenerator.h"
#include "rt/RealtimeAnnotations.h"

namespace aether
{

/** The audio engine: everything that runs on the audio thread.

    Milestone 0 version: plays the test tone and measures input/output levels.
    The UI thread talks to it only through atomics (no locks), as required by the
    real-time rules in docs/ARCHITECTURE.md.
*/
class AudioEngine final : public juce::AudioIODeviceCallback
{
public:
    static constexpr int numMeteredChannels = 2;

    AudioEngine() = default;

    //==============================================================================
    // Called from the UI thread

    void setTestToneEnabled (bool shouldPlay) noexcept   { testToneRequested.store (shouldPlay, std::memory_order_relaxed); }
    bool isTestToneEnabled() const noexcept              { return testToneRequested.load (std::memory_order_relaxed); }

    /** Highest output level (linear, 0..1+) since the last call, then resets it. */
    float takeOutputPeak (int channel) noexcept;

    /** Highest input (e.g. microphone) level since the last call, then resets it. */
    float takeInputPeak (int channel) noexcept;

    //==============================================================================
    // The engine itself. prepare() is called before audio starts; process() on the audio thread.
    // These are public so the automated tests can drive the engine without a sound card.

    void prepare (double sampleRate, int maximumBlockSize);

    void process (const float* const* inputs, int numInputs,
                  float* const* outputs, int numOutputs,
                  int numSamples) noexcept AETHER_NONBLOCKING;

    //==============================================================================
    // juce::AudioIODeviceCallback

    void audioDeviceAboutToStart (juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;
    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData, int numInputChannels,
                                           float* const* outputChannelData, int numOutputChannels,
                                           int numSamples,
                                           const juce::AudioIODeviceCallbackContext& context) override;

private:
    static void updatePeak (std::atomic<float>& meter, float value) noexcept AETHER_NONBLOCKING;
    static float takePeak (std::array<std::atomic<float>, numMeteredChannels>& meters, int channel) noexcept;

    TestToneGenerator testTone;
    std::atomic<bool> testToneRequested { false };

    std::array<std::atomic<float>, numMeteredChannels> outputPeaks {};
    std::array<std::atomic<float>, numMeteredChannels> inputPeaks {};
};

} // namespace aether
