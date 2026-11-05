// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include <catch2/catch_test_macros.hpp>

#include "io/ProjectFormat.h"
#include "model/EditCommands.h"
#include "model/Project.h"
#include "model/TrackColours.h"

using namespace aether;

namespace
{
    /** The whole project as text: two projects are equal if their texts are equal. */
    juce::String snapshot (const Project& project)
    {
        return project.getState().toXmlString();
    }

    /** Checks that an edit can be undone back to exactly the previous state, and redone. */
    template <typename EditFunction>
    void checkUndoRedo (Project& project, EditFunction&& edit)
    {
        const auto before = snapshot (project);
        edit();
        const auto after = snapshot (project);
        REQUIRE (after != before);

        REQUIRE (project.getUndoManager().undo());
        CHECK (snapshot (project) == before);

        REQUIRE (project.getUndoManager().redo());
        CHECK (snapshot (project) == after);
    }
}

TEST_CASE ("A new project is empty and has no unsaved changes", "[model]")
{
    Project project ("My Song");
    CHECK (project.getName() == "My Song");
    CHECK (project.getNumTracks() == 0);
    CHECK_FALSE (project.hasUnsavedChanges());
    CHECK_FALSE (project.getUndoManager().canUndo());
}

TEST_CASE ("Tracks get automatic names and colours", "[model]")
{
    Project project;
    auto a1 = edit::addTrack (project, TrackKind::audio);
    auto a2 = edit::addTrack (project, TrackKind::audio);
    auto m1 = edit::addTrack (project, TrackKind::midi);

    CHECK (a1.getName() == "Audio 1");
    CHECK (a2.getName() == "Audio 2");
    CHECK (m1.getName() == "MIDI 1");
    CHECK (a1.getKind() == TrackKind::audio);
    CHECK (m1.getKind() == TrackKind::midi);

    // Every track has a different id and (while the palette lasts) a different colour.
    CHECK (a1.getId() != a2.getId());
    CHECK (a1.getColour() == trackColours::palette()[0]);
    CHECK (a2.getColour() == trackColours::palette()[1]);
    CHECK (m1.getColour() == trackColours::palette()[2]);

    // Numbering continues after the highest number, even after deleting or renaming.
    edit::renameTrack (project, a1.getId(), "Audio 7");
    CHECK (edit::nextTrackName (project, TrackKind::audio) == "Audio 8");
}

TEST_CASE ("Every track edit can be undone and redone exactly", "[model][undo]")
{
    Project project;
    auto first = edit::addTrack (project, TrackKind::audio);
    auto second = edit::addTrack (project, TrackKind::midi);
    const auto firstId = first.getId();

    SECTION ("add")     { checkUndoRedo (project, [&] { edit::addTrack (project, TrackKind::audio, 1); }); }
    SECTION ("delete")  { checkUndoRedo (project, [&] { edit::deleteTrack (project, firstId); }); }
    SECTION ("rename")  { checkUndoRedo (project, [&] { edit::renameTrack (project, firstId, "Vocals"); }); }
    SECTION ("colour")  { checkUndoRedo (project, [&] { edit::setTrackColour (project, firstId, juce::Colours::red); }); }
    SECTION ("move")    { checkUndoRedo (project, [&] { edit::moveTrack (project, firstId, 1); }); }
}

TEST_CASE ("Undo steps have names the user can understand", "[model][undo]")
{
    Project project;
    auto track = edit::addTrack (project, TrackKind::audio);
    CHECK (project.getUndoManager().getUndoDescription() == "Add audio track");

    edit::renameTrack (project, track.getId(), "Guitar");
    CHECK (project.getUndoManager().getUndoDescription() == "Rename track");

    edit::deleteTrack (project, track.getId());
    CHECK (project.getUndoManager().getUndoDescription() == "Delete track \"Guitar\"");

    project.getUndoManager().undo();
    CHECK (project.getUndoManager().getRedoDescription() == "Delete track \"Guitar\"");
}

TEST_CASE ("Edits that change nothing don't create undo steps", "[model][undo]")
{
    Project project;
    auto track = edit::addTrack (project, TrackKind::audio);
    project.getUndoManager().clearUndoHistory();

    CHECK_FALSE (edit::renameTrack (project, track.getId(), track.getName()));
    CHECK_FALSE (edit::renameTrack (project, track.getId(), "   "));
    CHECK_FALSE (edit::setTrackColour (project, track.getId(), track.getColour()));
    CHECK_FALSE (edit::moveTrack (project, track.getId(), 0));
    CHECK_FALSE (edit::deleteTrack (project, "no-such-id"));
    CHECK_FALSE (project.getUndoManager().canUndo());
}

TEST_CASE ("Renaming trims spaces", "[model]")
{
    Project project;
    auto track = edit::addTrack (project, TrackKind::audio);
    edit::renameTrack (project, track.getId(), "  Lead vocal  ");
    CHECK (track.getName() == "Lead vocal");
}

TEST_CASE ("Moving tracks reorders them", "[model]")
{
    Project project;
    auto a = edit::addTrack (project, TrackKind::audio);
    auto b = edit::addTrack (project, TrackKind::audio);
    auto c = edit::addTrack (project, TrackKind::audio);

    edit::moveTrack (project, c.getId(), 0);
    CHECK (project.getTrack (0) == c);
    CHECK (project.getTrack (1) == a);
    CHECK (project.getTrack (2) == b);

    edit::moveTrack (project, c.getId(), 99);   // past the end: goes to the bottom
    CHECK (project.getTrack (2) == c);
}

TEST_CASE ("Unsaved changes are tracked, including undo", "[model]")
{
    Project project;
    edit::addTrack (project, TrackKind::audio);
    CHECK (project.hasUnsavedChanges());

    project.markAsSaved();
    CHECK_FALSE (project.hasUnsavedChanges());

    project.getUndoManager().undo();
    CHECK (project.hasUnsavedChanges());
}

TEST_CASE ("Colours are stored as #RRGGBB text", "[model]")
{
    CHECK (Track::colourToString (juce::Colour (0xff4fc3c9)) == "#4FC3C9");
    CHECK (Track::colourFromString ("#4fc3c9", juce::Colours::black) == juce::Colour (0xff4fc3c9));
    CHECK (Track::colourFromString ("4FC3C9", juce::Colours::black) == juce::Colour (0xff4fc3c9));
    CHECK (Track::colourFromString ("not a colour", juce::Colours::black) == juce::Colours::black);
    CHECK (Track::colourFromString ("#12345", juce::Colours::black) == juce::Colours::black);
}

TEST_CASE ("Loaded projects get missing pieces filled in", "[model]")
{
    juce::ValueTree bare (ids::Project);
    juce::ValueTree tracks (ids::Tracks);
    juce::ValueTree track (ids::Track);   // no id, no name
    tracks.appendChild (track, nullptr);
    bare.appendChild (tracks, nullptr);

    auto project = Project::fromState (bare);
    REQUIRE (project != nullptr);
    CHECK (project->getName() == "Untitled");
    CHECK (project->getTrack (0).getId().isNotEmpty());
    CHECK (project->getTrack (0).getKind() == TrackKind::audio);
    CHECK_FALSE (project->hasUnsavedChanges());

    CHECK (Project::fromState (juce::ValueTree ("NotAProject")) == nullptr);
}
