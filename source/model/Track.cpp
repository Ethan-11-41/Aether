// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "Track.h"

#include "TrackColours.h"

namespace aether
{

juce::String toString (TrackKind kind)
{
    switch (kind)
    {
        case TrackKind::audio: return "audio";
        case TrackKind::midi:  return "midi";
    }

    return "audio";
}

std::optional<TrackKind> trackKindFromString (const juce::String& text)
{
    if (text == "audio") return TrackKind::audio;
    if (text == "midi")  return TrackKind::midi;
    return std::nullopt;
}

Track::Track (juce::ValueTree stateToUse)
    : state (std::move (stateToUse))
{
}

TrackKind Track::getKind() const
{
    return trackKindFromString (state[ids::kind].toString()).value_or (TrackKind::audio);
}

juce::Colour Track::getColour() const
{
    return colourFromString (state[ids::colour].toString(), trackColours::palette().front());
}

juce::String Track::colourToString (juce::Colour colour)
{
    return "#" + colour.toDisplayString (false);
}

juce::Colour Track::colourFromString (const juce::String& text, juce::Colour fallback)
{
    const auto hex = text.trim().trimCharactersAtStart ("#");

    if (hex.length() != 6 || ! hex.containsOnly ("0123456789abcdefABCDEF"))
        return fallback;

    return juce::Colour ((juce::uint32) (0xff000000u | (juce::uint32) hex.getHexValue32()));
}

} // namespace aether
