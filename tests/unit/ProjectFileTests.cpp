// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include <catch2/catch_test_macros.hpp>

#include "io/Autosaver.h"
#include "io/FileTypes.h"
#include "io/JsonValueTree.h"
#include "io/ProjectFiles.h"
#include "io/ProjectFormat.h"
#include "io/RecentProjects.h"
#include "model/EditCommands.h"

using namespace aether;

namespace
{
    /** A fresh, empty folder for one test, deleted afterwards. */
    struct TempFolder
    {
        TempFolder()
            : folder (juce::File::getSpecialLocation (juce::File::tempDirectory)
                          .getChildFile ("aether-tests")
                          .getNonexistentChildFile ("run", {}, false))
        {
            folder.createDirectory();
        }

        ~TempFolder()  { folder.deleteRecursively(); }

        juce::File folder;
    };

    juce::File fixture (const juce::String& name)
    {
        return juce::File (AETHER_TEST_FIXTURES_DIR).getChildFile (name);
    }

    std::unique_ptr<Project> makeSampleProject()
    {
        auto project = std::make_unique<Project> ("Demo Song");
        auto drums = edit::addTrack (*project, TrackKind::audio);
        edit::renameTrack (*project, drums.getId(), "Drums");
        auto keys = edit::addTrack (*project, TrackKind::midi);
        edit::setTrackColour (*project, keys.getId(), juce::Colour (0xffe07ad8));
        edit::renameTrack (*project, keys.getId(), juce::String::fromUTF8 ("Keys \xe2\x80\x94 \xc3\xa9t\xc3\xa9"));   // non-ASCII
        return project;
    }
}

TEST_CASE ("ValueTrees convert to JSON and back without losing anything", "[io][json]")
{
    juce::ValueTree tree ("Thing");
    tree.setProperty ("text", "hello \"world\"", nullptr);
    tree.setProperty ("whole", 42, nullptr);
    tree.setProperty ("big", (juce::int64) 960000LL * 1000000LL, nullptr);
    tree.setProperty ("decimal", 0.1, nullptr);
    tree.setProperty ("third", 1.0 / 3.0, nullptr);
    tree.setProperty ("tiny", 1.0e-7, nullptr);
    tree.setProperty ("flag", true, nullptr);

    juce::ValueTree child ("Child");
    child.setProperty ("n", -7, nullptr);
    tree.appendChild (child, nullptr);
    tree.appendChild (juce::ValueTree ("Child"), nullptr);

    const auto text = json::toJsonText (json::toVar (tree));
    const auto back = json::fromVar (juce::JSON::parse (text));

    REQUIRE (back.isValid());
    CHECK (back.isEquivalentTo (tree));
    CHECK ((double) back["decimal"] == 0.1);
    CHECK ((double) back["third"] == 1.0 / 3.0);
    CHECK ((juce::int64) back["big"] == 960000LL * 1000000LL);
}

TEST_CASE ("JSON that isn't a tree node is rejected", "[io][json]")
{
    CHECK_FALSE (json::fromVar (juce::var()).isValid());
    CHECK_FALSE (json::fromVar (juce::JSON::parse ("{\"name\": \"no type\"}")).isValid());
    CHECK_FALSE (json::fromVar (juce::JSON::parse ("{\"_type\": \"bad name!\"}")).isValid());
}

TEST_CASE ("A saved project loads back identically", "[io][project]")
{
    TempFolder temp;
    auto project = makeSampleProject();

    ProjectLocation location;
    REQUIRE (projectfiles::createProjectFolder (temp.folder, "Demo Song", location).wasOk());
    REQUIRE (projectfiles::save (*project, location).wasOk());

    CHECK (location.getMainFile() == temp.folder.getChildFile ("Demo Song/Demo Song.aether"));
    CHECK (location.getAudioFolder().isDirectory());
    CHECK (location.getBackupsFolder().isDirectory());
    CHECK (location.getAutosaveFolder().isDirectory());

    const auto loaded = projectfiles::load (location.getMainFile());
    REQUIRE (loaded.result.wasOk());
    CHECK (loaded.originalVersion == projectformat::currentVersion);
    CHECK (loaded.state.isEquivalentTo (project->getState()));

    auto reopened = Project::fromState (loaded.state);
    REQUIRE (reopened != nullptr);
    CHECK (reopened->getTrack (1).getName() == project->getTrack (1).getName());
    CHECK_FALSE (reopened->hasUnsavedChanges());
}

TEST_CASE ("Project files have the documented outer structure", "[io][project]")
{
    auto project = makeSampleProject();
    const auto parsed = juce::JSON::parse (projectformat::write (*project));

    CHECK (parsed["format"].toString() == "aether-project");
    CHECK ((int) parsed["formatVersion"] == projectformat::currentVersion);
    CHECK (parsed["savedWith"].toString().startsWith ("Aether "));
    CHECK (parsed["savedAt"].toString().isNotEmpty());
    CHECK (parsed["project"]["_type"].toString() == "Project");
}

TEST_CASE ("Every old project format version still opens", "[io][project][fixtures]")
{
    // One file per format version, kept forever. Add a new one whenever the format changes.
    const auto v1 = projectfiles::load (fixture ("projects/v1-basic.aether"));
    REQUIRE (v1.result.wasOk());
    CHECK (v1.originalVersion == 1);

    auto project = Project::fromState (v1.state);
    REQUIRE (project != nullptr);
    CHECK (project->getName() == "Fixture Song");
    REQUIRE (project->getNumTracks() == 2);
    CHECK (project->getTrack (0).getName() == "Vocals");
    CHECK (project->getTrack (0).getKind() == TrackKind::audio);
    CHECK (project->getTrack (0).getColour() == juce::Colour (0xffef6b73));
    CHECK (project->getTrack (1).getName() == "Piano");
    CHECK (project->getTrack (1).getKind() == TrackKind::midi);
}

TEST_CASE ("Bad project files are refused with a clear message, not a crash", "[io][project]")
{
    CHECK (projectformat::read ("").result.failed());
    CHECK (projectformat::read ("this is not json").result.getErrorMessage().contains ("damaged"));
    CHECK (projectformat::read ("{\"format\": \"something-else\"}").result.getErrorMessage().contains ("isn't an Aether project"));
    CHECK (projectformat::read ("{\"format\": \"aether-project\"}").result.getErrorMessage().contains ("version"));
    CHECK (projectformat::read ("{\"format\": \"aether-project\", \"formatVersion\": 1}").result.getErrorMessage().contains ("missing"));

    const auto newer = projectformat::read ("{\"format\": \"aether-project\", \"formatVersion\": 999, \"savedWith\": \"Aether 9.0\", "
                                            "\"project\": {\"_type\": \"Project\"}}");
    CHECK (newer.result.failed());
    CHECK (newer.result.getErrorMessage().contains ("newer version of Aether"));

    const auto missing = projectfiles::load (juce::File::getCurrentWorkingDirectory().getChildFile ("does-not-exist.aether"));
    CHECK (missing.result.getErrorMessage().contains ("doesn't exist"));
}

TEST_CASE ("Old files are upgraded one version at a time", "[io][project][upgrade]")
{
    // A pretend history: v1 called the property "title", v2 renamed it to "name", v3 added "tempo".
    std::map<int, projectformat::UpgradeStep> steps;
    steps[1] = [] (juce::DynamicObject& file)
    {
        auto* project = file.getProperty ("project").getDynamicObject();
        project->setProperty ("name", project->getProperty ("title"));
        project->removeProperty ("title");
        return juce::Result::ok();
    };
    steps[2] = [] (juce::DynamicObject& file)
    {
        file.getProperty ("project").getDynamicObject()->setProperty ("tempo", 120);
        return juce::Result::ok();
    };

    auto file = juce::JSON::parse ("{\"formatVersion\": 1, \"project\": {\"_type\": \"Project\", \"title\": \"Old\"}}");
    REQUIRE (projectformat::upgrade (*file.getDynamicObject(), 3, steps).wasOk());

    CHECK ((int) file["formatVersion"] == 3);
    CHECK (file["project"]["name"].toString() == "Old");
    CHECK (! file["project"].hasProperty ("title"));
    CHECK ((int) file["project"]["tempo"] == 120);

    // A gap in the chain is reported instead of producing a half-upgraded project.
    auto gap = juce::JSON::parse ("{\"formatVersion\": 1, \"project\": {}}");
    std::map<int, projectformat::UpgradeStep> incomplete { { 2, steps[2] } };
    CHECK (projectformat::upgrade (*gap.getDynamicObject(), 3, incomplete).failed());
}

TEST_CASE ("Saving keeps backups of previous versions, up to a limit", "[io][project]")
{
    TempFolder temp;
    Project project ("Backups");
    ProjectLocation location;
    REQUIRE (projectfiles::createProjectFolder (temp.folder, "Backups", location).wasOk());

    REQUIRE (projectfiles::save (project, location, 3).wasOk());   // first save: nothing to back up
    CHECK (location.getBackupsFolder().getNumberOfChildFiles (juce::File::findFiles) == 0);

    for (int i = 0; i < 5; ++i)
    {
        edit::addTrack (project, TrackKind::audio);
        REQUIRE (projectfiles::save (project, location, 3).wasOk());
    }

    CHECK (location.getBackupsFolder().getNumberOfChildFiles (juce::File::findFiles) == 3);

    // The main file always has the latest version.
    auto loaded = Project::fromState (projectfiles::load (location.getMainFile()).state);
    REQUIRE (loaded != nullptr);
    CHECK (loaded->getNumTracks() == 5);
}

TEST_CASE ("New project folders get safe, unique names", "[io][project]")
{
    TempFolder temp;
    ProjectLocation first, second;
    REQUIRE (projectfiles::createProjectFolder (temp.folder, "My Song", first).wasOk());
    REQUIRE (projectfiles::createProjectFolder (temp.folder, "My Song", second).wasOk());
    CHECK (first.getFolder().getFileName() == "My Song");
    CHECK (second.getFolder().getFileName() == "My Song 2");
    CHECK (second.getMainFile().getFileName() == "My Song 2.aether");

    CHECK (projectfiles::safeFileName ("a/b\\c:d*e?f\"g<h>i|j") == "a b c d e f g h i j");
    CHECK (projectfiles::safeFileName ("  trailing dots... ") == "trailing dots");
    CHECK (projectfiles::safeFileName ("CON") == "CON project");
    CHECK (projectfiles::safeFileName ("???") == "Untitled");
    CHECK (projectfiles::safeFileName (juce::String::fromUTF8 ("Caf\xc3\xa9")) == juce::String::fromUTF8 ("Caf\xc3\xa9"));
}

TEST_CASE ("Autosave writes a copy only when something changed", "[io][autosave]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    TempFolder temp;
    Project project ("Autosave");
    ProjectLocation location;
    REQUIRE (projectfiles::createProjectFolder (temp.folder, "Autosave", location).wasOk());
    REQUIRE (projectfiles::save (project, location).wasOk());
    project.markAsSaved();

    Autosaver autosaver (project, location, 0);   // 0 = no timer; the test triggers it
    CHECK_FALSE (autosaver.autosaveNow());       // nothing changed yet
    CHECK_FALSE (location.getAutosaveFile().exists());

    edit::addTrack (project, TrackKind::audio);
    CHECK (autosaver.autosaveNow());
    autosaver.waitUntilIdle();
    REQUIRE (location.getAutosaveFile().existsAsFile());
    CHECK_FALSE (autosaver.autosaveNow());       // no further changes

    // The autosave is a complete project that can be loaded.
    auto recovered = Project::fromState (projectfiles::load (location.getAutosaveFile()).state);
    REQUIRE (recovered != nullptr);
    CHECK (recovered->getNumTracks() == 1);

    autosaver.discard();
    CHECK_FALSE (location.getAutosaveFile().exists());
}

TEST_CASE ("Recovery is offered only when the autosave is newer than the saved project", "[io][autosave]")
{
    TempFolder temp;
    ProjectLocation location;
    REQUIRE (projectfiles::createProjectFolder (temp.folder, "Recover", location).wasOk());
    Project project ("Recover");
    REQUIRE (projectfiles::save (project, location).wasOk());

    CHECK_FALSE (projectfiles::hasRecoverableAutosave (location));   // no autosave at all

    REQUIRE (projectfiles::writeTextAtomically (location.getAutosaveFile(), projectformat::write (project)).wasOk());
    location.getMainFile().setLastModificationTime (juce::Time::getCurrentTime() - juce::RelativeTime::minutes (5));
    CHECK (projectfiles::hasRecoverableAutosave (location));         // autosave newer: offer it

    location.getAutosaveFile().setLastModificationTime (juce::Time::getCurrentTime() - juce::RelativeTime::minutes (10));
    CHECK_FALSE (projectfiles::hasRecoverableAutosave (location));   // saved after the autosave
}

TEST_CASE ("Recent projects: newest first, no duplicates, missing files hidden", "[io][recent]")
{
    TempFolder temp;
    juce::PropertySet settings;
    RecentProjects recent (settings);

    auto makeFile = [&] (const juce::String& name)
    {
        auto f = temp.folder.getChildFile (name + ".aether");
        f.replaceWithText ("{}");
        return f;
    };

    const auto a = makeFile ("a"), b = makeFile ("b"), c = makeFile ("c");
    recent.add (a);
    recent.add (b);
    recent.add (c);
    recent.add (a);   // opened again: moves to the top

    auto files = recent.getFiles();
    REQUIRE (files.size() == 3);
    CHECK (files[0] == a);
    CHECK (files[1] == c);
    CHECK (files[2] == b);

    b.deleteFile();
    CHECK (recent.getFiles().size() == 2);

    for (int i = 0; i < 20; ++i)
        recent.add (makeFile ("p" + juce::String (i)));
    CHECK (recent.getFiles().size() == RecentProjects::maxEntries);

    recent.clear();
    CHECK (recent.getFiles().isEmpty());
}

TEST_CASE ("Dropped files are recognised as projects, audio or MIDI", "[io]")
{
    using namespace aether;
    const juce::String audio = "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg";

    CHECK (filetypes::isProject (juce::File ("/songs/My Song/My Song.aether")));
    CHECK (filetypes::isProject (juce::File ("/songs/LOUD.AETHER")));
    CHECK_FALSE (filetypes::isProject (juce::File ("/songs/My Song/Autosave/My Song.autosave.aether")));
    CHECK_FALSE (filetypes::isProject (juce::File ("/songs/take.wav")));

    CHECK (filetypes::matchesWildcard (juce::File ("/a/Take 1.WAV"), audio));
    CHECK (filetypes::matchesWildcard (juce::File ("/a/b.flac"), audio));
    CHECK_FALSE (filetypes::matchesWildcard (juce::File ("/a/b.wavx"), audio));
    CHECK_FALSE (filetypes::matchesWildcard (juce::File ("/a/README"), audio));

    CHECK (filetypes::isImportable (juce::File ("/a/b.mid"), audio));
    CHECK (filetypes::isImportable (juce::File ("/a/b.mp3"), audio));
    CHECK_FALSE (filetypes::isImportable (juce::File ("/a/b.txt"), audio));
    CHECK_FALSE (filetypes::isImportable (juce::File ("/a/b.aether"), audio));
}
