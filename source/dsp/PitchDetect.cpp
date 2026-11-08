// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "PitchDetect.h"

#include <cmath>
#include <vector>

namespace aether::dsp::pitch
{

int frequencyToKey (double hz) noexcept
{
    if (hz <= 0.0)
        return -1;

    return juce::jlimit (0, 127, (int) std::lround (69.0 + 12.0 * std::log2 (hz / 440.0)));
}

std::optional<double> detectFrequency (const juce::AudioBuffer<float>& sound, double sampleRate)
{
    const auto length = sound.getNumSamples();
    const auto minLag = std::max (2, (int) (sampleRate / 2000.0));
    const auto maxLag = (int) (sampleRate / 25.0);
    const auto window = std::max (maxLag * 2, (int) (sampleRate * 0.1));

    if (length < window + maxLag + 2 || sampleRate <= 0.0)
        return std::nullopt;

    // Where to listen: after the attack (sounds such as kicks sweep down in pitch at first), but
    // while it's still loud. Start at the loudest point plus 100 ms, if that leaves enough.
    int peak = 0;
    float peakLevel = 0.0f;
    for (int i = 0; i < std::min (length, (int) (sampleRate * 0.5)); ++i)
    {
        const auto x = std::abs (sound.getSample (0, i));
        if (x > peakLevel) { peakLevel = x; peak = i; }
    }

    if (peakLevel <= 1.0e-4f)
        return std::nullopt;

    auto start = std::min (peak + (int) (sampleRate * 0.1), length - window - maxLag - 1);
    start = std::max (0, start);

    // A mono mixdown of the stretch.
    std::vector<double> x ((size_t) (window + maxLag + 1));
    for (size_t i = 0; i < x.size(); ++i)
    {
        double sum = 0.0;
        for (int c = 0; c < sound.getNumChannels(); ++c)
            sum += sound.getSample (c, start + (int) i);
        x[i] = sum / sound.getNumChannels();
    }

    // YIN: the difference function, normalised, and its first clear dip.
    std::vector<double> d ((size_t) maxLag + 1, 0.0);
    for (int lag = 1; lag <= maxLag; ++lag)
    {
        double sum = 0.0;
        for (int i = 0; i < window; ++i)
        {
            const auto diff = x[(size_t) i] - x[(size_t) (i + lag)];
            sum += diff * diff;
        }
        d[(size_t) lag] = sum;
    }

    std::vector<double> normalised ((size_t) maxLag + 1, 1.0);
    double running = 0.0;
    for (int lag = 1; lag <= maxLag; ++lag)
    {
        running += d[(size_t) lag];
        normalised[(size_t) lag] = running > 0.0 ? d[(size_t) lag] * lag / running : 1.0;
    }

    int best = -1;
    for (int lag = minLag; lag < maxLag; ++lag)
    {
        if (normalised[(size_t) lag] < 0.15)
        {
            // Down to the bottom of this dip.
            while (lag + 1 < maxLag && normalised[(size_t) lag + 1] < normalised[(size_t) lag])
                ++lag;
            best = lag;
            break;
        }
    }

    if (best < 0)
        return std::nullopt;

    // Between samples: the parabola through the dip and its neighbours.
    const auto a = normalised[(size_t) best - 1], b = normalised[(size_t) best], c = normalised[(size_t) best + 1];
    const auto denominator = a - 2.0 * b + c;
    const auto shift = std::abs (denominator) > 1.0e-12 ? 0.5 * (a - c) / denominator : 0.0;
    const auto period = best + std::clamp (shift, -1.0, 1.0);

    return sampleRate / period;
}

std::optional<int> detectKey (const juce::AudioBuffer<float>& sound, double sampleRate)
{
    if (const auto hz = detectFrequency (sound, sampleRate))
        return frequencyToKey (*hz);
    return std::nullopt;
}

} // namespace aether::dsp::pitch
