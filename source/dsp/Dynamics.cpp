// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "Dynamics.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>

#include "Biquad.h"
#include "Presets.h"

namespace aether::dsp
{

namespace
{
    constexpr double minimumDb = -120.0;

    inline double toDb (double linear) noexcept   { return linear > 1.0e-6 ? 20.0 * std::log10 (linear) : minimumDb; }
    inline double fromDb (double db) noexcept      { return std::pow (10.0, db / 20.0); }

    /** A one-pole coefficient that gets ~63% of the way in `ms` milliseconds. */
    inline double coefficient (double ms, double sampleRate) noexcept
    {
        return std::exp (-1.0 / std::max (1.0, ms * 0.001 * sampleRate));
    }

    /** The meter: the most it turned down (or the gate's opening) in the last block, read by the UI. */
    struct Meter
    {
        std::atomic<float> value { 0.0f };
        void set (float v) noexcept   { value.store (v, std::memory_order_relaxed); }
        float get() const noexcept    { return value.load (std::memory_order_relaxed); }
    };

    float dryGainForMix (float mix) noexcept   { return std::min (1.0f, 2.0f * (1.0f - mix)); }
    float wetGainForMix (float mix) noexcept   { return std::min (1.0f, 2.0f * mix); }
}

//==============================================================================
namespace compressor
{

std::vector<EffectParameter> parameters()
{
    std::vector<EffectParameter> list {
        { "threshold", "Threshold", -60.0f, 0.0f, -18.0f, "dB", {}, false },
        { "ratio",     "Ratio",     1.0f, 20.0f, 3.0f, ":1", {}, true },
        { "attack",    "Attack",    0.1f, 100.0f, 10.0f, "ms", {}, true },
        { "release",   "Release",   10.0f, 1000.0f, 120.0f, "ms", {}, true },
        { "knee",      "Knee",      0.0f, 12.0f, 6.0f, "dB", {}, false },
        { "makeup",    "Makeup",    0.0f, 24.0f, 0.0f, "dB", {}, false },
        { "mix",       "Mix",       0.0f, 100.0f, 100.0f, "%", {}, false },
    };
    jassert ((int) list.size() == numParameters);
    return list;
}

std::vector<EffectPreset> presets()
{
    const auto p = parameters();
    return {
        makePreset (p, "Gentle (start here)", "General", { { "threshold", -18 }, { "ratio", 2 }, { "makeup", 2 } }),
        makePreset (p, "Vocal: even", "Vocals", { { "threshold", -20 }, { "ratio", 3 }, { "attack", 5 }, { "release", 100 }, { "makeup", 5 } }),
        makePreset (p, "Vocal: up front", "Vocals", { { "threshold", -24 }, { "ratio", 4.5f }, { "attack", 3 }, { "release", 80 },
                                                      { "knee", 4 }, { "makeup", 8 } }),
        makePreset (p, "Vocal: rap / spoken", "Vocals", { { "threshold", -26 }, { "ratio", 6 }, { "attack", 2 }, { "release", 60 },
                                                          { "knee", 3 }, { "makeup", 9 } }),
        makePreset (p, "Vocal: smooth (parallel)", "Vocals", { { "threshold", -30 }, { "ratio", 8 }, { "attack", 5 }, { "release", 150 },
                                                               { "makeup", 10 }, { "mix", 45 } }),
        makePreset (p, "Piano: even", "Instruments", { { "threshold", -20 }, { "ratio", 2.5f }, { "attack", 15 }, { "release", 200 }, { "makeup", 3 } }),
        makePreset (p, "Strings: smooth", "Instruments", { { "threshold", -22 }, { "ratio", 2 }, { "attack", 30 }, { "release", 300 },
                                                           { "knee", 10 }, { "makeup", 3 } }),
        makePreset (p, "Acoustic: natural", "Instruments", { { "threshold", -18 }, { "ratio", 2.5f }, { "attack", 12 }, { "release", 150 },
                                                             { "makeup", 3 } }),
        makePreset (p, "Master: glue", "Master", { { "threshold", -12 }, { "ratio", 2 }, { "attack", 30 }, { "release", 200 },
                                                   { "knee", 8 }, { "makeup", 2 } }),
        makePreset (p, "Master: punchy", "Master", { { "threshold", -14 }, { "ratio", 3 }, { "attack", 20 }, { "release", 120 },
                                                     { "knee", 6 }, { "makeup", 3 } }),
    };
}

double outputDb (const EffectParameterValues& v, double in) noexcept
{
    // The standard "soft knee" curve: below the knee nothing changes, above it the level rises
    // 1/ratio as fast, and in between it bends smoothly.
    const auto t = (double) v[threshold], r = std::max (1.0, (double) v[ratio]), w = std::max (0.0, (double) v[knee]);
    const auto over = in - t;
    double out;

    if (2.0 * over < -w)
        out = in;
    else if (w > 0.0 && 2.0 * std::abs (over) <= w)
        out = in + (1.0 / r - 1.0) * (over + w / 2.0) * (over + w / 2.0) / (2.0 * w);
    else
        out = t + over / r;

    return out + v[makeup];
}

namespace
{
    class Compressor final : public Effect
    {
    public:
        void prepare (double newSampleRate, int) override
        {
            sampleRate = newSampleRate;
            reductionDb = 0.0;
            meter.set (0.0f);
        }

        float getMeter() const noexcept override   { return meter.get(); }

        void process (float* left, float* right, int numSamples, const EffectParameterValues& v,
                      const EffectContext&) noexcept AETHER_NONBLOCKING override
        {
            const auto attackCoefficient = coefficient (v[attack], sampleRate);
            const auto releaseCoefficient = coefficient (v[release], sampleRate);
            const auto makeupGain = fromDb (v[makeup]);
            const auto mixAmount = std::clamp (v[mix] / 100.0f, 0.0f, 1.0f);
            const auto dryGain = dryGainForMix (mixAmount), wetGain = wetGainForMix (mixAmount);
            EffectParameterValues curve = v;
            curve[makeup] = 0.0f;
            double most = 0.0;

            for (int i = 0; i < numSamples; ++i)
            {
                // Both sides together (so the stereo picture stays put).
                const auto level = toDb (std::max (std::abs (left[i]), std::abs (right[i])));
                const auto target = std::min (0.0, outputDb (curve, level) - level);   // 0 or less

                // Down quickly (attack), back up slowly (release).
                const auto c = target < reductionDb ? attackCoefficient : releaseCoefficient;
                reductionDb = target + c * (reductionDb - target);
                most = std::min (most, reductionDb);

                const auto gain = (float) (fromDb (reductionDb) * makeupGain);
                left[i] = left[i] * dryGain + left[i] * gain * wetGain;
                right[i] = right[i] * dryGain + right[i] * gain * wetGain;
            }

            meter.set ((float) -most);
        }

    private:
        double sampleRate = 48000.0, reductionDb = 0.0;
        Meter meter;
    };
}

std::unique_ptr<Effect> create()   { return std::make_unique<Compressor>(); }

} // namespace compressor

//==============================================================================
namespace limiter
{

std::vector<EffectParameter> parameters()
{
    std::vector<EffectParameter> list {
        { "gain",    "Gain",    0.0f, 24.0f, 0.0f, "dB", {}, false },
        { "ceiling", "Ceiling", -12.0f, 0.0f, -1.0f, "dB", {}, false },
        { "release", "Release", 10.0f, 1000.0f, 100.0f, "ms", {}, true },
    };
    jassert ((int) list.size() == numParameters);
    return list;
}

std::vector<EffectPreset> presets()
{
    const auto p = parameters();
    return {
        makePreset (p, "Safety (no overs)", "General", { { "gain", 0 }, { "ceiling", -1 } }),
        makePreset (p, "Master: a little louder", "Master", { { "gain", 3 }, { "ceiling", -1 }, { "release", 120 } }),
        makePreset (p, "Master: louder", "Master", { { "gain", 6 }, { "ceiling", -1 }, { "release", 80 } }),
        makePreset (p, "Master: maximum", "Master", { { "gain", 9 }, { "ceiling", -0.8f }, { "release", 50 } }),
    };
}

namespace
{
    class Limiter final : public Effect
    {
    public:
        static constexpr double lookaheadSeconds = 0.0015;
        static constexpr int maxLookahead = 512;   // enough for 1.5 ms at 192 kHz (and more)

        void prepare (double newSampleRate, int) override
        {
            sampleRate = newSampleRate;
            lookahead = std::clamp ((int) std::lround (lookaheadSeconds * sampleRate), 1, maxLookahead - 1);
            delayLeft.fill (0.0f);
            delayRight.fill (0.0f);
            needed.fill (1.0f);
            averaged.fill (1.0);
            windowSum = lookahead + 1;
            envelope = 1.0;
            position = 0;
            meter.set (0.0f);
        }

        float getMeter() const noexcept override   { return meter.get(); }

        void process (float* left, float* right, int numSamples, const EffectParameterValues& v,
                      const EffectContext&) noexcept AETHER_NONBLOCKING override
        {
            const auto inputGain = (float) fromDb (v[gain]);
            const auto limit = (float) fromDb (v[ceiling]);
            const auto releaseCoefficient = coefficient (v[release], sampleRate);
            const auto window = lookahead + 1;
            double lowest = 1.0;

            for (int i = 0; i < numSamples; ++i)
            {
                const auto l = left[i] * inputGain, r = right[i] * inputGain;
                const auto peak = std::max (std::abs (l), std::abs (r));

                // Each sample waits `lookahead` samples; meanwhile the gain it needs is known.
                delayLeft[(size_t) position] = l;
                delayRight[(size_t) position] = r;
                needed[(size_t) position] = peak > limit ? limit / peak : 1.0f;

                // The least gain any sample in the window needs...
                auto least = 1.0f;
                for (int k = 0; k < window; ++k)
                    least = std::min (least, needed[(size_t) k]);

                // ...followed down at once and back up slowly (the release)...
                envelope = least < envelope ? (double) least : least + releaseCoefficient * (envelope - least);

                // ...and averaged over the window, which turns the drop into a smooth slope that
                // has reached the needed gain by the time the peak comes out.
                windowSum += envelope - averaged[(size_t) position];
                averaged[(size_t) position] = envelope;
                const auto gainNow = std::min (1.0, windowSum / window);
                lowest = std::min (lowest, gainNow);

                position = (position + 1) % window;

                // The oldest sample comes out now; nothing passes the ceiling.
                left[i] = std::clamp ((float) (delayLeft[(size_t) position] * gainNow), -limit, limit);
                right[i] = std::clamp ((float) (delayRight[(size_t) position] * gainNow), -limit, limit);
            }

            meter.set ((float) -toDb (lowest));
        }

    private:
        double sampleRate = 48000.0, envelope = 1.0, windowSum = 0.0;
        int lookahead = 72, position = 0;
        std::array<float, maxLookahead> delayLeft {}, delayRight {}, needed {};
        std::array<double, maxLookahead> averaged {};
        Meter meter;
    };
}

std::unique_ptr<Effect> create()   { return std::make_unique<Limiter>(); }

} // namespace limiter

//==============================================================================
namespace gate
{

std::vector<EffectParameter> parameters()
{
    std::vector<EffectParameter> list {
        { "threshold", "Threshold", -80.0f, 0.0f, -45.0f, "dB", {}, false },
        { "range",     "Range",     -80.0f, 0.0f, -40.0f, "dB", {}, false },
        { "attack",    "Attack",    0.1f, 50.0f, 1.0f, "ms", {}, true },
        { "hold",      "Hold",      0.0f, 500.0f, 50.0f, "ms", {}, false },
        { "release",   "Release",   5.0f, 1000.0f, 120.0f, "ms", {}, true },
    };
    jassert ((int) list.size() == numParameters);
    return list;
}

std::vector<EffectPreset> presets()
{
    const auto p = parameters();
    return {
        makePreset (p, "Gentle (start here)", "General", {}),
        makePreset (p, "Vocal: quiet room", "Vocals", { { "threshold", -50 }, { "range", -20 }, { "hold", 80 }, { "release", 200 } }),
        makePreset (p, "Vocal: noisy room", "Vocals", { { "threshold", -40 }, { "range", -40 }, { "hold", 60 }, { "release", 150 } }),
        makePreset (p, "Tight", "Instruments", { { "threshold", -35 }, { "range", -80 }, { "attack", 0.5f }, { "hold", 20 }, { "release", 60 } }),
    };
}

double outputDb (const EffectParameterValues& v, double in) noexcept
{
    return in >= v[threshold] ? in : in + v[range];
}

namespace
{
    class Gate final : public Effect
    {
    public:
        void prepare (double newSampleRate, int) override
        {
            sampleRate = newSampleRate;
            envelope = 0.0;
            gainNow = 1.0;
            holdLeft = 0;
            open = true;
            meter.set (1.0f);
        }

        float getMeter() const noexcept override   { return meter.get(); }

        void process (float* left, float* right, int numSamples, const EffectParameterValues& v,
                      const EffectContext&) noexcept AETHER_NONBLOCKING override
        {
            const auto openLevel = fromDb (v[threshold]);
            const auto closeLevel = fromDb (v[threshold] - 3.0f);   // a little lower, so it doesn't flutter
            const auto closedGain = fromDb (v[range]);
            const auto attackCoefficient = coefficient (v[attack], sampleRate);
            const auto releaseCoefficient = coefficient (v[release], sampleRate);
            const auto holdSamples = (int) (v[hold] * 0.001 * sampleRate);
            const auto envelopeRelease = coefficient (10.0, sampleRate);
            double sum = 0.0;

            for (int i = 0; i < numSamples; ++i)
            {
                // The level: follows peaks at once, falls over ~10 ms.
                const auto peak = (double) std::max (std::abs (left[i]), std::abs (right[i]));
                envelope = peak > envelope ? peak : peak + envelopeRelease * (envelope - peak);

                if (envelope >= openLevel)
                {
                    open = true;
                    holdLeft = holdSamples;
                }
                else if (envelope < closeLevel)
                {
                    if (holdLeft > 0) --holdLeft;
                    else open = false;
                }

                const auto target = open ? 1.0 : closedGain;
                const auto c = target > gainNow ? attackCoefficient : releaseCoefficient;
                gainNow = target + c * (gainNow - target);
                sum += gainNow;

                left[i] *= (float) gainNow;
                right[i] *= (float) gainNow;
            }

            if (numSamples > 0)
                meter.set ((float) (sum / numSamples));
        }

    private:
        double sampleRate = 48000.0, envelope = 0.0, gainNow = 1.0;
        int holdLeft = 0;
        bool open = true;
        Meter meter;
    };
}

std::unique_ptr<Effect> create()   { return std::make_unique<Gate>(); }

} // namespace gate

//==============================================================================
namespace deesser
{

std::vector<EffectParameter> parameters()
{
    std::vector<EffectParameter> list {
        { "frequency", "Frequency", 2000.0f, 12000.0f, 6000.0f, "Hz", {}, true },
        { "threshold", "Threshold", -60.0f, 0.0f, -30.0f, "dB", {}, false },
        { "amount",    "Amount",    0.0f, 24.0f, 8.0f, "dB", {}, false },
    };
    jassert ((int) list.size() == numParameters);
    return list;
}

std::vector<EffectPreset> presets()
{
    const auto p = parameters();
    return {
        makePreset (p, "Gentle", "Vocals", { { "frequency", 6500 }, { "threshold", -30 }, { "amount", 6 } }),
        makePreset (p, "Strong", "Vocals", { { "frequency", 6000 }, { "threshold", -36 }, { "amount", 12 } }),
        makePreset (p, "Deep voice", "Vocals", { { "frequency", 4500 }, { "threshold", -32 }, { "amount", 8 } }),
        makePreset (p, "Bright voice", "Vocals", { { "frequency", 8000 }, { "threshold", -30 }, { "amount", 8 } }),
    };
}

namespace
{
    class DeEsser final : public Effect
    {
    public:
        void prepare (double newSampleRate, int) override
        {
            sampleRate = newSampleRate;
            for (auto& s : detector) s.reset();
            for (auto& s : output) s.reset();
            envelope = 0.0;
            reductionDb = 0.0;
            shelfDb = 0.0;
            designedFor = -1.0f;
            meter.set (0.0f);
        }

        float getMeter() const noexcept override   { return meter.get(); }

        void process (float* left, float* right, int numSamples, const EffectParameterValues& v,
                      const EffectContext&) noexcept AETHER_NONBLOCKING override
        {
            if (v[frequency] != designedFor)
            {
                highPass = BiquadCoefficients::highPass (v[frequency], 0.7071, sampleRate);
                designedFor = v[frequency];
            }

            const auto attackCoefficient = coefficient (1.0, sampleRate);
            const auto releaseCoefficient = coefficient (60.0, sampleRate);
            const auto envelopeRelease = coefficient (20.0, sampleRate);
            const auto limit = (double) v[amount];
            double most = 0.0;

            for (int i = 0; i < numSamples; ++i)
            {
                // How loud the high part of the sound is (where "s" lives).
                const auto highLeft = detector[0].process (highPass, left[i]);
                const auto highRight = detector[1].process (highPass, right[i]);
                const auto peak = (double) std::max (std::abs (highLeft), std::abs (highRight));
                envelope = peak > envelope ? peak : peak + envelopeRelease * (envelope - peak);

                // Above the threshold, the highs are turned down (up to `amount`): a high shelf
                // that dips only while there's an "s".
                const auto target = -std::clamp (toDb (envelope) - (double) v[threshold], 0.0, limit);
                const auto c = target < reductionDb ? attackCoefficient : releaseCoefficient;
                reductionDb = target + c * (reductionDb - target);
                most = std::min (most, reductionDb);

                if ((i & 15) == 0 && std::abs (reductionDb - shelfDb) > 0.05)
                {
                    shelf = BiquadCoefficients::highShelf (v[frequency] * 0.8, reductionDb, sampleRate);
                    shelfDb = reductionDb;
                }

                if (shelfDb < -0.01)
                {
                    left[i] = output[0].process (shelf, left[i]);
                    right[i] = output[1].process (shelf, right[i]);
                }
                else
                {
                    output[0].reset();
                    output[1].reset();
                }
            }

            meter.set ((float) -most);
        }

    private:
        double sampleRate = 48000.0, envelope = 0.0, reductionDb = 0.0, shelfDb = 0.0;
        BiquadCoefficients highPass, shelf;
        std::array<BiquadState, 2> detector {}, output {};
        float designedFor = -1.0f;
        Meter meter;
    };
}

std::unique_ptr<Effect> create()   { return std::make_unique<DeEsser>(); }

} // namespace deesser

} // namespace aether::dsp
