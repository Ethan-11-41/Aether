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

bool AudioDeviceService::tryToOpen (const juce::AudioDeviceManager::AudioDeviceSetup& setup)
{
    return deviceManager.setAudioDeviceSetup (setup, true).isEmpty() && deviceManager.getCurrentAudioDevice() != nullptr;
}

bool AudioDeviceService::tryToOpenInMono (const juce::AudioDeviceManager::AudioDeviceSetup& setup)
{
    if (setup.inputDeviceName.isEmpty())
        return false;

    auto mono = setup;
    mono.useDefaultInputChannels = false;
    mono.inputChannels.clear();
    mono.inputChannels.setBit (0);

    if (! tryToOpen (mono))
        return false;

    juce::Logger::writeToLog ("Audio device: " + setup.inputDeviceName + " opened in mono");
    return true;
}

juce::String AudioDeviceService::applySetup (const juce::AudioDeviceManager::AudioDeviceSetup& setup)
{
    const auto error = deviceManager.setAudioDeviceSetup (setup, true);

    if (error.isEmpty() && deviceManager.getCurrentAudioDevice() != nullptr)
        return {};

    const auto wantsStereoInput = setup.inputDeviceName.isNotEmpty()
                               && (setup.useDefaultInputChannels || setup.inputChannels.getHighestBit() > 0);

    if (wantsStereoInput && tryToOpenInMono (setup))
        return {};

    return error.isNotEmpty() ? error : juce::String ("The device didn't open.");
}

void AudioDeviceService::switchDriverType (const juce::String& typeName)
{
    if (typeName == deviceManager.getCurrentAudioDeviceType() && deviceManager.getCurrentAudioDevice() != nullptr)
        return;

    // What the user has now: the same speakers (and microphone, or none) are wanted with the new driver.
    const auto previous = deviceManager.getAudioDeviceSetup();
    problemMessage.clear();

    deviceManager.setCurrentAudioDeviceType (typeName, true);   // JUCE's own guess of devices
    auto* type = deviceManager.getCurrentDeviceTypeObject();

    if (type == nullptr || deviceManager.getCurrentAudioDeviceType() != typeName)
        return;

    const auto separate = type->hasSeparateInputsAndOutputs();
    const auto outputs = type->getDeviceNames (false);
    const auto inputs = type->getDeviceNames (true);

    juce::AudioDeviceManager::AudioDeviceSetup wanted;
    wanted.outputDeviceName = outputs.contains (previous.outputDeviceName)
                                  ? previous.outputDeviceName
                                  : outputs[std::max (0, type->getDefaultDeviceIndex (false))];

    if (! separate)
        wanted.inputDeviceName = wanted.outputDeviceName;
    else if (previous.inputDeviceName.isNotEmpty())
        wanted.inputDeviceName = inputs.contains (previous.inputDeviceName)
                                     ? previous.inputDeviceName
                                     : inputs[std::max (0, type->getDefaultDeviceIndex (true))];

    wanted.sampleRate = previous.sampleRate;
    wanted.useDefaultOutputChannels = true;

    // A microphone: its first two inputs (asked for explicitly, so a mono fallback made earlier
    // doesn't stick to the next driver).
    wanted.useDefaultInputChannels = wanted.inputDeviceName.isEmpty();
    if (wanted.inputDeviceName.isNotEmpty())
        wanted.inputChannels.setRange (0, 2, true);

    // JUCE's guess worked and is what the user wants (it adds a microphone even if they had none).
    if (deviceManager.getCurrentAudioDevice() != nullptr)
    {
        const auto now = deviceManager.getAudioDeviceSetup();
        if (! separate || (now.outputDeviceName == wanted.outputDeviceName && now.inputDeviceName == wanted.inputDeviceName))
            return;
    }

    if (tryToOpen (wanted))
        return;

    // That didn't open. Often one device (typically a microphone) can't be used in this mode, or
    // the device only runs at certain sample rates in it.
    const auto firstError = currentDriverError();
    const auto canDropInput = separate && wanted.inputDeviceName.isNotEmpty();
    const auto errorNote = firstError.isNotEmpty() ? "\n\n(Error code: " + firstError + ")" : juce::String();
    juce::Logger::writeToLog ("Audio device: " + typeName + " didn't open with " + wanted.outputDeviceName + " and "
                              + (wanted.inputDeviceName.isNotEmpty() ? wanted.inputDeviceName : juce::String ("no input"))
                              + (firstError.isNotEmpty() ? " (" + firstError + ")" : juce::String()) + "; trying alternatives");

    const auto micDropped = "Your microphone (" + wanted.inputDeviceName + ") can't be used with \"" + typeName
                          + "\" (Aether tried it in stereo and mono, at every common sample rate), so it's switched off "
                            "and nothing can be recorded. Your speakers work in this mode. To record, choose "
                            "\"Windows Audio\" or \"Windows Audio (Low Latency Mode)\" as the driver: their delay is "
                            "nearly as low.";

    const auto kHzText = [] (double rate) { return juce::String (rate / 1000.0, rate == 44100.0 || rate == 88200.0 ? 1 : 0) + " kHz"; };
    const auto triedRate = wanted.sampleRate > 0.0 ? wanted.sampleRate : 48000.0;

    std::vector<double> otherRates;   // the common rates, apart from the one just tried
    for (const auto rate : { 48000.0, 44100.0, 96000.0, 88200.0 })
        if (std::abs (rate - triedRate) >= 0.5)
            otherRates.push_back (rate);

    const auto rateNote = [&] (double rate)
    {
        return "\"" + typeName + "\" only works at " + kHzText (rate) + " with these devices, so Aether switched to "
               "that sample rate.";
    };

    // 1. The microphone in mono (some USB microphones only work in mono in this mode).
    if (canDropInput && tryToOpenInMono (wanted))
        return;

    // 2. Other sample rates, keeping the microphone (in stereo, then in mono).
    for (const auto rate : otherRates)
    {
        auto atRate = wanted;
        atRate.sampleRate = rate;

        if (tryToOpen (atRate) || (canDropInput && tryToOpenInMono (atRate)))
        {
            problemMessage = rateNote (rate) + errorNote;
            juce::Logger::writeToLog ("Audio device: opened at " + juce::String (rate) + " Hz");
            return;
        }
    }

    // 3. Without the microphone: at the same rate, then at the others.
    if (canDropInput)
    {
        auto rates = otherRates;
        rates.insert (rates.begin(), triedRate);

        for (const auto rate : rates)
        {
            auto withoutInput = wanted;
            withoutInput.inputDeviceName = {};
            withoutInput.inputChannels.clear();
            withoutInput.useDefaultInputChannels = true;
            withoutInput.sampleRate = rate;

            if (tryToOpen (withoutInput))
            {
                problemMessage = micDropped
                               + (rate != triedRate ? " (The sample rate is now " + kHzText (rate) + ".)" : juce::String())
                               + errorNote;
                juce::Logger::writeToLog ("Audio device: opened without the microphone at " + juce::String (rate) + " Hz");
                return;
            }
        }
    }

    // 3. Nothing worked: back to the previous setting, with an explanation.
    restoreLastWorkingConfig (typeName, firstError);
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
