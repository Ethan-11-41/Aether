// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <vector>

#include <juce_graphics/juce_graphics.h>

namespace aether::trackColours
{

/** The colours offered for tracks. New tracks take the next colour in this list. */
inline const std::vector<juce::Colour>& palette()
{
    static const std::vector<juce::Colour> colours {
        juce::Colour (0xff4fc3c9),   // teal
        juce::Colour (0xff7c9cff),   // blue
        juce::Colour (0xffa98bff),   // violet
        juce::Colour (0xffe07ad8),   // pink
        juce::Colour (0xffef6b73),   // red
        juce::Colour (0xfff29c5b),   // orange
        juce::Colour (0xffe6c84a),   // yellow
        juce::Colour (0xff9ad35b),   // lime
        juce::Colour (0xff4cd07d),   // green
        juce::Colour (0xff53b3e8),   // sky
        juce::Colour (0xffb0b6c3),   // grey
        juce::Colour (0xffc79b6d),   // brown
    };
    return colours;
}

/** Human-readable names for the palette, in the same order. */
inline const std::vector<const char*>& names()
{
    static const std::vector<const char*> list { "Teal", "Blue", "Violet", "Pink", "Red", "Orange",
                                                 "Yellow", "Lime", "Green", "Sky", "Grey", "Brown" };
    return list;
}

} // namespace aether::trackColours
