// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <array>
#include <atomic>
#include <memory>
#include <vector>

#include "AudioFileCache.h"
#include "dsp/Effect.h"
#include "model/Clip.h"
#include "rt/RealtimeAnnotations.h"
#include "time/TempoMap.h"

namespace aether
{

/** The engine mixes in pieces of at most this many samples (a device block is split if needed),
    so every mixer buffer can be allocated in advance at this size. */
constexpr int mixChunkSize = 512;

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

/** A gain that moves smoothly (over a few milliseconds) to each new value instead of jumping,
    so moving a fader or pressing mute never clicks. Audio thread only. */
class GainRamp
{
public:
    void setTarget (float newTarget, int rampSamples) noexcept AETHER_NONBLOCKING
    {
        if (! started)
        {
            current = target = newTarget;   // first use: start at the value straight away
            started = true;
            return;
        }

        if (newTarget != target)
        {
            target = newTarget;
            remaining = rampSamples > 0 ? rampSamples : 1;
            step = (target - current) / (float) remaining;
        }
    }

    bool isSteady() const noexcept   { return remaining == 0; }
    float getCurrent() const noexcept   { return current; }

    float next() noexcept AETHER_NONBLOCKING
    {
        if (remaining > 0)
        {
            current += step;

            if (--remaining == 0)
                current = target;
        }

        return current;
    }

private:
    float current = 0.0f, target = 0.0f, step = 0.0f;
    int remaining = 0;
    bool started = false;
};

/** The audio thread's working state for one mixer channel (a track, bus or the master). It
    is kept from one snapshot to the next (by MixerResources), so fades carry on smoothly while
    you edit. Created on the UI thread. */
struct ChannelState
{
    ChannelState() : input (2, mixChunkSize)   { input.clear(); }

    juce::AudioBuffer<float> input;          // what plays into the channel during the current chunk
    GainRamp mute, left, right, output;

    /** Highest level (linear) per side since the UI last asked. */
    std::array<std::atomic<float>, 2> peaks {};

    float takePeak (int side) noexcept       { return peaks[(size_t) (side & 1)].exchange (0.0f, std::memory_order_relaxed); }
};

/** A send's working state (its smoothed level). */
struct SendState
{
    GainRamp gain;
};

struct InsertPlayback
{
    std::shared_ptr<dsp::Effect> effect;     // prepared for the snapshot's sample rate
    dsp::EffectParameterValues parameters {};
    bool bypassed = false;
};

struct SendPlayback
{
    int destination = -1;                    // index into TimelineSnapshot::tracks (always a bus after the sender)
    float gain = 1.0f;                       // level, and 0 if solo means it isn't heard
    bool preFader = false;
    std::shared_ptr<SendState> state = std::make_shared<SendState>();
};

/** One mixer channel: a track (with its clips), a bus, or the master. */
struct TrackPlayback
{
    TrackPlayback() : state (std::make_shared<ChannelState>()) {}
    explicit TrackPlayback (std::shared_ptr<ChannelState> s) : state (std::move (s)) {}

    std::vector<ClipPlayback> clips;         // sorted by start
    std::shared_ptr<ChannelState> state;
    std::vector<InsertPlayback> inserts;
    std::vector<SendPlayback> sends;

    float muteGain = 1.0f;                   // 0 when muted (after the effects)
    float leftGain = 1.0f, rightGain = 1.0f; // volume and pan
    float outputGain = 1.0f;                 // 0 when solo means the channel's output isn't heard
    int destination = -1;                    // index into TimelineSnapshot::tracks of the bus it
                                             // plays into (always later in the list); -1 = master
};

/** Everything about the project that the audio thread needs, as one read-only object. A new
    one is built (on the UI thread) whenever anything changes, and handed over with
    rt::SnapshotExchange; the old one is freed on the UI thread. */
struct TimelineSnapshot
{
    TempoMap tempo;
    bool loopEnabled = false;
    Tick loopStart = 0;
    Tick loopEnd = 16 * ticksPerQuarter;

    double sampleRate = 0.0;            // the engine rate the positions were worked out for

    /** Tracks and buses in processing order: every bus comes after everything that plays into it. */
    std::vector<TrackPlayback> tracks;
    TrackPlayback master;

    std::vector<std::shared_ptr<const AudioData>> keepAlive;   // the audio the clips point to
};

} // namespace aether
