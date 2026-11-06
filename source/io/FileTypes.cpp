// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "FileTypes.h"

#include "MidiImport.h"
#include "ProjectFormat.h"

namespace aether::filetypes
{

bool isProject (const juce::File& file)
{
    return file.hasFileExtension (projectformat::fileExtension)
        && ! file.getFileName().containsIgnoreCase (".autosave.");
}

bool matchesWildcard (const juce::File& file, const juce::String& wildcard)
{
    const auto extension = file.getFileExtension();

    if (extension.isEmpty())
        return false;

    for (auto pattern : juce::StringArray::fromTokens (wildcard, ";,", {}))
        if (pattern.trim().equalsIgnoreCase ("*" + extension))
            return true;

    return false;
}

bool isImportable (const juce::File& file, const juce::String& audioWildcard)
{
    return isMidiFile (file) || matchesWildcard (file, audioWildcard);
}

} // namespace aether::filetypes
