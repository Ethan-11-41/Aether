// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <functional>

#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"

namespace aether::ui
{

/** A button with a drawn icon (play, stop, loop, ...). Icons are drawn with vector shapes,
    so they're sharp at any screen scaling and need no image files. When it is a toggle and
    switched on, the button is highlighted in the accent colour. */
class IconButton final : public juce::Button
{
public:
    enum class Icon { play, stop, toStart, loop, metronome, record, punch, automation };

    IconButton (const Theme& theme, Icon icon, const juce::String& name);

    /** Shows a different icon (e.g. pause instead of play) without changing what it does. */
    void setAlternateIcon (bool useAlternate);

    /** Called on a right-click (instead of pressing the button), e.g. to show options. */
    std::function<void()> onRightClick;

    void paintButton (juce::Graphics&, bool isMouseOver, bool isButtonDown) override;
    void mouseDown (const juce::MouseEvent&) override;

    static juce::Path makeIcon (Icon icon, juce::Rectangle<float> area, bool alternate);

private:
    const Theme& theme;
    Icon icon;
    bool alternate = false;
};

} // namespace aether::ui
