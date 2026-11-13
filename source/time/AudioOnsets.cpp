// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "AudioOnsets.h"

#include <algorithm>
#include <cmath>

namespace aether::audioonsets
{

namespace
{
    constexpr double hopSeconds = 0.005;
    constexpr double minimumGap = 0.03;   // two hits at least 30 ms apart
    constexpr double bandCentres[] { 70.0, 220.0, 650.0, 1800.0, 4500.0, 10000.0 };
    constexpr int numBands = (int) std::size (bandCentres);

    double percentile (std::vector<double> values, double fraction)
    {
        if (values.empty())
            return 0.0;

        const auto k = (size_t) std::clamp (fraction * (double) (values.size() - 1), 0.0, (double) values.size() - 1.0);
        std::nth_element (values.begin(), values.begin() + (std::ptrdiff_t) k, values.end());
        return values[k];
    }
}

std::vector<Hit> detect (const juce::AudioBuffer<float>& audio, double sampleRate)
{
    const auto length = audio.getNumSamples();
    const auto hop = std::max (1, (int) std::lround (sampleRate * hopSeconds));

    if (length < hop * 8 || audio.getNumChannels() == 0 || sampleRate <= 0.0)
        return {};

    // Both channels together.
    std::vector<float> mono ((size_t) length, 0.0f);
    for (int c = 0; c < audio.getNumChannels(); ++c)
        juce::FloatVectorOperations::addWithMultiply (mono.data(), audio.getReadPointer (c), 1.0f / (float) audio.getNumChannels(), length);

    // Each band's loudness every 5 ms (log, so a jump means the same at any level).
    const auto frames = length / hop;
    std::vector<std::vector<double>> level ((size_t) numBands, std::vector<double> ((size_t) frames, 0.0));
    std::vector<std::vector<double>> energy ((size_t) numBands, std::vector<double> ((size_t) frames, 0.0));

    for (int b = 0; b < numBands; ++b)
    {
        juce::IIRFilter filter;
        filter.setCoefficients (juce::IIRCoefficients::makeBandPass (sampleRate, std::min (bandCentres[b], sampleRate * 0.45), 1.2));

        std::vector<float> band (mono);
        filter.processSamples (band.data(), length);

        for (int f = 0; f < frames; ++f)
        {
            double sum = 0.0;
            for (int i = f * hop; i < (f + 1) * hop; ++i)
                sum += (double) band[(size_t) i] * band[(size_t) i];

            energy[(size_t) b][(size_t) f] = sum / hop;
            level[(size_t) b][(size_t) f] = std::log10 (sum / hop + 1.0e-10);
        }
    }

    // How much louder the bands got (compared with 10 ms before), added up.
    std::vector<double> novelty ((size_t) frames, 0.0), lowShare ((size_t) frames, 0.0);

    for (int f = 6; f < frames; ++f)
    {
        double total = 0.0, gained = 0.0, gainedLow = 0.0;

        for (int b = 0; b < numBands; ++b)
        {
            // Compared with the loudest it was 10 to 30 ms before, so notes ringing together
            // (their sound wobbling up and down) don't count as new hits; a hit is louder than
            // anything just before it.
            const auto& l = level[(size_t) b];
            const auto before = std::max ({ l[(size_t) f - 2], l[(size_t) f - 3], l[(size_t) f - 4], l[(size_t) f - 5], l[(size_t) f - 6] });
            const auto rise = std::max (0.0, l[(size_t) f] - before);
            // (Quiet bands don't count: below -6 (about -60 dB) a jump is only noise.)
            total += l[(size_t) f] > -6.0 ? rise : 0.0;

            // How much louder it got, in energy: how low a hit is.
            const auto& e = energy[(size_t) b];
            const auto gain = std::max (0.0, e[(size_t) f] - e[(size_t) f - 2]);
            gained += gain;
            if (b <= 1)   // below about 400 Hz
                gainedLow += gain;
        }

        novelty[(size_t) f] = total;
        lowShare[(size_t) f] = gained > 0.0 ? gainedLow / gained : 0.0;
    }

    // Peaks that stand out: above the typical level around them (half a second either side) and
    // above a floor from the whole recording.
    const auto floor = std::max (0.05, 0.25 * percentile (novelty, 0.98));
    const auto around = (int) std::lround (0.5 / hopSeconds);
    std::vector<int> peaks;

    for (int f = 2; f + 2 < frames; ++f)
    {
        const auto v = novelty[(size_t) f];

        if (v < floor || v < novelty[(size_t) f - 1] || v < novelty[(size_t) f + 1] || v < novelty[(size_t) f - 2] || v < novelty[(size_t) f + 2])
            continue;

        std::vector<double> nearby;
        for (int g = std::max (0, f - around); g < std::min (frames, f + around); g += 2)
            nearby.push_back (novelty[(size_t) g]);

        if (v < 1.5 * percentile (nearby, 0.5) + 0.02)
            continue;

        if (! peaks.empty() && (f - peaks.back()) * hopSeconds < minimumGap)
        {
            if (v > novelty[(size_t) peaks.back()])
                peaks.back() = f;
            continue;
        }

        peaks.push_back (f);
    }

    if (peaks.empty())
        return {};

    double strongest = 0.0;
    for (const auto f : peaks)
        strongest = std::max (strongest, novelty[(size_t) f]);

    // Each hit at the sample where its sound starts rising: the first moment (within 15 ms
    // before the peak frame) that it reaches a quarter of its loudness just after.
    std::vector<Hit> hits;
    const auto window = std::max (1, (int) std::lround (sampleRate * 0.001));

    for (const auto f : peaks)
    {
        const auto from = std::max (0, f * hop - 3 * hop);
        const auto to = std::min (length, (f + 2) * hop);

        auto energyAt = [&] (int i)
        {
            double sum = 0.0;
            for (int j = i; j < std::min (length, i + window); ++j)
                sum += (double) mono[(size_t) j] * mono[(size_t) j];
            return sum;
        };

        double loudest = 0.0;
        for (int i = from; i < to; i += window)
            loudest = std::max (loudest, energyAt (i));

        auto start = from;
        for (int i = from; i < to; i += window / 4 + 1)
            if (energyAt (i) >= 0.25 * loudest)
            {
                start = i;
                break;
            }

        hits.push_back ({ (juce::int64) start, (float) (novelty[(size_t) f] / strongest), (float) lowShare[(size_t) f] });
    }

    return hits;
}

std::vector<tempodetect::PlayedNote> asNotes (const std::vector<Hit>& hits, double sampleRate, double startSeconds)
{
    std::vector<tempodetect::PlayedNote> notes;

    for (const auto& h : hits)
    {
        tempodetect::PlayedNote n;
        n.seconds = startSeconds + (double) h.sample / sampleRate;
        // Loudness counts less than for MIDI (a drum kit's kick and snare are both "the beat"),
        // and low hits count a little like bass notes (for where bars start).
        n.velocity = juce::jlimit (1, 127, (int) std::lround (50.0 + 77.0 * std::sqrt (std::max (0.0f, h.strength))));
        n.pitch = h.low > 0.45f ? 48 : 64;
        n.length = 0.25;
        notes.push_back (n);
    }

    return notes;
}

} // namespace aether::audioonsets
