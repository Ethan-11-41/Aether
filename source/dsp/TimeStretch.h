// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <functional>
#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>

/** Time-stretching and pitch-shifting (Milestone 17), using Signalsmith Stretch (MIT licence,
    see CREDITS.md): a recording played faster or slower without its pitch changing, or higher or
    lower without its speed changing.

    Not for the audio thread: it allocates and does a lot of work (a few seconds of audio take a
    fraction of a second). Aether renders stretched clips in the background (engine/StretchCache)
    and the audio thread plays the result like any other audio.
*/
namespace aether::dsp::timestretch
{

/** One point of a time map: output sample `output` plays the input at `input` (samples, may be
    fractional). Between points the speed is constant; past the last point it carries on at the
    last segment's speed. */
struct MapPoint
{
    double output = 0.0;
    double input = 0.0;
};

/** Where the input comes from: `read (from, count, buffer)` fills `buffer` (at least `count`
    samples long, `numChannels` channels) with input samples [from, from + count); anything
    outside [0, length) must be silence. */
struct Source
{
    int numChannels = 2;
    juce::int64 length = 0;
    std::function<void (juce::int64 from, int count, juce::AudioBuffer<float>& buffer)> read;

    /** A source reading a buffer (silence outside it). */
    static Source of (const juce::AudioBuffer<float>& buffer);
};

struct Options
{
    double sampleRate = 48000.0;
    double semitones = 0.0;          // pitch shift (fractions are cents)

    /** Asked now and then; returning true stops (render() then returns an empty buffer). */
    std::function<bool()> shouldStop;
};

/** Renders `outputLength` samples of `source` following `map` (rising in both output and
    input; its first point's output should be 0). The result is always the same for the same
    input (no randomness). Empty if stopped, or if there's nothing to render. */
juce::AudioBuffer<float> render (const Source& source, const std::vector<MapPoint>& map, juce::int64 outputLength,
                                 const Options& options);

/** Where the map puts output sample `output` (in input samples). */
double inputAt (const std::vector<MapPoint>& map, double output) noexcept;

} // namespace aether::dsp::timestretch
