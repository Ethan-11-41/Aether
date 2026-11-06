// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include "Clip.h"
#include "Project.h"

/** Finding clips, and changing them as undoable steps.

    Functions used while dragging take `newUndoStep`: the caller starts one undo step when the
    drag begins (beginNewTransaction) and passes false, so the whole drag undoes in one step.
*/
namespace aether
{

/** What Aether needs to know about an audio file to make a clip of it. */
struct AudioSourceInfo
{
    juce::String file;          // path relative to the project folder, e.g. "Audio/Guitar.wav"
    juce::String name;          // shown on the clip
    double sampleRate = 48000.0;
    juce::int64 lengthInSamples = 0;
};

namespace clips
{
    juce::ValueTree clipsNodeOf (const juce::ValueTree& trackState);   // created if missing
    Clip findClip (const Project& project, const juce::String& clipId);
    juce::Array<Clip> clipsOnTrack (const juce::ValueTree& trackState);
    juce::Array<Clip> allClips (const Project& project);

    /** The shortest a clip can be made: 10 ms. */
    juce::int64 minimumLength (const Clip& clip);
}

namespace edit
{
    /** Adds an audio clip to a track. Returns its id (empty if the track doesn't exist). */
    juce::String addAudioClip (Project& project, const juce::String& trackId, const AudioSourceInfo& source,
                               Tick start, bool newUndoStep = true);

    /** Puts a clip at a new position, optionally on another track. */
    bool setClipPosition (Project& project, const juce::String& clipId, Tick newStart,
                          const juce::String& trackId, bool newUndoStep = true);

    /** Moves the clip's left edge, keeping the audio where it is in time (like trimming tape). */
    bool trimClipStart (Project& project, const juce::String& clipId, Tick newStart, bool newUndoStep = true);

    /** Moves the clip's right edge. A looping clip can be stretched as far as you like. */
    bool trimClipEnd (Project& project, const juce::String& clipId, Tick newEnd, bool newUndoStep = true);

    /** Cuts a clip in two at `position`. Returns the id of the new right-hand clip. */
    juce::String splitClip (Project& project, const juce::String& clipId, Tick position, bool newUndoStep = true);

    /** Copies clips and places the copies just after them (keeping their spacing). Returns the
        new ids, in the same order. */
    juce::StringArray duplicateClips (Project& project, const juce::StringArray& clipIds);

    bool deleteClips (Project& project, const juce::StringArray& clipIds);

    /** Fade lengths in samples of the clip's file. They are limited to fit inside the clip. */
    bool setClipFades (Project& project, const juce::String& clipId, juce::int64 fadeIn, juce::int64 fadeOut,
                       bool newUndoStep = true);
    bool setClipFadeCurve (Project& project, const juce::String& clipId, bool fadeIn, FadeCurve curve);

    /** Clip volume in dB (-60 to +18). */
    bool setClipGain (Project& project, const juce::String& clipId, double gainDb, bool newUndoStep = true);

    bool setClipLooping (Project& project, const juce::String& clipId, bool shouldLoop);
    bool renameClip (Project& project, const juce::String& clipId, const juce::String& newName);
}

} // namespace aether
