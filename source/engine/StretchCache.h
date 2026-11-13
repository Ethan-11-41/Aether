// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <set>

#include <juce_events/juce_events.h>

#include "AudioFileCache.h"
#include "dsp/TimeStretch.h"
#include "model/Warp.h"

namespace aether
{

/** What a stretched clip needs: its audio (already at the engine's rate), which part of it plays
    in which order (as the clip plays it: a loop repeats), and when (the time map), with its pitch.
    Built from a clip by `make`. */
struct StretchJob
{
    juce::String clipId;
    juce::String key;                              // everything below, in short: equal keys, equal results
    std::shared_ptr<const AudioData> audio;
    std::int64_t offset = 0, contentLength = 1, phase = 0, duration = 1;   // engine samples
    bool loop = false;
    std::vector<dsp::timestretch::MapPoint> map;   // output (engine samples from the clip's start) -> played
    std::int64_t outputLength = 0;
    double semitones = 0.0;

    /** The job for an audio clip at the song's tempo (engine rate = the audio's rate). */
    static StretchJob make (const Clip& clip, const TempoMap& tempo, std::shared_ptr<const AudioData> audio);

    /** Does it (on any thread but the audio thread). Null if stopped or there's nothing to do. */
    std::shared_ptr<const AudioData> render (const std::function<bool()>& shouldStop = {}) const;
};

/** Stretched audio for clips that need it (warp::needsStretching), made in the background.

    Asked for a job, it gives the result if it's ready; otherwise it starts making it and gives
    the clip's previous result, if any (close enough while the new one is made: e.g. while a
    warp marker is dragged), or null (the clip then plays unstretched for a moment). When a result
    is ready, it sends a change message (so the engine gets it). A newer job for the same clip
    stops an older one still being made.

    UI thread only (the work happens on a background thread).
*/
class StretchCache final : public juce::ChangeBroadcaster
{
public:
    StretchCache();
    ~StretchCache() override;

    struct Result
    {
        std::shared_ptr<const AudioData> audio;
        bool exact = false;   // false: an earlier result for the clip, while the new one is made
    };

    Result get (const StretchJob& job);

    /** True while anything is being made (e.g. to show "Stretching..."). */
    bool isBusy() const;

    /** True while the clip's latest job is being made. */
    bool isPreparing (const juce::String& clipId) const;

    /** Forgets every result whose key isn't in `inUse` (stopping what's being made for them). */
    void keepOnly (const std::set<juce::String>& inUse);

    void clear();

    /** Waits until nothing is being made (for tests). */
    void waitUntilIdle();

    int getNumEntries() const;

private:
    struct Entry
    {
        juce::String clipId;
        std::shared_ptr<const AudioData> audio;   // when ready
        std::shared_ptr<std::atomic<bool>> stop = std::make_shared<std::atomic<bool>> (false);
        bool ready = false;
        std::uint64_t order = 0;                  // when it was asked for first
    };

    mutable std::mutex lock;
    std::map<juce::String, Entry> entries;
    std::uint64_t counter = 0;
    juce::ThreadPool workers;
};

} // namespace aether
