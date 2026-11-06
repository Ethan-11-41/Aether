// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include <catch2/catch_test_macros.hpp>

#include "time/Grid.h"

using namespace aether;

namespace
{
    constexpr Tick q = ticksPerQuarter;
}

TEST_CASE ("Every grid size snaps to the right positions in 4/4", "[time][grid]")
{
    TempoMap map;
    const auto pos = 5 * q + q / 3 + 1000;   // bar 2, a bit after a triplet line

    CHECK (snapToGrid (map, pos, GridSize::bar) == 4 * q);
    CHECK (snapToGrid (map, pos, GridSize::beat) == 5 * q);
    CHECK (snapToGrid (map, pos, GridSize::eighth) == 5 * q + q / 2);
    CHECK (snapToGrid (map, pos, GridSize::sixteenth) == 5 * q + q / 4);
    CHECK (snapToGrid (map, pos, GridSize::eighthTriplet) == 5 * q + q / 3);
    CHECK (snapToGrid (map, pos, GridSize::sixteenthTriplet) == 5 * q + q / 3);
    CHECK (snapToGrid (map, pos, GridSize::off) == pos);

    // Exactly on a line stays there.
    CHECK (snapToGrid (map, 6 * q, GridSize::sixteenth) == 6 * q);
}

TEST_CASE ("Grid lines follow time signature changes", "[time][grid]")
{
    TempoMap map ({}, { { 0, 4, 4 }, { 1, 7, 8 } });   // bar 2 onwards is 7/8
    const auto bar2 = 4 * q;
    const auto bar3 = bar2 + 7 * q / 2;
    CHECK (map.barToTicks (2) == bar3);

    CHECK (snapToGrid (map, bar2 + q / 2 + 10, GridSize::beat) == bar2 + q / 2);   // eighth-note beats
    CHECK (snapToGrid (map, bar3 - q / 5, GridSize::bar) == bar3);
    CHECK (gridStepAt (map, bar2, GridSize::bar) == 7 * q / 2);

    // 1/8 triplets don't divide a 7/8 bar evenly: near the end of the bar it snaps to the bar line.
    CHECK (snapToGrid (map, bar3 - 100, GridSize::eighthTriplet) == bar3);
}

TEST_CASE ("Nudging moves by whole grid steps", "[time][grid]")
{
    TempoMap map;
    CHECK (moveByGridSteps (map, 4 * q, GridSize::beat, 1) == 5 * q);
    CHECK (moveByGridSteps (map, 4 * q, GridSize::beat, -2) == 2 * q);
    CHECK (moveByGridSteps (map, 4 * q + 10, GridSize::sixteenth, 1) == 4 * q + q / 4);
    CHECK (moveByGridSteps (map, 0, GridSize::bar, -1) == 0);           // never before the start
    CHECK (moveByGridSteps (map, 3 * q, GridSize::bar, 1) == 8 * q);   // snaps to 4q, then one bar on
    CHECK (moveByGridSteps (map, 7, GridSize::off, 3) == 7);
}

TEST_CASE ("Grid sizes have names and stable ids", "[time][grid]")
{
    for (auto size : allGridSizes)
        CHECK (gridSizeFromId (gridSizeId (size)) == size);

    CHECK (std::string (gridSizeName (GridSize::sixteenthTriplet)) == "1/16 triplet");
    CHECK (gridSizeFromId ("nonsense") == GridSize::beat);
    CHECK (gridSizeFromId (nullptr) == GridSize::beat);
}
