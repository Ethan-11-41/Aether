// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <optional>

#include <juce_graphics/juce_graphics.h>

#include "Ids.h"

namespace aether
{

enum class TrackKind
{
    audio,   // recorded or imported sound
    midi,    // notes that play an instrument
    bus      // no clips: other tracks play into it (for shared effects or group volume)
};

juce::String toString (TrackKind kind);
std::optional<TrackKind> trackKindFromString (const juce::String& text);

/** A lightweight, typed view of one track in the project tree.

    Track objects are cheap to copy: they refer to the shared data, they don't own it.
    Reading is always allowed; changes should go through the functions in EditCommands.h
    so that they are named and undoable.
*/
class Track
{
public:
    Track() = default;
    explicit Track (juce::ValueTree state);

    bool isValid() const                    { return state.isValid() && state.hasType (ids::Track); }

    juce::String getId() const              { return state[ids::id].toString(); }
    juce::String getName() const            { return state[ids::name].toString(); }
    TrackKind getKind() const;
    bool isBus() const                      { return getKind() == TrackKind::bus; }
    juce::Colour getColour() const;

    // Mixer settings (see model/Mixer.h)
    double getVolumeDb() const;
    double getPan() const;
    bool isMuted() const                    { return (bool) state[ids::mute]; }
    bool isSoloed() const                   { return (bool) state[ids::solo]; }
    /** The id of the bus this track plays into, or empty for the master. */
    juce::String getOutput() const          { return state[ids::output].toString(); }

    juce::ValueTree getState() const        { return state; }

    bool operator== (const Track& other) const   { return state == other.state; }
    bool operator!= (const Track& other) const   { return ! operator== (other); }

    /** Converts between a colour and the "#RRGGBB" text stored in project files. */
    static juce::String colourToString (juce::Colour colour);
    static juce::Colour colourFromString (const juce::String& text, juce::Colour fallback);

private:
    juce::ValueTree state;
};

} // namespace aether
