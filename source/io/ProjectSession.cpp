// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "ProjectSession.h"

namespace aether
{

ProjectSession::ProjectSession (juce::PropertySet& settingsToUse, RecentProjects& recentProjects, int autosaveIntervalSeconds)
    : settings (settingsToUse), recent (recentProjects), autosaveInterval (autosaveIntervalSeconds)
{
    // A project still marked as open means the last session ended without closing it.
    const auto leftOpen = settings.getValue (openProjectKey);

    if (leftOpen.isNotEmpty() && juce::File::isAbsolutePath (leftOpen))
        leftOpenLastTime = juce::File (leftOpen);
}

ProjectSession::~ProjectSession()
{
    // Leaving with a project open (e.g. the app is quitting) is a clean exit if the caller has
    // already dealt with unsaved changes; the autosave is kept in case they chose not to save.
    autosaver = nullptr;
    project = nullptr;
    settings.removeValue (openProjectKey);
}

std::optional<juce::File> ProjectSession::getProjectLeftOpenLastTime() const
{
    return leftOpenLastTime;
}

void ProjectSession::adopt (std::unique_ptr<Project> newProject, const ProjectLocation& newLocation)
{
    autosaver = nullptr;
    project = std::move (newProject);
    location = newLocation;
    autosaver = std::make_unique<Autosaver> (*project, location, autosaveInterval);

    settings.setValue (openProjectKey, location.getMainFile().getFullPathName());
    recent.add (location.getMainFile());
    leftOpenLastTime.reset();
}

juce::Result ProjectSession::createNew (const juce::File& parentFolder, const juce::String& name)
{
    ProjectLocation newLocation;

    if (auto r = projectfiles::createProjectFolder (parentFolder, name, newLocation); r.failed())
        return r;

    auto newProject = std::make_unique<Project> (newLocation.getName());

    if (auto r = projectfiles::save (*newProject, newLocation); r.failed())
        return r;

    newProject->markAsSaved();
    adopt (std::move (newProject), newLocation);
    return juce::Result::ok();
}

juce::Result ProjectSession::open (const juce::File& projectFile, bool fromAutosave)
{
    const ProjectLocation newLocation (projectFile);
    const auto fileToRead = fromAutosave ? newLocation.getAutosaveFile() : projectFile;
    auto loaded = projectfiles::load (fileToRead);

    if (loaded.result.failed())
        return loaded.result;

    // The name follows the file name (it may have been renamed while it had unsaved changes).
    if (loaded.state.isValid())
        loaded.state.setProperty (ids::name, newLocation.getName(), nullptr);

    auto newProject = Project::fromState (loaded.state);

    if (newProject == nullptr)
        return juce::Result::fail ("The project file couldn't be read.");

    // A recovered autosave differs from the saved file, so it counts as unsaved changes: closing
    // will ask whether to keep them. The autosave file itself stays until the user saves, in
    // case Aether crashes again before then.
    if (fromAutosave)
        newProject->markAsUnsaved();

    adopt (std::move (newProject), newLocation);
    return juce::Result::ok();
}

juce::Result ProjectSession::save()
{
    if (! hasProject())
        return juce::Result::fail ("No project is open.");

    if (auto r = projectfiles::save (*project, location); r.failed())
        return r;

    project->markAsSaved();
    autosaver->discard();
    recent.add (location.getMainFile());
    return juce::Result::ok();
}

juce::Result ProjectSession::saveAs (const juce::File& parentFolder, const juce::String& name)
{
    if (! hasProject())
        return juce::Result::fail ("No project is open.");

    ProjectLocation newLocation;

    if (auto r = projectfiles::createProjectFolder (parentFolder, name, newLocation); r.failed())
        return r;

    // Bring the project's audio along, so the copy is complete on its own.
    if (location.getAudioFolder().isDirectory())
        location.getAudioFolder().copyDirectoryTo (newLocation.getAudioFolder());

    project->getState().setProperty (ids::name, newLocation.getName(), nullptr);   // follows the file name; not an undo step

    if (auto r = projectfiles::save (*project, newLocation); r.failed())
        return r;

    autosaver->discard();   // the old location's autosave is no longer needed
    project->markAsSaved();

    auto moved = std::move (project);
    adopt (std::move (moved), newLocation);
    return juce::Result::ok();
}

juce::Result ProjectSession::rename (const juce::String& newName)
{
    if (! hasProject())
        return juce::Result::fail ("No project is open.");

    // The autosaver writes into the folder from another thread: stopped while it moves.
    autosaver = nullptr;

    juce::File renamedTo;
    const auto oldFile = location.getMainFile();
    const auto result = projectfiles::rename (oldFile, newName, renamedTo);

    if (result.wasOk() && renamedTo != oldFile)
    {
        const auto wasSaved = ! project->hasUnsavedChanges();
        location = ProjectLocation (renamedTo);
        project->getState().setProperty (ids::name, location.getName(), nullptr);   // follows the file name; not an undo step

        if (wasSaved)
            project->markAsSaved();   // only the name changed, and the file is named so

        settings.setValue (openProjectKey, location.getMainFile().getFullPathName());
        recent.replace (oldFile, location.getMainFile());
    }

    autosaver = std::make_unique<Autosaver> (*project, location, autosaveInterval);
    return result;
}

void ProjectSession::close()
{
    if (autosaver != nullptr)
        autosaver->discard();

    autosaver = nullptr;
    project = nullptr;
    location = {};
    settings.removeValue (openProjectKey);
}

} // namespace aether
