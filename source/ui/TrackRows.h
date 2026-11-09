// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <vector>

#include "model/Project.h"

namespace aether::ui
{

/** Where each track and each of its shown automation lanes are, top to bottom, in the track
    list and the timeline (which line up with each other). A track's row comes first, then its
    lanes under it. */
class TrackRows
{
public:
    static constexpr int trackHeight = 56;
    static constexpr int laneHeight = 50;

    explicit TrackRows (const Project& project);

    struct Row
    {
        int trackIndex = 0;
        juce::String trackId;
        juce::String laneId;    // empty: the track itself
        int top = 0, height = 0;

        bool isLane() const noexcept       { return laneId.isNotEmpty(); }
        int bottom() const noexcept        { return top + height; }
    };

    const std::vector<Row>& getRows() const noexcept   { return rows; }

    /** The top of a track's row (the number of tracks: the bottom of the last track's lanes). */
    int trackTop (int trackIndex) const noexcept;

    /** The top and bottom of a track with its lanes. */
    int trackBottom (int trackIndex) const noexcept;

    int totalHeight() const noexcept   { return total; }

    /** The row at a height (null below the last track). */
    const Row* rowAt (int y) const noexcept;

    /** The track whose row or lanes are at a height (-1 above, the number of tracks below). */
    int trackIndexAt (int y) const noexcept;

    /** The nearest gap between tracks to a height, as an index to insert at (0..tracks). */
    int insertIndexAt (int y) const noexcept;

    /** A lane's row (null if it isn't shown). */
    const Row* laneRow (const juce::String& laneId) const noexcept;

private:
    std::vector<Row> rows;
    std::vector<int> trackTops;   // one per track, plus the total height
    int total = 0;
};

} // namespace aether::ui
