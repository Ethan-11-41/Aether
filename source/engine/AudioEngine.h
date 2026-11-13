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

/** A MIDI message played live (on a keyboard, or the on-screen one), with when it arrived. */
struct LiveMidiMessage
{
    double timeMs = 0.0;               // juce::Time::getMillisecondCounterHiRes() when it arrived
    std::uint8_t data[3] {};
    std::uint8_t size = 0;
};

/** A note previewed on one track (see AudioEngine::sendPreviewNote). */
struct PreviewNote
{
    std::uint64_t track = 0;     // AudioEngine::hashTrackId
    std::uint8_t pitch = 60, velocity = 100;
    bool on = true;
};

/** A MIDI message captured while recording: `index` counts samples from the start of the
    recording (like a position in a recorded audio file). */
struct RecordedMidiMessage
{
    std::int64_t index = 0;
    std::uint8_t data[3] {};
    std::uint8_t size = 0;
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
class AudioEngine final : public juce::AudioIODeviceCallback,
                          public juce::MidiInputCallback
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

    /** Plays a MIDI message as if from a keyboard (UI thread: the on-screen keyboard). */
    void sendLiveMidi (const juce::MidiMessage& message);

    /** Plays (on = true) or stops a note on one track's instrument, e.g. while editing notes in
        the piano roll. Unlike sendLiveMidi(), it goes only to that track and is never recorded. */
    void sendPreviewNote (const juce::String& trackId, int pitch, int velocity, bool on);

    /** How a track is recognised by sendPreviewNote() (computed when the snapshot is built). */
    static std::uint64_t hashTrackId (const juce::String& trackId) noexcept   { return (std::uint64_t) trackId.hashCode64(); }

    /** Goes up by one for every MIDI message that arrives (for an activity light). */
    std::uint32_t getMidiActivity() const noexcept   { return midiActivity.load (std::memory_order_relaxed); }

    /** UI thread: MIDI captured while recording, in order. Returns false when there's no more. */
    bool takeRecordedMidi (RecordedMidiMessage& message)   { return recordedMidi.tryPop (message); }

    /** juce::MidiInputCallback: a message from a MIDI device (on a MIDI thread). */
    void handleIncomingMidiMessage (juce::MidiInput* source, const juce::MidiMessage& message) override;

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
    static constexpr int maxLiveEvents = 256;      // live MIDI messages per block
    static constexpr int maxNoteEvents = 1024;     // note events per channel per chunk
    static constexpr int maxSegments = 32;         // pieces of a chunk between loop jumps

    /** A part of a chunk the transport played continuously: timeline [from, to) at `offset`. */
    struct Segment
    {
        std::int64_t from = 0, to = 0;
        int offset = 0;
        bool afterJump = false;        // the playhead jumped (loop) just before this
    };

    struct LiveEvent
    {
        int offset = 0;                // sample within the block
        dsp::NoteEvent event;
        LiveMidiMessage raw;
    };

    /** A latency measurement waiting for (or being run by) the audio thread. */
    struct ProbeJob
    {
        std::shared_ptr<LatencyCapture> capture;
    };

    void handleCommands (const TimelineSnapshot& timeline) noexcept AETHER_NONBLOCKING;
    void processChunk (const TimelineSnapshot& timeline, const float* const* inputs, int numInputs,
                       float* const* outputs, int numOutputs, int numSamples, int chunkStart) noexcept AETHER_NONBLOCKING;
    void stopRecording() noexcept AETHER_NONBLOCKING;
    void collectLiveMidi (int numSamples) noexcept AETHER_NONBLOCKING;
    void applyMidiFades (const TrackPlayback& track, int numSamples, bool playedLive) noexcept AETHER_NONBLOCKING;
    void playInstruments (const TimelineSnapshot& timeline, int chunkStart, int numSamples) noexcept AETHER_NONBLOCKING;
    void recordMidi (int chunkStart, int recordFrom, int numSamples, std::int64_t recordBase) noexcept AETHER_NONBLOCKING;
    void monitorInputs (const TimelineSnapshot& timeline, const float* const* inputs, int numInputs, int numSamples) noexcept AETHER_NONBLOCKING;
    void startCountIn (const TimelineSnapshot& timeline) noexcept AETHER_NONBLOCKING;
    int runCountIn (int numSamples) noexcept AETHER_NONBLOCKING;
    void runPlayback (const TimelineSnapshot& timeline, int blockOffset, int numSamples) noexcept AETHER_NONBLOCKING;
    void clickBeats (const TempoMap& tempo, std::int64_t fromSample, std::int64_t toSample, int blockOffset) noexcept AETHER_NONBLOCKING;
    void renderClips (const TimelineSnapshot& timeline, std::int64_t fromSample, std::int64_t toSample,
                      int blockOffset) noexcept AETHER_NONBLOCKING;
    void mixChannels (const TimelineSnapshot& timeline, float* const* outputs, int numOutputs, int numSamples) noexcept AETHER_NONBLOCKING;
    /** Runs one effect on a channel's sound (with these settings: automation may have changed
        them), fading in or out when it's switched on or off. */
    static void runInsert (const InsertPlayback& insert, const dsp::EffectParameterValues& parameters, bool bypassed,
                           float* left, float* right, int numSamples, const dsp::EffectContext& context) noexcept AETHER_NONBLOCKING;
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
    rt::SpscQueue<LiveMidiMessage> deviceMidi { 2048 };     // from MIDI devices (JUCE calls one at a time)
    rt::SpscQueue<LiveMidiMessage> uiMidi { 512 };          // from the on-screen keyboard
    static constexpr int maxPreviewNotes = 64;
    rt::SpscQueue<PreviewNote> previewQueue { 256 };
    std::array<PreviewNote, maxPreviewNotes> previewNotes {};
    int numPreviewNotes = 0;
    rt::SpscQueue<RecordedMidiMessage> recordedMidi { 8192 };
    std::atomic<std::uint32_t> midiActivity { 0 };
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
    bool stopClipNotes = false;               // playback stopped or jumped: release clip notes
    std::array<Segment, maxSegments> segments {};
    int numSegments = 0;
    std::array<LiveEvent, maxLiveEvents> liveEvents {};
    int numLiveEvents = 0;
    std::array<dsp::NoteEvent, maxNoteEvents> noteEvents {};
    std::int64_t positionSamples = 0;   // the playhead, in samples from the start of the timeline
    std::int64_t automationSample = 0;                              // where automation is read this chunk (the playhead at its start)
    dsp::EffectParameterValues automatedInstrumentParameters {};   // an instrument's automated settings (kept here, not on the stack)
    double countInSamplesPerBeat = 0.0;
    int countInTotalBeats = 0, countInBeatsPerBar = 4;
    std::int64_t countInElapsed = 0;

    TestToneGenerator testTone;
    Metronome metronome;
};

} // namespace aether
