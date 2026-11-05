// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <juce_data_structures/juce_data_structures.h>

namespace aether::json
{

/** Converts between JUCE ValueTrees (Aether's in-memory project data) and JSON-style objects.

    Each tree node becomes a JSON object:
        { "_type": "Track", "name": "Vocals", "colour": "#4FC3C9", "_children": [ ... ] }

    - "_type" is the node type, "_children" the child nodes in order (left out if empty).
    - Every property becomes a key with the same name. Property names must not start with "_".
    - Values may be text, whole numbers (64-bit), decimal numbers or true/false.
*/

inline constexpr const char* typeKey     = "_type";
inline constexpr const char* childrenKey = "_children";

juce::var toVar (const juce::ValueTree& tree);

/** Returns an invalid ValueTree if `object` isn't a node object (no "_type"). Unknown keys are
    kept as properties, so data written by newer versions isn't thrown away. */
juce::ValueTree fromVar (const juce::var& object);

/** Formats a var as JSON text, in a way that reads back exactly (including decimal numbers). */
juce::String toJsonText (const juce::var& value);

} // namespace aether::json
