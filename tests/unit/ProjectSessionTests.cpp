// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include <catch2/catch_test_macros.hpp>

#include "io/ProjectSession.h"
#include "model/EditCommands.h"

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
