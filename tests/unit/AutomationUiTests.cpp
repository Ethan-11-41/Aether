// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
//
// Automation lanes in the window (Milestone 14): showing them under their tracks, and drawing
// points with the mouse.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "model/Automation.h"
#include "model/EditCommands.h"
#include "ui/AutomationLanes.h"
#include "ui/TrackListComponent.h"

using namespace aether;
using automation::Target;
using Catch::Matchers::WithinAbs;

namespace
{
    constexpr Tick beat = ticksPerQuarter;

    struct Fixture
    {
        Fixture()
        {
            first = edit::addTrack (project, TrackKind::audio, -1, true, "Vocals").getId();
            second = edit::addTrack (project, TrackKind::audio, -1, true, "Piano").getId();
            project.getUndoManager().clearUndoHistory();
            list.setSize (260, 600);

            // One beat is 100 pixels; the grid is a beat.
            ui::AutomationLaneArea::Host host;
            host.tickToX = [] (Tick t) { return (double) t / (double) beat * 100.0; };
            host.xToTick = [] (double x) { return (Tick) std::llround (x / 100.0 * (double) beat); };
            host.snap = [] (Tick t, juce::ModifierKeys mods)
            {
                return mods.isAltDown() ? t : (Tick) std::llround ((double) t / (double) beat) * beat;
            };
            host.scrollY = [] { return 0; };
            lanes = std::make_unique<ui::AutomationLaneArea> (theme, project, host);
            lanes->setSize (1000, 600);
        }

        static juce::MouseEvent event (juce::Component& c, juce::Point<float> position, juce::Point<float> down,
                                       juce::ModifierKeys mods = juce::ModifierKeys::leftButtonModifier, bool dragged = false)
        {
            return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), position, mods,
                                     juce::MouseInputSource::defaultPressure, 0.0f, 0.0f, 0.0f, 0.0f, &c, &c,
                                     juce::Time::getCurrentTime(), down, juce::Time::getCurrentTime(), 1, dragged);
        }

        void click (juce::Point<float> p, juce::ModifierKeys mods = juce::ModifierKeys::leftButtonModifier)
        {
            lanes->mouseDown (event (*lanes, p, p, mods));
            lanes->mouseUp (event (*lanes, p, p, mods));
        }

        void drag (juce::Point<float> from, juce::Point<float> to, juce::ModifierKeys mods = juce::ModifierKeys::leftButtonModifier)
        {
            lanes->mouseDown (event (*lanes, from, from, mods));
            lanes->mouseDrag (event (*lanes, (from + to) / 2.0f, from, mods, true));
            lanes->mouseDrag (event (*lanes, to, from, mods, true));
            lanes->mouseUp (event (*lanes, to, from, mods, true));
        }

        std::vector<automation::Point> points (const juce::String& laneId) const
        {
            return automation::pointsOf (automation::findLane (project, laneId));
        }

        juce::ScopedJuceInitialiser_GUI juceInit;
        ui::Theme theme = ui::Theme::dark();
        Project project;
        juce::String first, second;
        ui::TrackListComponent list { theme, project };
        std::unique_ptr<ui::AutomationLaneArea> lanes;
    };
}

TEST_CASE ("Lanes show under their track, and the tracks below move down", "[ui][automation]")
{
    Fixture f;
    const auto before = f.list.getIdealHeight();
    CHECK (f.list.getNumLaneHeaders() == 0);
    CHECK (ui::TrackRows (f.project).trackTop (1) == ui::TrackRows::trackHeight);

    edit::showAutomationLane (f.project, f.first, Target::volume());
    edit::showAutomationLane (f.project, f.first, Target::pan());
    CHECK (f.list.getNumLaneHeaders() == 2);
    CHECK (f.list.getIdealHeight() == before + 2 * ui::TrackRows::laneHeight);

    const ui::TrackRows rows (f.project);
    CHECK (rows.trackTop (1) == ui::TrackRows::trackHeight + 2 * ui::TrackRows::laneHeight);
    CHECK (rows.trackIndexAt (ui::TrackRows::trackHeight + 10) == 0);   // a lane belongs to its track
    CHECK (rows.rowAt (ui::TrackRows::trackHeight + 10)->isLane());
    CHECK (rows.insertIndexAt (rows.trackTop (1) - 5) == 1);

    // Hiding (an empty lane just goes).
    edit::hideAllAutomationLanes (f.project, f.first);
    CHECK (f.list.getNumLaneHeaders() == 0);
    CHECK (f.list.getIdealHeight() == before);
}

TEST_CASE ("Clicking in a lane adds a point; points can be dragged, bent and removed", "[ui][automation]")
{
    Fixture f;
    const auto lane = edit::showAutomationLane (f.project, f.first, Target::pan());
    const auto area = f.lanes->laneArea (lane);
    REQUIRE (! area.isEmpty());
    CHECK (area.getY() == ui::TrackRows::trackHeight);

    // Only the lane takes the mouse; the track's own row is left to the clips.
    CHECK (f.lanes->hitTest (300, area.getCentreY()));
    CHECK_FALSE (f.lanes->hitTest (300, 20));

    // A click a little after beat 2, at the top: a point on beat 2, fully right.
    f.click ({ 212.0f, f.lanes->positionFor (lane, 0, 1.0f).y });
    auto p = f.points (lane);
    REQUIRE (p.size() == 1);
    CHECK (p[0].tick == 2 * beat);
    CHECK_THAT (p[0].value, WithinAbs (1.0, 0.02));
    CHECK (f.project.getUndoManager().getUndoDescription() == "Add automation point");

    // Another, with Alt (off the grid), at the bottom.
    f.click ({ 450.0f, f.lanes->positionFor (lane, 0, -1.0f).y }, juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::altModifier));
    p = f.points (lane);
    REQUIRE (p.size() == 2);
    CHECK (p[1].tick == (Tick) (4.5 * (double) beat));
    CHECK_THAT (p[1].value, WithinAbs (-1.0, 0.02));

    // Drag the first point to the middle: one undo step.
    f.drag (f.lanes->pointPosition (lane, 0), f.lanes->positionFor (lane, 3 * beat, 0.0f));
    p = f.points (lane);
    CHECK (p[0].tick == 3 * beat);
    CHECK_THAT (p[0].value, WithinAbs (0.0, 0.03));
    CHECK (f.project.getUndoManager().getUndoDescription() == "Move automation point");
    f.project.getUndoManager().undo();
    CHECK (f.points (lane)[0].tick == 2 * beat);

    // Ctrl+drag the line between them: it bends.
    const auto middle = (f.lanes->pointPosition (lane, 0) + f.lanes->pointPosition (lane, 1)) / 2.0f;
    f.drag (middle, middle + juce::Point<float> (0.0f, 30.0f),
            juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::commandModifier));
    CHECK (std::abs (f.points (lane)[0].bend) > 0.2f);

    // Double-click a point: gone.
    const auto at = f.lanes->pointPosition (lane, 1);
    f.lanes->mouseDoubleClick (Fixture::event (*f.lanes, at, at));
    CHECK (f.points (lane).size() == 1);
}

TEST_CASE ("Switches and choices are drawn as steps and snap to their values", "[ui][automation]")
{
    Fixture f;
    const auto lane = edit::showAutomationLane (f.project, f.first, Target::mute());
    const auto area = f.lanes->laneArea (lane);

    f.click ({ 100.0f, (float) area.getY() + 20.0f });       // nearer the top: muted
    f.click ({ 300.0f, (float) area.getBottom() - 20.0f });  // nearer the bottom: not muted

    const auto p = f.points (lane);
    REQUIRE (p.size() == 2);
    CHECK (p[0].value == 1.0f);
    CHECK (p[1].value == 0.0f);

    // Every lane draws, with points or without.
    edit::showAutomationLane (f.project, f.first, Target::volume());
    juce::Image image (juce::Image::ARGB, 1000, 600, true);
    juce::Graphics g (image);
    f.lanes->paintEntireComponent (g, false);
}
