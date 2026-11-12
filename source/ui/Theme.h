// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <juce_graphics/juce_graphics.h>

namespace aether::ui
{

/** All colours used by Aether's interface, in one place.

    Components ask the theme for a colour by purpose ("panel", "accent") rather than hard-coding
    values, which is what will make dark/light themes (milestone M26) a matter of adding a new
    Theme rather than editing every component.
*/
struct Theme
{
    juce::Colour background;      // window background
    juce::Colour panel;           // cards and panels
    juce::Colour panelRaised;     // buttons, inputs and other controls sitting on a panel
    juce::Colour outline;         // thin borders
    juce::Colour text;            // main text
    juce::Colour textDim;         // secondary text, hints
    juce::Colour accent;          // highlights, active buttons
    juce::Colour accentText;      // text drawn on top of the accent colour
    juce::Colour warning;         // problems the user should look at
    juce::Colour meterBackground;
    juce::Colour meterLow;        // normal signal level
    juce::Colour meterMid;        // getting loud
    juce::Colour meterHigh;       // close to clipping

    static Theme dark()
    {
        Theme t;
        t.background      = juce::Colour (0xff15171c);
        t.panel           = juce::Colour (0xff1e2128);
        t.panelRaised     = juce::Colour (0xff2a2e37);
        t.outline         = juce::Colour (0xff343945);
        t.text            = juce::Colour (0xffe8eaf0);
        t.textDim         = juce::Colour (0xff9aa0ad);
        t.accent          = juce::Colour (0xff4fc3c9);
        t.accentText      = juce::Colour (0xff0d1114);
        t.warning         = juce::Colour (0xffffb454);
        t.meterBackground = juce::Colour (0xff111318);
        t.meterLow        = juce::Colour (0xff4cd07d);
        t.meterMid        = juce::Colour (0xffe6c84a);
        t.meterHigh       = juce::Colour (0xffef5350);
        return t;
    }
};

} // namespace aether::ui
