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

    // MIDI: everything on, now and whenever something is plugged in.
    refreshMidiInputs();
    midiDevicesChanged = juce::MidiDeviceListConnection::make ([this] { refreshMidiInputs(); });

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

void AudioDeviceService::addMidiCallback (juce::MidiInputCallback* callback)
{
    deviceManager.addMidiInputDeviceCallback ({}, callback);   // {} = from every enabled input
}

void AudioDeviceService::removeMidiCallback (juce::MidiInputCallback* callback)
{
    deviceManager.removeMidiInputDeviceCallback ({}, callback);
}

void AudioDeviceService::refreshMidiInputs()
{
    const auto switchedOff = juce::StringArray::fromLines (settings.getValue (midiOffKey));
    const juce::ScopedValueSetter<bool> refreshing (refreshingMidi, true);

    for (const auto& device : juce::MidiInput::getAvailableDevices())
        if (! switchedOff.contains (device.name) && ! deviceManager.isMidiInputDeviceEnabled (device.identifier))
        {
            deviceManager.setMidiInputDeviceEnabled (device.identifier, true);
            juce::Logger::writeToLog ("MIDI input switched on: " + device.name);
        }
}

void AudioDeviceService::rememberMidiChoices()
{
    // The user switched MIDI inputs on or off in the settings: remember the ones that are off.
    auto switchedOff = juce::StringArray::fromLines (settings.getValue (midiOffKey));

    for (const auto& device : juce::MidiInput::getAvailableDevices())
    {
        if (deviceManager.isMidiInputDeviceEnabled (device.identifier))
            switchedOff.removeString (device.name);
        else
            switchedOff.addIfNotAlreadyThere (device.name);
    }

    switchedOff.removeEmptyStrings();
    settings.setValue (midiOffKey, switchedOff.joinIntoString ("\n"));
}

juce::StringArray AudioDeviceService::getEnabledMidiInputNames() const
{
    juce::StringArray names;

    for (const auto& device : juce::MidiInput::getAvailableDevices())
        if (deviceManager.isMidiInputDeviceEnabled (device.identifier))
            names.add (device.name);

    return names;
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

        const auto key = measuredLatencyKey();
        if (key.isNotEmpty() && settings.containsKey (key))
            info.measuredRoundTripSamples = settings.getIntValue (key, -1);
    }

    return info;
}

juce::String AudioDeviceService::measuredLatencyKey() const
{
    auto* device = deviceManager.getCurrentAudioDevice();

    if (device == nullptr)
        return {};

    // A measurement only applies to exactly the setting it was made with.
    const auto setup = deviceManager.getAudioDeviceSetup();
    return "measuredLatency|" + deviceManager.getCurrentAudioDeviceType() + "|" + setup.inputDeviceName + "|"
         + setup.outputDeviceName + "|" + juce::String (device->getCurrentSampleRate()) + "|"
         + juce::String (device->getCurrentBufferSizeSamples());
}

void AudioDeviceService::setMeasuredRoundTrip (int samples)
{
    const auto key = measuredLatencyKey();

    if (key.isEmpty())
        return;

    if (samples < 0)
        settings.removeValue (key);
    else
        settings.setValue (key, samples);
}

int AudioDeviceService::getNumInputChannels() const
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
        return device->getActiveInputChannels().countNumberOfSetBits();

    return 0;
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
    if (! refreshingMidi)
        rememberMidiChoices();

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
