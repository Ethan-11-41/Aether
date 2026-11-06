// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <juce_core/juce_core.h>

namespace aether::filetypes
{

/** True for Aether project files (.aether). */
bool isProject (const juce::File& file);

/** True if the file's extension is in a list like "*.wav;*.aif;*.flac". */
bool matchesWildcard (const juce::File& file, const juce::String& wildcard);

/** True for files that can be imported into a project: audio (one of the types in
    `audioWildcard`, see AudioFileCache::getWildcard) or MIDI. */
bool isImportable (const juce::File& file, const juce::String& audioWildcard);

} // namespace aether::filetypes
