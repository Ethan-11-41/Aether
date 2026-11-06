// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <array>

#include "TempoMap.h"

namespace aether
{

/** The snap grid: what positions clips and the playhead jump to when you drag them. */
enum class GridSize
{
    bar,
    beat,
    eighth,
    sixteenth,
    eighthTriplet,
    sixteenthTriplet,
    off
};

inline constexpr std::array<GridSize, 7> allGridSizes { GridSize::bar, GridSize::beat, GridSize::eighth, GridSize::sixteenth,
                                                        GridSize::eighthTriplet, GridSize::sixteenthTriplet, GridSize::off };

/** "1/16", "Bar", "Off"... as shown in the grid menu. */
const char* gridSizeName (GridSize size) noexcept;

/** A stable name for saving in the settings ("sixteenth"), and back. */
const char* gridSizeId (GridSize size) noexcept;
GridSize gridSizeFromId (const char* id) noexcept;

/** The length of one grid step at a position, in ticks (0 for "off"). Bars and beats follow the
    time signature at that position; note values (1/8, 1/16, triplets) are fixed lengths. */
Tick gridStepAt (const TempoMap& tempo, Tick position, GridSize size) noexcept;

/** The nearest grid position. Grid lines start from the beginning of each bar, so odd time
    signatures and signature changes snap correctly. "Off" returns the position unchanged. */
Tick snapToGrid (const TempoMap& tempo, Tick position, GridSize size) noexcept;

/** The grid position `steps` grid steps away (e.g. +1 = the next grid line after the current
    position's snapped position). Used by nudging. */
Tick moveByGridSteps (const TempoMap& tempo, Tick position, GridSize size, int steps) noexcept;

} // namespace aether
