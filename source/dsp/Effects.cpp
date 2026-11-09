// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
//
// The reverb and delay (Milestone 13), and the list of every effect.

#include "Effect.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>

#include "Biquad.h"
#include "ColourEffects.h"
#include "Dynamics.h"
#include "Eq.h"
#include "Presets.h"
#include "SpaceEffects.h"

namespace aether::dsp
{

namespace
{
    /** Dry/wet balance: at 50 % both are at full level; towards either end the other fades out. */
    float dryGainForMix (float mix) noexcept   { return std::min (1.0f, 2.0f * (1.0f - mix)); }
    float wetGainForMix (float mix) noexcept   { return std::min (1.0f, 2.0f * mix); }
    float flush (float value) noexcept         { return std::abs (value) < 1.0e-15f ? 0.0f : value; }

    /** A ring buffer read at a fractional delay (linear interpolation). */
    struct DelayLine
    {
        std::vector<float> data;
        int write = 0;

        void prepare (int maxSamples)   { data.assign ((size_t) std::max (4, maxSamples + 3), 0.0f); write = 0; }

        void push (float x) noexcept
        {
            data[(size_t) write] = x;
            write = write + 1 == (int) data.size() ? 0 : write + 1;
        }

        /** The sample `delay` samples before the last one pushed (at least 1). */
        float read (double delay) const noexcept
        {
            const auto size = (int) data.size();
            auto position = (double) write - 1.0 - std::clamp (delay, 0.0, (double) size - 3);
            while (position < 0.0) position += size;
            auto index = (int) position;
            if (index >= size) { index -= size; position -= size; }   // (a tiny negative position can round up to `size`)
            const auto fraction = (float) (position - index);
            const auto next = index + 1 == size ? 0 : index + 1;
            return data[(size_t) index] + fraction * (data[(size_t) next] - data[(size_t) index]);
        }
    };
}

//==============================================================================
namespace reverb
{

std::vector<EffectParameter> parameters()
{
    std::vector<EffectParameter> list {
        { "size",     "Size",      0.0f, 100.0f, 50.0f,  "%", {}, false },
        { "damping",  "Damping",   0.0f, 100.0f, 50.0f,  "%", {}, false },
        { "width",    "Width",     0.0f, 100.0f, 100.0f, "%", {}, false },
        { "mix",      "Mix",       0.0f, 100.0f, 30.0f,  "%", {}, false },
        { "predelay", "Pre-delay", 0.0f, 250.0f, 10.0f,  "ms", {}, false },
        { "lowCut",   "Low cut",   20.0f, 1000.0f, 120.0f, "Hz", {}, true },
    };
    jassert ((int) list.size() == numParameters);
    return list;
}

std::vector<EffectPreset> presets()
{
    const auto p = parameters();
    return {
        makePreset (p, "Room (start here)", "General", { { "size", 35 }, { "damping", 50 }, { "mix", 22 } }),
        makePreset (p, "Vocal: plate", "Vocals", { { "size", 45 }, { "damping", 30 }, { "predelay", 25 }, { "lowCut", 200 }, { "mix", 20 } }),
        makePreset (p, "Vocal: hall", "Vocals", { { "size", 70 }, { "damping", 45 }, { "predelay", 40 }, { "lowCut", 180 }, { "mix", 18 } }),
        makePreset (p, "Vocal: small room", "Vocals", { { "size", 20 }, { "damping", 60 }, { "predelay", 5 }, { "lowCut", 200 }, { "mix", 15 } }),
        makePreset (p, "Vocal: dreamy", "Vocals", { { "size", 90 }, { "damping", 35 }, { "predelay", 60 }, { "lowCut", 250 }, { "mix", 35 } }),
        makePreset (p, "Piano: hall", "Instruments", { { "size", 65 }, { "damping", 40 }, { "predelay", 15 }, { "lowCut", 100 }, { "mix", 25 } }),
        makePreset (p, "Strings: big hall", "Instruments", { { "size", 85 }, { "damping", 45 }, { "predelay", 30 }, { "lowCut", 90 }, { "mix", 30 } }),
        makePreset (p, "Instrument: room", "Instruments", { { "size", 30 }, { "damping", 55 }, { "predelay", 8 }, { "mix", 18 } }),
        makePreset (p, "Master: subtle glue", "Master", { { "size", 30 }, { "damping", 60 }, { "lowCut", 250 }, { "mix", 8 } }),
    };
}

double decaySeconds (const EffectParameterValues& v) noexcept
{
    const auto s = std::clamp (v[size] / 100.0, 0.0, 1.0);
    return 0.3 + 6.0 * s * s;
}

namespace
{
    class Reverb final : public Effect
    {
    public:
        static constexpr int lines = 8;

        void prepare (double newSampleRate, int) override
        {
            sampleRate = newSampleRate;
            const auto ms = [this] (double m) { return (int) std::ceil (m * 0.001 * sampleRate); };

            predelayLine.prepare (ms (260.0));
            for (size_t i = 0; i < diffusers.size(); ++i)
                diffusers[i].prepare (ms (diffuserMs[i] * 1.0) + 4);
            for (size_t i = 0; i < lines; ++i)
                delays[i].prepare (ms (lineMs[i] * maxScale + modulationMs * 2.0) + 4);

            damp.fill (0.0f);
            phases.fill (0.0);
            for (auto& s : lowCutState) s.reset();
            designedLowCut = -1.0f;
            started = false;
        }

        void process (float* left, float* right, int numSamples, const EffectParameterValues& v,
                      const EffectContext&) noexcept AETHER_NONBLOCKING override
        {
            if (delays[0].data.empty())
                return;

            if (v[lowCut] != designedLowCut)
            {
                lowCutFilter = BiquadCoefficients::highPass (v[lowCut], 0.7071, sampleRate);
                designedLowCut = v[lowCut];
            }

            // The settings that shape the room: line lengths, how fast it dies away, damping.
            const auto scale = 0.5 + (maxScale - 0.5) * std::clamp (v[size] / 100.0, 0.0, 1.0);
            const auto seconds = decaySeconds (v);
            const auto dampAmount = std::clamp (v[damping] / 100.0f, 0.0f, 1.0f) * 0.75f;
            const auto predelaySamples = std::clamp (v[predelay], 0.0f, 250.0f) * 0.001 * sampleRate;
            const auto widthAmount = std::clamp (v[width] / 100.0f, 0.0f, 1.0f);
            const auto mixTarget = std::clamp (v[mix] / 100.0f, 0.0f, 1.0f);

            // Changes to the room's size and the pre-delay crossfade from the old lengths to the
            // new ones (over 40 ms) instead of gliding, which would bend the pitch of the tail
            // (a "laser" sound when switching presets as the song plays).
            if (! started)
            {
                currentScale = nextScale = scale;
                currentPredelay = nextPredelay = predelaySamples;
                fadeLeft = 0;
                dry = dryGainForMix (mixTarget);
                wet = wetGainForMix (mixTarget);
                started = true;
            }

            const auto fadeLength = std::max (1, (int) (0.04 * sampleRate));
            std::array<double, lines> lengthNow {}, lengthNext {};
            std::array<float, lines> gainNow {}, gainNext {};
            auto computeLines = [&]
            {
                for (size_t i = 0; i < lines; ++i)
                {
                    lengthNow[i] = lineMs[i] * currentScale * 0.001 * sampleRate;
                    lengthNext[i] = lineMs[i] * nextScale * 0.001 * sampleRate;
                    gainNow[i] = (float) std::pow (10.0, -3.0 * lengthNow[i] / (seconds * sampleRate));
                    gainNext[i] = (float) std::pow (10.0, -3.0 * lengthNext[i] / (seconds * sampleRate));
                }
            };
            computeLines();

            const auto modulationDepth = modulationMs * 0.001 * sampleRate;

            for (int n = 0; n < numSamples; ++n)
            {
                if (fadeLeft == 0 && (std::abs (scale - currentScale) > 1.0e-4 || std::abs (predelaySamples - currentPredelay) > 0.5))
                {
                    nextScale = scale;
                    nextPredelay = predelaySamples;
                    fadeLeft = fadeLength;
                    computeLines();
                }

                const auto fading = fadeLeft > 0;
                const auto f = fading ? 1.0f - (float) fadeLeft / (float) fadeLength : 0.0f;

                dry += (dryGainForMix (mixTarget) - dry) * 0.002f;
                wet += (wetGainForMix (mixTarget) - wet) * 0.002f;

                // Into the room: both sides together, after the pre-delay, without the lows.
                predelayLine.push (0.5f * (left[n] + right[n]));
                auto x = predelayLine.read (currentPredelay);
                if (fading)
                    x += f * (predelayLine.read (nextPredelay) - x);
                x = lowCutState[0].process (lowCutFilter, x);

                // Diffusion: four all-pass filters smear the sound before the lines.
                for (size_t d = 0; d < diffusers.size(); ++d)
                {
                    const auto delayed = diffusers[d].read (diffuserMs[d] * 0.001 * sampleRate - 1.0);
                    const auto input = x + 0.6f * delayed;
                    diffusers[d].push (input);
                    x = delayed - 0.6f * input;
                }

                // The eight lines, four of them wavering slightly (it softens metallic ringing).
                std::array<float, lines> out {};
                float sum = 0.0f;
                for (size_t i = 0; i < lines; ++i)
                {
                    auto wobble = 0.0;
                    if (i < 4)
                    {
                        phases[i] += modulationRates[i] / sampleRate;
                        if (phases[i] >= 1.0) phases[i] -= 1.0;
                        wobble = modulationDepth * (1.0 + std::sin (juce::MathConstants<double>::twoPi * phases[i]));
                    }

                    out[i] = delays[i].read (lengthNow[i] - 1.0 + wobble);
                    if (fading)
                        out[i] += f * (delays[i].read (lengthNext[i] - 1.0 + wobble) - out[i]);

                    sum += out[i];
                }

                // Mix the lines into each other (a Householder matrix: no line's energy is lost),
                // dampen the highs, let it die away, and add the new sound.
                const auto spread = sum * (2.0f / lines);
                for (size_t i = 0; i < lines; ++i)
                {
                    const auto fed = out[i] - spread;
                    damp[i] = flush (fed + dampAmount * (damp[i] - fed));
                    delays[i].push (damp[i] * (fading ? gainNow[i] + f * (gainNext[i] - gainNow[i]) : gainNow[i]) + x);
                }

                // Out: different mixes of the lines for left and right, then the width.
                auto l = 0.0f, r = 0.0f;
                for (size_t i = 0; i < lines; ++i)
                {
                    l += out[i] * leftSigns[i];
                    r += out[i] * rightSigns[i];
                }
                l *= outputGain;
                r *= outputGain;

                const auto mid = 0.5f * (l + r), side = 0.5f * (l - r) * widthAmount;
                left[n] = left[n] * dry + (mid + side) * wet;
                right[n] = right[n] * dry + (mid - side) * wet;

                if (fading && --fadeLeft == 0)
                {
                    currentScale = nextScale;
                    currentPredelay = nextPredelay;
                    computeLines();
                }
            }
        }

    private:
        static constexpr double maxScale = 2.0, modulationMs = 0.35;
        static constexpr std::array<double, lines> lineMs { 29.7, 37.1, 41.1, 43.7, 53.3, 59.9, 67.7, 73.3 };
        static constexpr std::array<double, 4> diffuserMs { 4.77, 3.59, 12.73, 9.31 };
        static constexpr std::array<double, 4> modulationRates { 0.53, 0.71, 0.89, 1.13 };
        static constexpr std::array<float, lines> leftSigns { 1, -1, 1, -1, 1, -1, 1, -1 };
        static constexpr std::array<float, lines> rightSigns { 1, 1, -1, -1, 1, 1, -1, -1 };
        static constexpr float outputGain = 0.35f;

        double sampleRate = 48000.0, currentScale = 1.0, nextScale = 1.0, currentPredelay = 0.0, nextPredelay = 0.0;
        int fadeLeft = 0;
        DelayLine predelayLine;
        std::array<DelayLine, 4> diffusers;
        std::array<DelayLine, lines> delays;
        std::array<float, lines> damp {};
        std::array<double, 4> phases {};
        BiquadCoefficients lowCutFilter;
        std::array<BiquadState, 1> lowCutState {};
        float designedLowCut = -1.0f, dry = 1.0f, wet = 0.0f;
        bool started = false;
    };
}

std::unique_ptr<Effect> create()   { return std::make_unique<Reverb>(); }

} // namespace reverb

//==============================================================================
namespace echo
{

std::vector<EffectParameter> parameters()
{
    std::vector<EffectParameter> list {
        { "time",     "Time",      0.0f, 10.0f, 5.0f, {},
          { "1/32", "1/16 T", "1/16", "1/8 T", "1/16 D", "1/8", "1/4 T", "1/8 D", "1/4", "1/4 D", "1/2" }, false },
        { "feedback", "Feedback",  0.0f, 90.0f, 35.0f, "%", {}, false },
        { "tone",     "Tone",      500.0f, 20000.0f, 8000.0f, "Hz", {}, true },
        { "pingPong", "Ping-pong", 0.0f, 1.0f, 0.0f, {}, { "Off", "On" }, false },
        { "mix",      "Mix",       0.0f, 100.0f, 25.0f, "%", {}, false },
        { "lowCut",   "Low cut",   20.0f, 1000.0f, 20.0f, "Hz", {}, true },
    };
    jassert ((int) list.size() == numParameters);
    return list;
}

std::vector<EffectPreset> presets()
{
    const auto p = parameters();
    return {
        makePreset (p, "Eighth-note echo (start here)", "General", {}),
        makePreset (p, "Vocal: slap", "Vocals", { { "time", 0 }, { "feedback", 10 }, { "tone", 6000 }, { "lowCut", 250 }, { "mix", 18 } }),
        makePreset (p, "Vocal: eighth-note throw", "Vocals", { { "time", 5 }, { "feedback", 30 }, { "tone", 5000 }, { "lowCut", 300 }, { "mix", 16 } }),
        makePreset (p, "Vocal: dotted eighth", "Vocals", { { "time", 7 }, { "feedback", 35 }, { "tone", 6000 }, { "pingPong", 1 },
                                                           { "lowCut", 300 }, { "mix", 18 } }),
        makePreset (p, "Ping-pong", "General", { { "time", 5 }, { "feedback", 45 }, { "pingPong", 1 }, { "mix", 25 } }),
        makePreset (p, "Piano: wide echo", "Instruments", { { "time", 8 }, { "feedback", 30 }, { "tone", 7000 }, { "pingPong", 1 },
                                                            { "lowCut", 150 }, { "mix", 15 } }),
    };
}

namespace
{
    /** A stereo echo in time with the tempo, with optional ping-pong, and tone and low-cut filters
        that change each repeat a little more. */
    class Delay final : public Effect
    {
    public:
        static constexpr double maxSeconds = 4.0;

        void prepare (double newSampleRate, int) override
        {
            sampleRate = newSampleRate;
            size = (int) std::ceil (sampleRate * maxSeconds) + 4;
            bufferLeft.assign ((size_t) size, 0.0f);
            bufferRight.assign ((size_t) size, 0.0f);
            writeIndex = 0;
            delaySamples = -1.0;
            fadeLeft = 0;
            toneLeft = toneRight = 0.0f;
            lowLeft = lowRight = 0.0f;
            glide = 1.0 - std::exp (-1.0 / (0.08 * sampleRate));        // tempo changes glide over ~80 ms
            mixSmoothing = 1.0f - (float) std::exp (-1.0 / (0.01 * sampleRate));
        }

        void process (float* left, float* right, int numSamples,
                      const EffectParameterValues& values, const EffectContext& context) noexcept AETHER_NONBLOCKING override
        {
            if (size <= 4)
                return;

            const auto& lengths = delay::timeChoicesInQuarters;
            const auto choice = std::clamp ((int) std::lround (values[time]), 0, (int) lengths.size() - 1);
            const auto bpm = std::max (1.0, context.bpm);
            const auto target = std::clamp (lengths[(size_t) choice] * 60.0 / bpm * sampleRate, 1.0, (double) (size - 3));
            const auto fb = std::clamp (values[feedback] / 100.0f, 0.0f, 0.95f);
            const auto toneCoefficient = (float) std::exp (-2.0 * juce::MathConstants<double>::pi
                                                           * std::clamp ((double) values[tone], 20.0, sampleRate * 0.45) / sampleRate);
            const auto lowCoefficient = (float) std::exp (-2.0 * juce::MathConstants<double>::pi
                                                          * std::clamp ((double) values[lowCut], 1.0, sampleRate * 0.45) / sampleRate);
            const auto cutting = values[lowCut] > 25.0f;
            const auto ping = values[pingPong] >= 0.5f;
            const auto mixAmount = std::clamp (values[mix] / 100.0f, 0.0f, 1.0f);
            const auto dryTarget = dryGainForMix (mixAmount), wetTarget = wetGainForMix (mixAmount);

            if (delaySamples < 0.0)
            {
                delaySamples = nextDelay = target;   // first use: start at the right time
                fadeLeft = 0;
                dry = dryTarget;
                wet = wetTarget;
            }

            const auto fadeLength = std::max (1, (int) (0.05 * sampleRate));

            for (int i = 0; i < numSamples; ++i)
            {
                // A small change (the tempo drifting) glides. A bigger one (a new time) crossfades
                // from the old echo time to the new one over 50 ms: gliding that far would bend the
                // echoes' pitch (a "laser" sound).
                if (fadeLeft == 0)
                {
                    if (std::abs (target - delaySamples) > 0.02 * delaySamples)
                    {
                        nextDelay = target;
                        fadeLeft = fadeLength;
                    }
                    else
                    {
                        delaySamples += (target - delaySamples) * glide;
                    }
                }

                dry += (dryTarget - dry) * mixSmoothing;
                wet += (wetTarget - wet) * mixSmoothing;

                auto echoLeft = read (bufferLeft, delaySamples);
                auto echoRight = read (bufferRight, delaySamples);

                if (fadeLeft > 0)
                {
                    const auto f = 1.0f - (float) fadeLeft / (float) fadeLength;
                    echoLeft += f * (read (bufferLeft, nextDelay) - echoLeft);
                    echoRight += f * (read (bufferRight, nextDelay) - echoRight);

                    if (--fadeLeft == 0)
                        delaySamples = nextDelay;
                }

                // Each repeat goes through the tone filter (and the low cut) before it is fed back.
                toneLeft = flush (echoLeft + toneCoefficient * (toneLeft - echoLeft));
                toneRight = flush (echoRight + toneCoefficient * (toneRight - echoRight));
                auto backLeft = toneLeft, backRight = toneRight;

                if (cutting)
                {
                    lowLeft = flush (backLeft + lowCoefficient * (lowLeft - backLeft));
                    lowRight = flush (backRight + lowCoefficient * (lowRight - backRight));
                    backLeft -= lowLeft;
                    backRight -= lowRight;
                    echoLeft = backLeft;
                    echoRight = backRight;
                }

                const auto inLeft = left[i], inRight = right[i];

                if (ping)
                {
                    // The input starts on the left; every repeat swaps sides.
                    bufferLeft[(size_t) writeIndex] = 0.5f * (inLeft + inRight) + backRight * fb;
                    bufferRight[(size_t) writeIndex] = backLeft * fb;
                }
                else
                {
                    bufferLeft[(size_t) writeIndex] = inLeft + backLeft * fb;
                    bufferRight[(size_t) writeIndex] = inRight + backRight * fb;
                }

                writeIndex = writeIndex + 1 == size ? 0 : writeIndex + 1;

                left[i] = inLeft * dry + echoLeft * wet;
                right[i] = inRight * dry + echoRight * wet;
            }
        }

    private:
        float read (const std::vector<float>& buffer, double delay) const noexcept AETHER_NONBLOCKING
        {
            // Linear interpolation between the two samples around `delay` samples ago.
            auto position = (double) writeIndex - delay;
            while (position < 0.0)
                position += size;

            auto index = (int) position;
            if (index >= size) { index -= size; position -= size; }   // (a tiny negative position can round up to `size`)
            const auto fraction = (float) (position - index);
            const auto next = index + 1 == size ? 0 : index + 1;
            return buffer[(size_t) index] + fraction * (buffer[(size_t) next] - buffer[(size_t) index]);
        }

        double sampleRate = 48000.0;
        int size = 0, writeIndex = 0;
        std::vector<float> bufferLeft, bufferRight;
        double delaySamples = -1.0, nextDelay = 0.0, glide = 0.0;
        int fadeLeft = 0;
        float toneLeft = 0.0f, toneRight = 0.0f, lowLeft = 0.0f, lowRight = 0.0f;
        float dry = 1.0f, wet = 0.0f, mixSmoothing = 0.0f;
    };
}

std::unique_ptr<Effect> create()   { return std::make_unique<Delay>(); }

} // namespace echo

//==============================================================================
std::unique_ptr<Effect> createEffect (const juce::String& typeId)
{
    if (typeId == reverb::typeId)      return reverb::create();
    if (typeId == echo::typeId)        return echo::create();
    if (typeId == eq::typeId)          return eq::create();
    if (typeId == compressor::typeId)  return compressor::create();
    if (typeId == limiter::typeId)     return limiter::create();
    if (typeId == gate::typeId)        return gate::create();
    if (typeId == deesser::typeId)     return deesser::create();
    if (typeId == chorus::typeId)      return chorus::create();
    if (typeId == saturation::typeId)  return saturation::create();
    if (typeId == filter::typeId)      return filter::create();
    return nullptr;
}

} // namespace aether::dsp
