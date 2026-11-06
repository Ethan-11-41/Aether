// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "AudioFileCache.h"

#include <algorithm>

#include "Resampler.h"

namespace aether
{

std::pair<float, float> WaveformPeaks::range (int channel, std::int64_t from, std::int64_t to) const noexcept
{
    if (! juce::isPositiveAndBelow (channel, getNumChannels()) || to <= from)
        return { 0.0f, 0.0f };

    const auto& data = bins[(size_t) channel];
    const auto first = std::clamp<std::int64_t> (from / samplesPerBin, 0, (std::int64_t) data.size() - 1);
    const auto last = std::clamp<std::int64_t> ((to - 1) / samplesPerBin, first, (std::int64_t) data.size() - 1);

    auto lo = data[(size_t) first].first, hi = data[(size_t) first].second;
    for (auto i = first + 1; i <= last; ++i)
    {
        lo = std::min (lo, data[(size_t) i].first);
        hi = std::max (hi, data[(size_t) i].second);
    }

    return { lo, hi };
}

//==============================================================================
AudioFileCache::AudioFileCache()
    : loader (juce::ThreadPoolOptions{}.withThreadName ("Aether audio loader").withNumberOfThreads (2))
{
    formats.registerBasicFormats();
}

AudioFileCache::~AudioFileCache()
{
    loader.removeAllJobs (true, 10000);
}

juce::String AudioFileCache::getWildcard() const
{
    return formats.getWildcardForAllFormats();
}

std::optional<AudioFileInfo> AudioFileCache::readInfo (const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));

    if (reader == nullptr || reader->sampleRate <= 0.0 || reader->lengthInSamples <= 0)
        return std::nullopt;

    return AudioFileInfo { reader->sampleRate, reader->lengthInSamples, (int) reader->numChannels, reader->getFormatName() };
}

void AudioFileCache::setTargetSampleRate (double newRate)
{
    if (newRate <= 0.0)
        return;

    {
        std::lock_guard<std::mutex> guard (lock);

        if (newRate == targetRate)
            return;

        targetRate = newRate;
        ++generation;

        // Everything that was loaded needs converting again; keep the peaks meanwhile.
        for (auto& [path, entry] : entries)
            if (entry.state != State::failed)
                startLoading (path, entry);
    }

    sendChangeMessage();
}

double AudioFileCache::getTargetSampleRate() const
{
    std::lock_guard<std::mutex> guard (lock);
    return targetRate;
}

void AudioFileCache::startLoading (const juce::String& path, Entry& entry)
{
    // (called with the lock held)
    entry.state = State::loading;
    entry.generation = generation;
    const auto rate = targetRate;
    const auto gen = generation;
    loader.addJob ([this, path, rate, gen] { load (path, rate, gen); });
}

std::shared_ptr<const AudioData> AudioFileCache::getAudio (const juce::File& file)
{
    std::lock_guard<std::mutex> guard (lock);
    auto& entry = entries[file.getFullPathName()];

    if (entry.state == State::notLoaded)
        startLoading (file.getFullPathName(), entry);

    if (entry.state == State::ready && entry.audio != nullptr && entry.audio->sampleRate == targetRate)
        return entry.audio;

    return nullptr;
}

std::shared_ptr<const WaveformPeaks> AudioFileCache::getPeaks (const juce::File& file)
{
    std::lock_guard<std::mutex> guard (lock);
    auto& entry = entries[file.getFullPathName()];

    if (entry.state == State::notLoaded)
        startLoading (file.getFullPathName(), entry);

    return entry.peaks;
}

AudioFileCache::State AudioFileCache::getState (const juce::File& file) const
{
    std::lock_guard<std::mutex> guard (lock);
    const auto it = entries.find (file.getFullPathName());
    return it == entries.end() ? State::notLoaded : it->second.state;
}

juce::String AudioFileCache::getError (const juce::File& file) const
{
    std::lock_guard<std::mutex> guard (lock);
    const auto it = entries.find (file.getFullPathName());
    return it == entries.end() ? juce::String() : it->second.error;
}

void AudioFileCache::clear()
{
    loader.removeAllJobs (true, 10000);
    std::lock_guard<std::mutex> guard (lock);
    entries.clear();
    ++generation;
}

void AudioFileCache::waitUntilIdle()
{
    while (loader.getNumJobs() > 0)
        juce::Thread::sleep (2);
}

void AudioFileCache::load (const juce::String& path, double rate, int gen)
{
    // Runs on a background thread.
    auto finish = [&] (std::shared_ptr<const AudioData> audio, std::shared_ptr<const WaveformPeaks> peaks, const juce::String& error)
    {
        {
            std::lock_guard<std::mutex> guard (lock);
            auto it = entries.find (path);

            if (it == entries.end() || it->second.generation != gen)
                return;   // something newer is on its way (e.g. the sample rate changed again)

            auto& entry = it->second;
            entry.state = error.isEmpty() ? State::ready : State::failed;
            entry.audio = std::move (audio);
            if (peaks != nullptr)
                entry.peaks = std::move (peaks);
            entry.error = error;
        }

        sendChangeMessage();
    };

    const juce::File file (path);

    if (! file.existsAsFile())
    {
        finish (nullptr, nullptr, "The audio file is missing: " + path);
        return;
    }

    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));

    if (reader == nullptr || reader->lengthInSamples <= 0)
    {
        finish (nullptr, nullptr, "Aether can't read this file: " + file.getFileName());
        return;
    }

    try
    {
        const auto channels = (int) std::min (2u, reader->numChannels);
        const auto length = (int) std::min<juce::int64> (reader->lengthInSamples, std::numeric_limits<int>::max() - 1);

        juce::AudioBuffer<float> original (channels, length);
        reader->read (&original, 0, length, 0, true, channels > 1);

        // Waveform peaks from the original samples.
        auto peaks = std::make_shared<WaveformPeaks>();
        peaks->sourceRate = reader->sampleRate;
        peaks->sourceLength = length;
        peaks->bins.resize ((size_t) channels);

        for (int ch = 0; ch < channels; ++ch)
        {
            const auto* data = original.getReadPointer (ch);
            auto& bins = peaks->bins[(size_t) ch];
            bins.reserve ((size_t) (length / WaveformPeaks::samplesPerBin + 1));

            for (int start = 0; start < length; start += WaveformPeaks::samplesPerBin)
            {
                const auto end = std::min (length, start + WaveformPeaks::samplesPerBin);
                const auto [lo, hi] = std::minmax_element (data + start, data + end);
                bins.emplace_back (*lo, *hi);
            }
        }

        // Convert to the engine's rate if needed.
        auto audio = std::make_shared<AudioData>();
        audio->sampleRate = rate;
        audio->sourceRate = reader->sampleRate;

        if (reader->sampleRate == rate)
        {
            audio->samples = std::move (original);
        }
        else
        {
            const auto newLength = (int) resampler::outputLength (length, reader->sampleRate, rate);
            audio->samples.setSize (channels, newLength);

            for (int ch = 0; ch < channels; ++ch)
            {
                const auto converted = resampler::process (original.getReadPointer (ch), length, reader->sampleRate, rate);
                audio->samples.copyFrom (ch, 0, converted.data(), newLength);
            }
        }

        finish (std::move (audio), std::move (peaks), {});
    }
    catch (const std::bad_alloc&)
    {
        finish (nullptr, nullptr, "Not enough memory to load " + file.getFileName());
    }
}

} // namespace aether
