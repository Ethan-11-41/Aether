// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "LatencyProbe.h"

#include <algorithm>
#include <cmath>

namespace aether
{

namespace
{
    constexpr double chirpSeconds = 0.010;   // each chirp: 10 ms, sweeping 500 Hz to 6 kHz
    constexpr double spacingSeconds = 0.5;   // one every half second
    constexpr double maximumLatencySeconds = 0.45;
}

LatencyCapture::LatencyCapture (double rate, int channel)
    : sampleRate (rate > 0.0 ? rate : 48000.0), inputChannel (channel)
{
    // A chirp (a quick rising sweep) has one clear peak when matched against itself, so its
    // arrival time can be found to the exact sample, even in a noisy room.
    const auto length = (int) std::lround (chirpSeconds * sampleRate);
    chirp.resize ((size_t) length);

    for (int i = 0; i < length; ++i)
    {
        const auto t = i / sampleRate;
        const auto sweep = 500.0 + (6000.0 - 500.0) * (t / chirpSeconds) * 0.5;   // instantaneous phase rate
        const auto window = 0.5 - 0.5 * std::cos (2.0 * juce::MathConstants<double>::pi * i / (length - 1));
        chirp[(size_t) i] = (float) (0.5 * window * std::sin (2.0 * juce::MathConstants<double>::pi * sweep * t));
    }

    const auto spacing = (std::int64_t) std::llround (spacingSeconds * sampleRate);
    for (int k = 0; k < numChirps; ++k)
        chirpStarts.push_back (spacing / 2 + k * spacing);

    captured.assign ((size_t) (chirpStarts.back() + spacing), 0.0f);
}

void LatencyCapture::process (const float* const* inputs, int numInputs, float* const* outputs, int numOutputs,
                              int numSamples) noexcept AETHER_NONBLOCKING
{
    if (finished.load (std::memory_order_relaxed))
        return;

    const auto start = position.load (std::memory_order_relaxed);
    const auto total = (std::int64_t) captured.size();
    const auto count = (int) std::min<std::int64_t> (numSamples, total - start);

    // The output: the chirps, and silence in between (nothing else plays during a measurement).
    for (int ch = 0; ch < numOutputs; ++ch)
    {
        if (outputs[ch] == nullptr)
            continue;

        for (int i = 0; i < numSamples; ++i)
        {
            float value = 0.0f;

            if (ch < 2 && i < count)
            {
                const auto t = start + i;
                for (const auto chirpStart : chirpStarts)
                    if (t >= chirpStart && t < chirpStart + (std::int64_t) chirp.size())
                        value = chirp[(size_t) (t - chirpStart)];
            }

            outputs[ch][i] = value;
        }
    }

    // The input, at the same moments.
    const float* in = (inputs != nullptr && inputChannel >= 0 && inputChannel < numInputs) ? inputs[inputChannel] : nullptr;

    for (int i = 0; i < count; ++i)
        captured[(size_t) (start + i)] = in != nullptr ? in[i] : 0.0f;

    position.store (start + count, std::memory_order_relaxed);

    if (start + count >= total)
        finished.store (true, std::memory_order_release);
}

LatencyMeasurement analyseLatencyCapture (const LatencyCapture& capture)
{
    LatencyMeasurement result;

    if (! capture.finished.load (std::memory_order_acquire))
    {
        result.problem = "The measurement didn't finish (is the sound card running?).";
        return result;
    }

    const auto& chirp = capture.chirp;
    const auto& x = capture.captured;
    const auto chirpLength = (std::int64_t) chirp.size();
    const auto maxLag = (std::int64_t) std::lround (maximumLatencySeconds * capture.sampleRate);

    double chirpEnergy = 0.0;
    for (const auto v : chirp)
        chirpEnergy += (double) v * v;

    // For each chirp, find where it best matches the recording (normalised cross-correlation).
    std::vector<std::int64_t> lags;
    std::vector<double> scores;

    for (const auto start : capture.chirpStarts)
    {
        std::int64_t bestLag = 0;
        double best = 0.0;

        for (std::int64_t lag = 0; lag < maxLag; ++lag)
        {
            const auto from = start + lag;
            if (from + chirpLength > (std::int64_t) x.size())
                break;

            double dot = 0.0, energy = 0.0;
            for (std::int64_t i = 0; i < chirpLength; ++i)
            {
                const auto s = (double) x[(size_t) (from + i)];
                dot += s * chirp[(size_t) i];
                energy += s * s;
            }

            const auto score = energy > 1.0e-12 ? std::abs (dot) / std::sqrt (energy * chirpEnergy) : 0.0;

            if (score > best)
            {
                best = score;
                bestLag = lag;
            }
        }

        lags.push_back (bestLag);
        scores.push_back (best);
    }

    // The chirps must have been heard clearly...
    auto sortedScores = scores;
    std::sort (sortedScores.begin(), sortedScores.end());
    if (sortedScores[sortedScores.size() / 2] < 0.5)
    {
        result.problem = "Aether couldn't hear its test sounds. Turn the speakers up a little (or connect the output "
                         "to the input with a cable), check the right input is chosen, and try again.";
        return result;
    }

    // ...and mostly agree with each other.
    auto sortedLags = lags;
    std::sort (sortedLags.begin(), sortedLags.end());
    const auto median = sortedLags[sortedLags.size() / 2];
    const auto agreeing = std::count_if (lags.begin(), lags.end(), [median] (std::int64_t l) { return std::abs (l - median) <= 3; });

    if (agreeing * 10 < (std::int64_t) lags.size() * 6)
    {
        result.problem = "The measurements didn't agree with each other (a noisy room?). Try again somewhere quieter, "
                         "or with a cable from the output to the input.";
        return result;
    }

    result.ok = true;
    result.roundTripSamples = (int) median;
    return result;
}

} // namespace aether
