// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include <catch2/catch_test_macros.hpp>

#include "engine/TimelineSync.h"
#include "io/ProjectFiles.h"
#include "model/TimelineEdits.h"

using namespace aether;

namespace
{
    constexpr Tick q = ticksPerQuarter;

    juce::String snapshot (const Project& p)   { return p.getState().toXmlString(); }

    template <typename Edit>
    void checkUndoRedo (Project& project, Edit&& edit)
    {
        const auto before = snapshot (project);
        REQUIRE (edit());
        const auto after = snapshot (project);
        REQUIRE (project.getUndoManager().undo());
        CHECK (snapshot (project) == before);
        REQUIRE (project.getUndoManager().redo());
        CHECK (snapshot (project) == after);
    }
}

TEST_CASE ("New projects start at 120 BPM in 4/4 with the loop off", "[model][timeline]")
{
    Project project;
    const auto map = timeline::buildTempoMap (project);
    CHECK (map.getTempoPoints().size() == 1);
    CHECK (map.tempoAt (0) == 120.0);
    CHECK (map.getTimeSignatures().front() == TimeSignature { 0, 4, 4 });

    const auto loop = timeline::getLoop (project);
    CHECK_FALSE (loop.enabled);
    CHECK (loop.start == 0);
    CHECK (loop.end == 16 * q);
    CHECK_FALSE (project.hasUnsavedChanges());
}

TEST_CASE ("Tempo, signature and loop edits are all undoable", "[model][timeline][undo]")
{
    Project project;
    edit::addTempoChange (project, 8 * q, 90.0);

    SECTION ("set tempo")        { checkUndoRedo (project, [&] { return edit::setTempo (project, 0, 98.5); }); }
    SECTION ("add change")       { checkUndoRedo (project, [&] { return edit::addTempoChange (project, 16 * q, 140.0) == 2; }); }
    SECTION ("move change")      { checkUndoRedo (project, [&] { return edit::moveTempoChange (project, 1, 12 * q); }); }
    SECTION ("make a ramp")      { checkUndoRedo (project, [&] { return edit::setTempoCurve (project, 0, TempoCurve::ramp); }); }
    SECTION ("delete change")    { checkUndoRedo (project, [&] { return edit::deleteTempoChange (project, 1); }); }
    SECTION ("set signature")    { checkUndoRedo (project, [&] { return edit::setTimeSignature (project, 0, 3, 4); }); }
    SECTION ("add signature")    { checkUndoRedo (project, [&] { return edit::setTimeSignature (project, 4, 6, 8); }); }
    SECTION ("loop range")       { checkUndoRedo (project, [&] { return edit::setLoopRange (project, 4 * q, 8 * q); }); }
    SECTION ("loop on")          { checkUndoRedo (project, [&] { return edit::setLoopEnabled (project, true); }); }

    SECTION ("delete signature")
    {
        edit::setTimeSignature (project, 2, 7, 8);
        checkUndoRedo (project, [&] { return edit::deleteTimeSignature (project, 2); });
    }
}

TEST_CASE ("A whole drag is one undo step", "[model][timeline][undo]")
{
    Project project;
    project.getUndoManager().clearUndoHistory();

    // Like dragging the tempo with the mouse: many changes, only the first starts a new step.
    for (int i = 0; i < 30; ++i)
        edit::setTempo (project, 0, 120.0 + i, i == 0);

    CHECK (timeline::buildTempoMap (project).tempoAt (0) == 149.0);
    REQUIRE (project.getUndoManager().undo());
    CHECK (timeline::buildTempoMap (project).tempoAt (0) == 120.0);
    CHECK_FALSE (project.getUndoManager().canUndo());

    for (int i = 0; i < 10; ++i)
        edit::setLoopRange (project, i * q, (i + 4) * q, i == 0);

    REQUIRE (project.getUndoManager().undo());
    CHECK (timeline::getLoop (project).start == 0);
    CHECK (timeline::getLoop (project).end == 16 * q);
}

TEST_CASE ("Tempo changes stay in order and the first one is fixed", "[model][timeline]")
{
    Project project;
    CHECK (edit::addTempoChange (project, 16 * q, 100.0) == 1);
    CHECK (edit::addTempoChange (project, 8 * q, 90.0) == 1);    // inserted in time order
    CHECK (edit::addTempoChange (project, 8 * q, 95.0) == 1);    // same place: changes the existing one
    CHECK (timeline::getNumTempoChanges (project) == 3);
    CHECK (timeline::buildTempoMap (project).tempoAt (8 * q) == 95.0);

    // A change can't be dragged past its neighbours...
    edit::moveTempoChange (project, 1, 40 * q);
    CHECK (timeline::buildTempoMap (project).getTempoPoints()[1].tick == 16 * q - 1);
    edit::moveTempoChange (project, 1, 0);
    CHECK (timeline::buildTempoMap (project).getTempoPoints()[1].tick == 1);

    // ...and the first one can't be moved or deleted.
    CHECK_FALSE (edit::moveTempoChange (project, 0, 4 * q));
    CHECK_FALSE (edit::deleteTempoChange (project, 0));
    CHECK_FALSE (edit::deleteTimeSignature (project, 0));
}

TEST_CASE ("Out-of-range tempos and signatures are refused or limited", "[model][timeline]")
{
    Project project;
    edit::setTempo (project, 0, 5000.0);
    CHECK (timeline::buildTempoMap (project).tempoAt (0) == TempoMap::maxBpm);
    edit::setTempo (project, 0, 1.0);
    CHECK (timeline::buildTempoMap (project).tempoAt (0) == TempoMap::minBpm);
    edit::setTempo (project, 0, 128.123456);
    CHECK (timeline::buildTempoMap (project).tempoAt (0) == 128.123);

    CHECK_FALSE (edit::setTimeSignature (project, 0, 0, 4));
    CHECK_FALSE (edit::setTimeSignature (project, 0, 4, 3));
    CHECK_FALSE (edit::setTimeSignature (project, -1, 4, 4));
    CHECK_FALSE (edit::setLoopRange (project, 8 * q, 4 * q));     // end before start
    CHECK_FALSE (edit::setLoopEnabled (project, false));           // already off
}

TEST_CASE ("The timeline is saved and loaded with the project", "[model][timeline][io]")
{
    Project project ("Timeline");
    edit::setTempo (project, 0, 87.5);
    edit::setTempoCurve (project, 0, TempoCurve::ramp);
    edit::addTempoChange (project, 32 * q, 140.0);
    edit::setTimeSignature (project, 8, 7, 8);
    edit::setLoopRange (project, 4 * q, 12 * q);
    edit::setLoopEnabled (project, true);

    const auto loaded = projectformat::read (projectformat::write (project));
    REQUIRE (loaded.result.wasOk());
    auto reopened = Project::fromState (loaded.state);
    REQUIRE (reopened != nullptr);

    const auto a = timeline::buildTempoMap (project);
    const auto b = timeline::buildTempoMap (*reopened);
    CHECK (a.getTempoPoints() == b.getTempoPoints());
    CHECK (a.getTimeSignatures() == b.getTimeSignatures());
    CHECK (timeline::getLoop (*reopened).enabled);
    CHECK (timeline::getLoop (*reopened).start == 4 * q);
}

TEST_CASE ("Projects from before Milestone 2 open with the default timeline", "[model][timeline][fixtures]")
{
    const auto v1 = projectfiles::load (juce::File (AETHER_TEST_FIXTURES_DIR).getChildFile ("projects/v1-basic.aether"));
    REQUIRE (v1.result.wasOk());
    auto project = Project::fromState (v1.state);
    REQUIRE (project != nullptr);

    CHECK (timeline::buildTempoMap (*project).tempoAt (0) == 120.0);
    CHECK_FALSE (timeline::getLoop (*project).enabled);
    CHECK_FALSE (project->hasUnsavedChanges());
}

TEST_CASE ("The engine gets a snapshot of the timeline", "[model][timeline][engine]")
{
    Project project;
    edit::setTempo (project, 0, 100.0);
    edit::setLoopRange (project, 2 * q, 6 * q);
    edit::setLoopEnabled (project, true);

    const auto snap = buildTimelineSnapshot (project);
    CHECK (snap->tempo.tempoAt (0) == 100.0);
    CHECK (snap->loopEnabled);
    CHECK (snap->loopStart == 2 * q);
    CHECK (snap->loopEnd == 6 * q);
}
