// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include "Project.h"
#include "time/TempoMap.h"

/** Reading the project's tempo, time signatures and loop, and changing them as undoable steps.

    Tempo changes are identified by their index in time order (0 = the first, at the start of
    the song, which can't be moved or deleted). Functions used while dragging take
    `newUndoStep`: pass true on the first change of a drag and false for the rest, so that the
    whole drag is undone in one step.
*/
namespace aether
{

struct LoopRange
{
    Tick start = 0;
    Tick end = 0;
    bool enabled = false;
};

namespace timeline
{
    TempoMap buildTempoMap (const Project& project);
    LoopRange getLoop (const Project& project);
    int getNumTempoChanges (const Project& project);
}

namespace edit
{
    // Tempo
    bool setTempo (Project& project, int pointIndex, double bpm, bool newUndoStep = true);

    /** Adds a tempo change (or updates the one already at `tick`). Returns its index. */
    int addTempoChange (Project& project, Tick tick, double bpm, TempoCurve curve = TempoCurve::jump);

    /** Moves a tempo change; it can't move past its neighbours. Returns false for index 0. */
    bool moveTempoChange (Project& project, int pointIndex, Tick newTick, bool newUndoStep = true);

    bool setTempoCurve (Project& project, int pointIndex, TempoCurve curve);
    bool deleteTempoChange (Project& project, int pointIndex);

    /** Replaces all the tempo changes at once (sorted; one at tick 0 is added if missing).
        Used by the timing tools (Milestone 15). */
    void replaceTempo (Project& project, std::vector<TempoPoint> points, const juce::String& undoName, bool newUndoStep = true);

    // Time signatures (bars count from 0)
    bool setTimeSignature (Project& project, int bar, int numerator, int denominator);
    bool deleteTimeSignature (Project& project, int bar);

    // Loop
    bool setLoopRange (Project& project, Tick start, Tick end, bool newUndoStep = true);
    bool setLoopEnabled (Project& project, bool enabled, bool newUndoStep = true);
}

} // namespace aether
