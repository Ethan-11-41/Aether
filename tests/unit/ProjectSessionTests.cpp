// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include <catch2/catch_test_macros.hpp>

#include "io/ProjectSession.h"
#include "model/EditCommands.h"
#include "model/TimelineEdits.h"

using namespace aether;

namespace
{
    struct Fixture
    {
        Fixture()
            : folder (juce::File::getSpecialLocation (juce::File::tempDirectory)
                          .getChildFile ("aether-tests")
                          .getNonexistentChildFile ("session", {}, false))
        {
            folder.createDirectory();
        }

        ~Fixture()  { folder.deleteRecursively(); }

        juce::ScopedJuceInitialiser_GUI juce;
        juce::File folder;
        juce::PropertySet settings;
        RecentProjects recent { settings };
    };
}

TEST_CASE ("Creating a project makes its folder, saves it and lists it as recent", "[session]")
{
    Fixture f;
    ProjectSession session (f.settings, f.recent, 0);
    CHECK_FALSE (session.hasProject());

    REQUIRE (session.createNew (f.folder, "First Song").wasOk());
    REQUIRE (session.hasProject());
    CHECK (session.getProject()->getName() == "First Song");
    CHECK (session.getLocation().getMainFile().existsAsFile());
    CHECK_FALSE (session.getProject()->hasUnsavedChanges());
    CHECK (f.recent.getFiles().getFirst() == session.getLocation().getMainFile());
}

TEST_CASE ("Save, close and reopen gives back the same project", "[session]")
{
    Fixture f;
    juce::File file;

    {
        ProjectSession session (f.settings, f.recent, 0);
        REQUIRE (session.createNew (f.folder, "Round Trip").wasOk());
        auto track = edit::addTrack (*session.getProject(), TrackKind::audio);
        edit::renameTrack (*session.getProject(), track.getId(), "Bass");
        CHECK (session.getProject()->hasUnsavedChanges());

        REQUIRE (session.save().wasOk());
        CHECK_FALSE (session.getProject()->hasUnsavedChanges());
        file = session.getLocation().getMainFile();
        session.close();
        CHECK_FALSE (session.hasProject());
    }

    ProjectSession session (f.settings, f.recent, 0);
    REQUIRE (session.open (file).wasOk());
    REQUIRE (session.getProject()->getNumTracks() == 1);
    CHECK (session.getProject()->getTrack (0).getName() == "Bass");
    CHECK_FALSE (session.getProject()->hasUnsavedChanges());
}

TEST_CASE ("After a crash, unsaved changes can be recovered from the autosave", "[session][recovery]")
{
    Fixture f;
    juce::File file;

    {
        ProjectSession session (f.settings, f.recent, 0);
        REQUIRE (session.createNew (f.folder, "Crashy").wasOk());
        file = session.getLocation().getMainFile();
        file.setLastModificationTime (juce::Time::getCurrentTime() - juce::RelativeTime::minutes (2));

        edit::addTrack (*session.getProject(), TrackKind::midi);   // never saved
        REQUIRE (session.getAutosaver()->autosaveNow());
        session.getAutosaver()->waitUntilIdle();
    }

    // Simulate a crash: in a real crash the session is never shut down, so the note saying
    // "this project is open" stays in the settings. (Shutting down above removed it, so put it back.)
    f.settings.setValue (ProjectSession::openProjectKey, file.getFullPathName());

    ProjectSession next (f.settings, f.recent, 0);
    const auto leftOpen = next.getProjectLeftOpenLastTime();
    REQUIRE (leftOpen.has_value());
    CHECK (*leftOpen == file);
    CHECK (projectfiles::hasRecoverableAutosave (ProjectLocation (file)));

    // The saved file has no tracks; the autosave has the unsaved MIDI track.
    REQUIRE (next.open (file, true).wasOk());
    REQUIRE (next.getProject()->getNumTracks() == 1);
    CHECK (next.getProject()->getTrack (0).getKind() == TrackKind::midi);
    CHECK (next.getProject()->hasUnsavedChanges());

    // Saving makes it permanent and removes the autosave.
    REQUIRE (next.save().wasOk());
    CHECK_FALSE (ProjectLocation (file).getAutosaveFile().exists());
    CHECK_FALSE (next.getProject()->hasUnsavedChanges());
}

TEST_CASE ("Closing without saving throws away the autosave", "[session][recovery]")
{
    Fixture f;
    ProjectSession session (f.settings, f.recent, 0);
    REQUIRE (session.createNew (f.folder, "Discard").wasOk());
    const auto location = session.getLocation();

    edit::addTrack (*session.getProject(), TrackKind::audio);
    session.getAutosaver()->autosaveNow();
    session.getAutosaver()->waitUntilIdle();
    REQUIRE (location.getAutosaveFile().existsAsFile());

    session.close();
    CHECK_FALSE (location.getAutosaveFile().exists());
    CHECK (f.settings.getValue (ProjectSession::openProjectKey).isEmpty());
}

TEST_CASE ("A clean exit doesn't count as being left open", "[session]")
{
    Fixture f;

    {
        ProjectSession session (f.settings, f.recent, 0);
        REQUIRE (session.createNew (f.folder, "Clean").wasOk());
    }   // the app quits normally

    ProjectSession next (f.settings, f.recent, 0);
    CHECK_FALSE (next.getProjectLeftOpenLastTime().has_value());
}

TEST_CASE ("Save As makes an independent copy in a new folder", "[session]")
{
    Fixture f;
    ProjectSession session (f.settings, f.recent, 0);
    REQUIRE (session.createNew (f.folder, "Original").wasOk());
    const auto original = session.getLocation();
    original.getAudioFolder().getChildFile ("take1.wav").replaceWithText ("pretend audio");

    edit::addTrack (*session.getProject(), TrackKind::audio);
    REQUIRE (session.saveAs (f.folder, "Copy").wasOk());

    CHECK (session.getLocation().getFolder().getFileName() == "Copy");
    CHECK (session.getProject()->getName() == "Copy");
    CHECK (session.getLocation().getAudioFolder().getChildFile ("take1.wav").existsAsFile());
    CHECK_FALSE (session.getProject()->hasUnsavedChanges());

    // The original file still has the state it was last saved with (no tracks).
    auto originalProject = Project::fromState (projectfiles::load (original.getMainFile()).state);
    REQUIRE (originalProject != nullptr);
    CHECK (originalProject->getNumTracks() == 0);
}

TEST_CASE ("Opening a bad file leaves the current project open", "[session]")
{
    Fixture f;
    ProjectSession session (f.settings, f.recent, 0);
    REQUIRE (session.createNew (f.folder, "Keep Me").wasOk());

    auto junk = f.folder.getChildFile ("junk.aether");
    junk.replaceWithText ("not a project");

    CHECK (session.open (junk).failed());
    REQUIRE (session.hasProject());
    CHECK (session.getProject()->getName() == "Keep Me");
}

TEST_CASE ("Tempo, time signature and loop changes survive save, close and reopen", "[session][timeline]")
{
    Fixture f;
    juce::File file;

    {
        ProjectSession session (f.settings, f.recent, 0);
        REQUIRE (session.createNew (f.folder, "Timeline Song").wasOk());
        auto& project = *session.getProject();
        edit::setTempo (project, 0, 93.0);
        edit::addTempoChange (project, 16 * ticksPerQuarter, 140.0);
        edit::setTimeSignature (project, 0, 3, 4);
        edit::setLoopRange (project, 4 * ticksPerQuarter, 8 * ticksPerQuarter);
        edit::setLoopEnabled (project, true);
        REQUIRE (session.save().wasOk());
        file = session.getLocation().getMainFile();
        session.close();
    }

    INFO (file.loadFileAsString());
    ProjectSession session (f.settings, f.recent, 0);
    REQUIRE (session.open (file).wasOk());
    const auto map = timeline::buildTempoMap (*session.getProject());
    CHECK (map.tempoAt (0) == 93.0);
    CHECK (map.getTempoPoints().size() == 2);
    CHECK (map.getTimeSignatures().front().numerator == 3);
    CHECK (timeline::getLoop (*session.getProject()).enabled);
    CHECK (timeline::getLoop (*session.getProject()).start == 4 * ticksPerQuarter);
}

TEST_CASE ("Renaming a song renames its folder and file, and keeps everything in them", "[session]")
{
    Fixture f;
    ProjectSession session (f.settings, f.recent, 0);
    REQUIRE (session.createNew (f.folder, "Old Name").wasOk());
    auto& project = *session.getProject();
    const auto oldFile = session.getLocation().getMainFile();

    // A recording in the Audio folder, a backup and an autosave: they all come along.
    session.getLocation().getAudioFolder().getChildFile ("Take 1.wav").replaceWithText ("audio");
    REQUIRE (session.save().wasOk());   // makes a backup of the first version
    edit::addTrack (project, TrackKind::audio);
    session.getLocation().getAutosaveFile().replaceWithText ("autosaved");
    CHECK (project.hasUnsavedChanges());

    REQUIRE (session.rename ("New Name").wasOk());
    const auto& renamed = session.getLocation();
    CHECK (renamed.getName() == "New Name");
    CHECK (renamed.getFolder() == f.folder.getChildFile ("New Name"));
    CHECK (renamed.getMainFile() == f.folder.getChildFile ("New Name/New Name.aether"));
    CHECK (renamed.getMainFile().existsAsFile());
    CHECK_FALSE (oldFile.getParentDirectory().exists());
    CHECK (renamed.getAudioFolder().getChildFile ("Take 1.wav").existsAsFile());
    CHECK (renamed.getAutosaveFile().existsAsFile());
    CHECK (renamed.getBackupsFolder().findChildFiles (juce::File::findFiles, false, "New Name *.aether").size() == 1);

    // The name shown everywhere follows; the unsaved change is still unsaved.
    CHECK (project.getName() == "New Name");
    CHECK (project.hasUnsavedChanges());
    CHECK (f.recent.getFiles().getFirst() == renamed.getMainFile());
    CHECK (f.settings.getValue (ProjectSession::openProjectKey) == renamed.getMainFile().getFullPathName());

    // Saved and opened again, it's the new name.
    REQUIRE (session.save().wasOk());
    const auto file = renamed.getMainFile();
    session.close();
    REQUIRE (session.open (file).wasOk());
    CHECK (session.getProject()->getName() == "New Name");
    CHECK (session.getProject()->getNumTracks() == 1);
}

TEST_CASE ("Renaming a saved song leaves nothing unsaved; a taken name is refused", "[session]")
{
    Fixture f;
    ProjectSession session (f.settings, f.recent, 0);
    REQUIRE (session.createNew (f.folder, "Other").wasOk());
    session.close();
    REQUIRE (session.createNew (f.folder, "Mine").wasOk());

    REQUIRE (session.rename ("Mine, take 2").wasOk());
    CHECK_FALSE (session.getProject()->hasUnsavedChanges());
    CHECK (session.getProject()->getName() == "Mine, take 2");

    // Another song's folder is in the way: nothing changes.
    const auto before = session.getLocation().getMainFile();
    CHECK (session.rename ("Other").failed());
    CHECK (session.getLocation().getMainFile() == before);
    CHECK (before.existsAsFile());
    CHECK (session.rename ("  ").failed());

    // Only the capitals change.
    REQUIRE (session.rename ("MINE, TAKE 2").wasOk());
    CHECK (session.getLocation().getMainFile().getFileName() == "MINE, TAKE 2.aether");
    CHECK (session.getLocation().getFolder().getFileName() == "MINE, TAKE 2");
}

TEST_CASE ("A song saved in a folder with other things renames only its file", "[session]")
{
    Fixture f;
    const auto shared = f.folder.getChildFile ("My music");
    shared.createDirectory();
    shared.getChildFile ("notes.txt").replaceWithText ("not Aether's");

    ProjectSession session (f.settings, f.recent, 0);
    REQUIRE (session.createNew (f.folder, "Temp").wasOk());
    const auto file = shared.getChildFile ("Loose song.aether");
    REQUIRE (session.getLocation().getMainFile().copyFileTo (file));
    session.close();

    f.recent.add (file);
    juce::File renamedTo;
    REQUIRE (projectfiles::rename (file, "Better song", renamedTo).wasOk());
    CHECK (renamedTo == shared.getChildFile ("Better song.aether"));
    CHECK (renamedTo.existsAsFile());
    CHECK (shared.getChildFile ("notes.txt").existsAsFile());   // the folder stays as it was

    f.recent.replace (file, renamedTo);
    CHECK (f.recent.getFiles().getFirst() == renamedTo);
}
