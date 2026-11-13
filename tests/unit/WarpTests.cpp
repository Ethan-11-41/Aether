// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
//
// Audio clips that follow the song's tempo (Milestone 17): where their audio is on the timeline,
// with tempo changes, loops, trims, splits, warp markers and transposing.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "model/ClipEdits.h"
#include "model/EditCommands.h"
#include "model/TimelineEdits.h"
#include "model/Warp.h"

using namespace aether;

namespace
{
    constexpr Tick q = ticksPerQuarter;
    constexpr double rate = 48000.0;

    /** A song at 120 BPM with an audio clip of 4 seconds (8 beats at 120) starting at bar 2. */
    struct Fixture
    {
        Fixture()
        {
            track = edit::addTrack (project, TrackKind::audio).getId();
            clipId = edit::addAudioClip (project, track, { "Audio/drums.wav", "drums", rate, 4 * 48000 }, 4 * q);
        }

        Clip clip() const          { return clips::findClip (project, clipId); }
        TempoMap tempo() const     { return timeline::buildTempoMap (project); }
        Tick end() const           { return clip().getEnd (tempo()); }

        Project project;
        juce::String track, clipId;
    };

    double lastSeconds (const Clip& clip, const TempoMap& tempo)
    {
        const auto map = warp::timeMap (clip, tempo);
        return map.empty() ? 0.0 : map.back().seconds;
    }
}

TEST_CASE ("A clip that doesn't follow the tempo plays at its own speed, as before", "[warp]")
{
    Fixture f;
    CHECK_FALSE (warp::followsTempo (f.clip()));
    CHECK (f.end() == 12 * q);

    // Half the speed of the song: the clip lasts half as many beats.
    edit::setTempo (f.project, 0, 60.0);
    CHECK (f.end() == 8 * q);
    CHECK_FALSE (warp::needsStretching (f.clip(), f.tempo()));

    const warp::ClipTiming timing (f.clip());
    CHECK (timing.tickAt (f.tempo(), 48000.0) == Catch::Approx (5.0 * q).margin (1.0));   // 1 s at 60 BPM: a beat
    CHECK (timing.playedAt (f.tempo(), 6.0 * q) == Catch::Approx (96000.0).margin (1.0));
}

TEST_CASE ("A clip that follows the tempo stays in its bars when the tempo changes", "[warp]")
{
    Fixture f;
    REQUIRE (edit::setFollowTempo (f.project, { f.clipId }, true));
    CHECK (f.project.getUndoManager().getUndoDescription() == "Follow the song's tempo");
    CHECK (warp::followsTempo (f.clip()));
    CHECK (warp::bpmOf (f.clip()) == Catch::Approx (120.0));
    CHECK (f.end() == 12 * q);
    CHECK_FALSE (warp::needsStretching (f.clip(), f.tempo()));   // at its own tempo: played as it is

    // At 60 BPM it still lasts 8 beats, now 8 seconds: it's stretched.
    edit::setTempo (f.project, 0, 60.0);
    CHECK (f.end() == 12 * q);
    CHECK (warp::needsStretching (f.clip(), f.tempo()));
    CHECK (lastSeconds (f.clip(), f.tempo()) == Catch::Approx (8.0).margin (0.001));

    // Trimmed and split in beats.
    REQUIRE (edit::trimClipEnd (f.project, f.clipId, 10 * q));
    CHECK (f.clip().getDuration() == 144000);   // 6 beats of the 8: 3 of its seconds
    CHECK (f.end() == 10 * q);

    const auto right = edit::splitClip (f.project, f.clipId, 7 * q);
    REQUIRE (right.isNotEmpty());
    CHECK (f.clip().getDuration() == 72000);   // 3 beats at 120: 1.5 s
    const auto rightClip = clips::findClip (f.project, right);
    CHECK (rightClip.getStart() == 7 * q);
    CHECK (rightClip.getOffset() == 72000);
    CHECK (rightClip.getEnd (f.tempo()) == 10 * q);
    CHECK (warp::followsTempo (rightClip));

    REQUIRE (edit::trimClipStart (f.project, right, 8 * q));
    CHECK (clips::findClip (f.project, right).getOffset() == 96000);
    CHECK (clips::findClip (f.project, right).getEnd (f.tempo()) == 10 * q);

    // Off again: its own speed from where it starts.
    REQUIRE (edit::setFollowTempo (f.project, { f.clipId }, false));
    CHECK (f.end() == 4 * q + (Tick) (1.5 * q));   // 1.5 s at 60 BPM

    // Turned on again, it goes back into the bars at its own tempo (120).
    REQUIRE (edit::setFollowTempo (f.project, { f.clipId }, true));
    CHECK (f.end() == 7 * q);
}

TEST_CASE ("Following from now keeps a clip sounding the same across tempo changes", "[warp]")
{
    Fixture f;
    edit::addTempoChange (f.project, 8 * q, 90.0);   // the clip crosses it
    const auto endBefore = f.end();
    const auto mapBefore = warp::timeMap (f.clip(), f.tempo());

    REQUIRE (edit::setFollowTempo (f.project, { f.clipId }, true));
    CHECK (f.end() == endBefore);
    CHECK_FALSE (warp::needsStretching (f.clip(), f.tempo()));
    CHECK (warp::markersOf (f.clip()).size() >= 2);

    // Then the tempo change goes: the part after it plays faster again.
    edit::setTempo (f.project, 1, 120.0);
    CHECK (f.end() == endBefore);   // the same beats...
    CHECK (warp::needsStretching (f.clip(), f.tempo()));
    CHECK (lastSeconds (f.clip(), f.tempo()) < mapBefore.back().seconds - 0.1);   // ...in less time
}

TEST_CASE ("A loop that follows the tempo plays in time at any tempo", "[warp]")
{
    Project project;   // 120 BPM
    const auto track = edit::addTrack (project, TrackKind::audio).getId();

    // A one-bar loop recorded at 90 BPM (4 beats: 2.667 s), repeated for 4 bars.
    const auto loop = edit::addAudioClip (project, track, { "Audio/loop.wav", "loop", rate, 128000 }, 0);
    REQUIRE (edit::setClipLooping (project, loop, true));
    REQUIRE (edit::setFollowTempo (project, { loop }, true));
    REQUIRE (edit::setClipBpm (project, loop, 90.0));
    CHECK (project.getUndoManager().getUndoDescription() == "Set the clip's tempo");

    // At the song's 120 BPM, one repeat is one bar.
    auto clip = clips::findClip (project, loop);
    REQUIRE (edit::trimClipEnd (project, loop, 16 * q));
    clip = clips::findClip (project, loop);
    CHECK (clip.getDuration() == 4 * 128000);
    CHECK (clip.getEnd (timeline::buildTempoMap (project)) == 16 * q);
    CHECK (lastSeconds (clip, timeline::buildTempoMap (project)) == Catch::Approx (8.0).margin (0.001));
    CHECK (warp::needsStretching (clip, timeline::buildTempoMap (project)));

    // Trimming the start of a loop moves where in the loop it starts, still in time.
    REQUIRE (edit::trimClipStart (project, loop, 2 * q));
    clip = clips::findClip (project, loop);
    CHECK (clip.getPhase() == 64000);
    CHECK (clip.getEnd (timeline::buildTempoMap (project)) == 16 * q);
    const warp::ClipTiming timing (clip);
    CHECK (timing.tickAt (timeline::buildTempoMap (project), 64000.0) == Catch::Approx (4.0 * q).margin (1.0));
}

TEST_CASE ("Warp markers pin a moment of the audio to a beat, and can be dragged", "[warp]")
{
    Fixture f;
    auto& undo = f.project.getUndoManager();

    // A marker at 1 second (beat 2 of the clip): it pins the clip's start too. Nothing moves.
    REQUIRE (edit::addWarpMarker (f.project, f.clipId, 48000));
    CHECK (undo.getUndoDescription() == "Add warp marker");
    CHECK (warp::followsTempo (f.clip()));
    auto markers = warp::markersOf (f.clip());
    REQUIRE (markers.size() == 2);
    CHECK (markers[0].sample == 0);
    CHECK (markers[1].sample == 48000);
    CHECK (f.end() == 12 * q);
    CHECK_FALSE (warp::needsStretching (f.clip(), f.tempo()));

    // Dragged a beat later: the first second now lasts two beats, the rest moves a beat later.
    REQUIRE (edit::moveWarpMarker (f.project, f.clipId, 1, 7 * q));
    CHECK (undo.getUndoDescription() == "Move warp marker");
    CHECK (f.end() == 13 * q);
    const warp::ClipTiming timing (f.clip());
    CHECK (timing.tickAt (f.tempo(), 48000.0) == Catch::Approx (7.0 * q).margin (1.0));
    CHECK (timing.tickAt (f.tempo(), 24000.0) == Catch::Approx (5.5 * q).margin (1.0));
    CHECK (warp::needsStretching (f.clip(), f.tempo()));

    // It can't pass its neighbour, and the clip's start can't be dragged.
    REQUIRE (edit::moveWarpMarker (f.project, f.clipId, 1, 3 * q));
    CHECK (warp::markersOf (f.clip())[1].tick > warp::markersOf (f.clip())[0].tick);
    CHECK_FALSE (edit::moveWarpMarker (f.project, f.clipId, 0, 2 * q));

    // Too close to one that's there: nothing added.
    CHECK_FALSE (edit::addWarpMarker (f.project, f.clipId, 48001));

    // Removed: back to the clip's own tempo.
    REQUIRE (edit::removeWarpMarker (f.project, f.clipId, 1));
    CHECK (f.end() == 12 * q);
    REQUIRE (edit::clearWarpMarkers (f.project, f.clipId));
    CHECK (warp::markersOf (f.clip()).empty());

    // Each was one undo step.
    undo.undo();
    undo.undo();
    CHECK (warp::markersOf (f.clip()).size() == 2);
}

TEST_CASE ("Transposing a clip changes its pitch, following the tempo or not", "[warp]")
{
    Fixture f;
    CHECK (warp::transposeOf (f.clip()) == 0.0);
    REQUIRE (edit::setClipTranspose (f.project, { f.clipId }, 3.0));
    CHECK (f.project.getUndoManager().getUndoDescription() == "Transpose");
    CHECK (warp::transposeOf (f.clip()) == 3.0);
    CHECK (warp::needsStretching (f.clip(), f.tempo()));
    CHECK (f.end() == 12 * q);   // the same length

    CHECK_FALSE (edit::setClipTranspose (f.project, { f.clipId }, 3.0));
    REQUIRE (edit::setClipTranspose (f.project, { f.clipId }, 30.0));
    CHECK (warp::transposeOf (f.clip()) == 24.0);

    REQUIRE (edit::setClipTranspose (f.project, { f.clipId }, 0.0));
    CHECK (f.project.getUndoManager().getUndoDescription() == "Back to the original pitch");
    CHECK_FALSE (f.clip().getState().hasProperty (ids::transpose));
    CHECK_FALSE (warp::needsStretching (f.clip(), f.tempo()));
}

TEST_CASE ("A loop's tempo is read from its file name", "[warp][import]")
{
    // Said outright.
    CHECK (warp::bpmFromFileName ("Drums 90 BPM", 3.0) == 90.0);
    CHECK (warp::bpmFromFileName ("beat_120bpm_Am", 7.0) == 120.0);
    CHECK (warp::bpmFromFileName ("Groove-BPM-95", 1.0) == 95.0);
    CHECK (warp::bpmFromFileName ("Shaker (87.5 bpm)", 1.0) == 87.5);

    // A bare number, only if the recording is whole bars at that tempo (2 bars at 96: 5 s).
    CHECK (warp::bpmFromFileName ("Funk_Loop_96_Am", 5.0) == 96.0);
    CHECK (warp::bpmFromFileName ("Funk_Loop_96_Am", 4.3) == 0.0);
    CHECK (warp::bpmFromFileName ("Take 3", 5.0) == 0.0);
    CHECK (warp::bpmFromFileName ("Vocals 2026", 5.0) == 0.0);
    CHECK (warp::bpmFromFileName ("Guitar", 5.0) == 0.0);
}
