// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_data_structures/juce_data_structures.h>

#include "LatencyInfo.h"

namespace aether
{

/** Owns the connection to the sound card (via JUCE's AudioDeviceManager).

    - Opens the device the user chose last time (or the system default on first run).
    - Remembers the user's choice (device, sample rate, buffer size) in the settings file.
    - If the user picks a setting that fails to open, it goes back to the last setting that
      worked and explains the likely reason (see getProblemMessage()), instead of leaving
      Aether with no sound at all.
    - Reports latency and a human-readable description of the current device.

    Use from the UI (message) thread only.
*/
class AudioDeviceService final : private juce::ChangeListener
{
public:
    using DeviceTypeList = std::vector<std::unique_ptr<juce::AudioIODeviceType>>;

    /** `settings` is where the device choice is remembered; it must outlive this object.
        `customDeviceTypes` replaces the real sound-card drivers (used by the automated tests). */
    explicit AudioDeviceService (juce::PropertySet& settings, DeviceTypeList customDeviceTypes = {});
    ~AudioDeviceService() override;

    juce::AudioDeviceManager& getDeviceManager() noexcept   { return deviceManager; }

    void addAudioCallback (juce::AudioIODeviceCallback* callback);
    void removeAudioCallback (juce::AudioIODeviceCallback* callback);

    /** MIDI from every switched-on MIDI input goes to `callback` (on a MIDI thread). */
    void addMidiCallback (juce::MidiInputCallback* callback);
    void removeMidiCallback (juce::MidiInputCallback* callback);

    /** Switches on every MIDI input (keyboards, the Kawai over USB...) except those the user
        switched off in the settings. Called at start-up and whenever a MIDI device is plugged in
        or out; public so the tests can call it. */
    void refreshMidiInputs();

    /** The names of the MIDI inputs that are switched on. */
    juce::StringArray getEnabledMidiInputNames() const;

    static constexpr const char* midiOffKey = "midiInputsSwitchedOff";

    /** Switches to another kind of driver (e.g. "Windows Audio (Exclusive Mode)"). If the usual
        setting doesn't open, it tries again without the microphone, then at other sample rates,
        and keeps the first that works, explaining the change in getProblemMessage(). If nothing
        works, it goes back to the previous setting (and explains why). */
    void switchDriverType (const juce::String& typeName);

    /** Applies a device setting (from the settings panel). If the microphone refuses it in stereo,
        tries it in mono (some USB microphones only work in mono in Exclusive Mode). Returns the
        error if neither worked (Aether then goes back to the previous setting, as usual). */
    juce::String applySetup (const juce::AudioDeviceManager::AudioDeviceSetup& setup);

        /** Latency of the currently open device (invalid if no device is open), including the
        measured round trip if this exact setting (driver, devices, sample rate, buffer size)
        has been measured. */
    LatencyInfo getLatencyInfo() const;

    /** Remembers a measured round trip for the current setting (-1 forgets it). */
    void setMeasuredRoundTrip (int samples);

    /** How many inputs the current device has switched on. */
    int getNumInputChannels() const;

    /** e.g. "Windows Audio · Speakers (Realtek(R) Audio) · 48 kHz". Empty if no device is open. */
    juce::String describeCurrentDevice() const;

    /** The name of the current driver type, e.g. "Windows Audio (Exclusive Mode)". */
    juce::String getCurrentDeviceTypeName() const   { return deviceManager.getCurrentAudioDeviceType(); }

    /** The error from opening a device at start-up, or empty if it worked. */
    juce::String getStartupError() const            { return startupError; }

    /** A plain-language explanation of the most recent device problem, e.g. why Aether went back
        to the previous settings. Empty when there is nothing to report. */
    juce::String getProblemMessage() const          { return problemMessage; }
    void dismissProblemMessage()                    { problemMessage.clear(); }

    /** Proportion of the available audio-thread time being used (0..1). */
    double getCpuUsage() const                      { return deviceManager.getCpuUsage(); }

    /** Saves the current device choice to the settings file. */
    void saveState();

    /** Reacts to a change of device. Called automatically; public so tests can call it directly. */
    void handleDeviceChange();

    /** How deferred work is run. By default it is posted to the message thread; tests replace
        this to run it immediately. */
    void setDeferredRunner (std::function<void (std::function<void()>)> runner);

    /** Where to get the sound driver's most recent error code (e.g. "AUDCLNT_E_DEVICE_IN_USE"),
        so problems can be explained precisely. Usually AppLog::getRecentDriverError. */
    void setDriverErrorSource (std::function<juce::String()> source);

    /** Explains, in plain language, why a device of the given driver type may have failed to open.
        `driverError` is the driver's error code, if known. */
    static juce::String explainOpenFailure (const juce::String& driverTypeName, const juce::String& restoredDescription,
                                            const juce::String& driverError = {});

    static constexpr const char* settingsKey = "audioDeviceState";

private:
    struct WorkingConfig
    {
        juce::String typeName;
        juce::AudioDeviceManager::AudioDeviceSetup setup;
    };

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    juce::String measuredLatencyKey() const;
    void rememberMidiChoices();
    void restoreLastWorkingConfig (const juce::String& failedTypeName, const juce::String& driverError);
    bool tryToOpen (const juce::AudioDeviceManager::AudioDeviceSetup& setup);
    bool tryToOpenInMono (const juce::AudioDeviceManager::AudioDeviceSetup& setup);
    juce::String currentDriverError() const   { return driverErrorSource ? driverErrorSource() : juce::String(); }

    juce::PropertySet& settings;
    juce::AudioDeviceManager deviceManager;
    juce::String startupError, problemMessage;
    std::optional<WorkingConfig> lastWorking;
    bool restorePending = false;
    std::function<void (std::function<void()>)> runDeferred;
    std::function<juce::String()> driverErrorSource;
    juce::MidiDeviceListConnection midiDevicesChanged;
    bool refreshingMidi = false;

    JUCE_DECLARE_WEAK_REFERENCEABLE (AudioDeviceService)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioDeviceService)
};

} // namespace aether
