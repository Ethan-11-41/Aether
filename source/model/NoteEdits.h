// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <vector>

#include "MidiClips.h"

/** Editing the notes inside a MIDI clip (the piano roll).

    Notes are referred to by their Note nodes (children of the clip's node). A node stays the
    same object through edits, undo and redo, so the piano roll can keep its selection.

    Every function is one undo step, except where `newUndoStep` is false: a drag starts with one
    step and continues it, so a whole drag undoes at once. When notes end past the clip's end, the
    clip grows to the end of that bar.
*/
namespace aether
{

using NoteNodes = std::vector<juce::ValueTree>;

namespace notes
{
    /** A note node's values. */
    NoteData read (const juce::ValueTree& note);

    /** All of a clip's note nodes, sorted by start (then pitch). */
    NoteNodes nodesOf (const Clip& clip);

    /** The clip a note belongs to. */
    Clip clipOf (const juce::ValueTree& note);

    /** Quantizing one position: `tick` moved towards the nearest multiple of `step` by `strength`
        (0 = not at all, 1 = all the way). With `swing` (0 to 1), every second step is pushed later,
        up to a triplet feel at 1. Positions are measured from the start of the clip's content. */
    Tick quantizeTick (Tick tick, Tick step, double strength, double swing) noexcept;
}

namespace edit
{
    /** Adds a note (`start` from the start of the clip's content). Returns its node. */
    juce::ValueTree addNote (Project& project, const juce::String& clipId, const NoteData& note, bool newUndoStep = true);

    /** Adds several notes (e.g. pasted), in one undo step. Returns their nodes. */
    NoteNodes addNotes (Project& project, const juce::String& clipId, const std::vector<NoteData>& notes,
                        const juce::String& undoName = "Paste notes");

    void deleteNotes (Project& project, const NoteNodes& notes);

    /** Moves notes in time and pitch together (kept within 0..127 and after the clip's start). */
    void moveNotes (Project& project, const NoteNodes& notes, Tick deltaTicks, int deltaPitch, bool newUndoStep = true);

    /** Lengthens or shortens notes (each at least `minimumLength`). */
    void resizeNotes (Project& project, const NoteNodes& notes, Tick deltaLength, Tick minimumLength = 1, bool newUndoStep = true);

    /** Sets notes' velocity (1 to 127). */
    void setNoteVelocity (Project& project, const NoteNodes& notes, int velocity, bool newUndoStep = true);

    /** Changes notes' velocities by `delta`, keeping their differences. */
    void changeNoteVelocity (Project& project, const NoteNodes& notes, int delta, bool newUndoStep = true);

    /** Moves note starts (and ends, if `ends`) to the grid; see notes::quantizeTick. */
    void quantizeNotes (Project& project, const NoteNodes& notes, Tick step, double strength, double swing, bool ends);

    /** Makes notes less mechanical: each start moves by up to +/- `timing` ticks and each
        velocity by up to +/- `velocity`, at random. */
    void humanizeNotes (Project& project, const NoteNodes& notes, Tick timing, int velocity, juce::Random& random);

    /** Copies notes and puts the copies right after them (after the last one's end, rounded up to
        `step`). Returns the copies. */
    NoteNodes duplicateNotes (Project& project, const NoteNodes& notes, Tick step);

    /** Sets the song's key (0 = C) and scale (see Scales.h), used to highlight notes. */
    void setScale (Project& project, int key, const juce::String& scaleId);
}

} // namespace aether
