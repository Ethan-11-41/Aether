// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <memory>
#include <vector>

#include "AudioFileCache.h"
#include "RecordingRing.h"
#include "dsp/Effect.h"
#include "dsp/Instrument.h"
#include "model/Automation.h"
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

    // MIDI clip fades (audio thread only): the level now, and a silence after a fade-out that
    // playing live has ended (its `quietUntil`).
    float midiFadeGain = 1.0f;
    std::int64_t quietBrokenUntil = -1;

    /** Highest level (linear) per side since the UI last asked. */
    std::array<std::atomic<float>, 2> peaks {};

    float takePeak (int side) noexcept       { return peaks[(size_t) (side & 1)].exchange (0.0f, std::memory_order_relaxed); }
};

/** A send's working state (its smoothed level). */
struct SendState
{
    GainRamp gain;
};

/** An insert's working state, kept from one snapshot to the next: how much of the effect is
    heard. It fades (over 20 ms) when the effect is switched on or off, or added while playing, so
    that never clicks. */
struct InsertState
{
    float amount = 0.0f;                                  // 0: the dry sound, 1: through the effect
    std::array<float, mixChunkSize> scratchLeft {}, scratchRight {};
};

struct InsertPlayback
{
    std::shared_ptr<dsp::Effect> effect;     // prepared for the snapshot's sample rate
    std::shared_ptr<InsertState> state = std::make_shared<InsertState>();
    dsp::EffectParameterValues parameters {};
    bool bypassed = false;
};

/** Pan as a balance control: the centre leaves the sound unchanged; turning it to one side
    gradually turns the other side down (the side it points to stays at full level). */
inline std::pair<float, float> panGains (double pan) noexcept
{
    auto turnDown = [] (double amount) { return amount >= 1.0 ? 0.0f : (float) std::cos (amount * juce::MathConstants<double>::halfPi); };
    return { pan > 0.0 ? turnDown (pan) : 1.0f, pan < 0.0 ? turnDown (-pan) : 1.0f };
}

/** One automation lane, ready for the audio thread: its points in samples, and their values as
    heights on the lane (0..1), turned back into values with `range`. */
struct AutomationPlayback
{
    enum class Kind : std::uint8_t { volume, pan, mute, send, effect, effectOn, instrument };

    struct Point
    {
        std::int64_t sample = 0;
        float position = 0.0f;
        float bend = 0.0f;
    };

    Kind kind = Kind::volume;
    int index = -1;                          // which send or effect, in the channel's lists
    int parameter = -1;                      // which setting of the effect or instrument
    automation::Range range;
    std::vector<Point> points;               // sorted by time; never empty

    /** The setting's value at a sample (the same maths as automation::valueAt). */
    float valueAt (std::int64_t sample) const noexcept AETHER_NONBLOCKING
    {
        if (sample <= points.front().sample)
            return range.fromPosition (points.front().position);

        if (sample >= points.back().sample)
            return range.fromPosition (points.back().position);

        size_t low = 0, high = points.size() - 1;   // points[low] <= sample < points[high]
        while (high - low > 1)
        {
            const auto middle = (low + high) / 2;
            (points[middle].sample <= sample ? low : high) = middle;
        }

        const auto& a = points[low];
        const auto& b = points[high];

        if (range.holds() || b.sample == a.sample)
            return range.fromPosition (a.position);

        const auto along = (float) ((double) (sample - a.sample) / (double) (b.sample - a.sample));
        return range.fromPosition (automation::bendPosition (a.position, b.position, along, a.bend));
    }
};

struct SendPlayback
{
    int destination = -1;                    // index into TimelineSnapshot::tracks (always a bus after the sender)
    float gain = 1.0f;                       // level, and 0 if solo means it isn't heard
    bool audible = true;                     // false if solo means it isn't heard (automation keeps it silent)
    bool preFader = false;
    std::shared_ptr<SendState> state = std::make_shared<SendState>();
};

/** A MIDI clip's fades (0.16.2), in samples at the engine's rate: while it plays, its track's
    instrument is turned down along them. After a fade-out the track stays silent until its next
    clip starts (at most a few seconds), so notes' release and echo tails don't come back. */
struct MidiFadePlayback
{
    std::int64_t start = 0, end = 0;          // the clip
    std::int64_t fadeIn = 0, fadeOut = 0;     // lengths (0: none)
    FadeCurve fadeInCurve = FadeCurve::smooth, fadeOutCurve = FadeCurve::smooth;
    std::int64_t quietUntil = 0;              // after a fade-out: silent until here

    static constexpr double quietSeconds = 4.0;
};

/** How loud a MIDI track's instrument is at a position, from its clips' fades: inside clips, the
    loudest of the clips there; between clips, silent after a fade-out (`quietUntil` says until
    when, else -1), otherwise full. `fades` is sorted by start. */
struct MidiFadeLevel
{
    float gain = 1.0f;
    std::int64_t quietUntil = -1;
};

inline MidiFadeLevel midiFadeLevel (const std::vector<MidiFadePlayback>& fades, std::int64_t position) noexcept AETHER_NONBLOCKING
{
    float inside = -1.0f;
    const MidiFadePlayback* lastEnded = nullptr;

    for (const auto& f : fades)
    {
        if (f.start > position)
            break;

        if (position < f.end)
        {
            auto gain = 1.0f;
            const auto in = position - f.start, left = f.end - position;

            if (in < f.fadeIn)
                gain *= fadeShape (f.fadeInCurve, (double) in / (double) f.fadeIn);
            if (left <= f.fadeOut)
                gain *= fadeShape (f.fadeOutCurve, (double) (left - 1) / (double) f.fadeOut);

            inside = std::max (inside, gain);
        }
        else if (lastEnded == nullptr || f.end > lastEnded->end)
        {
            lastEnded = &f;
        }
    }

    if (inside >= 0.0f)
        return { inside, -1 };

    if (lastEnded != nullptr && lastEnded->fadeOut > 0 && position < lastEnded->quietUntil)
        return { 0.0f, lastEnded->quietUntil };

    return {};
}

/** One note from a MIDI clip, ready to play: in samples at the engine's rate. */
struct NotePlayback
{
    std::int64_t start = 0, length = 0;
    std::uint8_t pitch = 60, velocity = 100;
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

    // Automation: the lanes with points, and what volume, pan and mute are without them.
    std::vector<AutomationPlayback> automation;
    float volumeDb = 0.0f, pan = 0.0f;
    bool muteFixed = false;                  // played on its own (a part loop): mute automation doesn't apply
    float outputGain = 1.0f;                 // 0 when solo means the channel's output isn't heard
    int destination = -1;                    // index into TimelineSnapshot::tracks of the bus it
                                             // plays into (always later in the list); -1 = master

    int monitorChannel = -1;                 // input monitoring: the sound-card input heard through
    bool monitorStereo = false;              // this channel (-1 = none); with the next one if stereo

    int armedChannel = -1;                   // an armed track's input, shown on its meter even when
    bool armedStereo = false;                // not monitoring (so you can check the level)

    // MIDI tracks
    std::shared_ptr<dsp::Instrument> instrument;           // plays the notes into this channel
    dsp::EffectParameterValues instrumentParameters {};
    std::vector<NotePlayback> notes;                       // every note of its clips, sorted by start
    std::vector<MidiFadePlayback> midiFades;               // its clips' fades (empty when none has any), by start
    bool liveInput = false;                                // plays what's played on a MIDI keyboard
    std::uint64_t idHash = 0;                              // the track's id, for notes previewed on it

    // Drum kits: the pads' sounds (kept alive here; `sounds` points into them). Samplers: their
    // map of recordings (`sounds.map` points to it) and the audio it plays.
    std::array<std::shared_ptr<const AudioData>, dsp::InstrumentSounds::maxSounds> soundData;
    std::shared_ptr<const dsp::SampleMap> sampleMap;
    std::vector<std::shared_ptr<const AudioData>> sampleAudio;
    dsp::InstrumentSounds sounds;
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

    /** While recording is set up: where the audio thread puts the recorded input. Being part of
        the snapshot keeps it alive for as long as the audio thread might use it. */
    std::shared_ptr<RecordingRing> recordRing;
};

} // namespace aether
