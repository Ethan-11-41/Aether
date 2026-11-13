// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
//
// Timing for recordings (Milestone 17): the beat of an audio clip is found from its hits; making
// it steady or making the song follow it works through warp markers.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <functional>

#include "model/ClipEdits.h"
#include "model/EditCommands.h"
#include "model/MidiTiming.h"
#include "model/TimelineEdits.h"
#include "model/Warp.h"

using namespace aether;

namespace
{
    constexpr Tick q = ticksPerQuarter;
    constexpr double rate = 48000.0;

    /** A recording (at the start of a 120 BPM song) of someone playing freely: a hit on every
        beat at `beatTime(b)` seconds (a low one at the start of each bar). */
    struct Recorded
    {
        Recorded (int bars, std::function<double (double)> beatTimeToUse) : beatTime (std::move (beatTimeToUse))
        {
            track = edit::addTrack (project, TrackKind::audio).getId();

            for (int b = 0; b < bars * 4; ++b)
            {
                const auto t = beatTime (b);
                times.push_back (t);
                hitList.push_back ({ (juce::int64) std::llround (t * rate), b % 4 == 0 ? 1.0f : 0.7f, b % 4 == 0 ? 0.9f : 0.1f });
            }

            const auto length = (juce::int64) std::llround ((beatTime (bars * 4) + 0.5) * rate);
            clipId = edit::addAudioClip (project, track, { "Audio/take.wav", "Take 1", rate, length }, 0);
            hits = [this] (const Clip&) { return hitList; };
        }

        Clip clip() const          { return clips::findClip (project, clipId); }
        TempoMap tempo() const     { return timeline::buildTempoMap (project); }

        /** When each hit plays now (seconds), through the clip's warp. */
        std::vector<double> hitSeconds() const
        {
            const auto c = clip();
            const auto map = tempo();
            const warp::ClipTiming timing (c);
            std::vector<double> result;
            for (const auto& h : hitList)
                result.push_back (map.ticksToSeconds ((Tick) std::llround (timing.tickAt (map, (double) (h.sample - c.getOffset())))));
            return result;
        }

        /** Where each hit is on the timeline now (ticks). */
        std::vector<Tick> hitTicks() const
        {
            const auto c = clip();
            const warp::ClipTiming timing (c);
            std::vector<Tick> result;
            for (const auto& h : hitList)
                result.push_back ((Tick) std::llround (timing.tickAt (tempo(), (double) (h.sample - c.getOffset()))));
            return result;
        }

        Project project;
        juce::String track, clipId;
        std::function<double (double)> beatTime;
        std::vector<double> times;
        std::vector<audioonsets::Hit> hitList;
        miditiming::AudioHits hits;
    };

    std::function<double (double)> speedingUp (double from, double to, double beats, double start)
    {
        return [=] (double beat)
        {
            const auto b = std::clamp (beat, 0.0, beats);
            const auto slope = (60.0 / to - 60.0 / from) / beats;
            auto t = start + 60.0 / from * b + 0.5 * slope * b * b;
            if (beat > beats) t += (beat - beats) * 60.0 / to;
            return t;
        };
    }

    miditiming::SteadyOptions atBpm (double bpm)
    {
        miditiming::SteadyOptions options;
        options.songBpm = bpm;
        return options;
    }
}

TEST_CASE ("The beat of a recording is found from its hits", "[timing][audio]")
{
    Recorded r (8, speedingUp (84.0, 100.0, 32.0, 0.6));
    CHECK (miditiming::usableClips (r.project, { r.clipId }).size() == 1);

    // Without its hits (audio not loaded), nothing to go on.
    CHECK_FALSE (miditiming::findBeat (r.project, { r.clipId }).found);

    const auto beat = miditiming::findBeat (r.project, { r.clipId }, 0.0, r.hits);
    REQUIRE (beat.found);
    CHECK (beat.slowestBpm == Catch::Approx (84.0).margin (4.0));
    CHECK (beat.fastestBpm == Catch::Approx (100.0).margin (4.0));
    CHECK (beat.downbeat == 0);

    // A looping clip is left alone.
    edit::setClipLooping (r.project, r.clipId, true);
    CHECK (miditiming::usableClips (r.project, { r.clipId }).isEmpty());
}

TEST_CASE ("Making a recording steady stretches every beat onto the grid", "[timing][audio]")
{
    Recorded r (8, speedingUp (84.0, 100.0, 32.0, 0.6));
    const auto beat = miditiming::findBeat (r.project, { r.clipId }, 0.0, r.hits);
    REQUIRE (edit::makeTimingSteady (r.project, { r.clipId }, beat, atBpm (90.0), r.hits));
    CHECK (r.project.getUndoManager().getUndoDescription() == "Make timing steady");

    // The song is at 90 and the recording follows it, each hit on its beat.
    CHECK (r.tempo().getTempoPoints().size() == 1);
    CHECK (r.tempo().tempoAt (0) == Catch::Approx (90.0));
    CHECK (warp::followsTempo (r.clip()));
    CHECK (warp::markersOf (r.clip()).size() >= 30);

    const auto ticks = r.hitTicks();
    const auto first = ticks.front();
    CHECK (first % (4 * q) == 0);   // bar 1 of the playing on a bar line
    for (size_t k = 0; k < ticks.size(); ++k)
    {
        INFO ("beat " << k);
        CHECK (std::abs (ticks[k] - (first + (Tick) k * q)) < q / 100);
    }

    // Opened again, it finds the beat as played, not the steady one.
    const auto again = miditiming::findBeat (r.project, { r.clipId }, 0.0, r.hits);
    CHECK (again.bpm == Catch::Approx (beat.bpm).margin (0.01));
    CHECK (miditiming::wasFixed (r.project, { r.clipId }, r.hits));
}

TEST_CASE ("The song's tempo can follow a recording, which keeps its time", "[timing][audio]")
{
    Recorded r (8, speedingUp (84.0, 100.0, 32.0, 0.6));
    const auto beat = miditiming::findBeat (r.project, { r.clipId }, 0.0, r.hits);
    REQUIRE (edit::makeTempoFollow (r.project, { r.clipId }, beat, {}, r.hits));
    CHECK (timeline::getNumTempoChanges (r.project) >= 8);

    // Every hit sounds when it was played, and the recording isn't stretched at all.
    const auto seconds = r.hitSeconds();
    for (size_t k = 0; k < seconds.size(); ++k)
        CHECK (seconds[k] == Catch::Approx (r.times[k]).margin (0.001));

    CHECK_FALSE (warp::needsStretching (r.clip(), r.tempo()));
    CHECK (warp::followsTempo (r.clip()));

    // The bars follow it: the low hits are on bar lines.
    const auto ticks = r.hitTicks();
    for (size_t k = 0; k < ticks.size(); k += 4)
    {
        const auto position = r.tempo().ticksToBarBeat (ticks[k]);
        const auto fromBar = position.beat == 0 ? position.tickInBeat : position.tickInBeat - r.tempo().beatLengthAt (ticks[k]);
        CHECK (std::abs ((double) fromBar) < 0.03 * q);
    }
}

TEST_CASE ("Fixing a recording again starts from how it was played; it can be put back", "[timing][audio]")
{
    // Following straight away...
    Recorded reference (8, speedingUp (84.0, 100.0, 32.0, 0.6));
    REQUIRE (edit::makeTempoFollow (reference.project, { reference.clipId },
                                    miditiming::findBeat (reference.project, { reference.clipId }, 0.0, reference.hits), {}, reference.hits));

    // ...is what following gives after making it steady at 90 first.
    Recorded r (8, speedingUp (84.0, 100.0, 32.0, 0.6));
    REQUIRE (edit::makeTimingSteady (r.project, { r.clipId }, miditiming::findBeat (r.project, { r.clipId }, 0.0, r.hits), atBpm (90.0), r.hits));
    REQUIRE (edit::makeTempoFollow (r.project, { r.clipId }, miditiming::findBeat (r.project, { r.clipId }, 0.0, r.hits), {}, r.hits));

    const auto referenceTempo = reference.tempo();
    const auto& expected = referenceTempo.getTempoPoints();
    const auto got = r.tempo();
    REQUIRE (got.getTempoPoints().size() == expected.size());
    for (size_t i = 0; i < expected.size(); ++i)
    {
        CHECK (std::abs (got.getTempoPoints()[i].tick - expected[i].tick) <= 1);
        CHECK (got.getTempoPoints()[i].bpm == Catch::Approx (expected[i].bpm).margin (0.002));
    }

    const auto seconds = r.hitSeconds();
    for (size_t k = 0; k < seconds.size(); ++k)
        CHECK (seconds[k] == Catch::Approx (r.times[k]).margin (0.001));

    // Put back: at the start, at its own speed, the song at 120 again.
    REQUIRE (edit::putBackAsPlayed (r.project, { r.clipId }, r.hits));
    CHECK (r.clip().getStart() == 0);
    CHECK_FALSE (warp::followsTempo (r.clip()));
    CHECK (warp::markersOf (r.clip()).empty());
    CHECK (r.tempo().getTempoPoints().size() == 1);
    CHECK (r.tempo().tempoAt (0) == Catch::Approx (120.0));
    CHECK_FALSE (miditiming::wasFixed (r.project, { r.clipId }, r.hits));
}

TEST_CASE ("Quantizing a recording moves its hits onto the grid", "[timing][audio]")
{
    // Played at 120 with the metronome, a little loosely: every 8th note, up to 15 ms early or late.
    Project project;
    const auto track = edit::addTrack (project, TrackKind::audio).getId();
    std::vector<audioonsets::Hit> hitList;
    juce::Random random (4);

    for (int k = 0; k < 32; ++k)
    {
        const auto t = 0.5 + 0.25 * k + (random.nextDouble() * 2.0 - 1.0) * 0.015;
        hitList.push_back ({ (juce::int64) std::llround (t * rate), 0.8f, 0.2f });
    }

    const auto clipId = edit::addAudioClip (project, track, { "Audio/loose.wav", "Loose", rate, 10 * 48000 }, 0);
    const miditiming::AudioHits hits = [&hitList] (const Clip&) { return hitList; };
    const auto endBefore = clips::findClip (project, clipId).getEnd (timeline::buildTempoMap (project));

    REQUIRE (edit::quantizeAudioClips (project, { clipId }, hits, GridSize::eighth, 1.0, 0.0));
    CHECK (project.getUndoManager().getUndoDescription() == "Quantize");

    const auto clip = clips::findClip (project, clipId);
    const auto tempo = timeline::buildTempoMap (project);
    const warp::ClipTiming timing (clip);
    for (size_t k = 0; k < hitList.size(); ++k)
    {
        const auto tick = (Tick) std::llround (timing.tickAt (tempo, (double) hitList[k].sample));
        INFO ("hit " << k);
        CHECK (std::abs (tick - (Tick) (q + (Tick) k * q / 2)) <= 2);
    }

    CHECK (clip.getStart() == 0);
    CHECK (clip.getEnd (tempo) == endBefore);
    CHECK (warp::needsStretching (clip, tempo));
}
