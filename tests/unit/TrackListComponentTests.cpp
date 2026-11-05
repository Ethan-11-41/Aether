// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
//
// Drives the track list the way the mouse and keyboard would, without opening a window.

#include <catch2/catch_test_macros.hpp>

#include "model/EditCommands.h"
#include "ui/TrackListComponent.h"

using namespace aether;

namespace
{
    struct Fixture
    {
        Fixture()
        {
            for (int i = 0; i < 4; ++i)
                ids.add (edit::addTrack (project, TrackKind::audio).getId());   // Audio 1..4

            list.setSize (300, list.getIdealHeight());
        }

        juce::String nameAt (int index) const   { return project.getTrack (index).getName(); }

        juce::ScopedJuceInitialiser_GUI juce;
        ui::Theme theme = ui::Theme::dark();
        Project project;
        juce::StringArray ids;
        ui::TrackListComponent list { theme, project };
    };

    constexpr int row = ui::TrackListComponent::rowHeight;
}

TEST_CASE ("The track list shows one row per track and follows undo", "[ui][tracks]")
{
    Fixture f;
    CHECK (f.list.getNumChildComponents() == 4);

    edit::addTrack (f.project, TrackKind::midi);
    CHECK (f.list.getNumChildComponents() == 5);

    f.project.getUndoManager().undo();
    CHECK (f.list.getNumChildComponents() == 4);
}

TEST_CASE ("Selection moves with the arrow keys and stays valid after deleting", "[ui][tracks]")
{
    Fixture f;
    CHECK (f.list.getSelectedTrackId() == f.ids[0]);   // the first track is selected to start with

    f.list.keyPressed (juce::KeyPress (juce::KeyPress::downKey));
    f.list.keyPressed (juce::KeyPress (juce::KeyPress::downKey));
    CHECK (f.list.getSelectedTrackId() == f.ids[2]);

    f.list.keyPressed (juce::KeyPress (juce::KeyPress::upKey));
    CHECK (f.list.getSelectedTrackId() == f.ids[1]);

    edit::deleteTrack (f.project, f.ids[1]);
    CHECK (f.project.indexOfTrack (f.list.getSelectedTrackId()) >= 0);
}

TEST_CASE ("Renaming in the list: Enter keeps the name, Escape cancels", "[ui][tracks]")
{
    Fixture f;
    f.list.selectTrack (f.ids[1]);

    f.list.startRenamingSelected();
    auto* editor = f.list.getActiveRenameEditor();
    REQUIRE (editor != nullptr);
    CHECK (editor->getText() == "Audio 2");

    editor->setText ("Lead guitar");
    editor->onReturnKey();
    CHECK (f.list.getActiveRenameEditor() == nullptr);
    CHECK (f.nameAt (1) == "Lead guitar");
    CHECK (f.project.getUndoManager().getUndoDescription() == "Rename track");

    f.list.startRenamingSelected();
    editor = f.list.getActiveRenameEditor();
    REQUIRE (editor != nullptr);
    editor->setText ("Should not stick");
    editor->onEscapeKey();
    CHECK (f.list.getActiveRenameEditor() == nullptr);
    CHECK (f.nameAt (1) == "Lead guitar");
}

TEST_CASE ("Dragging a track moves it to where it is dropped", "[ui][tracks]")
{
    Fixture f;

    SECTION ("down: drop Audio 1 between Audio 3 and Audio 4")
    {
        f.list.dragStarted (f.ids[0]);
        f.list.dragMoved (3 * row - 5);   // just above the boundary between rows 2 and 3
        f.list.dragEnded();

        CHECK (f.nameAt (0) == "Audio 2");
        CHECK (f.nameAt (1) == "Audio 3");
        CHECK (f.nameAt (2) == "Audio 1");
        CHECK (f.nameAt (3) == "Audio 4");
    }

    SECTION ("up: drop Audio 4 at the top")
    {
        f.list.dragStarted (f.ids[3]);
        f.list.dragMoved (2);
        f.list.dragEnded();

        CHECK (f.nameAt (0) == "Audio 4");
        CHECK (f.nameAt (1) == "Audio 1");
    }

    SECTION ("to the very bottom")
    {
        f.list.dragStarted (f.ids[1]);
        f.list.dragMoved (10 * row);
        f.list.dragEnded();

        CHECK (f.nameAt (3) == "Audio 2");
    }

    SECTION ("dropping where it started changes nothing and adds no undo step")
    {
        f.project.getUndoManager().clearUndoHistory();
        f.list.dragStarted (f.ids[2]);
        f.list.dragMoved (2 * row + row / 2 - 10);
        f.list.dragEnded();

        CHECK (f.nameAt (2) == "Audio 3");
        CHECK_FALSE (f.project.getUndoManager().canUndo());
    }

    SECTION ("a drag is one undo step")
    {
        f.list.dragStarted (f.ids[0]);
        f.list.dragMoved (4 * row);
        f.list.dragEnded();
        f.project.getUndoManager().undo();

        for (int i = 0; i < 4; ++i)
            CHECK (f.nameAt (i) == "Audio " + juce::String (i + 1));
    }
}
