// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <juce_data_structures/juce_data_structures.h>

/** Names of the nodes and properties in Aether's project tree.

    These names are written into project files, so they must NEVER be renamed: doing so would
    break every saved project. To change the structure, add a new name and write a format
    upgrade step (see docs/FILE_FORMAT.md).
*/
namespace aether::ids
{
    // Node types
    inline const juce::Identifier Project        { "Project" };
    inline const juce::Identifier Tracks         { "Tracks" };
    inline const juce::Identifier Track          { "Track" };
    inline const juce::Identifier Tempo          { "Tempo" };            // list of TempoPoint
    inline const juce::Identifier TempoPoint     { "TempoPoint" };
    inline const juce::Identifier TimeSignatures { "TimeSignatures" };   // list of TimeSignature
    inline const juce::Identifier TimeSignature  { "TimeSignature" };

    // Properties
    inline const juce::Identifier id      { "id" };       // unique id (UUID text) of a track, clip, etc.
    inline const juce::Identifier name    { "name" };
    inline const juce::Identifier kind    { "kind" };     // track kind: "audio" or "midi"
    inline const juce::Identifier colour  { "colour" };   // "#RRGGBB"

    // Timeline (positions are ticks: 960000 per quarter note)
    inline const juce::Identifier tick        { "tick" };
    inline const juce::Identifier bpm         { "bpm" };
    inline const juce::Identifier curve       { "curve" };        // "jump" or "ramp"
    inline const juce::Identifier bar         { "bar" };          // counting from 0
    inline const juce::Identifier numerator   { "numerator" };
    inline const juce::Identifier denominator { "denominator" };
    inline const juce::Identifier loopStart   { "loopStart" };
    inline const juce::Identifier loopEnd     { "loopEnd" };
    inline const juce::Identifier loopEnabled { "loopEnabled" };
}
