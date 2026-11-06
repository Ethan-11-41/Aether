// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "KeyboardPanel.h"

namespace aether::ui
{

/** JUCE's keyboard, with Z / X for the octave and a hint while it's listening to the keys. */
class KeyboardPanel::Keys final : public juce::MidiKeyboardComponent,
                                  public juce::SettableTooltipClient
{
public:
    Keys (KeyboardPanel& ownerToUse, juce::MidiKeyboardState& keyboardState)
        : juce::MidiKeyboardComponent (keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard),
          owner (ownerToUse)
    {
    }

    bool keyPressed (const juce::KeyPress& key) override
    {
        if (key.getModifiers().isAnyModifierKeyDown())
            return false;   // Ctrl+... are shortcuts

        if (key.getTextCharacter() == 'z' || key.getTextCharacter() == 'Z') { owner.setOctave (owner.octave - 1); return true; }
        if (key.getTextCharacter() == 'x' || key.getTextCharacter() == 'X') { owner.setOctave (owner.octave + 1); return true; }

        return juce::MidiKeyboardComponent::keyPressed (key);
    }

    void focusGained (FocusChangeType) override   { owner.repaint(); owner.resized(); }
    void focusLost (FocusChangeType) override     { owner.repaint(); owner.resized(); }

private:
    KeyboardPanel& owner;
};

KeyboardPanel::KeyboardPanel (const Theme& themeToUse, std::function<void (const juce::MidiMessage&)> sendToEngine)
    : theme (themeToUse), send (std::move (sendToEngine))
{
    keys = std::make_unique<Keys> (*this, state);
    keys->setVelocity (0.8f, true);   // softer near the top of a key, harder near the bottom
    keys->setAvailableRange (21, 108); // a piano's 88 keys
    keys->setOctaveForMiddleC (4);     // middle C is C4, as on most pianos (and elsewhere in Aether)
    keys->setWantsKeyboardFocus (true);
    keys->setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, theme.accent.withAlpha (0.8f));
    keys->setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, theme.accent.withAlpha (0.25f));
    keys->setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, theme.outline);
    keys->setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colours::transparentBlack);
    keys->setColour (juce::MidiKeyboardComponent::upDownButtonBackgroundColourId, theme.panelRaised);
    keys->setColour (juce::MidiKeyboardComponent::upDownButtonArrowColourId, theme.text);
    keys->setTooltip ("Click the keys to play. Or click here and play with your computer keyboard: "
                      "A W S E D F T G Y H U J K (Z / X: octave down / up).");
    addAndMakeVisible (*keys);

    for (auto* button : { &octaveDown, &octaveUp })
    {
        button->setWantsKeyboardFocus (false);
        addAndMakeVisible (*button);
    }

    octaveDown.setTooltip ("Computer keys play an octave lower (Z)");
    octaveUp.setTooltip ("Computer keys play an octave higher (X)");
    octaveDown.onClick = [this] { setOctave (octave - 1); };
    octaveUp.onClick = [this] { setOctave (octave + 1); };

    hint.setColour (juce::Label::textColourId, theme.textDim);
    hint.setFont (juce::FontOptions (12.0f));
    hint.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (hint);

    state.addListener (this);
    setOctave (4);
}

KeyboardPanel::~KeyboardPanel()
{
    state.removeListener (this);
}

void KeyboardPanel::setOctave (int newOctave)
{
    // JUCE counts octaves from C-1, so octave 4 (middle C, note 60) is its "5".
    octave = juce::jlimit (0, 8, newOctave);
    keys->setKeyPressBaseOctave (octave + 1);
    keys->setLowestVisibleKey (std::max (21, (octave + 1) * 12 - 12));
    resized();
}

void KeyboardPanel::handleNoteOn (juce::MidiKeyboardState*, int channel, int note, float velocity)
{
    if (send)
        send (juce::MidiMessage::noteOn (channel, note, velocity));
}

void KeyboardPanel::handleNoteOff (juce::MidiKeyboardState*, int channel, int note, float velocity)
{
    if (send)
        send (juce::MidiMessage::noteOff (channel, note, velocity));
}

void KeyboardPanel::paint (juce::Graphics& g)
{
    g.fillAll (theme.panel);
    g.setColour (theme.outline);
    g.drawHorizontalLine (0, 0.0f, (float) getWidth());

    if (keys->hasKeyboardFocus (false))
    {
        g.setColour (theme.accent);
        g.drawRect (keys->getBounds().expanded (2), 2);
    }
}

void KeyboardPanel::resized()
{
    auto area = getLocalBounds().reduced (10, 6);
    auto top = area.removeFromTop (24);
    octaveUp.setBounds (top.removeFromRight (56));
    top.removeFromRight (6);
    octaveDown.setBounds (top.removeFromRight (56));

    hint.setText (keys->hasKeyboardFocus (false)
                      ? "Your computer keyboard plays notes now (octave from C" + juce::String (octave)
                            + "). Click anywhere else to use shortcuts again."
                      : "Notes go to the selected MIDI track. Click the keyboard to play it with your computer keys "
                        "(A W S E D F T G Y H U J K).",
                  juce::dontSendNotification);
    hint.setBounds (top);

    area.removeFromTop (4);
    keys->setBounds (area);
    keys->setKeyWidth (std::max (14.0f, (float) area.getWidth() / 52.0f));   // about 7 octaves across
}

} // namespace aether::ui
