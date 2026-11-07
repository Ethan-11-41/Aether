// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include "MidiClips.h"

/** Drum patterns (Milestone 11): a MIDI clip on a drum track, edited as a grid of steps.

    A pattern is an ordinary MIDI clip that repeats (see edit::setPatternLooping): each pad's
    hits are notes (pad 1 is note 36, see dsp::drums), so the piano roll, quantize, copy and paste
    all work on them too. The step sequencer just shows the notes that sit on its steps:

    - the step size (1/8, 1/16 or 1/32 note) is stored on the clip ("stepLength"), so the pattern
      opens the way it was left;
    - swing ("swing", 0 to 1) pushes every second step later, up to a triplet shuffle at 1; the
      notes really move, so what you hear is what's stored. */
namespace aether
{

namespace drumpattern
{
    constexpr Tick defaultStepLength = ticksPerQuarter / 4;   // 1/16 notes
    constexpr int maxSteps = 128;
    constexpr int defaultVelocity = 100, accentVelocity = 127;

    /** Is this a drum track (a MIDI track playing the drum kit)? */
    bool isDrumTrack (const Project& project, const juce::String& trackId);
    bool isDrumClip (const Project& project, const Clip& clip);

    Tick stepLengthOf (const Clip& clip);
    double swingOf (const Clip& clip);

    /** How many steps the pattern has (its length in steps, at most maxSteps). */
    int numSteps (const Clip& clip);

    /** Where a step starts, from the start of the pattern (with swing). */
    Tick stepTime (int step, Tick stepLength, double swing) noexcept;

    /** The note of a pad on a step (invalid if the step is off). Notes that are near a step (less
        than half a step away) count as on it. */
    juce::ValueTree findStep (const Clip& clip, int pad, int step);

    /** How hard a step is hit (0: off). */
    int stepVelocity (const Clip& clip, int pad, int step);
}

namespace edit
{
    /** Adds a drum track (a MIDI track named "Drums" with the drum kit), as one undo step. */
    Track addDrumTrack (Project& project, int insertIndex = -1, const juce::String& kitName = {});

    /** Adds an empty repeating pattern (`length` ticks long) to a MIDI track. One undo step. */
    juce::String addPattern (Project& project, const juce::String& trackId, Tick start, Tick length, const juce::String& name = {});

    /** Turns a pad's step on (velocity 1 to 127) or off (0), or changes how hard it's hit. */
    bool setStep (Project& project, const juce::String& clipId, int pad, int step, int velocity, bool newUndoStep = true);

    /** The step sequencer's step size (ticks). */
    bool setStepLength (Project& project, const juce::String& clipId, Tick stepLength);

    /** Swing (0 to 1): moves the notes on every second step. */
    bool setSwing (Project& project, const juce::String& clipId, double swing, bool newUndoStep = true);
}

} // namespace aether
