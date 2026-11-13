// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "TimeStretch.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <signalsmith-stretch/signalsmith-stretch.h>

namespace aether::dsp::timestretch
{

namespace
{
    /** Channel pointers into a buffer, starting part-way in: what the stretcher reads and writes. */
    struct Channels
    {
        juce::AudioBuffer<float>& buffer;
        int offset = 0;

        float* operator[] (int channel) const   { return buffer.getWritePointer (channel, offset); }
    };
}

Source Source::of (const juce::AudioBuffer<float>& buffer)
{
    Source source;
    source.numChannels = buffer.getNumChannels();
    source.length = buffer.getNumSamples();
    source.read = [&buffer] (juce::int64 from, int count, juce::AudioBuffer<float>& out)
    {
        for (int c = 0; c < out.getNumChannels(); ++c)
            juce::FloatVectorOperations::clear (out.getWritePointer (c), count);

        const auto first = std::max<juce::int64> (from, 0);
        const auto last = std::min<juce::int64> (from + count, buffer.getNumSamples());

        for (int c = 0; c < out.getNumChannels() && last > first; ++c)
            out.copyFrom (c, (int) (first - from), buffer, std::min (c, buffer.getNumChannels() - 1), (int) first, (int) (last - first));
    };
    return source;
}

double inputAt (const std::vector<MapPoint>& map, double output) noexcept
{
    if (map.empty())
        return output;

    if (map.size() == 1)
        return map[0].input + (output - map[0].output);

    // The segment it's in (or the last one, carried on).
    size_t k = 0;
    while (k + 2 < map.size() && output >= map[k + 1].output)
        ++k;

    if (output < map[0].output)
        k = 0;

    const auto& a = map[k];
    const auto& b = map[k + 1];
    const auto span = b.output - a.output;
    return span > 0.0 ? a.input + (output - a.output) * (b.input - a.input) / span : b.input;
}

juce::AudioBuffer<float> render (const Source& source, const std::vector<MapPoint>& map, juce::int64 outputLength,
                                 const Options& options)
{
    const auto channels = std::clamp (source.numChannels, 1, 2);

    if (outputLength <= 0 || source.length <= 0 || ! source.read || outputLength > (juce::int64) std::numeric_limits<int>::max())
        return {};

    // Seeded, so the same clip always renders the same.
    signalsmith::stretch::SignalsmithStretch<float> stretch (20261113L);
    stretch.presetDefault (channels, (float) options.sampleRate);
    stretch.setTransposeSemitones ((float) options.semitones);

    const auto inputLatency = stretch.inputLatency();
    const auto outputLatency = stretch.outputLatency();
    const auto speedAt = [&map] (double output)
    {
        return std::max (0.0, inputAt (map, output + 1.0) - inputAt (map, output));
    };

    // The start, as Signalsmith's exact(): "outputSeek" with enough input that the first output
    // sample is the first input sample (no pre-roll in the output).
    const auto start = inputAt (map, 0.0);
    auto inputPosition = (juce::int64) std::floor (start);
    const auto seekLength = std::max (inputLatency, stretch.outputSeekLength ((float) std::max (0.01, speedAt (0.0))));

    juce::AudioBuffer<float> input (channels, std::max (seekLength, 4096));
    source.read (inputPosition, seekLength, input);
    stretch.outputSeek (Channels { input, 0 }, seekLength);
    inputPosition += seekLength;

    // Then a block at a time: by the end of each block, the stretcher has read the input up to
    // where the map says (plus its latency).
    juce::AudioBuffer<float> output (channels, (int) outputLength);
    output.clear();
    constexpr int block = 256;

    for (juce::int64 done = 0; done < outputLength; done += block)
    {
        if (options.shouldStop && (done / block) % 64 == 0 && options.shouldStop())
            return {};

        const auto n = (int) std::min<juce::int64> (block, outputLength - done);
        const auto wanted = (juce::int64) std::floor (inputAt (map, (double) (done + n + outputLatency))) + inputLatency;
        const auto count = (int) std::clamp<juce::int64> (wanted - inputPosition, 0, 1 << 22);

        if (count > input.getNumSamples())
            input.setSize (channels, count, false, false, true);

        source.read (inputPosition, count, input);
        stretch.process (Channels { input, 0 }, count, Channels { output, (int) done }, n);
        inputPosition += count;
    }

    return output;
}

} // namespace aether::dsp::timestretch
