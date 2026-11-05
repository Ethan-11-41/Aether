// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "devices/LatencyInfo.h"

using aether::LatencyInfo;
using Catch::Matchers::WithinAbs;

TEST_CASE ("Samples are converted to milliseconds", "[latency]")
{
    CHECK_THAT (LatencyInfo::samplesToMs (48, 48000.0), WithinAbs (1.0, 1e-12));
    CHECK_THAT (LatencyInfo::samplesToMs (256, 48000.0), WithinAbs (5.333333333, 1e-6));
    CHECK_THAT (LatencyInfo::samplesToMs (441, 44100.0), WithinAbs (10.0, 1e-12));
    CHECK_THAT (LatencyInfo::samplesToMs (0, 48000.0), WithinAbs (0.0, 1e-12));
}

TEST_CASE ("An invalid sample rate gives zero instead of dividing by zero", "[latency]")
{
    CHECK (LatencyInfo::samplesToMs (256, 0.0) == 0.0);
    CHECK (LatencyInfo::samplesToMs (256, -1.0) == 0.0);

    LatencyInfo info;
    CHECK_FALSE (info.isValid());
    CHECK (info.roundTripMs() == 0.0);
}

TEST_CASE ("Round trip is input plus output latency", "[latency]")
{
    LatencyInfo info;
    info.sampleRate = 48000.0;
    info.bufferSizeSamples = 256;
    info.inputLatencySamples = 300;
    info.outputLatencySamples = 420;

    CHECK (info.isValid());
    CHECK (info.roundTripSamples() == 720);
    CHECK_THAT (info.roundTripMs(), WithinAbs (15.0, 1e-9));
    CHECK_THAT (info.bufferMs(), WithinAbs (5.3333333, 1e-6));
    CHECK_THAT (info.inputMs(), WithinAbs (6.25, 1e-9));
    CHECK_THAT (info.outputMs(), WithinAbs (8.75, 1e-9));
}

TEST_CASE ("Latency is formatted for display", "[latency]")
{
    CHECK (LatencyInfo::formatMs (5.333333) == "5.3 ms");
    CHECK (LatencyInfo::formatMs (10.0) == "10.0 ms");
    CHECK (LatencyInfo::formatMs (0.04) == "0.0 ms");
    CHECK (LatencyInfo::formatSamplesAndMs (256, 48000.0) == "256 samples (5.3 ms)");
    CHECK (LatencyInfo::formatSamplesAndMs (1, 1000.0) == "1 sample (1.0 ms)");
}
