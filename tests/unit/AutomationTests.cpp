// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
//
// Automation (Milestone 14): lanes, points, the value between points, and their undo.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "dsp/EffectTypes.h"
#include "io/ProjectFormat.h"
#include "engine/AutomationRecorder.h"
#include "model/Automation.h"
#include "model/EditCommands.h"
#include "model/MidiClips.h"
#include "model/Mixer.h"

using namespace aether;
using namespace aether::automation;
using Catch::Matchers::WithinAbs;

namespace
{
    constexpr Tick bar = ticksPerQuarter * 4;

    struct Fixture
    {
        Fixture()
        {
            track = edit::addTrack (project, TrackKind::audio, -1, true, "Vocals").getId();
            project.getUndoManager().clearUndoHistory();
        }

        Project project;
        juce::String track;
    };
}

TEST_CASE ("Automation targets are written as text and read back", "[automation]")
{
    for (const auto& t : { Target::volume(), Target::pan(), Target::mute(), Target::send ("s1"),
                           Target::effect ("fx1", "mix"), Target::effectOn ("fx1"), Target::instrument ("cutoff") })
    {
        CHECK (Target::fromString (t.toString()) == t);
    }

    CHECK_FALSE (Target::fromString ("nonsense").isValid());
    CHECK_FALSE (Target::fromString ("effect:").isValid());
}

TEST_CASE ("Ranges put values on the lane the way the controls do", "[automation]")
{
    Fixture f;
    const auto volume = rangeOf (f.project, f.track, Target::volume());
    REQUIRE (volume);
    CHECK_THAT (volume->toPosition (0.0f), WithinAbs (mixer::volumeToFaderPosition (0.0), 1.0e-5));
    CHECK_THAT (volume->fromPosition (volume->toPosition (-12.0f)), WithinAbs (-12.0, 1.0e-3));

    const auto eq = edit::addInsert (f.project, f.track, "eq");
    const auto frequency = rangeOf (f.project, f.track, Target::effect (eq, "band2Freq"));
    REQUIRE (frequency);
    CHECK (frequency->scale == Range::Scale::logarithmic);
    CHECK_THAT (frequency->toPosition (std::sqrt (200.0f * 8000.0f)), WithinAbs (0.5, 1.0e-4));   // the middle, in octaves

    CHECK (rangeOf (f.project, f.track, Target::effect (eq, "lowCutOn"))->scale == Range::Scale::toggle);
    CHECK (rangeOf (f.project, f.track, Target::effectOn (eq))->scale == Range::Scale::toggle);
    CHECK (rangeOf (f.project, f.track, Target::mute())->holds());
    CHECK_FALSE (rangeOf (f.project, f.track, Target::effect ("gone", "mix")));
    CHECK_FALSE (rangeOf (f.project, "no-track", Target::volume()));
}

TEST_CASE ("Between points the value moves along a line, straight or bent; switches jump", "[automation]")
{
    Range linear;
    linear.minimum = 0.0f;
    linear.maximum = 100.0f;

    const std::vector<Point> points { { bar, 20.0f, 0.0f }, { 2 * bar, 60.0f, 0.0f }, { 3 * bar, 60.0f, 0.0f } };
    CHECK (valueAt (points, linear, 0, 5.0f) == 20.0f);                       // before the first: its value
    CHECK_THAT (valueAt (points, linear, bar + bar / 2, 5.0f), WithinAbs (40.0, 1.0e-3));
    CHECK (valueAt (points, linear, 10 * bar, 5.0f) == 60.0f);                // after the last
    CHECK (valueAt ({}, linear, bar, 5.0f) == 5.0f);                          // no points: the static value

    // Bent: slower first (positive), quicker first (negative).
    auto bent = points;
    bent[0].bend = 0.8f;
    CHECK (valueAt (bent, linear, bar + bar / 2, 5.0f) < 35.0f);
    bent[0].bend = -0.8f;
    CHECK (valueAt (bent, linear, bar + bar / 2, 5.0f) > 45.0f);

    // A switch holds until the next point.
    Range toggle;
    toggle.scale = Range::Scale::toggle;
    const std::vector<Point> onOff { { 0, 1.0f, 0.0f }, { bar, 0.0f, 0.0f } };
    CHECK (valueAt (onOff, toggle, bar - 1, 0.0f) == 1.0f);
    CHECK (valueAt (onOff, toggle, bar, 0.0f) == 0.0f);

    // Volume moves evenly on the fader, not in dB.
    Range volume;
    volume.minimum = -100.0f;
    volume.maximum = 6.0f;
    volume.scale = Range::Scale::volume;
    const std::vector<Point> fade { { 0, 0.0f, 0.0f }, { bar, -100.0f, 0.0f } };
    const auto halfway = valueAt (fade, volume, bar / 2, 0.0f);
    CHECK_THAT (mixer::volumeToFaderPosition (halfway), WithinAbs (mixer::volumeToFaderPosition (0.0) / 2.0, 1.0e-3));
}

TEST_CASE ("Points can be added, moved, bent and removed, each an undo step", "[automation]")
{
    Fixture f;
    auto& undo = f.project.getUndoManager();
    const auto lane = edit::showAutomationLane (f.project, f.track, Target::volume());
    REQUIRE (lane.isNotEmpty());
    CHECK_FALSE (undo.canUndo());   // showing a lane isn't an undo step
    CHECK_FALSE (isAutomated (f.project, f.track, Target::volume()));
    CHECK (edit::showAutomationLane (f.project, f.track, Target::volume()) == lane);   // the same lane again

    CHECK (edit::addAutomationPoint (f.project, lane, 2 * bar, -6.0f) == 0);
    CHECK (edit::addAutomationPoint (f.project, lane, 0, 0.0f) == 0);          // kept in time order
    CHECK (edit::addAutomationPoint (f.project, lane, 4 * bar, 50.0f) == 2);   // kept within range: +6 dB
    CHECK (isAutomated (f.project, f.track, Target::volume()));

    auto points = pointsOf (findLane (f.project, lane));
    REQUIRE (points.size() == 3);
    CHECK (points[0].tick == 0);
    CHECK (points[1].value == -6.0f);
    CHECK_THAT (points[2].value, WithinAbs (6.0, 1.0e-4));

    // A point can't pass its neighbours.
    REQUIRE (edit::moveAutomationPoint (f.project, lane, 1, 10 * bar, -12.0f));
    points = pointsOf (findLane (f.project, lane));
    CHECK (points[1].tick == 4 * bar);
    CHECK (points[1].value == -12.0f);
    CHECK (undo.getUndoDescription() == "Move automation point");

    REQUIRE (edit::setAutomationBend (f.project, lane, 0, 0.5f));
    CHECK (pointsOf (findLane (f.project, lane))[0].bend == 0.5f);

    // The value the setting has at a time.
    CHECK_THAT (currentValue (f.project, f.track, Target::volume(), 0), WithinAbs (0.0, 1.0e-4));
    CHECK_THAT (currentValue (f.project, f.track, Target::volume(), 8 * bar), WithinAbs (6.0, 1.0e-4));

    REQUIRE (edit::removeAutomationPoints (f.project, lane, { 0, 2 }));
    CHECK (pointsOf (findLane (f.project, lane)).size() == 1);
    undo.undo();
    CHECK (pointsOf (findLane (f.project, lane)).size() == 3);

    REQUIRE (edit::clearAutomationLane (f.project, lane));
    CHECK_FALSE (isAutomated (f.project, f.track, Target::volume()));
    CHECK (currentValue (f.project, f.track, Target::volume(), 0) == 0.0f);   // back to the fader's value
    undo.undo();
    CHECK (isAutomated (f.project, f.track, Target::volume()));

    // Removing the lane takes the points with it; undo brings them back.
    REQUIRE (edit::removeAutomationLane (f.project, lane));
    CHECK_FALSE (findLane (f.project, lane).isValid());
    undo.undo();
    CHECK (pointsOf (findLane (f.project, lane)).size() == 3);
}

TEST_CASE ("Hiding a lane keeps its points; an empty one just goes", "[automation]")
{
    Fixture f;
    const auto withPoints = edit::showAutomationLane (f.project, f.track, Target::pan());
    edit::addAutomationPoint (f.project, withPoints, 0, -1.0f);
    const auto empty = edit::showAutomationLane (f.project, f.track, Target::mute());

    edit::hideAllAutomationLanes (f.project, f.track);
    CHECK (findLane (f.project, withPoints).isValid());
    CHECK_FALSE ((bool) findLane (f.project, withPoints)[ids::shown]);
    CHECK_FALSE (findLane (f.project, empty).isValid());
    CHECK (isAutomated (f.project, f.track, Target::pan()));
}

TEST_CASE ("The lane menu lists everything a track has", "[automation]")
{
    Project project;
    const auto keys = edit::addTrack (project, TrackKind::midi, -1, true, "Keys").getId();
    const auto bus = edit::addTrack (project, TrackKind::bus, -1, true, "Reverb bus").getId();
    edit::addSend (project, keys, bus);
    edit::addInsert (project, keys, "eq");
    edit::addInsert (project, keys, "eq");

    const auto choices = choicesFor (project, keys);
    auto has = [&] (const juce::String& name, const juce::String& group)
    {
        return std::any_of (choices.begin(), choices.end(), [&] (const Choice& c) { return c.name == name && c.group == group; });
    };

    CHECK (has ("Volume", {}));
    CHECK (has ("Pan", {}));
    CHECK (has ("Mute", {}));
    CHECK (has ("Send to Reverb bus", "Sends"));
    CHECK (has ("On/off", "EQ 1"));
    CHECK (has ("Mids", "EQ 2"));

    const auto* synth = dsp::findEffectType (midi::instrumentOf (project, keys)[ids::type].toString());
    REQUIRE (synth != nullptr);
    CHECK (has (synth->parameters.front().name, synth->name));

    for (const auto& c : choices)
    {
        INFO (c.target.toString());
        CHECK (rangeOf (project, keys, c.target).has_value());
        CHECK (nameOf (project, keys, c.target).isNotEmpty());
    }
}

TEST_CASE ("Recording writes the knob's moves over what was there", "[automation]")
{
    Fixture f;
    const auto eq = edit::addInsert (f.project, f.track, "eq");
    const auto target = Target::effect (eq, "output");
    const auto lane = edit::showAutomationLane (f.project, f.track, target);
    for (int i = 0; i <= 8; ++i)
        edit::addAutomationPoint (f.project, lane, i * bar, 0.0f);

    // The knob moved from bar 2 to bar 4 (each write replaces what's after the previous one).
    f.project.getUndoManager().beginNewTransaction ("Record");
    edit::writeAutomation (f.project, f.track, target, 2 * bar, 2 * bar, 3.0f);
    edit::writeAutomation (f.project, f.track, target, 2 * bar, 3 * bar, 6.0f);
    edit::writeAutomation (f.project, f.track, target, 3 * bar, 4 * bar, 9.0f);

    const auto points = pointsOf (findLane (f.project, lane));
    REQUIRE (points.size() == 10);   // bars 0, 1, 2 (old and new), 3 and 4 (new), 5..8: the old 3 and 4 are gone
    CHECK (valueAt (points, *rangeOf (f.project, f.track, target), 3 * bar, 0.0f) == 6.0f);
    CHECK (valueAt (points, *rangeOf (f.project, f.track, target), 4 * bar, 0.0f) == 9.0f);
    CHECK (valueAt (points, *rangeOf (f.project, f.track, target), 6 * bar, 0.0f) == 0.0f);

    // One undo puts it all back.
    f.project.getUndoManager().undo();
    CHECK (pointsOf (findLane (f.project, lane)).size() == 9);
}

TEST_CASE ("Recording knows which setting a changed property belongs to", "[automation]")
{
    Fixture f;
    const auto bus = edit::addTrack (f.project, TrackKind::bus).getId();
    const auto send = edit::addSend (f.project, f.track, bus);
    const auto eq = edit::addInsert (f.project, f.track, "eq");
    const auto trackNode = f.project.findTrack (f.track).getState();

    CHECK (targetOfProperty (f.project, trackNode, ids::volume).second == Target::volume());
    CHECK (targetOfProperty (f.project, trackNode, ids::volume).first == f.track);
    CHECK (targetOfProperty (f.project, trackNode, ids::pan).second == Target::pan());
    CHECK_FALSE (targetOfProperty (f.project, trackNode, ids::name).second.isValid());
    CHECK (targetOfProperty (f.project, mixer::findSend (f.project, send), ids::level).second == Target::send (send));
    CHECK (targetOfProperty (f.project, mixer::findInsert (f.project, eq), "band2Gain").second == Target::effect (eq, "band2Gain"));
    CHECK (targetOfProperty (f.project, mixer::findInsert (f.project, eq), ids::bypass).second == Target::effectOn (eq));
    CHECK_FALSE (targetOfProperty (f.project, mixer::findInsert (f.project, eq), ids::preset).second.isValid());

    // The master isn't automated (it has no lanes yet).
    const auto masterEq = edit::addInsert (f.project, mixer::masterId, "eq");
    CHECK_FALSE (targetOfProperty (f.project, mixer::findInsert (f.project, masterEq), "output").second.isValid());

    const auto keys = edit::addTrack (f.project, TrackKind::midi).getId();
    const auto instrument = midi::instrumentOf (f.project, keys);
    const auto* type = dsp::findEffectType (instrument[ids::type].toString());
    REQUIRE (type != nullptr);
    CHECK (targetOfProperty (f.project, instrument, type->parameters.front().id).second == Target::instrument (type->parameters.front().id.toString()));
}

TEST_CASE ("Removing an effect or a send removes its automation (in the same undo step)", "[automation]")
{
    Fixture f;
    const auto eq = edit::addInsert (f.project, f.track, "eq");
    const auto lane = edit::showAutomationLane (f.project, f.track, Target::effect (eq, "output"));
    edit::addAutomationPoint (f.project, lane, 0, 3.0f);
    const auto volumeLane = edit::showAutomationLane (f.project, f.track, Target::volume());
    edit::addAutomationPoint (f.project, volumeLane, 0, -3.0f);

    edit::removeInsert (f.project, eq);
    CHECK_FALSE (findLane (f.project, lane).isValid());
    CHECK (findLane (f.project, volumeLane).isValid());

    f.project.getUndoManager().undo();
    CHECK (findLane (f.project, lane).isValid());
    CHECK (isAutomated (f.project, f.track, Target::effect (eq, "output")));
}

TEST_CASE ("Automation is saved with the project", "[automation][io]")
{
    Fixture f;
    const auto lane = edit::showAutomationLane (f.project, f.track, Target::pan());
    edit::addAutomationPoint (f.project, lane, bar, -0.5f);
    edit::setAutomationBend (f.project, lane, 0, 0.25f);

    const auto loaded = projectformat::read (projectformat::write (f.project));
    REQUIRE (loaded.result.wasOk());
    auto reopened = Project::fromState (loaded.state);
    REQUIRE (reopened != nullptr);

    const auto points = pointsOf (findLane (*reopened, lane));
    REQUIRE (points.size() == 1);
    CHECK (points[0].tick == bar);
    CHECK (points[0].value == -0.5f);
    CHECK (points[0].bend == 0.25f);
    CHECK ((bool) findLane (*reopened, lane)[ids::shown]);
}

TEST_CASE ("Recording automation writes the moves of any control while playing", "[automation][recording]")
{
    Fixture f;
    bool playing = false;
    Tick playhead = 0;
    double now = 0.0;
    AutomationRecorder recorder (f.project, [&] { return playing; }, [&] { return playhead; });
    recorder.clock = [&] { return now; };

    // Off, or stopped: changes are just changes.
    edit::setVolume (f.project, f.track, -3.0);
    CHECK_FALSE (findLane (f.project, f.track, Target::volume()).isValid());

    recorder.setEnabled (true);
    edit::setVolume (f.project, f.track, -4.0);
    CHECK_FALSE (findLane (f.project, f.track, Target::volume()).isValid());

    // Playing: a fader move from bar 1 to bar 2, in one undo step (like a drag).
    playing = true;
    f.project.getUndoManager().beginNewTransaction ("Change volume");
    for (int i = 0; i <= 4; ++i)
    {
        playhead = bar + i * bar / 4;
        now = 1.0 + i * 0.1;
        edit::setVolume (f.project, f.track, -5.0 - i * 2.0, false);
    }

    const auto lane = findLane (f.project, f.track, Target::volume());
    REQUIRE (lane.isValid());
    CHECK ((bool) lane[ids::shown]);
    auto points = pointsOf (lane);
    REQUIRE (points.size() == 5);
    CHECK (points.front().tick == bar);
    CHECK (points.back().tick == 2 * bar);
    CHECK_THAT (points.back().value, WithinAbs (-13.0, 1.0e-4));
    CHECK (recorder.getNumActiveMoves() == 1);

    // Let go: the move ends.
    recorder.update (3.0);
    CHECK (recorder.getNumActiveMoves() == 0);

    // A second pass: moved to 0 dB at 1.25 bars, then held still (mouse down) until 1.75 bars,
    // replacing what was there.
    bool held = true;
    recorder.controlHeld = [&] { return held; };
    f.project.getUndoManager().beginNewTransaction ("Change volume");
    playhead = bar + bar / 4;
    now = 4.0;
    edit::setVolume (f.project, f.track, 0.0, false);
    playhead = bar + 3 * bar / 4;
    now = 4.5;
    recorder.update (now);
    CHECK (recorder.getNumActiveMoves() == 1);   // still held
    points = pointsOf (lane);
    CHECK (points.size() == 5);   // 1, 1.25 (old and new), 1.75 (new), 2: the old 1.5 and 1.75 are gone
    CHECK_THAT (valueAt (points, *rangeOf (f.project, f.track, Target::volume()), bar + bar / 2, 0.0f), WithinAbs (0.0, 1.0e-4));

    held = false;
    recorder.update (5.5);
    CHECK (recorder.getNumActiveMoves() == 0);

    // One undo takes back the second pass (the fader move and what it wrote).
    f.project.getUndoManager().undo();
    CHECK (pointsOf (lane).size() == 5);
    CHECK_THAT (valueAt (pointsOf (lane), *rangeOf (f.project, f.track, Target::volume()), bar + bar / 2, 0.0f), WithinAbs (-9.0, 1.0e-3));

    // Effects (and switching them off) record too.
    const auto eq = edit::addInsert (f.project, f.track, "eq");
    playhead = 4 * bar;
    edit::setInsertParameter (f.project, eq, "output", 6.0f);
    edit::setInsertBypassed (f.project, eq, true);
    CHECK (isAutomated (f.project, f.track, Target::effect (eq, "output")));
    CHECK (isAutomated (f.project, f.track, Target::effectOn (eq)));
    CHECK (currentValue (f.project, f.track, Target::effectOn (eq), 4 * bar) == 0.0f);
}
