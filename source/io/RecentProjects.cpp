// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "RecentProjects.h"

#include <algorithm>

namespace aether
{

juce::StringArray RecentProjects::load() const
{
    auto paths = juce::StringArray::fromLines (settings.getValue (settingsKey));
    paths.removeEmptyStrings();
    return paths;
}

void RecentProjects::store (const juce::StringArray& paths)
{
    settings.setValue (settingsKey, paths.joinIntoString ("\n"));
}

juce::Array<juce::File> RecentProjects::getFiles() const
{
    juce::Array<juce::File> files;

    for (const auto& path : load())
        if (juce::File::isAbsolutePath (path))
            if (juce::File file (path); file.existsAsFile())
                files.add (file);

    return files;
}

void RecentProjects::add (const juce::File& projectFile)
{
    auto paths = load();
    paths.removeString (projectFile.getFullPathName(), ! juce::File::areFileNamesCaseSensitive());
    paths.insert (0, projectFile.getFullPathName());

    while (paths.size() > maxEntries)
        paths.remove (paths.size() - 1);

    store (paths);
}

void RecentProjects::replace (const juce::File& oldFile, const juce::File& newFile)
{
    auto paths = load();
    const auto index = paths.indexOf (oldFile.getFullPathName(), ! juce::File::areFileNamesCaseSensitive());

    if (index < 0)
    {
        add (newFile);
        return;
    }

    paths.removeString (newFile.getFullPathName(), ! juce::File::areFileNamesCaseSensitive());
    paths.set (std::min (index, paths.size()), newFile.getFullPathName());
    store (paths);
}

void RecentProjects::remove (const juce::File& projectFile)
{
    auto paths = load();
    paths.removeString (projectFile.getFullPathName(), ! juce::File::areFileNamesCaseSensitive());
    store (paths);
}

void RecentProjects::clear()
{
    store ({});
}

} // namespace aether
