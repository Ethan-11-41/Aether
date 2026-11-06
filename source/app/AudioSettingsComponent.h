// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "devices/AudioDeviceService.h"
#include "engine/AudioEngine.h"
#include "ui/DelayedTooltipWindow.h"
#include "ui/DeviceSettingsPanel.h"
#include "ui/LatencyPanel.h"
#include "ui/LevelMeter.h"
#include "ui/MessageBanner.h"
#include "ui/Theme.h"

namespace aether
{

/** The "Audio settings" window: sound device choice, a test tone, input/output meters,
    latency and CPU readouts.
*/
class AudioSettingsComponent final : public juce::Component,
                            private juce::Timer
{
public:
    AudioSettingsComponent (AudioDeviceService& devices, AudioEngine& engine, const ui::Theme& theme);
    ~AudioSettingsComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void updateTestToneButton();
    void updateStatus();

    AudioDeviceService& devices;
    AudioEngine& engine;
    const ui::Theme& theme;

    ui::DelayedTooltipWindow tooltipWindow { this };

    juce::Label titleLabel, subtitleLabel, versionLabel;

    juce::Label deviceHeading;
    ui::MessageBanner problemBanner { theme };
    juce::Viewport deviceViewport;
    std::unique_ptr<ui::DeviceSettingsPanel> devicePanel;
    void layOutDevicePanel();

    juce::Label soundHeading, outputLabel, inputLabel, inputHint;
    juce::TextButton testToneButton;
    ui::LevelMeter outputLeft, outputRight, inputLeft, inputRight;

    juce::Label latencyHeading;
    ui::LatencyPanel latencyPanel;
    juce::TextButton measureButton;
    juce::Label measureResult;
    std::shared_ptr<LatencyCapture> latencyCapture;   // while measuring

    void startMeasuring();
    void checkMeasurement();

    juce::Label statusLabel, cpuLabel;

    juce::Rectangle<int> deviceCard, soundCard, latencyCard, statusBar;
    double lastTimerSeconds = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioSettingsComponent)
};

} // namespace aether
