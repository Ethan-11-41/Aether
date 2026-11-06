// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include "time/TempoMap.h"

namespace aether
{

/** Everything about the project's timeline that the audio thread needs, as one read-only
    object. A new one is built (on the UI thread) whenever the tempo, time signatures or loop
    change, and handed over with rt::SnapshotExchange. Clips and tracks join it in M3. */
struct TimelineSnapshot
{
    TempoMap tempo;
    bool loopEnabled = false;
    Tick loopStart = 0;
    Tick loopEnd = 16 * ticksPerQuarter;
};

} // namespace aether
