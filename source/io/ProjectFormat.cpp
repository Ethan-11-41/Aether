// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "ProjectFormat.h"

#include "JsonValueTree.h"

namespace aether::projectformat
{

const std::map<int, UpgradeStep>& upgradeSteps()
{
    // Version 1 is the first format, so there is nothing to upgrade yet. When version 2 is
    // introduced, add:   { 1, [] (juce::DynamicObject& file) { ...; return juce::Result::ok(); } }
    static const std::map<int, UpgradeStep> steps;
    return steps;
}

juce::Result upgrade (juce::DynamicObject& file, int targetVersion, const std::map<int, UpgradeStep>& steps)
{
    auto version = (int) file.getProperty ("formatVersion");

    while (version < targetVersion)
    {
        const auto step = steps.find (version);

        if (step == steps.end())
            return juce::Result::fail ("There is no way to upgrade a version " + juce::String (version)
                                       + " project file (this is a bug in Aether).");

        if (auto r = step->second (file); r.failed())
            return juce::Result::fail ("Upgrading the project from version " + juce::String (version)
                                       + " failed: " + r.getErrorMessage());

        ++version;
        file.setProperty ("formatVersion", version);
    }

    return juce::Result::ok();
}

juce::String write (const juce::ValueTree& projectState)
{
    auto file = std::make_unique<juce::DynamicObject>();
    file->setProperty ("format", formatName);
    file->setProperty ("formatVersion", currentVersion);
    file->setProperty ("savedWith", juce::String ("Aether ") + AETHER_VERSION_STRING);
    file->setProperty ("savedAt", juce::Time::getCurrentTime().toISO8601 (true));
    file->setProperty ("project", json::toVar (projectState));

    return json::toJsonText (juce::var (file.release())) + "\n";
}

juce::String write (const Project& project)
{
    return write (project.getState());
}

LoadResult read (const juce::String& fileText)
{
    LoadResult load;
    juce::var parsed;

    if (auto r = juce::JSON::parse (fileText, parsed); r.failed())
    {
        load.result = juce::Result::fail ("The project file is damaged or isn't an Aether project ("
                                          + r.getErrorMessage() + ").");
        return load;
    }

    auto* file = parsed.getDynamicObject();

    if (file == nullptr || file->getProperty ("format").toString() != formatName)
    {
        load.result = juce::Result::fail ("This file isn't an Aether project.");
        return load;
    }

    const auto versionVar = file->getProperty ("formatVersion");

    if (! (versionVar.isInt() || versionVar.isInt64()) || (int) versionVar < 1)
    {
        load.result = juce::Result::fail ("The project file has no valid format version.");
        return load;
    }

    load.originalVersion = (int) versionVar;

    if (load.originalVersion > currentVersion)
    {
        load.result = juce::Result::fail ("This project was saved by a newer version of Aether ("
                                          + file->getProperty ("savedWith").toString()
                                          + "). Please update Aether to open it. The file has not been changed.");
        return load;
    }

    if (auto r = upgrade (*file, currentVersion, upgradeSteps()); r.failed())
    {
        load.result = r;
        return load;
    }

    load.state = json::fromVar (file->getProperty ("project"));

    if (! load.state.hasType (ids::Project))
    {
        load.state = {};
        load.result = juce::Result::fail ("The project file is missing its project data.");
    }

    return load;
}

} // namespace aether::projectformat
