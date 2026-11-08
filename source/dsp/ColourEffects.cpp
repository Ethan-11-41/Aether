// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "ColourEffects.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <vector>

#include "Presets.h"

namespace aether::dsp
{

namespace
{
    float dryGainForMix (float mix) noexcept   { return std::min (1.0f, 2.0f * (1.0f - mix)); }
    float wetGainForMix (float mix) noexcept   { return std::min (1.0f, 2.0f * mix); }
    constexpr double twoPi = juce::MathConstants<double>::twoPi;

    /** A value that glides to where it's set, over about `ms` (so knob moves don't crackle). */
    struct Smoothed
    {
        double value = 0.0, coefficient = 0.0;
        bool started = false;

        void prepare (double ms, double sampleRate) noexcept   { coefficient = std::exp (-1.0 / std::max (1.0, ms * 0.001 * sampleRate)); started = false; }
        double next (double target) noexcept
        {
            if (! started) { value = target; started = true; }
            value = target + coefficient * (value - target);
            return value;
        }
    };
}

//==============================================================================
namespace chorus
{

std::vector<EffectParameter> parameters()
{
    std::vector<EffectParameter> list {
        { "rate",  "Rate",  0.05f, 5.0f, 0.8f, "Hz", {}, true },
        { "depth", "Depth", 0.0f, 100.0f, 40.0f, "%", {}, false },
        { "delay", "Delay", 5.0f, 30.0f, 12.0f, "ms", {}, false },
        { "width", "Width", 0.0f, 100.0f, 100.0f, "%", {}, false },
        { "mix",   "Mix",   0.0f, 100.0f, 50.0f, "%", {}, false },
    };
    jassert ((int) list.size() == numParameters);
    return list;
}

std::vector<EffectPreset> presets()
{
    const auto p = parameters();
    return {
        makePreset (p, "Gentle (start here)", "General", {}),
        makePreset (p, "Vocal: doubled", "Vocals", { { "rate", 0.4f }, { "depth", 25 }, { "delay", 18 }, { "mix", 30 } }),
        makePreset (p, "Piano: shimmer", "Instruments", { { "rate", 0.6f }, { "depth", 35 }, { "delay", 10 }, { "mix", 35 } }),
        makePreset (p, "Pad: wide", "Instruments", { { "rate", 0.3f }, { "depth", 60 }, { "delay", 15 }, { "mix", 50 } }),
        makePreset (p, "Vibrato-ish", "Instruments", { { "rate", 4.5f }, { "depth", 30 }, { "delay", 6 }, { "width", 0 }, { "mix", 100 } }),
    };
}

namespace
{
    class Chorus final : public Effect
    {
    public:
        void prepare (double newSampleRate, int) override
        {
            sampleRate = newSampleRate;
            size = (int) std::ceil (sampleRate * 0.05) + 4;   // up to 50 ms
            bufferLeft.assign ((size_t) size, 0.0f);
            bufferRight.assign ((size_t) size, 0.0f);
            write = 0;
            phase = 0.0;
            for (auto* s : { &delayMs, &depthMs, &widthAmount, &mixAmount })
                s->prepare (20.0, sampleRate);
        }

        void process (float* left, float* right, int numSamples, const EffectParameterValues& v,
                      const EffectContext&) noexcept AETHER_NONBLOCKING override
        {
            if (size <= 4)
                return;

            const auto increment = std::clamp ((double) v[rate], 0.01, 20.0) / sampleRate;

            for (int i = 0; i < numSamples; ++i)
            {
                const auto base = delayMs.next (v[delay]);
                const auto swing = depthMs.next (v[depth] / 100.0 * base * 0.5);
                const auto spread = widthAmount.next (v[width] / 100.0);
                const auto m = (float) mixAmount.next (v[mix] / 100.0);

                // The two sides waver a quarter-cycle apart (with full width), which spreads them.
                const auto lfoLeft = std::sin (twoPi * phase);
                const auto lfoRight = std::sin (twoPi * (phase + 0.25 * spread));
                phase += increment;
                if (phase >= 1.0) phase -= 1.0;

                bufferLeft[(size_t) write] = left[i];
                bufferRight[(size_t) write] = right[i];

                const auto wetLeft = read (bufferLeft, (base + swing * lfoLeft) * 0.001 * sampleRate);
                const auto wetRight = read (bufferRight, (base + swing * lfoRight) * 0.001 * sampleRate);
                write = write + 1 == size ? 0 : write + 1;

                // (An equal-power balance: the copy is so like the original that a plain one would
                // make it louder.)
                const auto dryGain = std::cos (m * juce::MathConstants<float>::halfPi), wetGain = std::sin (m * juce::MathConstants<float>::halfPi);
                left[i] = left[i] * dryGain + wetLeft * wetGain;
                right[i] = right[i] * dryGain + wetRight * wetGain;
            }
        }

    private:
        float read (const std::vector<float>& buffer, double delaySamples) const noexcept
        {
            auto position = (double) write - std::clamp (delaySamples, 1.0, (double) size - 3);
            while (position < 0.0) position += size;
            auto index = (int) position;
            if (index >= size) { index -= size; position -= size; }   // (a tiny negative position can round up to `size`)
            const auto fraction = (float) (position - index);
            const auto next = index + 1 == size ? 0 : index + 1;
            return buffer[(size_t) index] + fraction * (buffer[(size_t) next] - buffer[(size_t) index]);
        }

        double sampleRate = 48000.0, phase = 0.0;
        int size = 0, write = 0;
        std::vector<float> bufferLeft, bufferRight;
        Smoothed delayMs, depthMs, widthAmount, mixAmount;
    };
}

std::unique_ptr<Effect> create()   { return std::make_unique<Chorus>(); }

} // namespace chorus

//==============================================================================
namespace saturation
{

std::vector<EffectParameter> parameters()
{
    std::vector<EffectParameter> list {
        { "drive",  "Drive",  0.0f, 36.0f, 6.0f, "dB", {}, false },
        { "character", "Type", 0.0f, 3.0f, 0.0f, {}, { "Tape", "Tube", "Fuzz", "Hard clip" }, false },
        { "tone",   "Tone",   1000.0f, 20000.0f, 20000.0f, "Hz", {}, true },
        { "mix",    "Mix",    0.0f, 100.0f, 100.0f, "%", {}, false },
        { "output", "Output", -24.0f, 6.0f, 0.0f, "dB", {}, false },
    };
    jassert ((int) list.size() == numParameters);
    return list;
}

std::vector<EffectPreset> presets()
{
    const auto p = parameters();
    return {
        makePreset (p, "Warm tape (start here)", "General", { { "drive", 6 }, { "character", tape } }),
        makePreset (p, "Vocal: warmth", "Vocals", { { "drive", 8 }, { "character", tube }, { "tone", 12000 }, { "mix", 40 } }),
        makePreset (p, "Vocal: gritty", "Vocals", { { "drive", 18 }, { "character", tube }, { "tone", 9000 }, { "mix", 60 }, { "output", -4 } }),
        makePreset (p, "Piano: tape", "Instruments", { { "drive", 9 }, { "character", tape }, { "tone", 14000 }, { "mix", 70 } }),
        makePreset (p, "Synth: fuzz", "Instruments", { { "drive", 24 }, { "character", fuzz }, { "tone", 7000 }, { "output", -6 } }),
        makePreset (p, "Master: glow", "Master", { { "drive", 4 }, { "character", tape }, { "mix", 50 } }),
    };
}

namespace
{
    /** The drive, curve and level compensation for a set of settings (worked out once a block). */
    struct Shaper
    {
        float g = 1.0f, compensation = 1.0f;
        int kind = tape;

        float curve (float s) const noexcept
        {
            switch (kind)
            {
                case tube:     return std::tanh (s + 0.25f) - std::tanh (0.25f);           // lopsided: adds even harmonics
                case fuzz:     return std::copysign (1.0f - std::exp (-2.0f * std::abs (s)), s);
                case hardClip: return std::clamp (s, -1.0f, 1.0f);
                default:       return std::tanh (s);
            }
        }

        float operator() (float input) const noexcept   { return curve (input * g) * compensation; }
    };

    Shaper makeShaper (const EffectParameterValues& v) noexcept
    {
        Shaper s;
        s.g = (float) std::pow (10.0, v[drive] / 20.0);
        s.kind = std::clamp ((int) std::lround (v[type]), 0, 3);

        // More drive shouldn't mean much louder overall: a quiet signal (-12 dB) comes out about
        // as loud as it went in, so Drive changes the character more than the level.
        constexpr auto reference = 0.25f;
        s.compensation = reference / std::max (1.0e-4f, std::abs (s.curve (reference * s.g)));
        return s;
    }
}

float shape (const EffectParameterValues& v, float input) noexcept
{
    return makeShaper (v) (input);
}

namespace
{
    class Saturation final : public Effect
    {
    public:
        void prepare (double newSampleRate, int) override
        {
            sampleRate = newSampleRate;
            toneLeft = toneRight = 0.0f;
            for (auto* s : { &mixAmount, &outputGain })
                s->prepare (20.0, sampleRate);
        }

        void process (float* left, float* right, int numSamples, const EffectParameterValues& v,
                      const EffectContext&) noexcept AETHER_NONBLOCKING override
        {
            const auto toneCoefficient = (float) std::exp (-twoPi * std::clamp ((double) v[tone], 20.0, sampleRate * 0.45) / sampleRate);
            const auto filtering = v[tone] < 19900.0f;
            const auto shaper = makeShaper (v);

            for (int i = 0; i < numSamples; ++i)
            {
                const auto m = (float) mixAmount.next (v[mix] / 100.0);
                const auto out = (float) outputGain.next (std::pow (10.0, v[output] / 20.0));

                auto wetLeft = shaper (left[i]), wetRight = shaper (right[i]);

                if (filtering)
                {
                    toneLeft = wetLeft + toneCoefficient * (toneLeft - wetLeft);
                    toneRight = wetRight + toneCoefficient * (toneRight - wetRight);
                    wetLeft = toneLeft;
                    wetRight = toneRight;
                }

                left[i] = (left[i] * dryGainForMix (m) + wetLeft * wetGainForMix (m)) * out;
                right[i] = (right[i] * dryGainForMix (m) + wetRight * wetGainForMix (m)) * out;
            }
        }

    private:
        double sampleRate = 48000.0;
        float toneLeft = 0.0f, toneRight = 0.0f;
        Smoothed mixAmount, outputGain;
    };
}

std::unique_ptr<Effect> create()   { return std::make_unique<Saturation>(); }

} // namespace saturation

//==============================================================================
namespace filter
{

std::vector<EffectParameter> parameters()
{
    std::vector<EffectParameter> list {
        { "filterType", "Type",      0.0f, 3.0f, 0.0f, {}, { "Low-pass", "High-pass", "Band-pass", "Notch" }, false },
        { "cutoff",    "Cutoff",     20.0f, 20000.0f, 2000.0f, "Hz", {}, true },
        { "resonance", "Resonance",  0.0f, 100.0f, 20.0f, "%", {}, false },
        { "lfoRate",   "Sweep rate", 0.05f, 10.0f, 1.0f, "Hz", {}, true },
        { "lfoDepth",  "Sweep",      0.0f, 100.0f, 0.0f, "%", {}, false },
        { "mix",       "Mix",        0.0f, 100.0f, 100.0f, "%", {}, false },
    };
    jassert ((int) list.size() == numParameters);
    return list;
}

std::vector<EffectPreset> presets()
{
    const auto p = parameters();
    return {
        makePreset (p, "Darker (start here)", "General", { { "filterType", lowPass }, { "cutoff", 3000 }, { "resonance", 10 } }),
        makePreset (p, "Thinner", "General", { { "filterType", highPass }, { "cutoff", 400 }, { "resonance", 10 } }),
        makePreset (p, "Vocal: radio", "Vocals", { { "filterType", bandPass }, { "cutoff", 1500 }, { "resonance", 30 } }),
        makePreset (p, "Slow sweep", "Instruments", { { "filterType", lowPass }, { "cutoff", 1200 }, { "resonance", 45 }, { "lfoRate", 0.15f },
                                                      { "lfoDepth", 60 } }),
        makePreset (p, "Wobble", "Instruments", { { "filterType", lowPass }, { "cutoff", 800 }, { "resonance", 55 }, { "lfoRate", 3 }, { "lfoDepth", 70 } }),
    };
}

namespace
{
    double damping (float resonance) noexcept   { return 2.0 - 1.95 * std::clamp (resonance / 100.0f, 0.0f, 1.0f); }   // 2: none .. 0.05: very peaky
}

double responseDb (const EffectParameterValues& v, double cutoffHz, double hz, double sampleRate) noexcept
{
    // The filter is a state-variable filter made with the bilinear transform, so its response is
    // the analogue one at the pre-warped frequency.
    const auto warp = [sampleRate] (double f) { return std::tan (juce::MathConstants<double>::pi * std::clamp (f, 1.0, sampleRate * 0.49) / sampleRate); };
    const std::complex<double> s (0.0, warp (hz) / warp (cutoffHz));
    const auto k = damping (v[resonance]);
    const auto denominator = s * s + k * s + 1.0;

    std::complex<double> h;
    switch (std::clamp ((int) std::lround (v[type]), 0, 3))
    {
        case highPass: h = s * s / denominator; break;
        case bandPass: h = k * s / denominator; break;
        case notch:    h = (s * s + 1.0) / denominator; break;
        default:       h = 1.0 / denominator; break;
    }

    return 20.0 * std::log10 (std::max (1.0e-9, std::abs (h)));
}

namespace
{
    class Filter final : public Effect
    {
    public:
        void prepare (double newSampleRate, int) override
        {
            sampleRate = newSampleRate;
            for (auto& s : state) s = {};
            phase = 0.0;
            cutoffOctaves.prepare (15.0, sampleRate);
            mixAmount.prepare (20.0, sampleRate);
        }

        void process (float* left, float* right, int numSamples, const EffectParameterValues& v,
                      const EffectContext&) noexcept AETHER_NONBLOCKING override
        {
            const auto kind = std::clamp ((int) std::lround (v[type]), 0, 3);
            const auto k = damping (v[resonance]);
            const auto increment = std::clamp ((double) v[lfoRate], 0.01, 20.0) / sampleRate;
            const auto sweepOctaves = v[lfoDepth] / 100.0 * 3.0;

            for (int i = 0; i < numSamples; ++i)
            {
                // The cutoff (and its sweep) in octaves, smoothed; coefficients every 8 samples.
                const auto octaves = cutoffOctaves.next (std::log2 (std::max (20.0f, v[cutoff])));
                phase += increment;
                if (phase >= 1.0) phase -= 1.0;

                if ((i & 7) == 0)
                {
                    const auto hz = std::pow (2.0, octaves + sweepOctaves * std::sin (twoPi * phase));
                    const auto g = std::tan (juce::MathConstants<double>::pi * std::clamp (hz, 20.0, sampleRate * 0.45) / sampleRate);
                    a1 = 1.0 / (1.0 + g * (g + k));
                    a2 = g * a1;
                    a3 = g * a2;
                }

                const auto m = (float) mixAmount.next (v[mix] / 100.0);
                const auto wetLeft = (float) run (state[0], left[i], kind, k);
                const auto wetRight = (float) run (state[1], right[i], kind, k);
                left[i] = left[i] * dryGainForMix (m) + wetLeft * wetGainForMix (m);
                right[i] = right[i] * dryGainForMix (m) + wetRight * wetGainForMix (m);
            }
        }

    private:
        struct State { double ic1 = 0.0, ic2 = 0.0; };

        double run (State& s, double x, int kind, double k) const noexcept
        {
            const auto v3 = x - s.ic2;
            const auto v1 = a1 * s.ic1 + a2 * v3;
            const auto v2 = s.ic2 + a2 * s.ic1 + a3 * v3;
            s.ic1 = 2.0 * v1 - s.ic1;
            s.ic2 = 2.0 * v2 - s.ic2;
            if (std::abs (s.ic1) < 1.0e-20) s.ic1 = 0.0;
            if (std::abs (s.ic2) < 1.0e-20) s.ic2 = 0.0;

            switch (kind)
            {
                case highPass: return x - k * v1 - v2;
                case bandPass: return k * v1;
                case notch:    return x - k * v1;
                default:       return v2;
            }
        }

        double sampleRate = 48000.0, phase = 0.0, a1 = 1.0, a2 = 0.0, a3 = 0.0;
        std::array<State, 2> state {};
        Smoothed cutoffOctaves, mixAmount;
    };
}

std::unique_ptr<Effect> create()   { return std::make_unique<Filter>(); }

} // namespace filter

} // namespace aether::dsp
