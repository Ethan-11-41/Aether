// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "ProjectFiles.h"

namespace aether::projectfiles
{

juce::File defaultProjectsFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Aether Projects");
}

juce::String safeFileName (const juce::String& name)
{
    // Characters Windows forbids in file names, plus control characters.
    auto cleaned = name.replaceCharacters ("<>:\"/\\|?*", "         ");
    juce::String result;

    for (auto c : cleaned)
        result << (c < 32 ? juce::juce_wchar (' ') : c);

    // No leading/trailing spaces or dots (Windows ignores them), and no doubled spaces.
    result = result.trim().trimCharactersAtEnd (". ").trim();
    while (result.contains ("  "))
        result = result.replace ("  ", " ");

    // Names Windows reserves for devices.
    static const juce::StringArray reserved { "CON", "PRN", "AUX", "NUL",
                                              "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
                                              "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9" };
    if (reserved.contains (result, true))
        result << " project";

    if (result.length() > 100)
        result = result.substring (0, 100).trim();

    return result.isEmpty() ? juce::String ("Untitled") : result;
}

juce::Result createProjectFolder (const juce::File& parentFolder, const juce::String& projectName,
                                  ProjectLocation& location)
{
    if (! parentFolder.isDirectory())
        if (auto r = parentFolder.createDirectory(); r.failed())
            return juce::Result::fail ("Couldn't create the folder " + parentFolder.getFullPathName() + ": " + r.getErrorMessage());

    const auto baseName = safeFileName (projectName);
    auto folder = parentFolder.getChildFile (baseName);

    for (int n = 2; folder.exists(); ++n)
        folder = parentFolder.getChildFile (baseName + " " + juce::String (n));

    ProjectLocation newLocation (folder.getChildFile (folder.getFileName() + projectformat::fileExtension));

    for (auto dir : { folder, newLocation.getAudioFolder(), newLocation.getBackupsFolder(), newLocation.getAutosaveFolder() })
        if (auto r = dir.createDirectory(); r.failed())
            return juce::Result::fail ("Couldn't create the project folder " + dir.getFullPathName() + ": " + r.getErrorMessage());

    location = newLocation;
    return juce::Result::ok();
}

juce::Result writeTextAtomically (const juce::File& file, const juce::String& text)
{
    if (auto r = file.getParentDirectory().createDirectory(); r.failed())
        return r;

    juce::TemporaryFile temp (file, juce::TemporaryFile::useHiddenFile);

    {
        juce::FileOutputStream out (temp.getFile());

        if (! out.openedOk())
            return juce::Result::fail ("Couldn't write to " + temp.getFile().getFullPathName()
                                       + " (" + out.getStatus().getErrorMessage() + ")");

        out.writeText (text, false, false, "\n");
        out.flush();

        if (out.getStatus().failed())
            return juce::Result::fail ("Writing failed (is the disk full?): " + out.getStatus().getErrorMessage());
    }

    if (! temp.overwriteTargetFileWithTemporary())
        return juce::Result::fail ("Couldn't replace " + file.getFullPathName()
                                   + ". Is it open in another program, or read-only?");

    return juce::Result::ok();
}

void pruneBackups (const ProjectLocation& location, int maxBackups)
{
    auto backups = location.getBackupsFolder().findChildFiles (juce::File::findFiles, false,
                                                               "*" + juce::String (projectformat::fileExtension));

    // Newest first.
    std::sort (backups.begin(), backups.end(), [] (const juce::File& a, const juce::File& b)
    {
        return a.getLastModificationTime() > b.getLastModificationTime();
    });

    for (int i = juce::jmax (0, maxBackups); i < backups.size(); ++i)
        backups.getReference (i).deleteFile();
}

juce::Result save (const Project& project, const ProjectLocation& location, int maxBackups)
{
    if (! location.isValid())
        return juce::Result::fail ("The project has no file to save to.");

    const auto mainFile = location.getMainFile();

    // Keep the previous version as a backup before replacing it.
    if (mainFile.existsAsFile() && maxBackups > 0)
    {
        const auto stamp = juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H-%M-%S");
        auto backup = location.getBackupsFolder().getChildFile (location.getName() + " " + stamp + projectformat::fileExtension);
        location.getBackupsFolder().createDirectory();

        if (backup.existsAsFile())
            backup = backup.getNonexistentSibling();

        mainFile.copyFileTo (backup);   // a failed backup shouldn't stop the save itself
        pruneBackups (location, maxBackups);
    }

    return writeTextAtomically (mainFile, projectformat::write (project));
}

projectformat::LoadResult load (const juce::File& file)
{
    if (! file.existsAsFile())
    {
        projectformat::LoadResult missing;
        missing.result = juce::Result::fail ("The project file " + file.getFullPathName() + " doesn't exist.");
        return missing;
    }

    return projectformat::read (file.loadFileAsString());
}

bool hasRecoverableAutosave (const ProjectLocation& location)
{
    const auto autosave = location.getAutosaveFile();

    if (! autosave.existsAsFile())
        return false;

    const auto mainFile = location.getMainFile();
    return ! mainFile.existsAsFile() || autosave.getLastModificationTime() > mainFile.getLastModificationTime();
}

juce::File whatToDelete (const juce::File& projectFile)
{
    const ProjectLocation location (projectFile);
    const auto folder = location.getFolder();

    if (folder.getFileName() != projectFile.getFileNameWithoutExtension())
        return projectFile;

    // Only what Aether puts there: the project file and its own folders.
    for (const auto& entry : juce::RangedDirectoryIterator (folder, false, "*", juce::File::findFilesAndDirectories))
    {
        const auto f = entry.getFile();
        const auto ours = f == projectFile || f == location.getAudioFolder() || f == location.getBackupsFolder()
                       || f == location.getAutosaveFolder() || f.getFileName() == "Exports"
                       || f.getFileName().startsWith (".") || f.getFileName().endsWithIgnoreCase (".tmp");

        if (! ours)
            return projectFile;
    }

    return folder;
}

} // namespace aether::projectfiles
