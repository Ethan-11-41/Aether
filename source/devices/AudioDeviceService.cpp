// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "AudioDeviceService.h"

#include "DriverErrors.h"

#include <cmath>

namespace aether
{

AudioDeviceService::AudioDeviceService (juce::PropertySet& settingsToUse, DeviceTypeList customDeviceTypes)
    : settings (settingsToUse),
      runDeferred ([] (std::function<void()> work) { juce::MessageManager::callAsync (std::move (work)); })
{
    // When custom types are added first, JUCE doesn't add the real drivers.
    for (auto& type : customDeviceTypes)
        deviceManager.addAudioDeviceType (std::move (type));

    // Up to 2 inputs (e.g. a microphone) and 2 outputs (left/right speakers).
    constexpr int numInputChannels = 2;
    constexpr int numOutputChannels = 2;
    constexpr bool selectDefaultDeviceOnFailure = true;

    auto savedState = settings.getXmlValue (settingsKey);
    startupError = deviceManager.initialise (numInputChannels, numOutputChannels,
                                             savedState.get(), selectDefaultDeviceOnFailure);

    if (deviceManager.getCurrentAudioDevice() != nullptr)
        lastWorking = WorkingConfig { deviceManager.getCurrentAudioDeviceType(), deviceManager.getAudioDeviceSetup() };

    deviceManager.addChangeListener (this);
}

AudioDeviceService::~AudioDeviceService()
{
    deviceManager.removeChangeListener (this);
    saveState();
    deviceManager.closeAudioDevice();
}

void AudioDeviceService::setDeferredRunner (std::function<void (std::function<void()>)> runner)
{
    runDeferred = std::move (runner);
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
        info.hasInput = ! device->getActiveInputChannels().isZero();
        info.hasOutput = ! device->getActiveOutputChannels().isZero();
        info.inputLatencySamples = info.hasInput ? device->getInputLatencyInSamples() : 0;
        info.outputLatencySamples = info.hasOutput ? device->getOutputLatencyInSamples() : 0;
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
    handleDeviceChange();
}

void AudioDeviceService::handleDeviceChange()
{
    if (deviceManager.getCurrentAudioDevice() != nullptr)
    {
        // Something is open and running: remember it as the setting to go back to.
        lastWorking = WorkingConfig { deviceManager.getCurrentAudioDeviceType(), deviceManager.getAudioDeviceSetup() };
        saveState();
        juce::Logger::writeToLog ("Audio device: " + describeCurrentDevice());
        return;
    }

    // Nothing could be opened after a change. Go back to the last setting that worked, but do it
    // after the current change has finished (the settings panel is still in the middle of it).
    if (lastWorking.has_value() && ! restorePending)
    {
        restorePending = true;
        const auto failedType = deviceManager.getCurrentAudioDeviceType();
        const auto driverError = driverErrorSource ? driverErrorSource() : juce::String();
        juce::Logger::writeToLog ("Audio device: couldn't open a " + failedType + " device"
                                  + (driverError.isNotEmpty() ? " (" + driverError + ")" : juce::String())
                                  + "; going back to the previous setting");
        juce::WeakReference<AudioDeviceService> weakThis (this);

        runDeferred ([weakThis, failedType, driverError]
        {
            if (auto* self = weakThis.get())
                self->restoreLastWorkingConfig (failedType, driverError);
        });
    }
}

void AudioDeviceService::setDriverErrorSource (std::function<juce::String()> source)
{
    driverErrorSource = std::move (source);
}

void AudioDeviceService::restoreLastWorkingConfig (const juce::String& failedTypeName, const juce::String& driverError)
{
    restorePending = false;

    if (! lastWorking.has_value() || deviceManager.getCurrentAudioDevice() != nullptr)
        return;

    const auto config = *lastWorking;

    if (deviceManager.getCurrentAudioDeviceType() != config.typeName)
        deviceManager.setCurrentAudioDeviceType (config.typeName, true);

    auto error = deviceManager.setAudioDeviceSetup (config.setup, true);

    if (error.isEmpty() && deviceManager.getCurrentAudioDevice() != nullptr)
    {
        problemMessage = explainOpenFailure (failedTypeName, describeCurrentDevice(), driverError);
        saveState();
    }
    else
    {
        lastWorking.reset();   // stop trying, or we'd retry forever
        problemMessage = "Aether couldn't open that device, and couldn't go back to your previous "
                         "settings either (" + error + "). Try choosing \"Windows Audio\" as the device type.";
    }

    // Tell everyone watching (including the settings panel) to show the restored settings.
    deviceManager.sendChangeMessage();
}

juce::String AudioDeviceService::explainOpenFailure (const juce::String& driverTypeName, const juce::String& restoredDescription,
                                                    const juce::String& driverError)
{
    const auto intro = "That device setting couldn't be opened, so Aether went back to your previous settings ("
                       + restoredDescription + ").\n\n";

    // If the driver said exactly what went wrong, explain that.
    if (const auto specific = explainDriverError (driverError); specific.isNotEmpty())
        return intro + "Windows reported that " + specific + "\n\n(Error code: " + driverError + ")";

    const auto codeNote = driverError.isNotEmpty() ? "\n\n(Error code: " + driverError + ")" : juce::String();

    if (driverTypeName.containsIgnoreCase ("Exclusive"))
        return intro
             + "In Exclusive Mode a device can't be shared, so this usually means another program "
               "(a web browser, Discord, Teams, a game...) is using it, or Windows doesn't allow "
               "exclusive control of it. To allow it: Windows Sound settings > the device > Properties > "
               "Advanced > tick \"Allow applications to take exclusive control of this device\". "
               "Some built-in microphones never work in Exclusive Mode: \"Windows Audio\" works with every device."
             + codeNote;

    if (driverTypeName.containsIgnoreCase ("ASIO"))
        return intro
             + "The ASIO driver refused to start. Another program may be using the audio interface, "
               "or the sample rate isn't supported. Close other audio programs, or check the interface's "
               "own control panel."
             + codeNote;

    return intro
         + "Another program may be using the device in exclusive mode, or it may have been unplugged."
         + codeNote;
}

} // namespace aether
