// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>

namespace aether
{

/** A decoded audio file, ready for the audio thread: up to 2 channels of float samples at the
    engine's sample rate. Read-only once created. */
struct AudioData
{
    juce::AudioBuffer<float> samples;
    double sampleRate = 48000.0;     // the rate of `samples` (the engine's rate)
    double sourceRate = 48000.0;     // the file's own rate
};

/** A summary of an audio file's shape, for drawing waveforms quickly at any zoom: the lowest
    and highest sample value in every block of 256 samples of the original file. */
struct WaveformPeaks
{
    static constexpr int samplesPerBin = 256;

    double sourceRate = 48000.0;
    std::int64_t sourceLength = 0;
    std::vector<std::vector<std::pair<float, float>>> bins;   // [channel][bin] = (min, max)

    int getNumChannels() const noexcept   { return (int) bins.size(); }

    /** The (min, max) of a channel over the file's samples [from, to). */
    std::pair<float, float> range (int channel, std::int64_t from, std::int64_t to) const noexcept;
};

/** The basic facts about an audio file, read from its header without decoding it. */
struct AudioFileInfo
{
    double sampleRate = 0.0;
    std::int64_t lengthInSamples = 0;
    int numChannels = 0;
    juce::String formatName;
};

/** Loads audio files into memory in the background, converts them to the engine's sample rate,
    and works out their waveform peaks.

    Everything here is used from the UI thread; the decoding happens on background threads. When
    a file finishes loading (or fails), a change message is sent, so the timeline can redraw and
    the engine can be given the new audio.

    Memory: audio is held in memory while a project is open (about 23 MB per stereo minute at
    48 kHz). Streaming very long files from disk is planned for a later milestone.
*/
class AudioFileCache final : public juce::ChangeBroadcaster
{
public:
    enum class State { notLoaded, loading, ready, failed };

    AudioFileCache();
    ~AudioFileCache() override;

    juce::AudioFormatManager& getFormats() noexcept   { return formats; }

    /** The file types that can be imported, e.g. "*.wav;*.aif;*.flac;*.mp3;*.ogg". */
    juce::String getWildcard() const;

    /** Reads a file's sample rate and length (fast: doesn't decode the audio). */
    std::optional<AudioFileInfo> readInfo (const juce::File& file);

    /** Audio is converted to this rate (the sound card's). Changing it reloads everything. */
    void setTargetSampleRate (double newRate);
    double getTargetSampleRate() const;

    /** The audio at the target rate if it's ready; otherwise starts loading it and returns null. */
    std::shared_ptr<const AudioData> getAudio (const juce::File& file);

    /** The waveform peaks if they're ready (starts loading otherwise). */
    std::shared_ptr<const WaveformPeaks> getPeaks (const juce::File& file);

    State getState (const juce::File& file) const;
    juce::String getError (const juce::File& file) const;

    /** Forgets everything (e.g. when a project closes). */
    void clear();

    /** Waits until all loading has finished (for tests). */
    void waitUntilIdle();

private:
    struct Entry
    {
        State state = State::notLoaded;
        std::shared_ptr<const AudioData> audio;
        std::shared_ptr<const WaveformPeaks> peaks;
        juce::String error;
        int generation = 0;   // which "target rate" this was loaded for
    };

    void startLoading (const juce::String& path, Entry& entry);
    void load (const juce::String& path, double rate, int generation);

    juce::AudioFormatManager formats;
    mutable std::mutex lock;
    std::map<juce::String, Entry> entries;
    double targetRate = 48000.0;
    int generation = 0;
    juce::ThreadPool loader;
};

} // namespace aether
