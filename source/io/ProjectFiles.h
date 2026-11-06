// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <juce_core/juce_core.h>

#include "ProjectFormat.h"
#include "model/Project.h"

namespace aether
{

/** Where a project's files live. A project is a folder:

        My Song/
          My Song.aether              the project file (JSON)
          Audio/                      recorded and imported audio
          Backups/                    the previous versions of the project file
          Autosave/                   automatic saves, used for crash recovery
*/
class ProjectLocation
{
public:
    ProjectLocation() = default;
    explicit ProjectLocation (juce::File projectFile)   : mainFile (std::move (projectFile)) {}

    bool isValid() const                    { return mainFile != juce::File(); }

    juce::File getMainFile() const          { return mainFile; }
    juce::File getFolder() const            { return mainFile.getParentDirectory(); }
    juce::File getAudioFolder() const       { return getFolder().getChildFile ("Audio"); }
    juce::File getBackupsFolder() const     { return getFolder().getChildFile ("Backups"); }
    juce::File getAutosaveFolder() const    { return getFolder().getChildFile ("Autosave"); }
    juce::File getAutosaveFile() const      { return getAutosaveFolder().getChildFile (mainFile.getFileNameWithoutExtension() + ".autosave" + projectformat::fileExtension); }

    /** The project's name, taken from the file name. */
    juce::String getName() const            { return mainFile.getFileNameWithoutExtension(); }

private:
    juce::File mainFile;
};

namespace projectfiles
{
    /** Where new projects go unless the user picks somewhere else: Documents\Aether Projects */
    juce::File defaultProjectsFolder();

    /** Removes characters that Windows doesn't allow in file names, e.g. "a/b?" becomes "a b". */
    juce::String safeFileName (const juce::String& name);

    /** Creates a new project folder (with its sub-folders) inside `parentFolder`. If the name is
        taken, adds a number ("My Song 2"). On success `location` is set. */
    juce::Result createProjectFolder (const juce::File& parentFolder, const juce::String& projectName,
                                      ProjectLocation& location);

    /** Saves the project. The previous version of the file is copied to Backups/ first (keeping
        the newest `maxBackups`), and the file is replaced in one step so a crash or power cut
        during saving can't leave a half-written project. */
    juce::Result save (const Project& project, const ProjectLocation& location, int maxBackups = 10);

    /** Reads a project file (or an autosave file). */
    projectformat::LoadResult load (const juce::File& file);

    /** Writes text to a file via a temporary file, replacing the old file only when complete. */
    juce::Result writeTextAtomically (const juce::File& file, const juce::String& text);

    /** Deletes the oldest backups so that at most `maxBackups` remain. */
    void pruneBackups (const ProjectLocation& location, int maxBackups);

    /** True if the project has an autosave that is newer than the saved project, i.e. there are
        changes that were never saved (usually because Aether closed unexpectedly). */
    bool hasRecoverableAutosave (const ProjectLocation& location);

    /** What "Delete project" removes: the project's whole folder if it's laid out the way Aether
        makes projects (a folder named like the project, holding only the project, its Audio,
        Backups, Autosave and Exports), otherwise just the project file, so nothing else that
        happens to be in the same folder is ever deleted. */
    juce::File whatToDelete (const juce::File& projectFile);
}

} // namespace aether
