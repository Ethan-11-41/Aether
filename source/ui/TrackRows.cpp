// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "TrackRows.h"

#include "model/Automation.h"

namespace aether::ui
{

TrackRows::TrackRows (const Project& project)
{
    int y = 0;

    for (int i = 0; i < project.getNumTracks(); ++i)
    {
        const auto trackId = project.getTrack (i).getId();
        trackTops.push_back (y);
        rows.push_back ({ i, trackId, {}, y, trackHeight });
        y += trackHeight;

        for (const auto& lane : automation::lanesOf (project, trackId))
        {
            if (! (bool) lane[ids::shown])
                continue;

            rows.push_back ({ i, trackId, lane[ids::id].toString(), y, laneHeight });
            y += laneHeight;
        }
    }

    trackTops.push_back (y);
    total = y;
}

int TrackRows::trackTop (int trackIndex) const noexcept
{
    if (trackIndex < 0)
        return 0;

    return trackTops[(size_t) std::min (trackIndex, (int) trackTops.size() - 1)];
}

int TrackRows::trackBottom (int trackIndex) const noexcept
{
    return trackTop (trackIndex + 1);
}

const TrackRows::Row* TrackRows::rowAt (int y) const noexcept
{
    for (const auto& row : rows)
        if (y >= row.top && y < row.bottom())
            return &row;

    return nullptr;
}

int TrackRows::trackIndexAt (int y) const noexcept
{
    if (y < 0)
        return -1;

    if (const auto* row = rowAt (y))
        return row->trackIndex;

    return (int) trackTops.size() - 1;   // below the last track
}

int TrackRows::insertIndexAt (int y) const noexcept
{
    const auto count = (int) trackTops.size() - 1;
    int best = 0;

    for (int i = 0; i <= count; ++i)
        if (std::abs (trackTops[(size_t) i] - y) < std::abs (trackTops[(size_t) best] - y))
            best = i;

    return best;
}

const TrackRows::Row* TrackRows::laneRow (const juce::String& laneId) const noexcept
{
    for (const auto& row : rows)
        if (row.laneId == laneId)
            return &row;

    return nullptr;
}

} // namespace aether::ui
