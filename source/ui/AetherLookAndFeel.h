// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"

namespace aether::ui
{

/** Aether's visual style for the standard JUCE widgets (buttons, menus, combo boxes...). */
class AetherLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    explicit AetherLookAndFeel (const Theme& theme = Theme::dark());

    const Theme& getTheme() const noexcept   { return theme; }

    /** Draws the rounded "card" background used to group related controls. */
    static void drawCard (juce::Graphics& g, juce::Rectangle<float> area, const Theme& theme);

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool isMouseOverButton, bool isButtonDown) override;

    /** Pop-up message boxes: a small round icon instead of JUCE's large, cut-off triangle. */
    void drawAlertBox (juce::Graphics&, juce::AlertWindow&, const juce::Rectangle<int>& textArea,
                       juce::TextLayout&) override;

private:
    Theme theme;
};

} // namespace aether::ui
