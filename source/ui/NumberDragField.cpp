// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "NumberDragField.h"

namespace aether::ui
{

NumberDragField::NumberDragField (const Theme& themeToUse, double minValue, double maxValue, int decimalPlaces, juce::String unit)
    : theme (themeToUse), minimum (minValue), maximum (maxValue), decimals (decimalPlaces), suffix (std::move (unit))
{
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    setWantsKeyboardFocus (false);
}

void NumberDragField::setValue (double newValue)
{
    newValue = juce::jlimit (minimum, maximum, newValue);

    if (newValue != value)
    {
        value = newValue;
        repaint();
    }
}

juce::String NumberDragField::getText() const
{
    return juce::String (value, decimals);
}

void NumberDragField::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (theme.meterBackground);
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (dragging ? theme.accent : theme.outline);
    g.drawRoundedRectangle (bounds, 6.0f, 1.0f);

    if (editor != nullptr)
        return;

    auto area = getLocalBounds().reduced (8, 0);
    g.setColour (theme.textDim);
    g.setFont (juce::FontOptions (12.0f));
    const auto suffixWidth = suffix.isEmpty() ? 0 : 34;
    g.drawText (suffix, area.removeFromRight (suffixWidth), juce::Justification::centredRight);

    g.setColour (theme.text);
    g.setFont (juce::FontOptions (17.0f, juce::Font::bold));
    g.drawText (getText(), area, juce::Justification::centredRight);
}

void NumberDragField::resized()
{
    if (editor != nullptr)
        editor->setBounds (getLocalBounds().reduced (3));
}

void NumberDragField::mouseDown (const juce::MouseEvent&)
{
    valueAtDragStart = value;
    dragging = false;
    gestureStarted = false;
}

void NumberDragField::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging && std::abs (e.getDistanceFromDragStartY()) < 3)
        return;

    dragging = true;

    // Up = bigger. One pixel is one unit (e.g. 1 BPM), or a tenth with Shift.
    const auto step = e.mods.isShiftDown() ? 0.1 : 1.0;
    auto newValue = valueAtDragStart - e.getDistanceFromDragStartY() * step;
    newValue = juce::jlimit (minimum, maximum, std::round (newValue / step) * step);

    if (newValue != value)
    {
        value = newValue;
        repaint();

        if (onValueChange)
            onValueChange (value, ! gestureStarted);

        gestureStarted = true;
    }
}

void NumberDragField::mouseUp (const juce::MouseEvent&)
{
    dragging = false;
    repaint();
}

void NumberDragField::mouseDoubleClick (const juce::MouseEvent&)
{
    startTyping();
}

void NumberDragField::startTyping()
{
    editor = std::make_unique<juce::TextEditor>();
    editor->setText (getText(), false);
    editor->setFont (juce::FontOptions (16.0f, juce::Font::bold));
    editor->setJustification (juce::Justification::centredRight);
    editor->setInputRestrictions (8, "0123456789.,");
    editor->onReturnKey = [this] { finishTyping (true); };
    editor->onEscapeKey = [this] { finishTyping (false); };
    editor->onFocusLost = [this] { finishTyping (true); };
    addAndMakeVisible (*editor);
    resized();
    editor->grabKeyboardFocus();
    editor->selectAll();
    repaint();
}

void NumberDragField::finishTyping (bool keep)
{
    if (editor == nullptr)
        return;

    auto finished = std::move (editor);
    finished->onFocusLost = nullptr;
    const auto text = finished->getText().replaceCharacter (',', '.').trim();
    removeChildComponent (finished.get());
    repaint();

    if (keep && text.containsAnyOf ("0123456789"))
    {
        const auto typed = juce::jlimit (minimum, maximum, text.getDoubleValue());

        if (typed != value)
        {
            value = typed;
            repaint();

            if (onValueChange)
                onValueChange (value, true);
        }
    }
}

} // namespace aether::ui
