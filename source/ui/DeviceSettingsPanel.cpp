// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "DeviceSettingsPanel.h"

namespace aether::ui
{

namespace
{
    constexpr int rowHeight = 28;
    constexpr int rowGap = 8;
    constexpr int maxInputToggles = 32;
    constexpr double midiLightSeconds = 0.15;

    juce::FontOptions labelFont()   { return juce::FontOptions (14.0f); }
    juce::FontOptions hintFont()    { return juce::FontOptions (13.0f); }

    /** Height of `text` wrapped to `width`. */
    int textHeight (const juce::String& text, juce::FontOptions font, int width)
    {
        if (text.isEmpty() || width <= 0)
            return 0;

        juce::AttributedString string;
        string.append (text, juce::Font (font));
        string.setWordWrap (juce::AttributedString::byWord);

        juce::TextLayout layout;
        layout.createLayout (string, (float) width);

        // A label only shows as many lines as fit whole, so round each line up generously.
        const auto lineHeight = (int) std::ceil (juce::Font (font).getHeight());
        return std::max (1, layout.getNumLines()) * lineHeight + 4;
    }

    int labelWidthFor (int width)   { return juce::jlimit (110, 180, width * 35 / 100); }
}

DeviceSettingsPanel::DeviceSettingsPanel (AudioDeviceService& deviceService, const Theme& themeToUse)
    : devices (deviceService), manager (deviceService.getDeviceManager()), theme (themeToUse)
{
    for (auto* label : { &driverLabel, &outputLabel, &inputLabel, &rateLabel, &bufferLabel, &outputPairLabel, &inputsLabel })
    {
        label->setFont (labelFont());
        label->setColour (juce::Label::textColourId, theme.text);
        label->setBorderSize ({});
        label->setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (*label);
    }

    for (auto* hint : { &driverHint, &bufferNote, &inputsNote, &midiNote })
    {
        hint->setFont (hintFont());
        hint->setColour (juce::Label::textColourId, theme.textDim);
        hint->setBorderSize ({});
        hint->setJustificationType (juce::Justification::topLeft);
        hint->setMinimumHorizontalScale (1.0f);   // wrap onto more lines rather than squash the text
        addAndMakeVisible (*hint);
    }

    midiHeading.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    midiHeading.setColour (juce::Label::textColourId, theme.text);
    midiHeading.setBorderSize ({});
    addAndMakeVisible (midiHeading);

    driverBox.setTooltip ("The kind of connection Aether uses to talk to your sound card. If you're not sure, "
                          "use \"Windows Audio\": it always works.");
    outputBox.setTooltip ("Where Aether plays sound: your speakers, headphones or audio interface.");
    inputBox.setTooltip ("Where Aether records from: your microphone or audio interface. Choose \"None\" "
                         "if you only play back.");
    rateBox.setTooltip ("How many times a second the sound is measured. 48 kHz is a good choice; 44.1 kHz is the "
                        "CD rate. Higher rates use more of your computer's power for little audible difference.");
    bufferBox.setTooltip ("How much sound Aether prepares at a time. Smaller = less delay when you play or sing "
                          "through Aether, but more work for your computer. If you hear crackles, choose a bigger one.");
    controlPanelButton.setTooltip ("Opens the settings window of your audio interface's own driver (for example "
                                   "to change its buffer size).");
    outputPairBox.setTooltip ("Which pair of outputs on your audio interface Aether plays through.");

    driverBox.onChange = [this] { driverChosen(); };
    outputBox.onChange = [this] { outputChosen(); };
    inputBox.onChange = [this] { inputChosen(); };
    rateBox.onChange = [this] { rateChosen(); };
    bufferBox.onChange = [this] { bufferChosen(); };
    outputPairBox.onChange = [this] { outputPairChosen(); };
    controlPanelButton.onClick = [this] { showControlPanel(); };

    for (auto* box : { &driverBox, &outputBox, &inputBox, &rateBox, &bufferBox, &outputPairBox })
        addAndMakeVisible (*box);

    driverBox.setTextWhenNothingSelected ("Choose a driver");
    driverBox.setTextWhenNoChoicesAvailable ("No sound drivers found");
    rateBox.setTextWhenNoChoicesAvailable ("(no device open)");
    bufferBox.setTextWhenNoChoicesAvailable ("(no device open)");

    addChildComponent (controlPanelButton);

    manager.addChangeListener (this);
    midiDevicesChanged = juce::MidiDeviceListConnection::make ([this] { refreshMidi(); });

    refresh();
    startTimerHz (15);
}

DeviceSettingsPanel::~DeviceSettingsPanel()
{
    manager.removeChangeListener (this);
}

//==============================================================================
juce::String DeviceSettingsPanel::friendlyTypeName (const juce::String& typeName)
{
    if (typeName == "Windows Audio")
        return "Windows Audio (recommended)";

    if (typeName == "ASIO")
        return "ASIO (audio interfaces)";

    return typeName;
}

juce::String DeviceSettingsPanel::describeType (const juce::String& typeName)
{
    if (typeName.containsIgnoreCase ("Exclusive"))
        return "Exclusive Mode: the least delay, and you can choose the buffer size. Other programs go silent "
               "while Aether uses the device, and some built-in microphones can't be used in this mode.";

    if (typeName.containsIgnoreCase ("Low Latency"))
        return "Low Latency Mode: like \"Windows Audio\", but with smaller buffers if your sound driver "
               "supports it. Many built-in sound cards don't, so the buffer size may stay the same.";

    if (typeName.containsIgnoreCase ("ASIO"))
        return "ASIO: for audio interfaces that come with their own driver. The buffer size is usually set in "
               "the driver's own settings.";

    if (typeName.containsIgnoreCase ("Windows Audio"))
        return "Works with every sound device and lets other programs play at the same time. Windows chooses "
               "the buffer size (usually 10 ms). For less delay, try \"Exclusive Mode\".";

    return {};
}

juce::String DeviceSettingsPanel::formatRate (double sampleRate)
{
    const auto kHz = sampleRate / 1000.0;
    const auto whole = std::abs (kHz - std::round (kHz)) < 0.001;
    return (whole ? juce::String (juce::roundToInt (kHz)) : juce::String (kHz, 1)) + " kHz";
}

juce::String DeviceSettingsPanel::formatBufferSize (int samples, double sampleRate)
{
    auto text = juce::String (samples) + " samples";

    if (sampleRate > 0.0)
    {
        const auto ms = samples * 1000.0 / sampleRate;
        text << " (" << (ms < 10.0 ? juce::String (ms, 1) : juce::String (juce::roundToInt (ms))) << " ms)";
    }

    return text;
}

//==============================================================================
juce::AudioIODeviceType* DeviceSettingsPanel::getCurrentType() const
{
    return manager.getCurrentDeviceTypeObject();
}

void DeviceSettingsPanel::refresh()
{
    const auto currentTypeName = manager.getCurrentAudioDeviceType();
    auto* type = getCurrentType();
    auto* device = manager.getCurrentAudioDevice();
    const auto setup = manager.getAudioDeviceSetup();

    // Driver
    typeNames.clear();
    driverBox.clear (juce::dontSendNotification);

    for (auto* t : manager.getAvailableDeviceTypes())
    {
        typeNames.add (t->getTypeName());
        driverBox.addItem (friendlyTypeName (t->getTypeName()), typeNames.size());
    }

    driverBox.setSelectedId (typeNames.indexOf (currentTypeName) + 1, juce::dontSendNotification);
    driverHint.setText (describeType (currentTypeName), juce::dontSendNotification);
    driverHint.setVisible (driverHint.getText().isNotEmpty());

    // Devices. Some drivers (ASIO) open one device for both directions.
    const auto separate = type == nullptr || type->hasSeparateInputsAndOutputs();
    outputLabel.setText (separate ? "Speakers / headphones" : "Device", juce::dontSendNotification);
    outputNames = type != nullptr ? type->getDeviceNames (false) : juce::StringArray();
    outputBox.clear (juce::dontSendNotification);

    for (int i = 0; i < outputNames.size(); ++i)
        outputBox.addItem (outputNames[i], i + 1);

    outputBox.setSelectedId (outputNames.indexOf (separate ? setup.outputDeviceName : (device != nullptr ? device->getName() : setup.outputDeviceName)) + 1,
                             juce::dontSendNotification);
    outputBox.setTextWhenNothingSelected (outputNames.isEmpty() ? "No devices found" : "Choose...");

    inputLabel.setVisible (separate);
    inputBox.setVisible (separate);
    inputNames = type != nullptr && separate ? type->getDeviceNames (true) : juce::StringArray();
    inputBox.clear (juce::dontSendNotification);
    inputBox.addItem ("None", 1);

    for (int i = 0; i < inputNames.size(); ++i)
        inputBox.addItem (inputNames[i], i + 2);

    inputBox.setSelectedId (setup.inputDeviceName.isEmpty() ? 1 : inputNames.indexOf (setup.inputDeviceName) + 2,
                            juce::dontSendNotification);

    // Sample rate and buffer size
    rates = device != nullptr ? device->getAvailableSampleRates() : juce::Array<double>();
    bufferSizes = device != nullptr ? device->getAvailableBufferSizes() : juce::Array<int>();
    const auto currentRate = device != nullptr ? device->getCurrentSampleRate() : 0.0;
    const auto currentBuffer = device != nullptr ? device->getCurrentBufferSizeSamples() : 0;

    rateBox.clear (juce::dontSendNotification);
    for (int i = 0; i < rates.size(); ++i)
    {
        rateBox.addItem (formatRate (rates[i]), i + 1);
        if (std::abs (rates[i] - currentRate) < 0.5)
            rateBox.setSelectedId (i + 1, juce::dontSendNotification);
    }

    if (currentBuffer > 0 && ! bufferSizes.contains (currentBuffer))
        bufferSizes.addUsingDefaultSort (currentBuffer);

    bufferBox.clear (juce::dontSendNotification);
    for (int i = 0; i < bufferSizes.size(); ++i)
    {
        bufferBox.addItem (formatBufferSize (bufferSizes[i], currentRate), i + 1);
        if (bufferSizes[i] == currentBuffer)
            bufferBox.setSelectedId (i + 1, juce::dontSendNotification);
    }

    rateBox.setEnabled (rates.size() > 1);
    bufferBox.setEnabled (bufferSizes.size() > 1);

    juce::String note;
    if (device != nullptr && bufferSizes.size() <= 1)
        note = currentTypeName.containsIgnoreCase ("ASIO") ? "Change this in the driver's own settings (button below)."
             : currentTypeName == "Windows Audio"          ? "In \"Windows Audio\", Windows chooses this. For a smaller buffer, choose "
                                                             "\"Windows Audio (Exclusive Mode)\" as the driver."
                                                           : "This device offers only this size.";
    bufferNote.setText (note, juce::dontSendNotification);
    bufferNote.setVisible (note.isNotEmpty());

    controlPanelButton.setVisible (device != nullptr && device->hasControlPanel());

    // Output pair (only for interfaces with more than two outputs)
    const auto outputChannelNames = device != nullptr ? device->getOutputChannelNames() : juce::StringArray();
    outputPairBox.clear (juce::dontSendNotification);

    for (int first = 0; first + 1 < outputChannelNames.size(); first += 2)
        outputPairBox.addItem (outputChannelNames[first] + " + " + outputChannelNames[first + 1], first / 2 + 1);

    if (device != nullptr)
        outputPairBox.setSelectedId (std::max (0, device->getActiveOutputChannels().findNextSetBit (0)) / 2 + 1,
                                     juce::dontSendNotification);

    outputPairLabel.setVisible (outputChannelNames.size() > 2);
    outputPairBox.setVisible (outputChannelNames.size() > 2);

    // Inputs to record from (rebuilt only if the device's channels changed, so ticking doesn't flicker)
    auto inputChannelNames = device != nullptr ? device->getInputChannelNames() : juce::StringArray();
    while (inputChannelNames.size() > maxInputToggles)
        inputChannelNames.remove (inputChannelNames.size() - 1);

    bool sameInputs = (int) inputToggles.size() == inputChannelNames.size();
    for (int i = 0; sameInputs && i < inputChannelNames.size(); ++i)
        sameInputs = inputToggles[(size_t) i]->getButtonText() == inputChannelNames[i];

    if (! sameInputs)
    {
        inputToggles.clear();

        for (int i = 0; i < inputChannelNames.size(); ++i)
        {
            auto toggle = std::make_unique<juce::ToggleButton> (inputChannelNames[i]);
            toggle->setTooltip ("Tick the inputs you want to record from. Each track then chooses one of them "
                                "(or a pair, for stereo) in its Input menu.");
            toggle->onClick = [this, i, t = toggle.get()] { inputToggled (i, t->getToggleState()); };
            addAndMakeVisible (*toggle);
            inputToggles.push_back (std::move (toggle));
        }
    }

    const auto activeInputs = device != nullptr ? device->getActiveInputChannels() : juce::BigInteger();
    for (int i = 0; i < (int) inputToggles.size(); ++i)
        inputToggles[(size_t) i]->setToggleState (activeInputs[i], juce::dontSendNotification);

    inputsLabel.setVisible (! inputToggles.empty());
    inputsNote.setText (! inputToggles.empty() && activeInputs.isZero()
                            ? "No input is ticked, so nothing can be recorded."
                            : juce::String(),
                        juce::dontSendNotification);
    inputsNote.setVisible (inputsNote.getText().isNotEmpty());
    inputsNote.setColour (juce::Label::textColourId, theme.warning);

    refreshMidi();
}

void DeviceSettingsPanel::refreshMidi()
{
    midiDevices = juce::MidiInput::getAvailableDevices();

    bool same = (int) midiToggles.size() == midiDevices.size();
    for (int i = 0; same && i < midiDevices.size(); ++i)
        same = midiToggles[(size_t) i]->getButtonText() == midiDevices[i].name;

    if (! same)
    {
        midiToggles.clear();

        for (int i = 0; i < midiDevices.size(); ++i)
        {
            auto toggle = std::make_unique<juce::ToggleButton> (midiDevices[i].name);
            toggle->setTooltip ("Ticked: Aether listens to this keyboard (you can play the selected MIDI track with it "
                                "and record). Unticked: ignored. Aether remembers your choice.");
            toggle->onClick = [this, i, t = toggle.get()]
            {
                if (juce::isPositiveAndBelow (i, midiDevices.size()))
                    manager.setMidiInputDeviceEnabled (midiDevices[i].identifier, t->getToggleState());
            };
            addAndMakeVisible (*toggle);
            midiToggles.push_back (std::move (toggle));
        }
    }

    for (int i = 0; i < (int) midiToggles.size(); ++i)
        midiToggles[(size_t) i]->setToggleState (manager.isMidiInputDeviceEnabled (midiDevices[i].identifier),
                                                 juce::dontSendNotification);

    midiNote.setText (midiDevices.isEmpty()
                          ? "No MIDI keyboard found. Plug one in (for example by USB) and it appears here, switched on. "
                            "You can also play with the on-screen keyboard (View menu)."
                          : "Ticked keyboards play the selected MIDI track and can be recorded. A newly plugged-in "
                            "keyboard is switched on automatically. The light shows when MIDI arrives.",
                      juce::dontSendNotification);

    resized();
    repaint();

    if (onLayoutChanged)
        onLayoutChanged();
}

//==============================================================================
void DeviceSettingsPanel::applySetup (const juce::AudioDeviceManager::AudioDeviceSetup& setup)
{
    // If this fails, AudioDeviceService goes back to the last working setting and explains why.
    const auto error = manager.setAudioDeviceSetup (setup, true);

    if (error.isNotEmpty())
        juce::Logger::writeToLog ("Audio settings: couldn't apply the change (" + error + ")");

    refresh();
}

void DeviceSettingsPanel::driverChosen()
{
    const auto name = typeNames[driverBox.getSelectedId() - 1];

    if (name.isNotEmpty() && name != manager.getCurrentAudioDeviceType())
    {
        manager.setCurrentAudioDeviceType (name, true);
        refresh();
    }
}

void DeviceSettingsPanel::outputChosen()
{
    const auto name = outputNames[outputBox.getSelectedId() - 1];
    if (name.isEmpty())
        return;

    auto setup = manager.getAudioDeviceSetup();
    setup.outputDeviceName = name;
    setup.useDefaultOutputChannels = true;

    if (auto* type = getCurrentType(); type != nullptr && ! type->hasSeparateInputsAndOutputs())
    {
        setup.inputDeviceName = name;
        setup.useDefaultInputChannels = true;
    }

    applySetup (setup);
}

void DeviceSettingsPanel::inputChosen()
{
    const auto id = inputBox.getSelectedId();
    auto setup = manager.getAudioDeviceSetup();
    setup.inputDeviceName = id <= 1 ? juce::String() : inputNames[id - 2];
    setup.useDefaultInputChannels = true;
    applySetup (setup);
}

void DeviceSettingsPanel::rateChosen()
{
    const auto index = rateBox.getSelectedId() - 1;
    if (! juce::isPositiveAndBelow (index, rates.size()))
        return;

    auto setup = manager.getAudioDeviceSetup();
    setup.sampleRate = rates[index];
    applySetup (setup);
}

void DeviceSettingsPanel::bufferChosen()
{
    const auto index = bufferBox.getSelectedId() - 1;
    if (! juce::isPositiveAndBelow (index, bufferSizes.size()))
        return;

    auto setup = manager.getAudioDeviceSetup();
    setup.bufferSize = bufferSizes[index];
    applySetup (setup);
}

void DeviceSettingsPanel::outputPairChosen()
{
    const auto pair = outputPairBox.getSelectedId() - 1;
    if (pair < 0)
        return;

    auto setup = manager.getAudioDeviceSetup();
    setup.outputChannels.clear();
    setup.outputChannels.setRange (pair * 2, 2, true);
    setup.useDefaultOutputChannels = false;
    applySetup (setup);
}

void DeviceSettingsPanel::inputToggled (int channel, bool on)
{
    auto* device = manager.getCurrentAudioDevice();
    if (device == nullptr)
        return;

    auto setup = manager.getAudioDeviceSetup();
    setup.inputChannels = device->getActiveInputChannels();
    setup.inputChannels.setBit (channel, on);
    setup.useDefaultInputChannels = false;
    applySetup (setup);
}

void DeviceSettingsPanel::showControlPanel()
{
    auto* device = manager.getCurrentAudioDevice();
    if (device == nullptr)
        return;

    // The driver's window is modal; while it's open, Aether's windows mustn't react to clicks.
    bool changed = false;
    {
        juce::Component blocker;
        blocker.setOpaque (true);
        blocker.addToDesktop (0);
        blocker.enterModalState();
        changed = device->showControlPanel();
    }

    if (changed)
    {
        // The driver's settings changed: reopen the device so Aether picks them up.
        manager.closeAudioDevice();
        manager.restartLastAudioDevice();
    }

    refresh();
}

//==============================================================================
void DeviceSettingsPanel::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refresh();
}

void DeviceSettingsPanel::timerCallback()
{
    if (! midiActivity)
        return;

    const auto now = juce::Time::getMillisecondCounterHiRes() * 0.001;

    if (const auto activity = midiActivity(); activity != lastMidiActivity)
    {
        lastMidiActivity = activity;
        midiLitUntil = now + midiLightSeconds;
    }

    if (const auto lit = now < midiLitUntil; lit != midiLit)
    {
        midiLit = lit;
        repaint (midiLightArea);
    }
}

void DeviceSettingsPanel::paint (juce::Graphics& g)
{
    // A line above the MIDI section.
    g.setColour (theme.outline);
    g.drawHorizontalLine (midiHeading.getY() - 12, 0.0f, (float) getWidth());

    if (! midiLightArea.isEmpty())
    {
        g.setColour (midiLit ? theme.accent : theme.panelRaised);
        g.fillEllipse (midiLightArea.toFloat());
        g.setColour (theme.outline);
        g.drawEllipse (midiLightArea.toFloat().reduced (0.5f), 1.0f);
    }
}

void DeviceSettingsPanel::resized()
{
    layout (getWidth(), true);
}

int DeviceSettingsPanel::getIdealHeight (int width) const
{
    return const_cast<DeviceSettingsPanel*> (this)->layout (width, false);
}

int DeviceSettingsPanel::layout (int width, bool apply)
{
    const auto labelWidth = labelWidthFor (width);
    const auto controlX = labelWidth + 8;
    const auto controlWidth = std::max (60, width - controlX);
    int y = 0;

    auto place = [apply] (juce::Component& c, juce::Rectangle<int> r)
    {
        if (apply)
            c.setBounds (r);
    };

    auto row = [&] (juce::Label& label, juce::Component& control)
    {
        place (label, { 0, y, labelWidth, rowHeight });
        place (control, { controlX, y, controlWidth, rowHeight });
        y += rowHeight + rowGap;
    };

    auto note = [&] (juce::Label& label, int x)
    {
        if (! label.isVisible())
            return;

        const auto h = textHeight (label.getText(), hintFont(), width - x);
        place (label, { x, y, width - x, h });
        y += h + rowGap;
    };

    row (driverLabel, driverBox);
    note (driverHint, controlX);
    row (outputLabel, outputBox);

    if (inputBox.isVisible())
        row (inputLabel, inputBox);

    row (rateLabel, rateBox);
    row (bufferLabel, bufferBox);
    note (bufferNote, controlX);

    if (controlPanelButton.isVisible())
    {
        place (controlPanelButton, { controlX, y, std::min (controlWidth, 240), rowHeight });
        y += rowHeight + rowGap;
    }

    if (outputPairBox.isVisible())
        row (outputPairLabel, outputPairBox);

    if (! inputToggles.empty())
    {
        place (inputsLabel, { 0, y, labelWidth, rowHeight });
        const auto columns = controlWidth >= 320 ? 2 : 1;
        const auto columnWidth = controlWidth / columns;

        for (int i = 0; i < (int) inputToggles.size(); ++i)
            place (*inputToggles[(size_t) i], { controlX + (i % columns) * columnWidth, y + (i / columns) * (rowHeight - 2),
                                                columnWidth, rowHeight - 2 });

        y += ((int) inputToggles.size() + columns - 1) / columns * (rowHeight - 2) + rowGap;
        note (inputsNote, controlX);
    }

    // MIDI
    y += 16;
    const auto headingWidth = juce::GlyphArrangement::getStringWidthInt (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)),
                                                                          midiHeading.getText()) + 4;
    place (midiHeading, { 0, y, headingWidth, rowHeight });

    if (apply)
        midiLightArea = midiActivity ? juce::Rectangle<int> (headingWidth + 6, y + rowHeight / 2 - 6, 12, 12)
                                     : juce::Rectangle<int>();

    y += rowHeight;
    note (midiNote, 0);

    for (auto& toggle : midiToggles)
    {
        place (*toggle, { 0, y, width, rowHeight - 2 });
        y += rowHeight - 2;
    }

    return y + rowGap;
}

} // namespace aether::ui
