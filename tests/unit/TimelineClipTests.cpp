// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
//
// The timeline's clip commands (split, duplicate, delete, nudge, select, snap, file drops),
// driven without opening a window.

#include <catch2/catch_test_macros.hpp>

#include "model/ClipEdits.h"
#include "model/EditCommands.h"
#include "model/TimelineEdits.h"
#include "ui/TimelineComponent.h"

using namespace aether;

namespace
{
    constexpr Tick q = ticksPerQuarter;   // 24000 samples at 120 BPM and 48 kHz

    struct Fixture
    {
        Fixture()
        {
            trackA = edit::addTrack (project, TrackKind::audio).getId();
            trackB = edit::addTrack (project, TrackKind::audio).getId();
            clipA = edit::addAudioClip (project, trackA, { "Audio/a.wav", "a", 48000.0, 4 * 24000 }, 4 * q);   // bar 2, 4 beats
            clipB = edit::addAudioClip (project, trackB, { "Audio/b.wav", "b", 48000.0, 2 * 24000 }, 8 * q);

            viewport.setViewedComponent (&trackList, false);
            viewport.setBounds (0, 0, 300, 600);
            timeline = std::make_unique<ui::TimelineComponent> (theme, project, engine, cache, [] { return ProjectLocation(); },
                                                                viewport, trackList);
            timeline->setBounds (0, 0, 1000, 600);
            trackList.setSize (300, trackList.getIdealHeight());
            engine.prepare (48000.0, 512);
        }

        Clip clip (const juce::String& id) const   { return clips::findClip (project, id); }

        juce::ScopedJuceInitialiser_GUI juce;
        ui::Theme theme = ui::Theme::dark();
        Project project;
        AudioEngine engine;
        AudioFileCache cache;
        juce::String trackA, trackB, clipA, clipB;
        ui::TrackListComponent trackList { theme, project };
        juce::Viewport viewport;
        std::unique_ptr<ui::TimelineComponent> timeline;
    };
}

TEST_CASE ("Splitting at the playhead cuts the selected clips there", "[ui][timeline][clips]")
{
    Fixture f;
    f.engine.locate (6 * q);   // half way through clip A
    f.timeline->selectClips ({ f.clipA, f.clipB });

    REQUIRE (f.timeline->splitClipsAtPlayhead());
    CHECK (clips::allClips (f.project).size() == 3);      // B isn't under the playhead, so it isn't cut
    CHECK (f.clip (f.clipA).getDuration() == 2 * 24000);

    // The right-hand part is selected, so pressing Delete next removes it.
    REQUIRE (f.timeline->getSelectedClips().size() == 1);
    CHECK (f.clip (f.timeline->getSelectedClips()[0]).getStart() == 6 * q);

    // The whole split was one undo step.
    f.project.getUndoManager().undo();
    CHECK (clips::allClips (f.project).size() == 2);
}

TEST_CASE ("With nothing selected, split cuts the clips on the selected track", "[ui][timeline][clips]")
{
    Fixture f;
    f.trackList.selectTrack (f.trackB);
    f.engine.locate (9 * q);

    REQUIRE (f.timeline->splitClipsAtPlayhead());
    CHECK (clips::clipsOnTrack (f.project.findTrack (f.trackB).getState()).size() == 2);
    CHECK (clips::clipsOnTrack (f.project.findTrack (f.trackA).getState()).size() == 1);
}

TEST_CASE ("Nudging moves selected clips by a grid step or by 10 ms", "[ui][timeline][clips]")
{
    Fixture f;
    f.timeline->setGrid (GridSize::beat);
    f.timeline->selectClips ({ f.clipA, f.clipB });

    f.timeline->nudge (+1, false);
    CHECK (f.clip (f.clipA).getStart() == 5 * q);
    CHECK (f.clip (f.clipB).getStart() == 9 * q);   // they keep their spacing

    f.timeline->nudge (-1, true);   // 10 ms earlier
    const auto map = timeline::buildTempoMap (f.project);
    CHECK (std::abs (map.ticksToSeconds (f.clip (f.clipA).getStart()) - (2.5 - 0.010)) < 1.0e-6);

    // Nothing selected: the playhead moves instead.
    f.timeline->clearClipSelection();
    f.engine.locate (0);
    f.timeline->nudge (+1, false);
    CHECK (f.engine.getTransportStatus().position == q);
}

TEST_CASE ("Duplicate, delete and select all", "[ui][timeline][clips]")
{
    Fixture f;
    f.timeline->selectClips ({ f.clipA });
    REQUIRE (f.timeline->duplicateSelectedClips());
    CHECK (clips::allClips (f.project).size() == 3);
    CHECK (f.clip (f.timeline->getSelectedClips()[0]).getStart() == 8 * q);   // the copy is selected

    f.timeline->selectAllClips();
    CHECK (f.timeline->getSelectedClips().size() == 3);

    REQUIRE (f.timeline->deleteSelectedClips());
    CHECK (clips::allClips (f.project).isEmpty());
    CHECK_FALSE (f.timeline->hasSelectedClips());

    // Undo brings them back; the selection forgets clips that no longer exist.
    f.project.getUndoManager().undo();
    CHECK (clips::allClips (f.project).size() == 3);
    f.timeline->selectClips ({ f.clipA });
    edit::deleteClips (f.project, { f.clipA });
    CHECK_FALSE (f.timeline->hasSelectedClips());
}

TEST_CASE ("Snapping can be switched off and back on", "[ui][timeline][grid]")
{
    Fixture f;
    int changes = 0;
    f.timeline->onGridChanged = [&] { ++changes; };

    f.timeline->setGrid (GridSize::sixteenth);
    f.timeline->toggleSnap();
    CHECK (f.timeline->getGrid() == GridSize::off);
    f.timeline->toggleSnap();
    CHECK (f.timeline->getGrid() == GridSize::sixteenth);
    CHECK (changes == 3);
}

TEST_CASE ("Dropped files go on the track and at the position under the mouse", "[ui][timeline][import]")
{
    Fixture f;
    f.timeline->setGrid (GridSize::bar);

    juce::Array<juce::File> dropped;
    juce::String droppedTrack = "unset";
    Tick droppedAt = -1;
    f.timeline->onImportFiles = [&] (const juce::Array<juce::File>& files, const juce::String& trackId, Tick position)
    {
        dropped = files;
        droppedTrack = trackId;
        droppedAt = position;
    };

    const auto music = juce::File::getSpecialLocation (juce::File::tempDirectory);
    const auto loop = music.getChildFile ("loop.wav").getFullPathName();
    CHECK (f.timeline->isInterestedInFileDrag ({ loop }));
    CHECK (f.timeline->isInterestedInFileDrag ({ music.getChildFile ("song.MP3").getFullPathName() }));
    CHECK_FALSE (f.timeline->isInterestedInFileDrag ({ music.getChildFile ("notes.txt").getFullPathName() }));

    // Second track's lane, just after bar 3 (the view starts at 0 with 40 pixels per beat).
    const auto y = ui::TimelineComponent::headerHeight + ui::TrackListComponent::rowHeight + 10;
    f.timeline->filesDropped ({ loop }, (int) (8 * 40 + 15), y);
    CHECK (dropped.size() == 1);
    CHECK (droppedTrack == f.trackB);
    CHECK (droppedAt == 8 * q);   // snapped to the bar

    // Below the last track: no track, so new ones are made.
    f.timeline->filesDropped ({ loop }, 10, ui::TimelineComponent::headerHeight + 500);
    CHECK (droppedTrack.isEmpty());
}

TEST_CASE ("Going to the start scrolls the timeline back to the start", "[ui][timeline]")
{
    Fixture f;
    f.timeline->getView().setViewStart (40 * q);   // scrolled well into the song
    f.engine.locate (44 * q);
    f.timeline->updatePlayhead();
    CHECK (f.timeline->getView().getViewStart() == 40 * q);   // the playhead is in view: no scrolling

    f.engine.locate (0);                                       // Go to start (Home)
    f.timeline->updatePlayhead();
    CHECK (f.timeline->getView().getViewStart() == 0);
}
