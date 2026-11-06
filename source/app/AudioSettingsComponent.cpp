// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "AudioSettingsComponent.h"

#include "ui/AetherLookAndFeel.h"

namespace aether
{

namespace
{
    constexpr int meterRefreshHz = 30;

    void styleHeading (juce::Label& label, const ui::Theme& theme)
    {
        label.setBorderSize ({});
        label.setFont (juce::FontOptions (16.0f, juce::Font::bold));
        label.setColour (juce::Label::textColourId, theme.text);
    }

    void styleHint (juce::Label& label, const ui::Theme& theme)
    {
        label.setBorderSize ({});
        label.setFont (juce::FontOptions (13.0f));
        label.setColour (juce::Label::textColourId, theme.textDim);
        label.setJustificationType (juce::Justification::topLeft);
    }
}

AudioSettingsComponent::AudioSettingsComponent (AudioDeviceService& deviceService, AudioEngine& audioEngine, const ui::Theme& themeToUse)
    : devices (deviceService),
      engine (audioEngine),
      theme (themeToUse),
      outputLeft (theme, "L"), outputRight (theme, "R"),
      inputLeft (theme, "L"), inputRight (theme, "R"),
      latencyPanel (theme)
{
    // Header ---------------------------------------------------------------------------------
    titleLabel.setText ("Audio settings", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, theme.text);
    titleLabel.setBorderSize ({});
    addAndMakeVisible (titleLabel);

    subtitleLabel.setText ("Choose your sound device, and check that sound comes out and goes in.", juce::dontSendNotification);
    styleHint (subtitleLabel, theme);
    subtitleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (subtitleLabel);

    versionLabel.setText ("Version " + juce::JUCEApplicationBase::getInstance()->getApplicationVersion(),
                          juce::dontSendNotification);
    styleHint (versionLabel, theme);
    versionLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (versionLabel);

    // Audio device card ------------------------------------------------------------------------
    deviceHeading.setText ("Audio device", juce::dontSendNotification);
    styleHeading (deviceHeading, theme);
    addAndMakeVisible (deviceHeading);

    styleHint (deviceHint, theme);
    deviceHint.setTooltip ("Choose where Aether plays and records sound. Your choice is remembered next time.");
    addAndMakeVisible (deviceHint);

    problemBanner.onDismiss = [this]
    {
        devices.dismissProblemMessage();
        updateStatus();
    };
    addChildComponent (problemBanner);

    deviceSelector = std::make_unique<juce::AudioDeviceSelectorComponent> (
        devices.getDeviceManager(),
        0, 2,     // input channels: none required, up to 2 (e.g. a stereo microphone)
        1, 2,     // output channels: at least 1, up to 2 (left/right)
        true,     // MIDI inputs (e.g. a keyboard over USB): tick to use, untick to ignore
        false,    // MIDI output
        true,     // show channels as stereo pairs
        false);   // don't hide the advanced options
    deviceViewport.setViewedComponent (deviceSelector.get(), false);
    deviceViewport.setScrollBarsShown (true, false);
    addAndMakeVisible (deviceViewport);

    // "Check your sound" card ------------------------------------------------------------------
    soundHeading.setText ("Check your sound", juce::dontSendNotification);
    styleHeading (soundHeading, theme);
    addAndMakeVisible (soundHeading);

    testToneButton.setClickingTogglesState (true);
    testToneButton.setTooltip ("Plays a quiet 440 Hz beep through your speakers or headphones, "
                               "so you can check that sound comes out. Click again to stop.");
    testToneButton.onClick = [this]
    {
        engine.setTestToneEnabled (testToneButton.getToggleState());
        updateTestToneButton();
    };
    addAndMakeVisible (testToneButton);
    updateTestToneButton();

    outputLabel.setText ("Output (speakers / headphones)", juce::dontSendNotification);
    styleHint (outputLabel, theme);
    addAndMakeVisible (outputLabel);

    inputLabel.setText ("Input (microphone)", juce::dontSendNotification);
    styleHint (inputLabel, theme);
    addAndMakeVisible (inputLabel);

    inputHint.setText ("Speak or clap: these meters should move. If they don't, pick your microphone "
                       "under \"Input\" on the left, and check Windows Settings > Privacy > Microphone.",
                       juce::dontSendNotification);
    styleHint (inputHint, theme);
    addAndMakeVisible (inputHint);

    for (auto* meter : { &outputLeft, &outputRight })
    {
        meter->setTooltip ("How loud the sound leaving Aether is. The test tone should sit at about -18 dB.");
        addAndMakeVisible (*meter);
    }

    for (auto* meter : { &inputLeft, &inputRight })
    {
        meter->setTooltip ("How loud the sound arriving at Aether's input is (e.g. your microphone).");
        addAndMakeVisible (*meter);
    }

    // Latency card -----------------------------------------------------------------------------
    latencyHeading.setText ("Latency (delay)", juce::dontSendNotification);
    styleHeading (latencyHeading, theme);
    addAndMakeVisible (latencyHeading);
    addAndMakeVisible (latencyPanel);

    measureButton.setButtonText ("Measure latency");
    measureButton.setTooltip ("Plays a few short chirps and listens for them through your input, to measure the real "
                              "delay. Recordings are then corrected by exactly that. Use speakers at a normal volume "
                              "with the microphone nearby, or a cable from the output to the input.");
    measureButton.onClick = [this] { startMeasuring(); };
    addAndMakeVisible (measureButton);
    styleHint (measureResult, theme);
    measureResult.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (measureResult);

    // Status bar -------------------------------------------------------------------------------
    styleHint (statusLabel, theme);
    statusLabel.setJustificationType (juce::Justification::centredLeft);
    statusLabel.setTooltip ("The audio device Aether is currently using.");
    addAndMakeVisible (statusLabel);

    styleHint (cpuLabel, theme);
    cpuLabel.setJustificationType (juce::Justification::centredRight);
    cpuLabel.setTooltip ("How much of the available time the audio engine is using. "
                         "If this gets close to 100%, you'll hear crackles: try a larger buffer size.");
    addAndMakeVisible (cpuLabel);

    updateStatus();
    lastTimerSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001;
    startTimerHz (meterRefreshHz);

    setSize (1100, 720);
}

AudioSettingsComponent::~AudioSettingsComponent()
{
    stopTimer();
    engine.setTestToneEnabled (false);
}

void AudioSettingsComponent::updateTestToneButton()
{
    const auto on = engine.isTestToneEnabled();
    testToneButton.setToggleState (on, juce::dontSendNotification);
    testToneButton.setButtonText (on ? "Stop test tone" : "Play test tone");
}

juce::String AudioSettingsComponent::describeDeviceType (const juce::String& typeName)
{
    if (typeName.containsIgnoreCase ("Exclusive"))
        return "Exclusive Mode: lowest delay, and you can choose the buffer size. Other programs go silent "
               "while Aether uses the device, and some built-in microphones can't be used in this mode.";

    if (typeName.containsIgnoreCase ("Low Latency"))
        return "Low Latency Mode: like \"Windows Audio\", but uses smaller buffers if your sound driver "
               "supports it. Many built-in sound cards don't, so the buffer size may stay the same.";

    if (typeName.containsIgnoreCase ("ASIO"))
        return "ASIO: for audio interfaces with their own driver. Change the buffer size in the driver's "
               "Control Panel.";

    return "\"Windows Audio\" works with every sound device and lets other programs play at the same time. "
           "Windows chooses the buffer size (usually 10 ms). For less delay, try \"Exclusive Mode\".";
}

void AudioSettingsComponent::updateStatus()
{
    const auto description = devices.describeCurrentDevice();
    const auto error = devices.getStartupError();

    const auto hint = describeDeviceType (devices.getCurrentDeviceTypeName());
    if (deviceHint.getText() != hint)
        deviceHint.setText (hint, juce::dontSendNotification);

    const auto problem = devices.getProblemMessage();
    if (problem != problemBanner.getMessage() || problem.isEmpty() == problemBanner.isVisible())
    {
        problemBanner.setMessage (problem);
        problemBanner.setVisible (problem.isNotEmpty());
        resized();
    }

    if (description.isNotEmpty())
    {
        statusLabel.setText (description, juce::dontSendNotification);
        statusLabel.setColour (juce::Label::textColourId, theme.textDim);
    }
    else
    {
        statusLabel.setText ("No audio device is open" + (error.isNotEmpty() ? " (" + error + ")" : juce::String())
                                 + ". Pick one under \"Audio device\".",
                             juce::dontSendNotification);
        statusLabel.setColour (juce::Label::textColourId, theme.warning);
    }

    latencyPanel.setLatency (devices.getLatencyInfo());
    checkMeasurement();
    cpuLabel.setText ("Audio CPU " + juce::String (juce::roundToInt (devices.getCpuUsage() * 100.0)) + "%",
                      juce::dontSendNotification);
}

void AudioSettingsComponent::timerCallback()
{
    const auto now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    const auto elapsed = static_cast<float> (juce::jlimit (0.0, 0.5, now - lastTimerSeconds));
    lastTimerSeconds = now;

    outputLeft.pushLevel (engine.takeOutputPeak (0), elapsed);
    outputRight.pushLevel (engine.takeOutputPeak (1), elapsed);
    inputLeft.pushLevel (engine.takeInputPeak (0), elapsed);
    inputRight.pushLevel (engine.takeInputPeak (1), elapsed);

    updateStatus();
}

void AudioSettingsComponent::paint (juce::Graphics& g)
{
    g.fillAll (theme.background);

    ui::AetherLookAndFeel::drawCard (g, deviceCard.toFloat(), theme);
    ui::AetherLookAndFeel::drawCard (g, soundCard.toFloat(), theme);
    ui::AetherLookAndFeel::drawCard (g, latencyCard.toFloat(), theme);

    g.setColour (theme.panel);
    g.fillRect (statusBar);
    g.setColour (theme.outline);
    g.drawHorizontalLine (statusBar.getY(), 0.0f, (float) getWidth());
}

void AudioSettingsComponent::resized()
{
    constexpr int margin = 16;
    constexpr int cardPadding = 16;

    auto area = getLocalBounds();

    statusBar = area.removeFromBottom (28);
    {
        auto bar = statusBar.reduced (margin, 0);
        cpuLabel.setBounds (bar.removeFromRight (160));
        statusLabel.setBounds (bar);
    }

    area.reduce (margin, margin);

    // Header
    {
        auto header = area.removeFromTop (40);
        titleLabel.setBounds (header.removeFromLeft (180));
        versionLabel.setBounds (header.removeFromRight (160));
        subtitleLabel.setBounds (header);
        area.removeFromTop (margin);
    }

    // Two columns: device setup on the left, sound check and latency on the right.
    auto left = area.removeFromLeft (area.getWidth() * 56 / 100);
    area.removeFromLeft (margin);
    auto right = area;

    deviceCard = left;
    {
        auto inner = deviceCard.reduced (cardPadding);
        deviceHeading.setBounds (inner.removeFromTop (24));
        deviceHint.setBounds (inner.removeFromTop (54));
        inner.removeFromTop (8);

        if (problemBanner.isVisible())
        {
            problemBanner.setBounds (inner.removeFromTop (problemBanner.getIdealHeight (inner.getWidth())));
            inner.removeFromTop (8);
        }

        deviceViewport.setBounds (inner);

        const auto selectorWidth = inner.getWidth() - deviceViewport.getScrollBarThickness();
        deviceSelector->setSize (selectorWidth, juce::jmax (deviceSelector->getHeight(), 100));
    }

    const auto latencyCardHeight = cardPadding * 2 + 24 + 8 + ui::LatencyPanel::rowHeight * ui::LatencyPanel::numRows + 8 + 32;
    latencyCard = right.removeFromBottom (latencyCardHeight);
    right.removeFromBottom (margin);
    soundCard = right;

    {
        auto inner = soundCard.reduced (cardPadding);
        soundHeading.setBounds (inner.removeFromTop (24));
        inner.removeFromTop (8);
        testToneButton.setBounds (inner.removeFromTop (36).removeFromLeft (180));
        inner.removeFromTop (16);

        outputLabel.setBounds (inner.removeFromTop (20));
        outputLeft.setBounds (inner.removeFromTop (22));
        outputRight.setBounds (inner.removeFromTop (22));
        inner.removeFromTop (16);

        inputLabel.setBounds (inner.removeFromTop (20));
        inputLeft.setBounds (inner.removeFromTop (22));
        inputRight.setBounds (inner.removeFromTop (22));
        inner.removeFromTop (8);
        inputHint.setBounds (inner.removeFromTop (54));
    }

    {
        auto inner = latencyCard.reduced (cardPadding);
        latencyHeading.setBounds (inner.removeFromTop (24));
        inner.removeFromTop (8);
        auto measureRow = inner.removeFromBottom (32);
        measureButton.setBounds (measureRow.removeFromLeft (150));
        measureRow.removeFromLeft (10);
        measureResult.setBounds (measureRow);
        inner.removeFromBottom (8);
        latencyPanel.setBounds (inner);
    }
}

void AudioSettingsComponent::startMeasuring()
{
    const auto latency = devices.getLatencyInfo();

    if (! latency.isValid() || ! latency.hasInput || ! latency.hasOutput)
    {
        measureResult.setText ("Needs a sound card with an input and an output switched on.", juce::dontSendNotification);
        return;
    }

    // Listen on the first input that's switched on.
    int input = 0;
    if (auto* device = devices.getDeviceManager().getCurrentAudioDevice())
        input = std::max (0, device->getActiveInputChannels().findNextSetBit (0));

    latencyCapture = std::make_shared<LatencyCapture> (latency.sampleRate, input);
    engine.startLatencyCapture (latencyCapture);
    measureButton.setEnabled (false);
    measureResult.setText ("Measuring... (you'll hear 8 chirps)", juce::dontSendNotification);
}

void AudioSettingsComponent::checkMeasurement()
{
    if (latencyCapture == nullptr || ! latencyCapture->finished.load())
        return;

    const auto result = analyseLatencyCapture (*latencyCapture);
    const auto rate = latencyCapture->sampleRate;
    latencyCapture = nullptr;
    measureButton.setEnabled (true);

    if (result.ok)
    {
        devices.setMeasuredRoundTrip (result.roundTripSamples);
        measureResult.setText ("Measured: " + juce::String (LatencyInfo::formatSamplesAndMs (result.roundTripSamples, rate))
                                   + ". Recordings will be corrected by this.",
                               juce::dontSendNotification);
        juce::Logger::writeToLog ("Measured round-trip latency: " + juce::String (result.roundTripSamples) + " samples");
    }
    else
    {
        measureResult.setText (result.problem, juce::dontSendNotification);
        juce::Logger::writeToLog ("Latency measurement failed: " + result.problem);
    }

    latencyPanel.setLatency (devices.getLatencyInfo());
}

} // namespace aether
