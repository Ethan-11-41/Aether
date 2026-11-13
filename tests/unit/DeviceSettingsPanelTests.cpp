// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
//
// Tests for Aether's own audio settings panel, driven with the fake sound card.

#include <catch2/catch_test_macros.hpp>

#include <juce_gui_basics/juce_gui_basics.h>

#include "FakeAudioDevice.h"
#include "ui/DeviceSettingsPanel.h"
#include "ui/Theme.h"

using aether::AudioDeviceService;
using aether::ui::DeviceSettingsPanel;
using namespace aether::test;

namespace
{
    int idOfItem (juce::ComboBox& box, const juce::String& text)
    {
        for (int i = 0; i < box.getNumItems(); ++i)
            if (box.getItemText (i) == text)
                return box.getItemId (i);

        return 0;
    }

    juce::StringArray itemsOf (juce::ComboBox& box)
    {
        juce::StringArray items;
        for (int i = 0; i < box.getNumItems(); ++i)
            items.add (box.getItemText (i));
        return items;
    }

    struct PanelFixture
    {
        PanelFixture()
        {
            service.setDeferredRunner ([] (std::function<void()> work) { work(); });

            // Start with the working microphone.
            auto setup = manager.getAudioDeviceSetup();
            setup.inputDeviceName = "Mic";
            setup.useDefaultInputChannels = true;
            REQUIRE (manager.setAudioDeviceSetup (setup, true).isEmpty());
            service.handleDeviceChange();

            panel.setSize (560, panel.getIdealHeight (560));
            panel.refresh();
        }

        juce::ScopedJuceInitialiser_GUI juce;
        juce::PropertySet settings;
        AudioDeviceService service { settings, fakeTypes() };
        juce::AudioDeviceManager& manager = service.getDeviceManager();
        aether::ui::Theme theme = aether::ui::Theme::dark();
        DeviceSettingsPanel panel { service, theme };
    };
}

TEST_CASE ("Sample rates and buffer sizes are shown in plain language", "[devices][ui]")
{
    CHECK (DeviceSettingsPanel::formatRate (48000.0) == "48 kHz");
    CHECK (DeviceSettingsPanel::formatRate (44100.0) == "44.1 kHz");
    CHECK (DeviceSettingsPanel::formatRate (96000.0) == "96 kHz");

    CHECK (DeviceSettingsPanel::formatBufferSize (256, 48000.0) == "256 samples (5.3 ms)");
    CHECK (DeviceSettingsPanel::formatBufferSize (480, 48000.0) == "480 samples (10 ms)");
    CHECK (DeviceSettingsPanel::formatBufferSize (2048, 44100.0) == "2048 samples (46 ms)");

    CHECK (DeviceSettingsPanel::friendlyTypeName ("Windows Audio") == "Windows Audio (recommended)");
    CHECK (DeviceSettingsPanel::friendlyTypeName ("ASIO") == "ASIO (audio interfaces)");
    CHECK (DeviceSettingsPanel::friendlyTypeName ("Windows Audio (Exclusive Mode)") == "Windows Audio (Exclusive Mode)");
    CHECK (DeviceSettingsPanel::describeType ("Windows Audio (Exclusive Mode)").contains ("buffer size"));

    CHECK (DeviceSettingsPanel::describeOutputPair ("Output channel 1", "Output channel 2") == "Outputs 1 + 2");
    CHECK (DeviceSettingsPanel::describeOutputPair ("Out 11", "Out 12") == "Outputs 11 + 12");
    CHECK (DeviceSettingsPanel::describeOutputPair ("Left", "Right") == "Left + Right");
}

TEST_CASE ("The settings panel shows the current device and what it offers", "[devices][ui]")
{
    PanelFixture f;
    auto& panel = f.panel;

    CHECK (itemsOf (panel.getDriverBox()) == juce::StringArray { fakeTypeName });
    CHECK (panel.getDriverBox().getText() == fakeTypeName);

    CHECK (panel.getOutputBox().getText() == "Speakers");
    CHECK (itemsOf (panel.getInputBox()) == juce::StringArray { "None", "Mic", "Broken Mic" });
    CHECK (panel.getInputBox().getText() == "Mic");

    CHECK (itemsOf (panel.getRateBox()) == juce::StringArray { "44.1 kHz", "48 kHz" });
    CHECK (panel.getBufferBox().getNumItems() == 3);
    CHECK (panel.getBufferBox().getText().startsWith (juce::String (f.manager.getCurrentAudioDevice()->getCurrentBufferSizeSamples())
                                                      + " samples"));

    // Both of the microphone's inputs are switched on (the default).
    REQUIRE (panel.getNumInputToggles() == 2);
    CHECK (panel.getInputToggle (0).getButtonText() == "In 1");
    CHECK (panel.getInputToggle (0).getToggleState());
    CHECK (panel.getInputToggle (1).getToggleState());

    // Only two outputs: no choice of output pair needed.
    CHECK_FALSE (panel.getOutputPairBox().isVisible());
    CHECK (panel.getIdealHeight (560) > 200);
}

TEST_CASE ("Changing a setting in the panel changes the device", "[devices][ui]")
{
    PanelFixture f;
    auto& panel = f.panel;

    SECTION ("Buffer size")
    {
        const auto rate = f.manager.getCurrentAudioDevice()->getCurrentSampleRate();
        const auto id = idOfItem (panel.getBufferBox(), DeviceSettingsPanel::formatBufferSize (128, rate));
        REQUIRE (id != 0);

        panel.getBufferBox().setSelectedId (id, juce::sendNotificationSync);

        CHECK (f.manager.getCurrentAudioDevice()->getCurrentBufferSizeSamples() == 128);
        CHECK (panel.getBufferBox().getText().startsWith ("128 samples"));
    }

    SECTION ("Sample rate")
    {
        panel.getRateBox().setSelectedId (idOfItem (panel.getRateBox(), "44.1 kHz"), juce::sendNotificationSync);
        CHECK (f.manager.getCurrentAudioDevice()->getCurrentSampleRate() == 44100.0);

        panel.getRateBox().setSelectedId (idOfItem (panel.getRateBox(), "48 kHz"), juce::sendNotificationSync);
        CHECK (f.manager.getCurrentAudioDevice()->getCurrentSampleRate() == 48000.0);
    }

    SECTION ("No input, then the microphone again")
    {
        panel.getInputBox().setSelectedId (idOfItem (panel.getInputBox(), "None"), juce::sendNotificationSync);
        REQUIRE (f.manager.getCurrentAudioDevice() != nullptr);
        CHECK (f.manager.getCurrentAudioDevice()->getActiveInputChannels().isZero());
        CHECK (panel.getNumInputToggles() == 0);

        panel.getInputBox().setSelectedId (idOfItem (panel.getInputBox(), "Mic"), juce::sendNotificationSync);
        CHECK (f.manager.getCurrentAudioDevice()->getActiveInputChannels().countNumberOfSetBits() == 2);
        CHECK (panel.getNumInputToggles() == 2);
    }

    SECTION ("Switching one input off")
    {
        panel.getInputToggle (1).setToggleState (false, juce::sendNotificationSync);

        const auto active = f.manager.getCurrentAudioDevice()->getActiveInputChannels();
        CHECK (active[0]);
        CHECK_FALSE (active[1]);
        CHECK (f.service.getNumInputChannels() == 1);
        CHECK_FALSE (panel.getInputToggle (1).getToggleState());
    }
}

TEST_CASE ("A device that won't open in the panel goes back to the previous one", "[devices][ui]")
{
    PanelFixture f;
    auto& panel = f.panel;

    panel.getInputBox().setSelectedId (idOfItem (panel.getInputBox(), "Broken Mic"), juce::sendNotificationSync);

    // AudioDeviceService notices (normally via a change message) and restores the microphone.
    f.service.handleDeviceChange();
    panel.refresh();

    REQUIRE (f.manager.getCurrentAudioDevice() != nullptr);
    CHECK (panel.getInputBox().getText() == "Mic");
    CHECK (f.service.getProblemMessage().isNotEmpty());
}

TEST_CASE ("The keyboards' touch setting is chosen in the panel and remembered", "[devices][ui][keyboard]")
{
    PanelFixture f;
    auto& touch = f.panel.getTouchBox();
    CHECK (itemsOf (touch) == juce::StringArray { "Very light", "Light", "Normal", "Heavy" });
    CHECK (touch.getText() == "Normal");
    CHECK (touch.getTooltip().isNotEmpty());
    CHECK (touch.isVisible());
    CHECK (touch.getBottom() <= f.panel.getHeight());

    int told = -1;
    f.panel.onKeyboardTouchChanged = [&told] (int value) { told = value; };
    touch.setSelectedItemIndex (1, juce::sendNotificationSync);
    CHECK (told == 1);
    CHECK (f.service.getKeyboardTouch() == 1);
    CHECK (f.settings.getIntValue ("keyboardTouch") == 1);

    // A new panel (the settings window opened again) shows it.
    DeviceSettingsPanel again { f.service, f.theme };
    CHECK (again.getTouchBox().getText() == "Light");
}
