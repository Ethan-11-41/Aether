// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include "Project.h"

/** Every change a user can make to a project, as named, undoable steps.

    Each function starts a new undo step (named, e.g. "Rename track"), so Edit > Undo shows
    "Undo Rename track". The user interface calls these instead of changing the tree itself.
*/
namespace aether::edit
{

/** Adds a track at `insertIndex` (or at the end if -1). Its name ("Audio 3") and colour are
    chosen automatically. Returns the new track. */
Track addTrack (Project& project, TrackKind kind, int insertIndex = -1, bool newUndoStep = true,
               const juce::String& name = {});

/** Removes a track. Returns false if no track has that id. */
bool deleteTrack (Project& project, const juce::String& trackId);

/** Removes several tracks in one undo step. Tracks that played into a deleted bus are sent to the
    master instead. Returns how many were deleted. */
int deleteTracks (Project& project, const juce::StringArray& trackIds);

/** Renames a track. Leading/trailing spaces are removed; an empty name is ignored. */
bool renameTrack (Project& project, const juce::String& trackId, const juce::String& newName);

bool setTrackColour (Project& project, const juce::String& trackId, juce::Colour colour);
bool setTracksColour (Project& project, const juce::StringArray& trackIds, juce::Colour colour);

/** Moves a track to a new position in the list (0 = top). */
bool moveTrack (Project& project, const juce::String& trackId, int newIndex);

/** The default name for the next new track of a kind, e.g. "Audio 1", "MIDI 2", "Bus 1". */
juce::String nextTrackName (const Project& project, TrackKind kind);

/** The colour for the next new track: the first palette colour not used yet. */
juce::Colour nextTrackColour (const Project& project);

} // namespace aether::edit
