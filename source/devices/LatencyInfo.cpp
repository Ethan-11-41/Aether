// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "LatencyInfo.h"

#include <cmath>
#include <cstdio>

namespace aether
{

std::string LatencyInfo::formatMs (double milliseconds)
{
    if (! std::isfinite (milliseconds))
        milliseconds = 0.0;

    char text[64];
    std::snprintf (text, sizeof (text), "%.1f ms", milliseconds);
    return text;
}

std::string LatencyInfo::formatSamplesAndMs (int samples, double rate)
{
    return std::to_string (samples) + (samples == 1 ? " sample (" : " samples (")
           + formatMs (samplesToMs (samples, rate)) + ")";
}

} // namespace aether
