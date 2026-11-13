// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <memory>
#include <optional>

#include <juce_data_structures/juce_data_structures.h>

#include "Autosaver.h"
#include "ProjectFiles.h"
#include "RecentProjects.h"

namespace aether
{

/** The project that is currently open, and everything that happens to it as a file:
    creating, opening, saving, autosaving, closing and recovering after a crash.

    No user interface here (that's in source/app), so all of this can be tested automatically.
*/
class ProjectSession
{
public:
    ProjectSession (juce::PropertySet& settings, RecentProjects& recentProjects,
                    int autosaveIntervalSeconds = Autosaver::defaultIntervalSeconds);
    ~ProjectSession();

    bool hasProject() const noexcept                    { return project != nullptr; }
    Project* getProject() noexcept                      { return project.get(); }
    const ProjectLocation& getLocation() const noexcept { return location; }
    Autosaver* getAutosaver() noexcept                  { return autosaver.get(); }

    /** Creates a project folder inside `parentFolder`, saves the empty project there and opens it. */
    juce::Result createNew (const juce::File& parentFolder, const juce::String& name);

    /** Opens a project file. With `fromAutosave`, the newer autosaved copy is used instead (the
        result then counts as unsaved changes, so the user can decide to save it). */
    juce::Result open (const juce::File& projectFile, bool fromAutosave = false);

    /** Saves the open project to its file (and removes the autosave, which is now out of date). */
    juce::Result save();

    /** Saves the project as a new project folder `name` inside `parentFolder`, copying its audio,
        and switches to the new copy. */
    juce::Result saveAs (const juce::File& parentFolder, const juce::String& name);

    /** Renames the open project ("Rename song"): its folder and file (see projectfiles::rename),
        and the name shown everywhere. Unsaved changes stay unsaved; nothing else changes. */
    juce::Result rename (const juce::String& newName);

    /** Closes the project without saving. Any autosave is removed (the user chose this). */
    void close();

    /** If Aether didn't close properly last time while a project was open, returns that
        project's file (it may or may not have an autosave to recover). */
    std::optional<juce::File> getProjectLeftOpenLastTime() const;

    static constexpr const char* openProjectKey = "openProject";

private:
    void adopt (std::unique_ptr<Project> newProject, const ProjectLocation& newLocation);

    juce::PropertySet& settings;
    RecentProjects& recent;
    const int autosaveInterval;
    std::optional<juce::File> leftOpenLastTime;

    std::unique_ptr<Project> project;
    ProjectLocation location;
    std::unique_ptr<Autosaver> autosaver;
};

} // namespace aether
