// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <functional>

#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"

namespace aether::ui
{

/** A number you can change by dragging up/down (like a knob) or double-clicking and typing,
    e.g. the tempo "120.0 BPM". Hold Shift while dragging for fine changes. */
class NumberDragField final : public juce::Component,
                              public juce::SettableTooltipClient
{
public:
    NumberDragField (const Theme& theme, double minimum, double maximum, int decimalPlaces, juce::String suffix);

    void setValue (double newValue);   // doesn't call the callbacks
    double getValue() const noexcept   { return value; }

    /** Called with the new value as it changes. `isStartOfGesture` is true for the first change
        of a drag or for a typed value, so the caller can start a new undo step. */
    std::function<void (double newValue, bool isStartOfGesture)> onValueChange;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    void startTyping();
    void finishTyping (bool keep);
    juce::String getText() const;

    const Theme& theme;
    const double minimum, maximum;
    const int decimals;
    const juce::String suffix;
    double value = 0.0, valueAtDragStart = 0.0;
    bool dragging = false, gestureStarted = false;
    std::unique_ptr<juce::TextEditor> editor;
};

} // namespace aether::ui
