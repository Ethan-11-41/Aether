// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <cmath>
#include <functional>

#include <juce_core/juce_core.h>

#include "time/TempoMap.h"

namespace aether::ui
{

/** How the timeline is zoomed and scrolled: which musical position is at the left edge, and
    how many pixels one quarter note takes. Shared by the ruler, the tempo lane, the track lanes
    (and from M3 the clips), so they always line up.

    The horizontal axis is musical time: every beat has the same width, even when the tempo
    changes, which is how most music software draws the arrangement. */
class TimelineView
{
public:
    static constexpr double minPixelsPerQuarter = 2.0;
    static constexpr double maxPixelsPerQuarter = 600.0;

    double getPixelsPerQuarter() const noexcept   { return pixelsPerQuarter; }
    Tick getViewStart() const noexcept             { return viewStart; }

    double tickToX (Tick tick) const noexcept
    {
        return (double) (tick - viewStart) / (double) ticksPerQuarter * pixelsPerQuarter;
    }

    Tick xToTick (double x) const noexcept
    {
        return viewStart + (Tick) std::llround (x / pixelsPerQuarter * (double) ticksPerQuarter);
    }

    /** The visible length in ticks for a given width in pixels. */
    Tick ticksForWidth (double width) const noexcept   { return xToTick (width) - viewStart; }

    void setViewStart (Tick tick)
    {
        tick = std::max<Tick> (0, tick);

        if (tick != viewStart)
        {
            viewStart = tick;
            changed();
        }
    }

    /** Zooms, keeping the position under `anchorX` (pixels from the left) where it is. */
    void setPixelsPerQuarter (double newValue, double anchorX)
    {
        newValue = std::clamp (newValue, minPixelsPerQuarter, maxPixelsPerQuarter);

        if (newValue == pixelsPerQuarter)
            return;

        const auto anchorTick = xToTick (anchorX);
        pixelsPerQuarter = newValue;
        viewStart = std::max<Tick> (0, anchorTick - (Tick) std::llround (anchorX / pixelsPerQuarter * (double) ticksPerQuarter));
        changed();
    }

    void zoomBy (double factor, double anchorX)   { setPixelsPerQuarter (pixelsPerQuarter * factor, anchorX); }

    /** Called whenever the zoom or scroll position changes. */
    std::function<void()> onChange;

private:
    void changed()
    {
        if (onChange)
            onChange();
    }

    double pixelsPerQuarter = 40.0;
    Tick viewStart = 0;
};

/** Rounds a position to the nearest beat (or bar, if beats are too close together to see). */
inline Tick snapToBeat (const TempoMap& tempo, Tick tick, double pixelsPerQuarter)
{
    const auto beat = tempo.beatLengthAt (tick);
    const auto pixelsPerBeat = pixelsPerQuarter * (double) beat / (double) ticksPerQuarter;
    const auto unit = pixelsPerBeat < 8.0 ? tempo.barLengthAt (tick) : beat;

    // Snap relative to the bar the position is in, so odd time signatures snap correctly.
    const auto bb = tempo.ticksToBarBeat (tick);
    const auto barStart = tempo.barToTicks (bb.bar);
    const auto offset = tick - barStart;
    return barStart + (Tick) std::llround ((double) offset / (double) unit) * unit;
}

} // namespace aether::ui
