// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <functional>
#include <map>

#include <juce_data_structures/juce_data_structures.h>

#include "model/Project.h"

/** Aether's project file format: versioned JSON. Described in docs/FILE_FORMAT.md.

    {
      "format": "aether-project",
      "formatVersion": 1,
      "savedWith": "Aether 0.2.0",
      "savedAt": "2026-11-05T21:30:00Z",
      "project": { "_type": "Project", ... }
    }

    When the structure changes, `currentVersion` goes up by one and an upgrade step is added to
    upgradeSteps() that turns a version N file into a version N+1 file. Old files are passed
    through each step in turn, so every project ever saved can still be opened.
*/
namespace aether::projectformat
{

inline constexpr const char* formatName = "aether-project";
inline constexpr int currentVersion = 1;
inline constexpr const char* fileExtension = ".aether";

/** Turns a whole file (the outer JSON object) from one version into the next. */
using UpgradeStep = std::function<juce::Result (juce::DynamicObject& file)>;

/** The upgrade steps, keyed by the version they upgrade FROM (step 1 turns v1 into v2). */
const std::map<int, UpgradeStep>& upgradeSteps();

/** Upgrades the outer file object to `targetVersion` using `steps`. Fails if a step is missing. */
juce::Result upgrade (juce::DynamicObject& file, int targetVersion, const std::map<int, UpgradeStep>& steps);

/** Writes a project as file text. */
juce::String write (const Project& project);
juce::String write (const juce::ValueTree& projectState);

struct LoadResult
{
    juce::Result result = juce::Result::ok();
    juce::ValueTree state;           // the "Project" tree, valid if result is OK
    int originalVersion = 0;         // the version the file had before upgrading
};

/** Reads file text. Never throws: problems are reported in `result` in plain language. */
LoadResult read (const juce::String& fileText);

} // namespace aether::projectformat
