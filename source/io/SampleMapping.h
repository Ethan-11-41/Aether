// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include "SfzReader.h"

/** Making an instrument from a set of recordings (Milestone 12), by their file names.

    Names usually say which note each recording is ("Piano_C4_mf_rr2.wav"): the note (C4 = 60,
    C#4, Db4), how loud it was played (ppp, pp, p, mp, mf, f, ff, fff, or v1, v2...) and which
    take it is (rr1, rr2...). Each note's recording covers the keys up to half way to the next
    one; loudness layers share out the velocities; takes alternate. Recordings whose names have
    no note are laid out one per key from C2 (like drum pads). */
namespace aether::samplemapping
{

/** The note named in a file name, or -1. */
int noteInName (const juce::String& fileName);

/** How loud a recording was played, as a rank (higher = louder), or 0 if the name doesn't say. */
int loudnessInName (const juce::String& fileName);

/** Which take (round robin) a recording is, or 0 if the name doesn't say. */
int takeInName (const juce::String& fileName);

/** An instrument from recordings, with each region's `sample` relative to `folder`. */
sfz::Instrument mapByNames (const juce::Array<juce::File>& files, const juce::File& folder);

} // namespace aether::samplemapping
