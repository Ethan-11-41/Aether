// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "Grid.h"

#include <cmath>
#include <cstring>

namespace aether
{

const char* gridSizeName (GridSize size) noexcept
{
    switch (size)
    {
        case GridSize::bar:              return "Bar";
        case GridSize::beat:             return "Beat";
        case GridSize::eighth:           return "1/8";
        case GridSize::sixteenth:        return "1/16";
        case GridSize::thirtySecond:     return "1/32";
        case GridSize::eighthTriplet:    return "1/8 triplet";
        case GridSize::sixteenthTriplet: return "1/16 triplet";
        case GridSize::off:              return "Off";
    }

    return "Off";
}

const char* gridSizeId (GridSize size) noexcept
{
    switch (size)
    {
        case GridSize::bar:              return "bar";
        case GridSize::beat:             return "beat";
        case GridSize::eighth:           return "eighth";
        case GridSize::sixteenth:        return "sixteenth";
        case GridSize::thirtySecond:     return "thirtySecond";
        case GridSize::eighthTriplet:    return "eighthTriplet";
        case GridSize::sixteenthTriplet: return "sixteenthTriplet";
        case GridSize::off:              return "off";
    }

    return "off";
}

GridSize gridSizeFromId (const char* id) noexcept
{
    for (auto size : allGridSizes)
        if (id != nullptr && std::strcmp (id, gridSizeId (size)) == 0)
            return size;

    return GridSize::beat;
}

Tick gridStepAt (const TempoMap& tempo, Tick position, GridSize size) noexcept
{
    switch (size)
    {
        case GridSize::bar:              return tempo.barLengthAt (position);
        case GridSize::beat:             return tempo.beatLengthAt (position);
        case GridSize::eighth:           return ticksPerQuarter / 2;
        case GridSize::sixteenth:        return ticksPerQuarter / 4;
        case GridSize::thirtySecond:     return ticksPerQuarter / 8;
        case GridSize::eighthTriplet:    return ticksPerQuarter / 3;
        case GridSize::sixteenthTriplet: return ticksPerQuarter / 6;
        case GridSize::off:              return 0;
    }

    return 0;
}

Tick snapToGrid (const TempoMap& tempo, Tick position, GridSize size) noexcept
{
    const auto step = gridStepAt (tempo, position, size);

    if (step <= 0)
        return position;

    // Measure from the start of the bar the position is in.
    const auto barStart = tempo.barToTicks (tempo.ticksToBarBeat (position).bar);
    const auto barLength = tempo.barLengthAt (barStart);
    const auto offset = position - barStart;
    auto snapped = barStart + (Tick) std::llround ((double) offset / (double) step) * step;

    // A grid that doesn't divide the bar evenly (e.g. 1/8 triplets in 7/8) snaps to the bar end
    // rather than past it, so the next bar's grid starts cleanly.
    if (snapped > barStart + barLength)
        snapped = barStart + barLength;

    if (position - snapped > barStart + barLength - position && snapped < barStart + barLength)
        snapped = barStart + barLength;

    return std::max<Tick> (0, snapped);
}

Tick moveByGridSteps (const TempoMap& tempo, Tick position, GridSize size, int steps) noexcept
{
    if (size == GridSize::off || steps == 0)
        return position;

    auto result = position;

    for (int i = 0; i < std::abs (steps); ++i)
    {
        const auto direction = steps > 0 ? 1 : -1;
        const auto step = gridStepAt (tempo, direction > 0 ? result : std::max<Tick> (0, result - 1), size);
        auto next = snapToGrid (tempo, result + direction * step, size);

        // Make sure each step actually moves (snapping can land back on the same line).
        if (next == result)
            next = snapToGrid (tempo, result + direction * step * 3 / 2, size);

        result = std::max<Tick> (0, next);
    }

    return result;
}

} // namespace aether
