// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <functional>
#include <memory>
#include <vector>

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>

#include "AudioEngine.h"
#include "TimelineSync.h"
#include "model/Recording.h"

namespace aether
{

/** Records the armed tracks.

    - start(): makes one WAV file per armed track (24-bit, in the project's Audio folder), gives
      the engine a RecordingRing to fill, and starts the transport recording (after the
      count-in, if one is set).
    - While recording, a background thread moves the sound from the ring into the files, and
      updates each file's header every second, so a crash or power cut loses at most a second.
    - stop(): stops the transport. When the engine confirms the recording has ended, the last
      sound is written, the files are closed, and the clips are added (all tracks in one undo
      step), corrected for latency, with punch and loop takes applied (see model/Recording.h).

    Use from the UI thread. It checks on the engine 20 times a second (or when poll() is called).
*/
class Recorder final : private juce::Timer
{
public:
    /** `latencySamples` gives the round trip to correct recordings by (measured, if the user
        has measured it; otherwise what the driver reports). `numInputChannels` says how many
        inputs the sound card has (to warn about tracks set to an input that doesn't exist). */
    Recorder (Project& project, AudioEngine& engine, TimelineSync& sync, std::function<ProjectLocation()> location,
              std::function<int()> latencySamples, std::function<int()> numInputChannels = {});
    ~Recorder() override;

    /** Starts recording. Returns a problem in plain words, or an empty string if it started. */
    juce::String start();

    /** Stops the transport, which ends the recording. */
    void stop();

    /** True from start() until the clips have been added (or the recording was abandoned). */
    bool isActive() const noexcept   { return session != nullptr; }

    /** The tracks being recorded, and what's been recorded so far (for drawing it as it grows). */
    juce::StringArray getRecordingTracks() const;
    RecordingStatus getStatus() const   { return engine.getRecordingStatus(); }

    /** Called with the new clips' ids once a recording has been turned into clips. */
    std::function<void (const juce::StringArray& clipIds)> onFinished;

    /** Called when something went wrong (e.g. the disk couldn't keep up). */
    std::function<void (const juce::String& problem)> onProblem;

    /** Checks on the engine now (the timer does this regularly; the tests call it directly). */
    void poll();

    /** How much sound the ring can hold before the disk writer must catch up. */
    static constexpr double ringSeconds = 20.0;

private:
    struct TrackFile
    {
        juce::String trackId, name;
        juce::File file;
        std::unique_ptr<juce::AudioFormatWriter> writer;
        std::vector<int> ringChannels;   // which of the ring's channels go into this file
        std::int64_t written = 0;
    };

    struct Session;
    class Writer;

    void timerCallback() override   { poll(); }
    void finish (const RecordingStatus& status);
    void abandon (bool deleteFiles);
    void reportProblem (const juce::String& problem);

    Project& project;
    AudioEngine& engine;
    TimelineSync& sync;
    std::function<ProjectLocation()> location;
    std::function<int()> latencySamples, numInputChannels;
    std::unique_ptr<Session> session;
};

} // namespace aether
