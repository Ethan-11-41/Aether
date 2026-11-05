// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <juce_data_structures/juce_data_structures.h>

namespace aether
{

/** The list of recently opened projects shown on the start screen (stored in the settings). */
class RecentProjects
{
public:
    static constexpr int maxEntries = 12;
    static constexpr const char* settingsKey = "recentProjects";

    explicit RecentProjects (juce::PropertySet& settingsToUse)  : settings (settingsToUse) {}

    /** Most recent first. Projects that no longer exist on disk are left out. */
    juce::Array<juce::File> getFiles() const;

    /** Puts a project at the top of the list. */
    void add (const juce::File& projectFile);
    void remove (const juce::File& projectFile);
    void clear();

private:
    juce::StringArray load() const;
    void store (const juce::StringArray& paths);

    juce::PropertySet& settings;
};

} // namespace aether
