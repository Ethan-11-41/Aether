// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include <cmath>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "time/TempoMap.h"

using namespace aether;
using Catch::Matchers::WithinAbs;

namespace
{
    constexpr Tick q = ticksPerQuarter;   // one quarter note

    /** ticks -> seconds -> ticks must come back within one tick. */
    void checkRoundTrip (const TempoMap& map, Tick from, Tick to, Tick step)
    {
        for (Tick t = from; t <= to; t += step)
        {
            const auto back = map.secondsToTicks (map.ticksToSeconds (t));
            INFO ("tick " << t);
            REQUIRE (std::abs (back - t) <= 1);
        }
    }
}

TEST_CASE ("The default tempo map is 120 BPM in 4/4", "[time][tempo]")
{
    TempoMap map;
    CHECK (map.getTempoPoints().size() == 1);
    CHECK (map.tempoAt (0) == 120.0);
    CHECK_THAT (map.ticksToSeconds (q), WithinAbs (0.5, 1e-12));        // one beat at 120 BPM
    CHECK_THAT (map.ticksToSeconds (4 * q), WithinAbs (2.0, 1e-12));    // one bar
    CHECK (map.secondsToTicks (0.5) == q);
    CHECK (map.secondsToTicks (60.0) == 120 * q);
    CHECK (map.barToTicks (1) == 4 * q);
    CHECK (map.barLengthAt (0) == 4 * q);
    CHECK (map.beatLengthAt (0) == q);
}

TEST_CASE ("Tick resolution divides exactly into every grid size", "[time]")
{
    // Whole, half, quarter, 8th, 16th, 32nd, 64th notes, and their triplets and quintuplets.
    for (const Tick division : { 1, 2, 4, 8, 16, 32, 64 })
    {
        CHECK ((4 * q) % division == 0);
        CHECK ((4 * q) % (division * 3) == 0);
        CHECK ((4 * q) % (division * 5) == 0);
    }
}

TEST_CASE ("A tempo jump changes the speed instantly", "[time][tempo]")
{
    // 120 BPM for one bar, then 60 BPM.
    TempoMap map ({ { 0, 120.0, TempoCurve::jump }, { 4 * q, 60.0, TempoCurve::jump } }, {});

    CHECK_THAT (map.ticksToSeconds (4 * q), WithinAbs (2.0, 1e-12));
    CHECK_THAT (map.ticksToSeconds (5 * q), WithinAbs (3.0, 1e-12));     // one beat at 60 BPM = 1 s
    CHECK_THAT (map.ticksToSeconds (8 * q), WithinAbs (6.0, 1e-12));
    CHECK (map.secondsToTicks (3.0) == 5 * q);
    CHECK (map.tempoAt (4 * q - 1) == 120.0);
    CHECK (map.tempoAt (4 * q) == 60.0);
    CHECK (map.tempoPointIndexAt (4 * q + 10) == 1);
    checkRoundTrip (map, 0, 16 * q, q / 7);
}

TEST_CASE ("A tempo ramp changes the speed gradually, with exact timing", "[time][tempo]")
{
    // Speeding up from 60 to 120 BPM over 4 beats, then steady at 120.
    TempoMap map ({ { 0, 60.0, TempoCurve::ramp }, { 4 * q, 120.0, TempoCurve::jump } }, {});

    // Time taken = 60 * beats / (b1 - b0) * ln(b1 / b0) = 4 * ln 2 seconds.
    CHECK_THAT (map.ticksToSeconds (4 * q), WithinAbs (4.0 * std::log (2.0), 1e-9));
    CHECK_THAT (map.tempoAt (2 * q), WithinAbs (90.0, 1e-9));           // half way
    CHECK_THAT (map.tempoAt (4 * q), WithinAbs (120.0, 1e-9));

    // Each beat is shorter than the one before.
    double previous = 10.0;
    for (int beat = 0; beat < 4; ++beat)
    {
        const auto length = map.ticksToSeconds ((beat + 1) * q) - map.ticksToSeconds (beat * q);
        CHECK (length < previous);
        previous = length;
    }

    // After the ramp: 120 BPM.
    CHECK_THAT (map.ticksToSeconds (5 * q) - map.ticksToSeconds (4 * q), WithinAbs (0.5, 1e-9));
    checkRoundTrip (map, 0, 12 * q, q / 13);
}

TEST_CASE ("Slowing-down ramps and chains of changes are exact", "[time][tempo]")
{
    TempoMap map ({ { 0, 140.0, TempoCurve::ramp },
                    { 8 * q, 70.0, TempoCurve::jump },
                    { 12 * q, 100.0, TempoCurve::ramp },   // a "ramp" to the same tempo is just steady
                    { 20 * q, 100.0, TempoCurve::ramp },
                    { 24 * q, 180.0, TempoCurve::jump } }, {});

    CHECK_THAT (map.ticksToSeconds (8 * q), WithinAbs (60.0 * 8.0 / (70.0 - 140.0) * std::log (70.0 / 140.0), 1e-9));
    CHECK_THAT (map.ticksToSeconds (20 * q) - map.ticksToSeconds (12 * q), WithinAbs (8 * 0.6, 1e-9));   // steady 100
    CHECK_THAT (map.ticksToSeconds (24 * q) - map.ticksToSeconds (20 * q),
                WithinAbs (60.0 * 4.0 / (180.0 - 100.0) * std::log (180.0 / 100.0), 1e-9));
    checkRoundTrip (map, 0, 40 * q, q / 3 + 17);
}

TEST_CASE ("Timing stays exact over long songs", "[time][tempo]")
{
    // Two hours at a tempo with a long ramp in the middle.
    TempoMap map ({ { 0, 97.3, TempoCurve::jump }, { 4000 * q, 97.3, TempoCurve::ramp }, { 8000 * q, 151.9, TempoCurve::jump } }, {});
    checkRoundTrip (map, 0, 14000 * q, 997 * q / 3);

    const auto end = map.ticksToSeconds (14000 * q);
    CHECK (end > 3600.0);
    CHECK (std::abs (map.secondsToTicks (end) - 14000 * q) <= 1);
}

TEST_CASE ("Bad tempo data is cleaned up", "[time][tempo]")
{
    TempoMap map ({ { 8 * q, 90.0, TempoCurve::ramp },
                    { 4 * q, 5000.0, TempoCurve::jump },      // too fast
                    { 4 * q, 100.0, TempoCurve::jump },       // duplicate position: the last one wins
                    { -q, 80.0, TempoCurve::jump },          // before the start: moved to 0
                    { 6 * q, std::nan (""), TempoCurve::jump } }, {});

    const auto& points = map.getTempoPoints();
    REQUIRE (points.size() == 4);
    CHECK (points[0].tick == 0);
    CHECK (points[0].bpm == 80.0);
    CHECK (points[1] == TempoPoint { 4 * q, 100.0, TempoCurve::jump });
    CHECK (points[2].bpm == 120.0);                         // not a number: replaced
    CHECK (points[3].curve == TempoCurve::jump);            // the last point can't ramp

    TempoMap noStart ({ { 4 * q, 90.0, TempoCurve::jump } }, {});
    CHECK (noStart.getTempoPoints().front().tick == 0);
    CHECK (noStart.tempoAt (0) == 90.0);

    TempoMap tooSlow ({ { 0, 1.0, TempoCurve::jump } }, {});
    CHECK (tooSlow.tempoAt (0) == TempoMap::minBpm);
}

TEST_CASE ("Time signature changes move the bar lines", "[time][signature]")
{
    // 4/4 for bars 1-2, 3/4 for bars 3-4, then 6/8.
    TempoMap map ({}, { { 0, 4, 4 }, { 2, 3, 4 }, { 4, 6, 8 } });

    CHECK (map.barToTicks (0) == 0);
    CHECK (map.barToTicks (2) == 8 * q);
    CHECK (map.barToTicks (3) == 11 * q);
    CHECK (map.barToTicks (4) == 14 * q);
    CHECK (map.barToTicks (5) == 17 * q);          // a 6/8 bar is 3 quarter notes long
    CHECK (map.beatLengthAt (14 * q) == q / 2);    // an 8th-note beat

    CHECK (map.ticksToBarBeat (12 * q) == BarBeat { 3, 1, 0 });
    CHECK (map.ticksToBarBeat (14 * q + q / 2 + 7) == BarBeat { 4, 1, 7 });
    CHECK (map.signatureAt (13 * q).numerator == 3);

    for (Tick t = 0; t < 30 * q; t += q / 3 + 11)
        REQUIRE (map.barBeatToTicks (map.ticksToBarBeat (t)) == t);
}

TEST_CASE ("Invalid time signatures become 4/4", "[time][signature]")
{
    TempoMap map ({}, { { 0, 0, 4 }, { 1, 5, 3 }, { 2, 7, 8 } });
    const auto& sigs = map.getTimeSignatures();
    REQUIRE (sigs.size() == 3);
    CHECK (sigs[0] == TimeSignature { 0, 4, 4 });
    CHECK (sigs[1] == TimeSignature { 1, 4, 4 });
    CHECK (sigs[2] == TimeSignature { 2, 7, 8 });

    TempoMap missingFirst ({}, { { 3, 3, 4 } });
    CHECK (missingFirst.getTimeSignatures().front() == TimeSignature { 0, 4, 4 });
    CHECK (missingFirst.barToTicks (3) == 12 * q);
}

TEST_CASE ("Beats are counted correctly, including across signature changes", "[time][signature]")
{
    TempoMap map ({}, { { 0, 4, 4 }, { 1, 3, 4 }, { 2, 6, 8 } });

    auto beat = map.firstBeatAtOrAfter (0);
    std::vector<Beat> beats;
    for (int i = 0; i < 4 + 3 + 6 + 1; ++i, beat = map.nextBeat (beat))
        beats.push_back (beat);

    CHECK (beats[0].tick == 0);
    CHECK (beats[0].beatInBar == 0);
    CHECK (beats[3].tick == 3 * q);
    CHECK (beats[3].beatInBar == 3);

    CHECK (beats[4].tick == 4 * q);     // bar 2 (3/4) starts
    CHECK (beats[4].bar == 1);
    CHECK (beats[4].beatInBar == 0);
    CHECK (beats[6].beatInBar == 2);

    CHECK (beats[7].tick == 7 * q);     // bar 3 (6/8) starts: beats every 8th note
    CHECK (beats[7].beatInBar == 0);
    CHECK (beats[8].tick == 7 * q + q / 2);
    CHECK (beats[12].beatInBar == 5);
    CHECK (beats[13].tick == 10 * q);   // next 6/8 bar
    CHECK (beats[13].beatInBar == 0);
    CHECK (beats[13].bar == 3);

    // Starting part-way through a beat gives the next one.
    CHECK (map.firstBeatAtOrAfter (q + 1).tick == 2 * q);
    CHECK (map.firstBeatAtOrAfter (2 * q).tick == 2 * q);
}

TEST_CASE ("Positions are formatted for the transport display", "[time]")
{
    CHECK (formatBarBeat ({ 0, 0, 0 }) == "1.1.1");
    CHECK (formatBarBeat ({ 2, 3, q / 2 }) == "3.4.3");
    CHECK (formatTime (0.0) == "0:00.000");
    CHECK (formatTime (65.25) == "1:05.250");
    CHECK (formatTime (3600.0) == "60:00.000");
    CHECK (formatTime (-1.5) == "-0:01.500");
}

TEST_CASE ("Positions before the start extend the first tempo backwards", "[time]")
{
    TempoMap map;
    CHECK_THAT (map.ticksToSeconds (-q), WithinAbs (-0.5, 1e-12));
    CHECK (map.secondsToTicks (-1.0) == -2 * q);
    CHECK (map.ticksToBarBeat (-q) == BarBeat { -1, 3, 0 });
}
