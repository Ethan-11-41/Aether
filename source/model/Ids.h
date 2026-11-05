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
    inline const juce::Identifier Project { "Project" };
    inline const juce::Identifier Tracks  { "Tracks" };
    inline const juce::Identifier Track   { "Track" };

    // Properties
    inline const juce::Identifier id      { "id" };       // unique id (UUID text) of a track, clip, etc.
    inline const juce::Identifier name    { "name" };
    inline const juce::Identifier kind    { "kind" };     // track kind: "audio" or "midi"
    inline const juce::Identifier colour  { "colour" };   // "#RRGGBB"
}
