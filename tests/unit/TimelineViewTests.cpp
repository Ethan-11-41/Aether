// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include <catch2/catch_test_macros.hpp>

#include "ui/TimelineView.h"

using namespace aether;
using aether::ui::TimelineView;

namespace
{
    constexpr Tick q = ticksPerQuarter;
}

TEST_CASE ("The timeline converts between positions and pixels", "[ui][timeline]")
{
    TimelineView view;   // 40 pixels per quarter note, starting at the beginning
    CHECK (view.tickToX (0) == 0.0);
    CHECK (view.tickToX (q) == 40.0);
    CHECK (view.xToTick (80.0) == 2 * q);

    view.setViewStart (4 * q);
    CHECK (view.tickToX (4 * q) == 0.0);
    CHECK (view.xToTick (40.0) == 5 * q);
    CHECK (view.ticksForWidth (400.0) == 10 * q);

    view.setViewStart (-q);   // can't scroll before the start
    CHECK (view.getViewStart() == 0);
}

TEST_CASE ("Zooming keeps the position under the mouse in place", "[ui][timeline]")
{
    TimelineView view;
    view.setViewStart (8 * q);
    const auto anchorX = 200.0;
    const auto anchorTick = view.xToTick (anchorX);

    view.zoomBy (2.0, anchorX);
    CHECK (view.getPixelsPerQuarter() == 80.0);
    CHECK (std::abs (view.xToTick (anchorX) - anchorTick) <= q / 100);

    view.zoomBy (0.25, anchorX);
    CHECK (std::abs (view.xToTick (anchorX) - anchorTick) <= q / 100);

    view.setPixelsPerQuarter (1.0e6, 0.0);
    CHECK (view.getPixelsPerQuarter() == TimelineView::maxPixelsPerQuarter);
    view.setPixelsPerQuarter (0.0, 0.0);
    CHECK (view.getPixelsPerQuarter() == TimelineView::minPixelsPerQuarter);
}

TEST_CASE ("The view says when it changes", "[ui][timeline]")
{
    TimelineView view;
    int changes = 0;
    view.onChange = [&] { ++changes; };
    view.setViewStart (q);
    view.setViewStart (q);         // no change, no call
    view.zoomBy (2.0, 0.0);
    CHECK (changes == 2);
}

TEST_CASE ("Clicks snap to the nearest beat, or bar when zoomed far out", "[ui][timeline]")
{
    TempoMap map ({}, { { 0, 4, 4 }, { 2, 6, 8 } });

    CHECK (ui::snapToBeat (map, q + q / 3, 40.0) == q);
    CHECK (ui::snapToBeat (map, q + 2 * q / 3, 40.0) == 2 * q);

    // In 6/8 the beat is an eighth note.
    const auto bar3 = map.barToTicks (2);
    CHECK (ui::snapToBeat (map, bar3 + q / 2 + q / 10, 40.0) == bar3 + q / 2);

    // Zoomed out so far that beats are only a few pixels apart: snap to bars.
    CHECK (ui::snapToBeat (map, 3 * q, 4.0) == 4 * q);
    CHECK (ui::snapToBeat (map, q, 4.0) == 0);
}
