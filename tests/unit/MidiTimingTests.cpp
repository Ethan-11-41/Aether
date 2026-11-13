// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
//
// Timing for MIDI played without the metronome (Milestone 15): making it steady, making the
// song's tempo follow it, and quantizing whole clips from the timeline.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <random>

#include "model/ClipEdits.h"
#include "model/EditCommands.h"
#include "model/MidiTiming.h"
#include "model/NoteEdits.h"
#include "model/TimelineEdits.h"

using namespace aether;

namespace
{
    constexpr Tick q = ticksPerQuarter;
    constexpr Tick bar = 4 * q;

    /** A song at 120 BPM with one MIDI track, and a clip recorded on it from the start of the
        song while someone played freely: `beatTime(b)` says when they played beat b, in seconds.
        Each beat: a chord (three notes; on the first beat of a bar a long bass note too), and a
        soft melody note half way to the next beat. Each beat is a little early or late (up to
        `wobble` seconds, the whole chord together). */
    struct Played
    {
        Played (int bars, std::function<double (double)> beatTimeToUse, double wobble = 0.0, int pickupBeats = 0)
            : beatTime (std::move (beatTimeToUse))
        {
            track = edit::addTrack (project, TrackKind::midi).getId();
            std::mt19937 random (17);
            std::uniform_real_distribution<double> early (-wobble, wobble);
            std::vector<NoteData> notes;

            const auto toTicks = [] (double seconds) { return (Tick) std::llround (seconds * 2.0 * (double) q); };   // 120 BPM

            for (int p = 0; p < pickupBeats; ++p)
                notes.push_back ({ 74 + p, 60, toTicks (beatTime (p - pickupBeats)), toTicks (0.4) });

            for (int b = 0; b < bars * 4; ++b)
            {
                const auto t = beatTime (b) + early (random);
                played.push_back (t);

                if (b % 4 == 0)
                    notes.push_back ({ 40, 100, toTicks (t), toTicks (2.0) });

                for (const auto pitch : { 60, 64, 67 })
                    notes.push_back ({ pitch, b % 4 == 0 ? 95 : 80, toTicks (t), toTicks (0.45) });

                notes.push_back ({ 72, 55, toTicks (0.5 * (beatTime (b) + beatTime (b + 1))), toTicks (0.2) });
            }

            clipId = edit::addMidiClip (project, track, "Played", 0, toTicks (beatTime (bars * 4) + 1.0), notes);
        }

        Clip clip() const   { return clips::findClip (project, clipId); }
        TempoMap tempo() const   { return timeline::buildTempoMap (project); }

        /** The clip's notes of one pitch, as positions on the timeline (ticks), in time order. */
        std::vector<Tick> positionsOf (int pitch) const
        {
            std::vector<Tick> result;
            const auto c = clip();
            for (const auto& n : midi::audibleNotes (c))
                if (n.pitch == pitch)
                    result.push_back (c.getStart() + n.start);

            std::sort (result.begin(), result.end());
            return result;
        }

        /** Every note's sounding time (seconds) and length, in a stable order. */
        std::vector<std::pair<double, double>> soundingTimes() const
        {
            const auto map = tempo();
            const auto c = clip();
            std::vector<std::pair<double, double>> result;

            for (const auto& n : midi::audibleNotes (c))
            {
                const auto at = map.ticksToSeconds (c.getStart() + n.start);
                result.emplace_back (at, map.ticksToSeconds (c.getStart() + n.start + n.length) - at);
            }

            std::sort (result.begin(), result.end());
            return result;
        }

        Project project;
        juce::String track, clipId;
        std::function<double (double)> beatTime;
        std::vector<double> played;
    };

    std::function<double (double)> steady (double bpm, double start)
    {
        return [=] (double beat) { return start + beat * 60.0 / bpm; };
    }

    std::function<double (double)> speedingUp (double from, double to, double beats, double start)
    {
        return [=] (double beat)
        {
            const auto b = std::clamp (beat, 0.0, beats);
            const auto slope = (60.0 / to - 60.0 / from) / beats;
            auto t = start + 60.0 / from * b + 0.5 * slope * b * b;
            if (beat > beats) t += (beat - beats) * 60.0 / to;
            if (beat < 0.0)   t += beat * 60.0 / from;
            return t;
        };
    }

    /** The largest distance (ticks) from each of `positions` to the beat grid that starts at
        `first` (one position per beat, in order). */
    Tick worstOffGrid (const std::vector<Tick>& positions, Tick first, Tick step)
    {
        Tick worst = 0;
        for (size_t i = 0; i < positions.size(); ++i)
            worst = std::max (worst, std::abs (positions[i] - (first + (Tick) i * step)));
        return worst;
    }
}

TEST_CASE ("The beat of MIDI clips is found from how they sound", "[timing][model]")
{
    Played p (12, speedingUp (84.0, 100.0, 48.0, 1.3), 0.0);
    const auto beat = miditiming::findBeat (p.project, { p.clipId });

    REQUIRE (beat.found);
    CHECK (beat.beatsPerBar == 4);
    CHECK (beat.slowestBpm < 88.0);
    CHECK (beat.fastestBpm > 96.0);
    CHECK (beat.downbeat == 0);
    CHECK (std::abs (beat.beats[0] - 1.3) < 0.02);
    CHECK (miditiming::songTempoAt (p.project, { p.clipId }) == Catch::Approx (120.0));

    // Repeating patterns are left alone.
    edit::setPatternLooping (p.project, p.clipId, true);
    CHECK (miditiming::usableClips (p.project, { p.clipId }).isEmpty());
    CHECK_FALSE (miditiming::findBeat (p.project, { p.clipId }).found);
}

TEST_CASE ("Making playing steady puts every beat on the grid, at the tempo played", "[timing][model]")
{
    Played p (12, speedingUp (84.0, 100.0, 48.0, 1.3), 0.0);
    const auto beat = miditiming::findBeat (p.project, { p.clipId });
    REQUIRE (beat.found);

    const auto before = p.positionsOf (60);
    const auto tempo = std::round (beat.bpm);

    miditiming::SteadyOptions options;
    options.songBpm = tempo;
    CHECK (miditiming::steadyStartBar (p.project, { p.clipId }, beat, options) == 0);
    REQUIRE (edit::makeTimingSteady (p.project, { p.clipId }, beat, options));

    // One steady tempo for the whole song.
    REQUIRE (timeline::getNumTempoChanges (p.project) == 1);
    CHECK (p.tempo().tempoAt (0) == Catch::Approx (tempo));

    // Each chord on its beat, from bar 1; each melody note half way through its beat.
    const auto chords = p.positionsOf (60);
    REQUIRE (chords.size() == 48);
    CHECK (chords.front() == 0);
    CHECK (worstOffGrid (chords, 0, q) <= 2);

    const auto melody = p.positionsOf (72);
    REQUIRE (melody.size() == 48);
    CHECK (worstOffGrid (melody, q / 2, q) < q / 50);

    // The bass notes on the bars.
    const auto bass = p.positionsOf (40);
    CHECK (worstOffGrid (bass, 0, bar) <= 2);

    // One undo step puts it all back.
    CHECK (p.project.getUndoManager().getUndoDescription() == "Make timing steady");
    p.project.getUndoManager().undo();
    CHECK (p.positionsOf (60) == before);
    CHECK (p.tempo().tempoAt (0) == Catch::Approx (120.0));
}

TEST_CASE ("Making playing steady at the song's own tempo", "[timing][model]")
{
    Played p (8, steady (90.0, 0.8), 0.0);
    const auto beat = miditiming::findBeat (p.project, { p.clipId });
    REQUIRE (beat.found);

    REQUIRE (edit::makeTimingSteady (p.project, { p.clipId }, beat, {}));

    // The song stays at 120; the notes now play on its beats (faster than they were played).
    CHECK (p.tempo().tempoAt (0) == Catch::Approx (120.0));
    const auto chords = p.positionsOf (64);
    REQUIRE (chords.size() == 32);
    CHECK (worstOffGrid (chords, chords.front(), q) <= 2);
    CHECK (chords.front() % bar == 0);
}

TEST_CASE ("Less strength keeps some of the player's push and pull", "[timing][model]")
{
    const auto offGrid = [] (double strength)
    {
        Played p (12, steady (96.0, 1.0), 0.03);
        const auto beat = miditiming::findBeat (p.project, { p.clipId });
        REQUIRE (beat.found);

        miditiming::SteadyOptions options;
        options.strength = strength;
        options.songBpm = 96.0;
        REQUIRE (edit::makeTimingSteady (p.project, { p.clipId }, beat, options));

        // How far the chords are from their beats, on average (from the bar the first is in:
        // played early, it may have needed the next bar to stay after the song's start).
        const auto chords = p.positionsOf (67);
        const auto first = (chords.front() + bar / 2) / bar * bar;
        double total = 0.0;
        for (size_t i = 0; i < chords.size(); ++i)
            total += std::abs ((double) (chords[i] - first - (Tick) i * q));

        return total / (double) chords.size() / (double) q;
    };

    const auto full = offGrid (1.0), half = offGrid (0.5), none = offGrid (0.0);
    CHECK (full < 0.001);
    CHECK (none > 0.012);   // the beats were played up to 30 ms early or late (half a beat is 0.31 s at 96 BPM)
    CHECK (half == Catch::Approx (none / 2.0).epsilon (0.15));
}

TEST_CASE ("A pick-up before bar 1 gets room before the bar line", "[timing][model]")
{
    // One melody note a beat before the first chord, played half a second into the recording.
    Played p (10, steady (100.0, 1.1), 0.0, 1);
    auto beat = miditiming::findBeat (p.project, { p.clipId });
    REQUIRE (beat.found);

    // Wherever the downbeat was guessed, bar 1 of the playing is the first chord.
    const auto first = std::find_if (beat.beats.begin(), beat.beats.end(), [] (double t) { return std::abs (t - 1.1) < 0.02; });
    REQUIRE (first != beat.beats.end());
    beat.downbeat = (int) (first - beat.beats.begin());
    CHECK (beat.downbeat == 1);

    miditiming::SteadyOptions options;
    options.songBpm = 100.0;
    CHECK (miditiming::steadyStartBar (p.project, { p.clipId }, beat, options) == 1);
    REQUIRE (edit::makeTimingSteady (p.project, { p.clipId }, beat, options));

    const auto chords = p.positionsOf (60);
    CHECK (chords.front() == bar);
    CHECK (worstOffGrid (chords, bar, q) <= 2);

    const auto pickup = p.positionsOf (74);
    REQUIRE (pickup.size() == 1);
    CHECK (std::abs (pickup.front() - (bar - q)) <= 2);
}

TEST_CASE ("Making the song's tempo follow the playing keeps every note's time", "[timing][model]")
{
    Played p (12, speedingUp (84.0, 100.0, 48.0, 1.3), 0.008);
    const auto beat = miditiming::findBeat (p.project, { p.clipId });
    REQUIRE (beat.found);

    const auto before = p.soundingTimes();
    CHECK (miditiming::followStartBar (p.project, { p.clipId }, beat) == 1);
    REQUIRE (edit::makeTempoFollow (p.project, { p.clipId }, beat, {}));
    CHECK (p.project.getUndoManager().getUndoDescription() == "Make the tempo follow the playing");

    // The notes sound exactly when they did (to within a millisecond).
    const auto after = p.soundingTimes();
    REQUIRE (after.size() == before.size());
    for (size_t i = 0; i < after.size(); ++i)
    {
        CHECK (after[i].first == Catch::Approx (before[i].first).margin (0.001));
        CHECK (after[i].second == Catch::Approx (before[i].second).margin (0.001));
    }

    // The bars now start where the playing's bars do: the bass notes are on bar lines (within
    // how unevenly the chords were played).
    const auto map = p.tempo();
    for (const auto b : p.positionsOf (40))
    {
        const auto position = map.ticksToBarBeat (b);
        const auto fromBar = position.beat == 0 ? position.tickInBeat : position.tickInBeat - map.beatLengthAt (b);
        CHECK (std::abs ((double) fromBar) < 0.04 * q);
    }

    // A tempo change per bar (plus the lead-in), following 84 up to 100 BPM.
    const auto changes = timeline::getNumTempoChanges (p.project);
    CHECK (changes >= 12);
    CHECK (changes <= 16);
    CHECK (map.tempoAt (map.barToTicks (1)) == Catch::Approx (84.0).margin (3.0));
    CHECK (map.tempoAt (map.barToTicks (12)) == Catch::Approx (100.0).margin (3.0));

    p.project.getUndoManager().undo();
    CHECK (timeline::getNumTempoChanges (p.project) == 1);
    CHECK (p.soundingTimes() == before);
}

TEST_CASE ("Playing that starts right after the start of the song follows from bar 1", "[timing][model]")
{
    // The first chord a fifth of a second in: too soon for a whole lead-in bar before it.
    Played p (8, speedingUp (96.0, 104.0, 32.0, 0.2), 0.0);
    const auto beat = miditiming::findBeat (p.project, { p.clipId });
    REQUIRE (beat.found);

    CHECK (miditiming::followStartBar (p.project, { p.clipId }, beat) == 0);
    const auto before = p.soundingTimes();
    REQUIRE (edit::makeTempoFollow (p.project, { p.clipId }, beat, {}));

    const auto after = p.soundingTimes();
    REQUIRE (after.size() == before.size());
    for (size_t i = 0; i < after.size(); ++i)
        CHECK (after[i].first == Catch::Approx (before[i].first).margin (0.001));

    // From the second bar on, the bass notes are exactly on bar lines (the first bar's first
    // beat took up the moment before the playing started).
    const auto map = p.tempo();
    const auto bass = p.positionsOf (40);
    REQUIRE (bass.size() == 8);
    for (size_t i = 1; i < bass.size(); ++i)
        CHECK (std::abs (bass[i] - map.barToTicks ((int) i)) < q / 100);
}

TEST_CASE ("The tempo can follow the playing beat by beat", "[timing][model]")
{
    Played p (8, speedingUp (88.0, 100.0, 32.0, 2.1), 0.01);
    const auto beat = miditiming::findBeat (p.project, { p.clipId });
    REQUIRE (beat.found);

    miditiming::FollowOptions options;
    options.everyBeat = true;
    REQUIRE (edit::makeTempoFollow (p.project, { p.clipId }, beat, options));
    CHECK (timeline::getNumTempoChanges (p.project) >= 32);

    // Every chord is on a beat.
    const auto map = p.tempo();
    for (const auto c : p.positionsOf (64))
    {
        const auto position = map.ticksToBarBeat (c);
        const auto offBeat = std::min (position.tickInBeat, map.beatLengthAt (c) - position.tickInBeat);
        CHECK (offBeat < q / 40);
    }
}

TEST_CASE ("Quantizing whole clips from the timeline uses the song's grid", "[timing][model]")
{
    Project project;
    const auto track = edit::addTrack (project, TrackKind::midi).getId();

    // A clip starting a third of a beat into the song, with notes a little off the 1/16 grid.
    const auto start = q / 3;
    const auto off = q / 20;
    const auto clipId = edit::addMidiClip (project, track, "Off grid", start, 2 * bar,
                                           { { 60, 100, q - start + off, q / 2 },          // just after beat 2
                                             { 62, 100, 2 * q - start - off, q / 2 },      // just before beat 3
                                             { 64, 100, 5 * q / 4 - start + off, q / 8 } });   // just after the 2nd sixteenth of beat 2
    const auto other = edit::addMidiClip (project, track, "Other", 2 * bar, bar, { { 48, 100, q + off, q } });

    const auto positions = [&] (const juce::String& id)
    {
        const auto clip = clips::findClip (project, id);
        std::vector<Tick> result;
        for (const auto& n : midi::audibleNotes (clip))
            result.push_back (clip.getStart() + n.start);
        std::sort (result.begin(), result.end());
        return result;
    };

    REQUIRE (edit::quantizeClips (project, { clipId, other }, GridSize::sixteenth, 1.0, 0.0, false));
    CHECK (project.getUndoManager().getUndoDescription() == "Quantize");
    CHECK (positions (clipId) == std::vector<Tick> { q, 5 * q / 4, 2 * q });
    CHECK (positions (other) == std::vector<Tick> { 2 * bar + q });

    // Half strength: half way there.
    project.getUndoManager().undo();
    REQUIRE (edit::quantizeClips (project, { clipId }, GridSize::sixteenth, 0.5, 0.0, false));
    CHECK (positions (clipId) == std::vector<Tick> { q + off / 2, 5 * q / 4 + off / 2, 2 * q - off / 2 });

    // Nothing to quantize.
    const auto empty = edit::addMidiClip (project, track, "Empty", 4 * bar, bar, {});
    CHECK_FALSE (edit::quantizeClips (project, { empty }, GridSize::sixteenth, 1.0, 0.0, false));
}

TEST_CASE ("Replacing the tempo is one undo step", "[timing][model]")
{
    Project project;
    edit::addTempoChange (project, 8 * q, 100.0);
    edit::replaceTempo (project, { { 4 * q, 90.0, TempoCurve::jump }, { 0, 95.5, TempoCurve::ramp } }, "Change it all");

    const auto map = timeline::buildTempoMap (project);
    REQUIRE (map.getTempoPoints().size() == 2);
    CHECK (map.getTempoPoints()[0].bpm == Catch::Approx (95.5));
    CHECK (map.getTempoPoints()[0].curve == TempoCurve::ramp);
    CHECK (map.getTempoPoints()[1].tick == 4 * q);
    CHECK (project.getUndoManager().getUndoDescription() == "Change it all");

    project.getUndoManager().undo();
    CHECK (timeline::getNumTempoChanges (project) == 2);
    CHECK (timeline::buildTempoMap (project).tempoAt (8 * q) == Catch::Approx (100.0));
}

//==============================================================================
// Each fix starts from how it was played (0.17.1)

namespace
{
    /** The song's tempo changes, as (tick, BPM). */
    std::vector<std::pair<Tick, double>> tempoPointsOf (const Project& project)
    {
        std::vector<std::pair<Tick, double>> result;
        const auto map = timeline::buildTempoMap (project);
        for (const auto& point : map.getTempoPoints())
            result.emplace_back (point.tick, point.bpm);
        return result;
    }

    void checkSameTimes (const std::vector<std::pair<double, double>>& a, const std::vector<std::pair<double, double>>& b,
                         double margin = 0.001)
    {
        REQUIRE (a.size() == b.size());
        for (size_t i = 0; i < a.size(); ++i)
        {
            CHECK (a[i].first == Catch::Approx (b[i].first).margin (margin));
            CHECK (a[i].second == Catch::Approx (b[i].second).margin (margin));
        }
    }

    miditiming::SteadyOptions atBpm (double bpm)
    {
        miditiming::SteadyOptions options;
        options.songBpm = bpm;
        return options;
    }
}

TEST_CASE ("A second timing fix starts from how the clip was played, not from the first fix", "[timing][model]")
{
    // What following the playing gives, done straight away...
    Played reference (12, speedingUp (84.0, 100.0, 48.0, 1.3), 0.008);
    const auto referenceBeat = miditiming::findBeat (reference.project, { reference.clipId });
    REQUIRE (edit::makeTempoFollow (reference.project, { reference.clipId }, referenceBeat, {}));

    // ...is what it gives after making it steady at 90 first.
    Played p (12, speedingUp (84.0, 100.0, 48.0, 1.3), 0.008);
    const auto asPlayed = p.soundingTimes();
    const auto first = miditiming::findBeat (p.project, { p.clipId });
    REQUIRE (edit::makeTimingSteady (p.project, { p.clipId }, first, atBpm (90.0)));
    CHECK (miditiming::remembersPlaying (p.project, { p.clipId }));
    CHECK (miditiming::wasFixed (p.project, { p.clipId }));

    // Looking again finds the beat as played (speeding up), not the steady 90.
    const auto again = miditiming::findBeat (p.project, { p.clipId });
    REQUIRE (again.found);
    CHECK (again.bpm == Catch::Approx (first.bpm).margin (0.01));
    CHECK (again.fastestBpm - again.slowestBpm > 10.0);
    CHECK (miditiming::playedNotes (p.project, { p.clipId }).size() == miditiming::playedNotes (reference.project, { reference.clipId }).size());

    REQUIRE (edit::makeTempoFollow (p.project, { p.clipId }, again, {}));

    // The same tempo changes, and the notes sound when they were played.
    const auto expected = tempoPointsOf (reference.project), got = tempoPointsOf (p.project);
    REQUIRE (got.size() == expected.size());
    for (size_t i = 0; i < got.size(); ++i)
    {
        CHECK (std::abs (got[i].first - expected[i].first) <= 1);
        CHECK (got[i].second == Catch::Approx (expected[i].second).margin (0.002));
    }

    checkSameTimes (p.soundingTimes(), asPlayed);

    // Each fix is one undo step: back to steady, then to as played.
    p.project.getUndoManager().undo();
    CHECK (tempoPointsOf (p.project) == std::vector<std::pair<Tick, double>> { { 0, 90.0 } });
    p.project.getUndoManager().undo();
    CHECK (p.soundingTimes() == asPlayed);
    CHECK_FALSE (miditiming::remembersPlaying (p.project, { p.clipId }));
}

TEST_CASE ("After following the playing, making it steady at the song's tempo uses the tempo from before", "[timing][model]")
{
    Played p (8, steady (96.0, 0.9), 0.01);
    REQUIRE (edit::makeTempoFollow (p.project, { p.clipId }, miditiming::findBeat (p.project, { p.clipId }), {}));
    REQUIRE (timeline::getNumTempoChanges (p.project) > 2);

    // The window offers the song's tempo from before (120), not the tempo that followed.
    CHECK (miditiming::songTempoAt (p.project, { p.clipId }) == Catch::Approx (120.0));
    CHECK (miditiming::songTempoChanges (p.project, { p.clipId }) == 1);

    const auto beat = miditiming::findBeat (p.project, { p.clipId });
    REQUIRE (beat.found);
    CHECK (beat.bpm == Catch::Approx (96.0).margin (1.0));
    REQUIRE (edit::makeTimingSteady (p.project, { p.clipId }, beat, {}));

    // The song is back at 120 and the chords are on its beats.
    CHECK (tempoPointsOf (p.project) == std::vector<std::pair<Tick, double>> { { 0, 120.0 } });
    const auto chords = p.positionsOf (60);
    REQUIRE (chords.size() == 32);
    CHECK (worstOffGrid (chords, chords.front(), q) < q / 100);
}

TEST_CASE ("Putting the timing back as played undoes every fix in one step", "[timing][model]")
{
    Played p (8, speedingUp (88.0, 98.0, 32.0, 0.7), 0.01);
    const auto asPlayed = p.soundingTimes();
    const auto positions = p.positionsOf (60);

    CHECK_FALSE (miditiming::wasFixed (p.project, { p.clipId }));
    CHECK_FALSE (edit::putBackAsPlayed (p.project, { p.clipId }));

    REQUIRE (edit::makeTimingSteady (p.project, { p.clipId }, miditiming::findBeat (p.project, { p.clipId }), atBpm (90.0)));
    REQUIRE (edit::makeTempoFollow (p.project, { p.clipId }, miditiming::findBeat (p.project, { p.clipId }), {}));
    REQUIRE (edit::makeTimingSteady (p.project, { p.clipId }, miditiming::findBeat (p.project, { p.clipId }), {}));

    REQUIRE (edit::putBackAsPlayed (p.project, { p.clipId }));
    CHECK (p.project.getUndoManager().getUndoDescription() == "Put the timing back as played");

    // The song's tempo and every note are exactly as they were.
    CHECK (tempoPointsOf (p.project) == std::vector<std::pair<Tick, double>> { { 0, 120.0 } });
    checkSameTimes (p.soundingTimes(), asPlayed, 1.0e-5);
    const auto back = p.positionsOf (60);
    REQUIRE (back.size() == positions.size());
    for (size_t i = 0; i < back.size(); ++i)
        CHECK (std::abs (back[i] - positions[i]) <= 1);

    // Nothing more is remembered.
    CHECK_FALSE (miditiming::wasFixed (p.project, { p.clipId }));
    CHECK_FALSE (p.clip().getState().getChildWithName (ids::PlayedTiming).isValid());

    // One undo: back to the last fix.
    p.project.getUndoManager().undo();
    CHECK (miditiming::wasFixed (p.project, { p.clipId }));
}

TEST_CASE ("Notes moved by hand after a fix keep their place; a clip moved by hand is taken as it is", "[timing][model]")
{
    Played p (8, speedingUp (88.0, 98.0, 32.0, 0.7), 0.0);
    const auto asPlayed = p.soundingTimes();
    REQUIRE (edit::makeTimingSteady (p.project, { p.clipId }, miditiming::findBeat (p.project, { p.clipId }), atBpm (90.0)));

    // Move the 6th melody note (half way between beats 5 and 6) a sixteenth later, by hand.
    NoteNodes melody;
    for (const auto& n : notes::nodesOf (p.clip()))
        if ((int) n[ids::pitch] == 72)
            melody.push_back (n);

    std::sort (melody.begin(), melody.end(), [] (const auto& a, const auto& b) { return (juce::int64) a[ids::tick] < (juce::int64) b[ids::tick]; });
    REQUIRE (melody.size() == 32);
    edit::moveNotes (p.project, { melody[5] }, q / 4, 0);

    // Following the playing: the other notes are where they were played; the moved one sits
    // between beats 5 and 6 as played, later than it was played.
    REQUIRE (edit::makeTempoFollow (p.project, { p.clipId }, miditiming::findBeat (p.project, { p.clipId }), {}));
    const auto map = p.tempo();
    const auto clip = p.clip();
    std::vector<double> melodyTimes;
    for (const auto& n : midi::audibleNotes (clip))
        if (n.pitch == 72)
            melodyTimes.push_back (map.ticksToSeconds (clip.getStart() + n.start));

    std::sort (melodyTimes.begin(), melodyTimes.end());
    REQUIRE (melodyTimes.size() == 32);
    const auto wasPlayed = 0.5 * (p.beatTime (5) + p.beatTime (6));
    CHECK (melodyTimes[5] > wasPlayed + 0.05);
    CHECK (melodyTimes[5] < p.beatTime (6));
    CHECK (melodyTimes[4] == Catch::Approx (0.5 * (p.beatTime (4) + p.beatTime (5))).margin (0.001));
    CHECK (melodyTimes[6] == Catch::Approx (0.5 * (p.beatTime (6) + p.beatTime (7))).margin (0.001));

    // Moving the whole clip: it's taken as it is now (steady at the song's tempo after this).
    REQUIRE (edit::makeTimingSteady (p.project, { p.clipId }, miditiming::findBeat (p.project, { p.clipId }), atBpm (90.0)));
    REQUIRE (edit::setClipPosition (p.project, p.clipId, p.clip().getStart() + bar, p.track));
    CHECK_FALSE (miditiming::remembersPlaying (p.project, { p.clipId }));
    const auto moved = miditiming::findBeat (p.project, { p.clipId });
    REQUIRE (moved.found);
    CHECK (moved.fastestBpm - moved.slowestBpm < 1.0);
    CHECK (moved.bpm == Catch::Approx (90.0).margin (0.5));
}
