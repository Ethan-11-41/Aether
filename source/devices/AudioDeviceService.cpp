// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "AudioDeviceService.h"

#include <cmath>

namespace aether
{

AudioDeviceService::AudioDeviceService (juce::PropertySet& settingsToUse)
    : settings (settingsToUse)
{
    // Up to 2 inputs (e.g. a microphone) and 2 outputs (left/right speakers).
    constexpr int numInputChannels = 2;
    constexpr int numOutputChannels = 2;
    constexpr bool selectDefaultDeviceOnFailure = true;

    auto savedState = settings.getXmlValue (settingsKey);
    lastError = deviceManager.initialise (numInputChannels, numOutputChannels,
                                          savedState.get(), selectDefaultDeviceOnFailure);

    deviceManager.addChangeListener (this);
}

AudioDeviceService::~AudioDeviceService()
{
    deviceManager.removeChangeListener (this);
    saveState();
    deviceManager.closeAudioDevice();
}

void AudioDeviceService::addAudioCallback (juce::AudioIODeviceCallback* callback)
{
    deviceManager.addAudioCallback (callback);
}

void AudioDeviceService::removeAudioCallback (juce::AudioIODeviceCallback* callback)
{
    deviceManager.removeAudioCallback (callback);
}

LatencyInfo AudioDeviceService::getLatencyInfo() const
{
    LatencyInfo info;

    if (auto* device = deviceManager.getCurrentAudioDevice())
    {
        info.sampleRate = device->getCurrentSampleRate();
        info.bufferSizeSamples = device->getCurrentBufferSizeSamples();
        info.inputLatencySamples = device->getInputLatencyInSamples();
        info.outputLatencySamples = device->getOutputLatencyInSamples();
    }

    return info;
}

juce::String AudioDeviceService::describeCurrentDevice() const
{
    auto* device = deviceManager.getCurrentAudioDevice();

    if (device == nullptr)
        return {};

    const auto rateKHz = device->getCurrentSampleRate() / 1000.0;
    const auto rateText = juce::String (rateKHz, rateKHz == std::floor (rateKHz) ? 0 : 1) + " kHz";

    const auto separator = juce::String::fromUTF8 (" \xc2\xb7 ");   // " · "
    return device->getTypeName() + separator + device->getName() + separator + rateText;
}

void AudioDeviceService::saveState()
{
    if (auto state = deviceManager.createStateXml())
        settings.setValue (settingsKey, state.get());
}

void AudioDeviceService::changeListenerCallback (juce::ChangeBroadcaster*)
{
    // The user picked a different device, sample rate or buffer size: remember it.
    lastError = {};
    saveState();
}

} // namespace aether
