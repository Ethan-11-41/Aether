// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <functional>
#include <memory>
#include <vector>

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"
#include "devices/AudioDeviceService.h"

namespace aether::ui
{

/** Aether's own sound-device settings, in plain language (it replaces JUCE's standard panel):
    driver, speakers, microphone, sample rate, buffer size (with its delay in milliseconds),
    which inputs to record from, and which MIDI keyboards to listen to.

    Every change is applied straight away through the AudioDeviceManager. If a setting doesn't
    work, AudioDeviceService goes back to the last one that did and explains why; this panel then
    shows the restored setting.
*/
class DeviceSettingsPanel final : public juce::Component,
                                  private juce::ChangeListener,
                                  private juce::Timer
{
public:
    DeviceSettingsPanel (AudioDeviceService& devices, const Theme& theme);
    ~DeviceSettingsPanel() override;

    /** Where MIDI activity comes from (AudioEngine::getMidiActivity: a counter that goes up with
        every message). Lights the little light next to "MIDI keyboards". */
    std::function<std::uint32_t()> midiActivity;

    /** Called when rows appear or disappear, so the owner can give the panel its new height. */
    std::function<void()> onLayoutChanged;

    /** Shows the current settings again (called automatically when the device changes). */
    void refresh();

    /** The height needed to show everything at the given width. */
    int getIdealHeight (int width) const;

    /** "Windows Audio (recommended)", "ASIO (audio interfaces)", ... */
    static juce::String friendlyTypeName (const juce::String& typeName);

    /** A sentence or two explaining a driver type. */
    static juce::String describeType (const juce::String& typeName);

    /** "Outputs 1 + 2" (from "Output channel 1" and "Output channel 2"). */
    static juce::String describeOutputPair (const juce::String& first, const juce::String& second);

        /** "48 kHz", "44.1 kHz". */
    static juce::String formatRate (double sampleRate);

    /** "256 samples (5.3 ms)". */
    static juce::String formatBufferSize (int samples, double sampleRate);

    // For the tests and the settings window.
    juce::ComboBox& getDriverBox() noexcept       { return driverBox; }
    juce::ComboBox& getOutputBox() noexcept       { return outputBox; }
    juce::ComboBox& getInputBox() noexcept        { return inputBox; }
    juce::ComboBox& getRateBox() noexcept         { return rateBox; }
    juce::ComboBox& getBufferBox() noexcept       { return bufferBox; }
    juce::ComboBox& getOutputPairBox() noexcept   { return outputPairBox; }
    int getNumInputToggles() const noexcept       { return (int) inputToggles.size(); }
    juce::ToggleButton& getInputToggle (int index) { return *inputToggles[(size_t) index]; }
    int getNumMidiToggles() const noexcept        { return (int) midiToggles.size(); }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;

    void driverChosen();
    void outputChosen();
    void inputChosen();
    void rateChosen();
    void bufferChosen();
    void outputPairChosen();
    void inputToggled (int channel, bool on);
    void applySetup (const juce::AudioDeviceManager::AudioDeviceSetup& setup);
    void showControlPanel();

    juce::AudioIODeviceType* getCurrentType() const;
    void refreshMidi();

    /** Lays the panel out (or just measures it, if `apply` is false); returns the height used. */
    int layout (int width, bool apply);

    AudioDeviceService& devices;
    juce::AudioDeviceManager& manager;
    const Theme& theme;

    juce::Label driverLabel { {}, "Driver" }, driverHint;
    juce::ComboBox driverBox;
    juce::StringArray typeNames;   // in driverBox order

    juce::Label outputLabel { {}, "Speakers / headphones" }, inputLabel { {}, "Microphone / input" };
    juce::ComboBox outputBox, inputBox;
    juce::StringArray outputNames, inputNames;   // in box order (inputBox item 1 is "None")

    juce::Label rateLabel { {}, "Sample rate" }, bufferLabel { {}, "Buffer size" }, bufferNote;
    juce::ComboBox rateBox, bufferBox;
    juce::Array<double> rates;
    juce::Array<int> bufferSizes;

    juce::TextButton controlPanelButton { "Open the driver's settings..." };

    juce::Label outputPairLabel { {}, "Play through" };
    juce::ComboBox outputPairBox;

    juce::Label inputsLabel { {}, "Record from" }, inputsNote;
    std::vector<std::unique_ptr<juce::ToggleButton>> inputToggles;

    juce::Label midiHeading { {}, "MIDI keyboards" }, midiNote;
    std::vector<std::unique_ptr<juce::ToggleButton>> midiToggles;
    juce::Array<juce::MidiDeviceInfo> midiDevices;   // in midiToggles order
    juce::Rectangle<int> midiLightArea;
    bool midiLit = false;
    std::uint32_t lastMidiActivity = 0;
    double midiLitUntil = 0.0;

    juce::MidiDeviceListConnection midiDevicesChanged;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeviceSettingsPanel)
};

} // namespace aether::ui
