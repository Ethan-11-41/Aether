// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_data_structures/juce_data_structures.h>

#include "LatencyInfo.h"

namespace aether
{

/** Owns the connection to the sound card (via JUCE's AudioDeviceManager).

    - Opens the device the user chose last time (or the system default on first run).
    - Remembers the user's choice (device, sample rate, buffer size) in the settings file.
    - Reports latency and a human-readable description of the current device.

    Use from the UI (message) thread only.
*/
class AudioDeviceService final : private juce::ChangeListener
{
public:
    /** `settings` is where the device choice is remembered; it must outlive this object. */
    explicit AudioDeviceService (juce::PropertySet& settings);
    ~AudioDeviceService() override;

    juce::AudioDeviceManager& getDeviceManager() noexcept   { return deviceManager; }

    void addAudioCallback (juce::AudioIODeviceCallback* callback);
    void removeAudioCallback (juce::AudioIODeviceCallback* callback);

    /** Latency of the currently open device (invalid if no device is open). */
    LatencyInfo getLatencyInfo() const;

    /** e.g. "Windows Audio · Speakers (Realtek(R) Audio) · 48 kHz". Empty if no device is open. */
    juce::String describeCurrentDevice() const;

    /** The error from the most recent attempt to open a device, or empty if it worked. */
    juce::String getLastError() const   { return lastError; }

    /** Proportion of the available audio-thread time being used (0..1). */
    double getCpuUsage() const          { return deviceManager.getCpuUsage(); }

    /** Saves the current device choice to the settings file. */
    void saveState();

    static constexpr const char* settingsKey = "audioDeviceState";

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    juce::PropertySet& settings;
    juce::AudioDeviceManager deviceManager;
    juce::String lastError;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioDeviceService)
};

} // namespace aether
