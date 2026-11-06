// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <array>
#include <atomic>
#include <cstdint>

#include <juce_audio_devices/juce_audio_devices.h>

#include "LatencyProbe.h"
#include "Metronome.h"
#include "TestToneGenerator.h"
#include "TimelineSnapshot.h"
#include "rt/RealtimeAnnotations.h"
#include "rt/SnapshotExchange.h"
#include "rt/SpscQueue.h"

namespace aether
{

/** What the transport is doing, as reported by the audio thread. */
struct TransportStatus
{
    bool playing = false;
    bool countingIn = false;
    Tick position = 0;
    int countInBeatsLeft = 0;
};

/** What recording is doing, as reported by the audio thread. */
struct RecordingStatus
{
    bool recording = false;
    std::int64_t startSample = 0;     // where on the timeline (samples) the recording began
    std::int64_t frames = 0;          // how much has been recorded so far
    std::uint32_t completed = 0;      // goes up by one each time a recording ends
};

/** A transport instruction from the UI thread to the audio thread. */
struct TransportCommand
{
    enum class Type { play, stop, locate, record };
    Type type = Type::stop;
    Tick position = 0;
};

/** The audio engine: everything that runs on the audio thread.

    The transport (play, stop, position, loop, count-in), the metronome, clip playback and the
    mixer (tracks, buses, sends, effects, master), plus the test tone and level meters.

    The UI thread talks to it only without locks:
      - transport commands (play, stop, locate) go through a lock-free queue,
      - the tempo map and loop arrive as a TimelineSnapshot through an rt::SnapshotExchange,
      - simple settings (metronome on/off, volume, count-in) are atomics,
      - the position and meter levels come back as atomics.
*/
class AudioEngine final : public juce::AudioIODeviceCallback
{
public:
    static constexpr int numMeteredChannels = 2;
    static constexpr int maxCountInBars = 4;

    AudioEngine();
    ~AudioEngine() override;

    //==============================================================================
    // Called from the UI thread

    /** Hands the engine a new tempo map / loop. Takes effect from the next audio block. */
    void setTimeline (std::unique_ptr<TimelineSnapshot> timeline);

    /** Deletes timelines the audio thread has finished with. Call regularly (e.g. from a timer). */
    void collectGarbage();

    void play();
    void stop();
    void locate (Tick position);

    /** Starts recording (after the count-in, if there is one; straight away if already playing)
        into the timeline's record ring (see TimelineSnapshot::recordRing). */
    void record();
    RecordingStatus getRecordingStatus() const noexcept;

    /** Starts a latency measurement (it replaces the normal output for a few seconds). */
    void startLatencyCapture (std::shared_ptr<LatencyCapture> capture);

    void setMetronomeEnabled (bool shouldClick) noexcept   { metronomeEnabled.store (shouldClick, std::memory_order_relaxed); }
    bool isMetronomeEnabled() const noexcept               { return metronomeEnabled.load (std::memory_order_relaxed); }
    void setMetronomeLevelDb (float decibels) noexcept;
    float getMetronomeLevelDb() const noexcept             { return metronomeLevelDb.load (std::memory_order_relaxed); }

    /** Bars of clicks before playback starts (0 = off). */
    void setCountInBars (int bars) noexcept;
    int getCountInBars() const noexcept                    { return countInBars.load (std::memory_order_relaxed); }

    TransportStatus getTransportStatus() const noexcept;

    void setTestToneEnabled (bool shouldPlay) noexcept   { testToneRequested.store (shouldPlay, std::memory_order_relaxed); }
    bool isTestToneEnabled() const noexcept              { return testToneRequested.load (std::memory_order_relaxed); }

    /** Highest output level (linear, 0..1+) since the last call, then resets it. */
    float takeOutputPeak (int channel) noexcept;

    /** Highest input (e.g. microphone) level since the last call, then resets it. */
    float takeInputPeak (int channel) noexcept;

    //==============================================================================
    // The engine itself. prepare() is called before audio starts; process() on the audio thread.
    // These are public so the automated tests can drive the engine without a sound card.

    void prepare (double sampleRate, int maximumBlockSize);

    void process (const float* const* inputs, int numInputs,
                  float* const* outputs, int numOutputs,
                  int numSamples) noexcept AETHER_NONBLOCKING;

    double getSampleRate() const noexcept   { return sampleRate; }

    /** The sample rate the engine is running at (safe to call from the UI thread). */
    double getCurrentSampleRate() const noexcept   { return currentRate.load (std::memory_order_relaxed); }

    //==============================================================================
    // juce::AudioIODeviceCallback

    void audioDeviceAboutToStart (juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;
    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData, int numInputChannels,
                                           float* const* outputChannelData, int numOutputChannels,
                                           int numSamples,
                                           const juce::AudioIODeviceCallbackContext& context) override;

private:
    using Command = TransportCommand;

    static constexpr int maxOutputChannels = 64;
    static constexpr int maxInputChannels = 64;

    /** A latency measurement waiting for (or being run by) the audio thread. */
    struct ProbeJob
    {
        std::shared_ptr<LatencyCapture> capture;
    };

    void handleCommands (const TimelineSnapshot& timeline) noexcept AETHER_NONBLOCKING;
    void processChunk (const TimelineSnapshot& timeline, const float* const* inputs, int numInputs,
                       float* const* outputs, int numOutputs, int numSamples) noexcept AETHER_NONBLOCKING;
    void stopRecording() noexcept AETHER_NONBLOCKING;
    void monitorInputs (const TimelineSnapshot& timeline, const float* const* inputs, int numInputs, int numSamples) noexcept AETHER_NONBLOCKING;
    void startCountIn (const TimelineSnapshot& timeline) noexcept AETHER_NONBLOCKING;
    int runCountIn (int numSamples) noexcept AETHER_NONBLOCKING;
    void runPlayback (const TimelineSnapshot& timeline, int blockOffset, int numSamples) noexcept AETHER_NONBLOCKING;
    void clickBeats (const TempoMap& tempo, std::int64_t fromSample, std::int64_t toSample, int blockOffset) noexcept AETHER_NONBLOCKING;
    void renderClips (const TimelineSnapshot& timeline, std::int64_t fromSample, std::int64_t toSample,
                      int blockOffset) noexcept AETHER_NONBLOCKING;
    void mixChannels (const TimelineSnapshot& timeline, float* const* outputs, int numOutputs, int numSamples) noexcept AETHER_NONBLOCKING;
    void processChannel (const TimelineSnapshot& timeline, const TrackPlayback& channel, int index,
                         const dsp::EffectContext& context, int numSamples) noexcept AETHER_NONBLOCKING;
    std::int64_t tickToSample (const TempoMap& tempo, Tick tick) const noexcept AETHER_NONBLOCKING;
    Tick sampleToTick (const TempoMap& tempo, std::int64_t sample) const noexcept AETHER_NONBLOCKING;

    static void updatePeak (std::atomic<float>& meter, float value) noexcept AETHER_NONBLOCKING;
    static float takePeak (std::array<std::atomic<float>, numMeteredChannels>& meters, int channel) noexcept;

    double sampleRate = 48000.0;
    std::atomic<double> currentRate { 0.0 };
    int rampSamples = 480;                    // how long volume/mute changes take to fade (10 ms)

    // Shared with the UI thread
    rt::SnapshotExchange<TimelineSnapshot> timelineExchange;
    rt::SnapshotExchange<ProbeJob> probeExchange;
    rt::SpscQueue<Command> commands { 256 };
    std::atomic<bool> metronomeEnabled { false };
    std::atomic<float> metronomeLevelDb { -6.0f };
    std::atomic<float> metronomeGain { 0.5f };
    std::atomic<int> countInBars { 0 };
    std::atomic<bool> testToneRequested { false };

    std::atomic<bool> reportPlaying { false }, reportCountingIn { false };
    std::atomic<std::int64_t> reportTick { 0 };
    std::atomic<int> reportCountInBeatsLeft { 0 };
    std::atomic<bool> reportRecording { false };
    std::atomic<std::int64_t> reportRecordStart { 0 }, reportRecordedFrames { 0 };
    std::atomic<std::uint32_t> reportRecordingsCompleted { 0 };
    std::array<std::atomic<float>, numMeteredChannels> outputPeaks {};
    std::array<std::atomic<float>, numMeteredChannels> inputPeaks {};

    // Audio thread only
    bool playing = false;
    bool countingIn = false;
    bool recordPending = false;               // start recording as soon as playback starts
    RecordingRing* recordingInto = nullptr;   // while recording (kept alive by the snapshot)
    std::int64_t recordedFrames = 0;
    std::int64_t positionSamples = 0;   // the playhead, in samples from the start of the timeline
    double countInSamplesPerBeat = 0.0;
    int countInTotalBeats = 0, countInBeatsPerBar = 4;
    std::int64_t countInElapsed = 0;

    TestToneGenerator testTone;
    Metronome metronome;
};

} // namespace aether
