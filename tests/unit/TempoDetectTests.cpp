// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <functional>
#include <algorithm>
#include <random>

#include "time/TempoDetect.h"

using namespace aether;
using namespace aether::tempodetect;

namespace
{
    /** A made-up performance: `beatTime(b)` says when (fractional) beat b is played, in seconds.
        Each bar: a long bass note on beat 1, a chord on every beat, and a soft melody note
        halfway through each beat. Every note is a little early or late (up to `jitter`). */
    struct Performance
    {
        std::vector<PlayedNote> notes;
        std::vector<double> trueBeats;   // the beats actually meant
    };

    Performance play (int bars, std::function<double (double)> beatTime, double jitter, unsigned seed = 1,
                      std::function<bool (int bar, int beat)> playsBeat = {})
    {
        std::mt19937 random (seed);
        std::uniform_real_distribution<double> wobble (-jitter, jitter);
        Performance p;

        for (int bar = 0; bar < bars; ++bar)
        {
            for (int beat = 0; beat < 4; ++beat)
            {
                const auto b = bar * 4 + beat;
                const auto t = beatTime (b);
                const auto length = beatTime (b + 1) - t;
                p.trueBeats.push_back (t);

                if (playsBeat && ! playsBeat (bar, beat))
                    continue;

                if (beat == 0)
                    p.notes.push_back ({ t + wobble (random), 3.5 * length, 40, 100 });

                for (const auto pitch : { 60, 64, 67 })
                    p.notes.push_back ({ t + wobble (random), 0.8 * length, pitch, beat == 0 ? 95 : 80 });

                p.notes.push_back ({ beatTime (b + 0.5) + wobble (random), 0.4 * length, 72 + (b % 3), 55 });
            }
        }

        p.trueBeats.push_back (beatTime (bars * 4));
        return p;
    }

    std::function<double (double)> steady (double bpm, double start = 1.0)
    {
        return [bpm, start] (double beat) { return start + beat * 60.0 / bpm; };
    }

    /** The tempo changes smoothly from `from` to `to` BPM over `beats` beats (then stays). */
    std::function<double (double)> changing (double from, double to, double beats, double start = 1.0)
    {
        return [=] (double beat)
        {
            // Integrate the beat length, which changes linearly with the beat number.
            const auto b = std::min (beat, beats);
            const auto slope = (60.0 / to - 60.0 / from) / beats;
            auto t = start + 60.0 / from * b + 0.5 * slope * b * b;

            if (beat > beats)
                t += (beat - beats) * 60.0 / to;

            return t;
        };
    }

    /** The largest distance from each meant beat to the nearest beat found. */
    double worstMiss (const Result& r, const std::vector<double>& meant)
    {
        double worst = 0.0;

        for (const auto t : meant)
        {
            double nearest = 1.0e9;
            for (const auto b : r.beats)
                nearest = std::min (nearest, std::abs (b - t));

            worst = std::max (worst, nearest);
        }

        return worst;
    }

    /** How many beats were found between the first and last meant beats (inclusive, give or take
        30 ms): one per meant beat if the tempo was found at the right speed. */
    int beatsWithin (const Result& r, const Performance& p)
    {
        int count = 0;
        for (const auto b : r.beats)
            if (b >= p.trueBeats.front() - 0.03 && b <= p.trueBeats[p.trueBeats.size() - 2] + 0.03)
                ++count;

        return count;
    }
}

TEST_CASE ("The beat of a steady performance is found, on the beats as played", "[tempodetect]")
{
    const auto p = play (16, steady (100.0), 0.012);
    const auto r = analyse (p.notes);

    REQUIRE (r.found);
    CHECK (r.bpm == Catch::Approx (100.0).epsilon (0.02));
    CHECK (beatsWithin (r, p) == 64);
    CHECK (worstMiss (r, { p.trueBeats.begin(), p.trueBeats.end() - 1 }) < 0.025);
    CHECK (r.downbeat == 0);
    CHECK (r.clarity > 0.8);
    CHECK (r.slowestBpm > 95.0);
    CHECK (r.fastestBpm < 105.0);

    // The beats cover every note.
    CHECK (r.beats.front() <= p.notes.front().seconds + 0.02);
    CHECK (r.beats.back() >= p.notes.back().seconds - 0.05);
}

TEST_CASE ("The beat follows a player who speeds up", "[tempodetect]")
{
    const auto p = play (24, changing (80.0, 104.0, 80.0), 0.012, 7);
    const auto r = analyse (p.notes);

    REQUIRE (r.found);
    CHECK (beatsWithin (r, p) == 96);
    CHECK (worstMiss (r, { p.trueBeats.begin(), p.trueBeats.end() - 1 }) < 0.03);
    CHECK (r.slowestBpm < 86.0);
    CHECK (r.fastestBpm > 98.0);
    CHECK (r.bpm > 85.0);
    CHECK (r.bpm < 102.0);
}

TEST_CASE ("The beat follows a player whose tempo wanders", "[tempodetect]")
{
    // From 90 BPM, slowing to about 81, then up to about 102 and back.
    const auto wave = [] (double beat) { return 1.0 + 60.0 / 90.0 * beat + 0.35 * (1.0 - std::cos (beat / 64.0 * 2.0 * 3.14159265)) * beat / 64.0 * 4.0; };
    const auto p = play (16, wave, 0.01, 3);
    const auto r = analyse (p.notes);

    REQUIRE (r.found);
    CHECK (beatsWithin (r, p) == 64);
    CHECK (worstMiss (r, { p.trueBeats.begin(), p.trueBeats.end() - 1 }) < 0.03);
}

TEST_CASE ("Beats carry on evenly through held chords and rests", "[tempodetect]")
{
    // Every other bar: only the first beat is played (a chord held for the whole bar).
    const auto p = play (16, steady (96.0), 0.01, 5, [] (int bar, int beat) { return bar % 2 == 0 || beat == 0 || bar == 15; });
    const auto r = analyse (p.notes);

    REQUIRE (r.found);
    CHECK (beatsWithin (r, p) == 64);
    CHECK (worstMiss (r, { p.trueBeats.begin(), p.trueBeats.end() - 1 }) < 0.03);
}

TEST_CASE ("The beat follows rubato: the tempo changing a little every bar", "[tempodetect]")
{
    // The tempo moves by about 6% at random every bar, between 70 and 110 BPM.
    for (unsigned seed = 1; seed <= 5; ++seed)
    {
        std::mt19937 random (seed);
        std::normal_distribution<double> change (0.0, 0.06);
        std::vector<double> times { 1.0 };
        auto tempo = 88.0;

        for (int b = 0; b < 72; ++b)
        {
            if (b % 4 == 0)
                tempo = std::clamp (tempo * (1.0 + change (random)), 70.0, 110.0);

            times.push_back (times.back() + 60.0 / tempo);
        }

        const auto beatTime = [times] (double b)
        {
            const auto i = std::clamp ((int) std::floor (b), 0, (int) times.size() - 2);
            return times[(size_t) i] + (b - i) * (times[(size_t) i + 1] - times[(size_t) i]);
        };

        const auto p = play (16, beatTime, 0.015, seed);
        const auto r = analyse (p.notes);

        INFO ("seed " << seed);
        REQUIRE (r.found);
        CHECK (beatsWithin (r, p) == 64);
        CHECK (worstMiss (r, { p.trueBeats.begin(), p.trueBeats.end() - 1 }) < 0.045);
    }
}

TEST_CASE ("The beat of a melody with mixed rhythms is found", "[tempodetect]")
{
    // Single notes (no chords): quarters, eighths, dotted notes, halves; on-beat notes a little louder.
    const double rhythm[] = { 1, 0.5, 0.5, 1, 1.5, 0.5, 2, 1, 1, 0.5, 0.5, 0.5, 0.5, 1, 1, 2 };
    std::vector<PlayedNote> notes;
    std::vector<double> meant;
    std::mt19937 random (2);
    std::normal_distribution<double> wobble (0.0, 0.012);
    double beat = 0.0;

    for (int i = 0; beat < 48.0; ++i)
    {
        const auto length = rhythm[i % 16];
        notes.push_back ({ 1.0 + beat * 60.0 / 110.0 + wobble (random), 0.9 * length * 60.0 / 110.0, 60 + (i * 5) % 12,
                           beat == std::floor (beat) ? 90 : 70 });
        beat += length;
    }

    for (int b = 0; b < 48; ++b)
        meant.push_back (1.0 + b * 60.0 / 110.0);

    const auto r = analyse (notes);
    REQUIRE (r.found);
    CHECK (r.bpm == Catch::Approx (110.0).epsilon (0.02));
    CHECK (worstMiss (r, meant) < 0.045);
}

TEST_CASE ("A tempo hint finds the beat at that speed", "[tempodetect]")
{
    const auto p = play (12, steady (100.0), 0.01);

    Options twice;
    twice.hintBpm = 200.0;
    const auto fast = analyse (p.notes, twice);
    REQUIRE (fast.found);
    CHECK (fast.bpm == Catch::Approx (200.0).epsilon (0.03));
    CHECK (fast.beats.size() > 90);

    Options half;
    half.hintBpm = 50.0;
    const auto slow = analyse (p.notes, half);
    REQUIRE (slow.found);
    CHECK (slow.bpm == Catch::Approx (50.0).epsilon (0.03));
    CHECK (slow.beats.size() < 30);

    // Half speed still lands on the bars' first beats (where the bass is).
    const auto onABar = [&] (double t)
    {
        for (size_t bar = 0; bar < p.trueBeats.size(); bar += 4)
            if (std::abs (p.trueBeats[bar] - t) < 0.03)
                return true;
        return false;
    };
    CHECK (onABar (slow.beats[(size_t) slow.downbeat]));
}

TEST_CASE ("A pick-up before the first bar is noticed", "[tempodetect]")
{
    // One soft melody note a beat before bar 1, then bars with the bass on their first beat.
    auto p = play (12, steady (100.0, 2.0), 0.008, 11);
    p.notes.push_back ({ 2.0 - 0.6, 0.5, 72, 60 });

    const auto r = analyse (p.notes);
    REQUIRE (r.found);
    CHECK (r.beats.front() <= 1.4 + 0.03);
    CHECK (r.downbeat >= 1);
    CHECK (std::abs (r.beats[(size_t) r.downbeat] - 2.0) < 0.03);
}

TEST_CASE ("A two-beat pick-up: bars start on the chords", "[tempodetect]")
{
    // Two melody notes, then bars of a chord (with a bass note) followed by three melody notes.
    std::vector<PlayedNote> notes;
    const auto at = [] (double beat) { return 1.0 + beat * 60.0 / 96.0; };
    notes.push_back ({ at (0), 0.5, 67, 70 });
    notes.push_back ({ at (1), 0.5, 69, 70 });

    for (int b = 2; b < 50; ++b)
    {
        if ((b - 2) % 4 == 0)
        {
            notes.push_back ({ at (b) + 0.004, 2.2, 38, 105 });
            for (const auto pitch : { 62, 66, 69 })
                notes.push_back ({ at (b) - 0.006, 1.1, pitch, 90 });
        }
        else
        {
            notes.push_back ({ at (b) + (b % 3 - 1) * 0.008, 0.55, 74 + b % 3, 70 });
        }
    }

    const auto r = analyse (notes);
    REQUIRE (r.found);
    CHECK (r.bpm == Catch::Approx (96.0).epsilon (0.02));
    CHECK (r.downbeat == 2);
    CHECK (std::abs (r.beats[(size_t) r.downbeat] - at (2)) < 0.02);
}

TEST_CASE ("Too few notes to find a beat", "[tempodetect]")
{
    const std::vector<PlayedNote> few { { 0.0, 0.5, 60, 100 }, { 0.5, 0.5, 62, 100 }, { 1.0, 0.5, 64, 100 }, { 1.5, 0.5, 65, 100 } };
    CHECK_FALSE (analyse (few).found);
    CHECK_FALSE (analyse ({}).found);
}

TEST_CASE ("Smoothed beats keep slow tempo changes and remove unevenness", "[tempodetect]")
{
    const auto p = play (16, changing (90.0, 110.0, 64.0), 0.02, 9);
    const auto r = analyse (p.notes);

    REQUIRE (r.found);
    REQUIRE (r.smoothBeats.size() == r.beats.size());

    // The smoothed beats are close to the meant ones (closer than the played unevenness).
    double worst = 0.0;
    for (size_t k = 0; k + 1 < r.smoothBeats.size(); ++k)
        CHECK (r.smoothBeats[k + 1] > r.smoothBeats[k]);

    for (size_t k = 2; k + 2 < r.beats.size(); ++k)
    {
        double nearest = 1.0e9;
        for (const auto t : p.trueBeats)
            nearest = std::min (nearest, std::abs (r.smoothBeats[k] - t));
        worst = std::max (worst, nearest);
    }

    CHECK (worst < 0.025);
}

TEST_CASE ("Beat positions count beats, carrying on past the ends", "[tempodetect]")
{
    const std::vector<double> beats { 1.0, 1.5, 2.0, 3.0 };
    CHECK (beatPosition (beats, 1.0) == Catch::Approx (0.0));
    CHECK (beatPosition (beats, 1.25) == Catch::Approx (0.5));
    CHECK (beatPosition (beats, 2.5) == Catch::Approx (2.5));
    CHECK (beatPosition (beats, 0.5) == Catch::Approx (-1.0));
    CHECK (beatPosition (beats, 4.0) == Catch::Approx (4.0));
}
