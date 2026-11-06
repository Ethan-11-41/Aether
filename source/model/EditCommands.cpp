// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "EditCommands.h"

#include "TrackColours.h"

namespace aether::edit
{

namespace
{
    juce::String baseName (TrackKind kind)
    {
        return kind == TrackKind::midi ? "MIDI" : "Audio";
    }
}

juce::String nextTrackName (const Project& project, TrackKind kind)
{
    // Find the highest number already used by names like "Audio 3", then add one.
    const auto prefix = baseName (kind) + " ";
    int highest = 0;

    for (int i = 0; i < project.getNumTracks(); ++i)
    {
        const auto name = project.getTrack (i).getName();

        if (name.startsWith (prefix) && name.substring (prefix.length()).containsOnly ("0123456789"))
            highest = juce::jmax (highest, name.substring (prefix.length()).getIntValue());
    }

    return prefix + juce::String (highest + 1);
}

juce::Colour nextTrackColour (const Project& project)
{
    const auto& colours = trackColours::palette();

    for (const auto& colour : colours)
    {
        bool used = false;

        for (int i = 0; i < project.getNumTracks() && ! used; ++i)
            used = project.getTrack (i).getColour() == colour;

        if (! used)
            return colour;
    }

    return colours[(size_t) project.getNumTracks() % colours.size()];
}

Track addTrack (Project& project, TrackKind kind, int insertIndex, bool newUndoStep, const juce::String& name)
{
    auto& undo = project.getUndoManager();

    if (newUndoStep)
        undo.beginNewTransaction (kind == TrackKind::midi ? "Add MIDI track" : "Add audio track");

    juce::ValueTree track (ids::Track);
    track.setProperty (ids::id, juce::Uuid().toString(), nullptr);
    track.setProperty (ids::kind, toString (kind), nullptr);
    track.setProperty (ids::name, name.trim().isNotEmpty() ? name.trim() : nextTrackName (project, kind), nullptr);
    track.setProperty (ids::colour, Track::colourToString (nextTrackColour (project)), nullptr);
    track.appendChild (juce::ValueTree (ids::Clips), nullptr);

    auto tracks = project.getTracksNode();
    const auto index = (insertIndex < 0 || insertIndex > tracks.getNumChildren()) ? -1 : insertIndex;
    tracks.addChild (track, index, &undo);

    return Track (track);
}

bool deleteTrack (Project& project, const juce::String& trackId)
{
    auto track = project.findTrack (trackId);

    if (! track.isValid())
        return false;

    auto& undo = project.getUndoManager();
    undo.beginNewTransaction ("Delete track \"" + track.getName() + "\"");
    project.getTracksNode().removeChild (track.getState(), &undo);
    return true;
}

bool renameTrack (Project& project, const juce::String& trackId, const juce::String& newName)
{
    auto track = project.findTrack (trackId);
    const auto trimmed = newName.trim();

    if (! track.isValid() || trimmed.isEmpty() || trimmed == track.getName())
        return false;

    auto& undo = project.getUndoManager();
    undo.beginNewTransaction ("Rename track");
    track.getState().setProperty (ids::name, trimmed, &undo);
    return true;
}

bool setTrackColour (Project& project, const juce::String& trackId, juce::Colour colour)
{
    auto track = project.findTrack (trackId);
    const auto text = Track::colourToString (colour.withAlpha (1.0f));

    if (! track.isValid() || track.getState()[ids::colour].toString() == text)
        return false;

    auto& undo = project.getUndoManager();
    undo.beginNewTransaction ("Change track colour");
    track.getState().setProperty (ids::colour, text, &undo);
    return true;
}

bool moveTrack (Project& project, const juce::String& trackId, int newIndex)
{
    auto tracks = project.getTracksNode();
    const auto currentIndex = project.indexOfTrack (trackId);

    if (currentIndex < 0)
        return false;

    newIndex = juce::jlimit (0, tracks.getNumChildren() - 1, newIndex);

    if (newIndex == currentIndex)
        return false;

    auto& undo = project.getUndoManager();
    undo.beginNewTransaction ("Move track");
    tracks.moveChild (currentIndex, newIndex, &undo);
    return true;
}

} // namespace aether::edit
