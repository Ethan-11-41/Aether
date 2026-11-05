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

    /** Latency of the currently open device (invalid if no device is open). */
    LatencyInfo getLatencyInfo() const;

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

    /** Explains, in plain language, why a device of the given driver type may fail to open. */
    static juce::String explainOpenFailure (const juce::String& driverTypeName, const juce::String& restoredDescription);

    static constexpr const char* settingsKey = "audioDeviceState";

private:
    struct WorkingConfig
    {
        juce::String typeName;
        juce::AudioDeviceManager::AudioDeviceSetup setup;
    };

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void restoreLastWorkingConfig (const juce::String& failedTypeName);

    juce::PropertySet& settings;
    juce::AudioDeviceManager deviceManager;
    juce::String startupError, problemMessage;
    std::optional<WorkingConfig> lastWorking;
    bool restorePending = false;
    std::function<void (std::function<void()>)> runDeferred;

    JUCE_DECLARE_WEAK_REFERENCEABLE (AudioDeviceService)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioDeviceService)
};

} // namespace aether
