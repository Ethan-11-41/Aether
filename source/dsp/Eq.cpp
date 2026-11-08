// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "Eq.h"

#include <algorithm>

#include "Presets.h"

namespace aether::dsp::eq
{

std::vector<EffectParameter> parameters()
{
    const juce::StringArray onOff { "Off", "On" };
    std::vector<EffectParameter> list {
        { "lowCutOn",      "Low cut",     0.0f, 1.0f, 0.0f, {}, onOff, false },
        { "lowCut",        "Low cut",     20.0f, 1000.0f, 80.0f, "Hz", {}, true },
        { "lowShelfFreq",  "Bass",        30.0f, 500.0f, 120.0f, "Hz", {}, true },
        { "lowShelfGain",  "Bass",        -18.0f, 18.0f, 0.0f, "dB", {}, false },
        { "band1Freq",     "Low mids",    60.0f, 2000.0f, 300.0f, "Hz", {}, true },
        { "band1Gain",     "Low mids",    -18.0f, 18.0f, 0.0f, "dB", {}, false },
        { "band1Q",        "Width",       0.2f, 10.0f, 1.0f, {}, {}, true },
        { "band2Freq",     "Mids",        200.0f, 8000.0f, 1200.0f, "Hz", {}, true },
        { "band2Gain",     "Mids",        -18.0f, 18.0f, 0.0f, "dB", {}, false },
        { "band2Q",        "Width",       0.2f, 10.0f, 1.0f, {}, {}, true },
        { "band3Freq",     "High mids",   800.0f, 16000.0f, 4000.0f, "Hz", {}, true },
        { "band3Gain",     "High mids",   -18.0f, 18.0f, 0.0f, "dB", {}, false },
        { "band3Q",        "Width",       0.2f, 10.0f, 1.0f, {}, {}, true },
        { "highShelfFreq", "Treble",      1500.0f, 18000.0f, 8000.0f, "Hz", {}, true },
        { "highShelfGain", "Treble",      -18.0f, 18.0f, 0.0f, "dB", {}, false },
        { "highCutOn",     "High cut",    0.0f, 1.0f, 0.0f, {}, onOff, false },
        { "highCut",       "High cut",    1000.0f, 20000.0f, 16000.0f, "Hz", {}, true },
        { "output",        "Output",      -18.0f, 18.0f, 0.0f, "dB", {}, false },
    };
    jassert ((int) list.size() == numParameters);
    return list;
}

std::vector<EffectPreset> presets()
{
    const auto p = parameters();
    return {
        makePreset (p, "Flat (start here)", "General", {}),
        makePreset (p, "Vocal: clarity", "Vocals", { { "lowCutOn", 1 }, { "lowCut", 100 }, { "band1Gain", -3 }, { "band1Freq", 300 },
                                                     { "band3Freq", 4000 }, { "band3Gain", 2.5f }, { "band3Q", 0.8f },
                                                     { "highShelfFreq", 10000 }, { "highShelfGain", 2 } }),
        makePreset (p, "Vocal: warm", "Vocals", { { "lowCutOn", 1 }, { "lowCut", 80 }, { "lowShelfFreq", 200 }, { "lowShelfGain", 2 },
                                                  { "band2Freq", 2500 }, { "band2Gain", -1.5f }, { "highShelfGain", -1.5f } }),
        makePreset (p, "Vocal: remove boxiness", "Vocals", { { "lowCutOn", 1 }, { "lowCut", 90 }, { "band1Freq", 450 }, { "band1Gain", -4 },
                                                             { "band1Q", 1.4f } }),
        makePreset (p, "Vocal: telephone", "Vocals", { { "lowCutOn", 1 }, { "lowCut", 500 }, { "highCutOn", 1 }, { "highCut", 3000 },
                                                       { "band2Freq", 1500 }, { "band2Gain", 6 }, { "output", -3 } }),
        makePreset (p, "Piano: bright", "Instruments", { { "lowCutOn", 1 }, { "lowCut", 40 }, { "band1Freq", 250 }, { "band1Gain", -2 },
                                                         { "highShelfGain", 3 }, { "highShelfFreq", 6000 } }),
        makePreset (p, "Piano: warm", "Instruments", { { "lowShelfGain", 2 }, { "lowShelfFreq", 150 }, { "band3Freq", 3500 },
                                                       { "band3Gain", -2 }, { "highShelfGain", -3 } }),
        makePreset (p, "Strings: air", "Instruments", { { "lowCutOn", 1 }, { "lowCut", 60 }, { "band1Freq", 400 }, { "band1Gain", -2 },
                                                        { "highShelfFreq", 9000 }, { "highShelfGain", 3 } }),
        makePreset (p, "Make room in the mix", "Instruments", { { "lowCutOn", 1 }, { "lowCut", 150 }, { "band2Freq", 2500 },
                                                                { "band2Gain", -3 }, { "band2Q", 0.7f } }),
        makePreset (p, "Master: gentle polish", "Master", { { "lowCutOn", 1 }, { "lowCut", 25 }, { "lowShelfFreq", 90 }, { "lowShelfGain", 1 },
                                                            { "band1Freq", 300 }, { "band1Gain", -1 }, { "band1Q", 0.7f },
                                                            { "highShelfFreq", 10000 }, { "highShelfGain", 1.5f } }),
        makePreset (p, "Master: brighter", "Master", { { "lowCutOn", 1 }, { "lowCut", 25 }, { "highShelfFreq", 8000 }, { "highShelfGain", 2.5f } }),
    };
}

std::array<BiquadCoefficients, numFilters> design (const EffectParameterValues& v, double sampleRate)
{
    std::array<BiquadCoefficients, numFilters> f {};   // flat unless set below

    if (v[lowCutOn] >= 0.5f)
        f[0] = BiquadCoefficients::highPass (v[lowCut], 0.7071, sampleRate);
    if (std::abs (v[lowShelfGain]) > 0.01f)
        f[1] = BiquadCoefficients::lowShelf (v[lowShelfFreq], v[lowShelfGain], sampleRate);

    const std::array<std::array<int, 3>, 3> bands { { { band1Freq, band1Gain, band1Q }, { band2Freq, band2Gain, band2Q },
                                                      { band3Freq, band3Gain, band3Q } } };
    for (size_t b = 0; b < bands.size(); ++b)
        if (std::abs (v[(size_t) bands[b][1]]) > 0.01f)
            f[2 + b] = BiquadCoefficients::peak (v[(size_t) bands[b][0]], v[(size_t) bands[b][2]], v[(size_t) bands[b][1]], sampleRate);

    if (std::abs (v[highShelfGain]) > 0.01f)
        f[5] = BiquadCoefficients::highShelf (v[highShelfFreq], v[highShelfGain], sampleRate);
    if (v[highCutOn] >= 0.5f)
        f[6] = BiquadCoefficients::lowPass (v[highCut], 0.7071, sampleRate);

    return f;
}

double responseDb (const EffectParameterValues& values, double hz, double sampleRate)
{
    double total = values[output];
    for (const auto& f : design (values, sampleRate))
        total += f.magnitudeDb (hz, sampleRate);
    return total;
}

namespace
{
    bool isFlat (const BiquadCoefficients& c) noexcept
    {
        return c.b0 == 1.0 && c.b1 == 0.0 && c.b2 == 0.0 && c.a1 == 0.0 && c.a2 == 0.0;
    }

    class Eq final : public Effect
    {
    public:
        void prepare (double newSampleRate, int) override
        {
            sampleRate = newSampleRate;
            for (auto& s : states)
                s.reset();
            first = true;
        }

        void process (float* left, float* right, int numSamples, const EffectParameterValues& values,
                      const EffectContext&) noexcept AETHER_NONBLOCKING override
        {
            // Settings glide (a few ms per 32-sample step) so dragging a band doesn't crackle.
            for (int done = 0; done < numSamples; done += step)
            {
                const auto n = std::min (step, numSamples - done);
                if (glide (values))
                    filters = design (current, sampleRate);   // (only when something moved)

                const auto gain = (float) std::pow (10.0, current[output] / 20.0);

                for (size_t f = 0; f < filters.size(); ++f)
                {
                    if (isFlat (filters[f]))
                        continue;

                    auto& l = states[f * 2];
                    auto& r = states[f * 2 + 1];
                    for (int i = done; i < done + n; ++i)
                    {
                        left[i] = l.process (filters[f], left[i]);
                        right[i] = r.process (filters[f], right[i]);
                    }
                }

                for (int i = done; i < done + n; ++i)
                {
                    left[i] *= gain;
                    right[i] *= gain;
                }
            }
        }

    private:
        static constexpr int step = 32;

        /** Moves the settings towards `target`; true if anything changed. */
        bool glide (const EffectParameterValues& target) noexcept
        {
            if (first)
            {
                current = target;
                first = false;
                return true;
            }

            if (current == target)
                return false;

            constexpr double amount = 0.25;   // about 3 ms per step at 48 kHz
            for (int i = 0; i < numParameters; ++i)
            {
                const auto t = target[(size_t) i];
                auto& c = current[(size_t) i];

                if (i == lowCutOn || i == highCutOn)
                    c = t;   // switches switch
                else if (std::abs (t - c) < 1.0e-4f * std::max (1.0f, std::abs (t)))
                    c = t;
                else if (i == lowCut || i == highCut || i == lowShelfFreq || i == highShelfFreq
                         || i == band1Freq || i == band2Freq || i == band3Freq)
                    c = (float) std::exp (std::log ((double) c) + (std::log ((double) t) - std::log ((double) c)) * amount);   // in octaves
                else
                    c += (float) ((t - c) * amount);
            }

            return true;
        }

        double sampleRate = 48000.0;
        std::array<BiquadState, numFilters * 2> states {};
        std::array<BiquadCoefficients, numFilters> filters {};
        EffectParameterValues current {};
        bool first = true;
    };
}

std::unique_ptr<Effect> create()   { return std::make_unique<Eq>(); }

} // namespace aether::dsp::eq
