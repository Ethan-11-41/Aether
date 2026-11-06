// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <memory>
#include <vector>

#include "AudioFileCache.h"
#include "model/Clip.h"
#include "time/TempoMap.h"

namespace aether
{

/** One clip, ready to play: every position and length is in samples at the engine's rate. */
struct ClipPlayback
{
    const AudioData* audio = nullptr;   // kept alive by TimelineSnapshot::keepAlive
    std::int64_t start = 0;             // where it starts on the timeline
    std::int64_t duration = 0;          // how long it plays
    std::int64_t offset = 0;            // where in `audio` the content starts
    std::int64_t contentLength = 0;     // length of the content (repeated when looping)
    std::int64_t phase = 0;             // looping: how far into the content it starts
    bool loop = false;
    float gain = 1.0f;
    std::int64_t fadeIn = 0, fadeOut = 0;
    FadeCurve fadeInCurve = FadeCurve::smooth, fadeOutCurve = FadeCurve::smooth;
};

struct TrackPlayback
{
    std::vector<ClipPlayback> clips;    // sorted by start
};

/** Everything about the project's timeline that the audio thread needs, as one read-only
    object. A new one is built (on the UI thread) whenever anything changes, and handed over
    with rt::SnapshotExchange; the old one is freed on the UI thread. */
struct TimelineSnapshot
{
    TempoMap tempo;
    bool loopEnabled = false;
    Tick loopStart = 0;
    Tick loopEnd = 16 * ticksPerQuarter;

    double sampleRate = 0.0;            // the engine rate the clip positions were worked out for
    std::vector<TrackPlayback> tracks;

    std::vector<std::shared_ptr<const AudioData>> keepAlive;   // the audio the clips point to
};

} // namespace aether
