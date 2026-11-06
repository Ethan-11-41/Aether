// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
//
// The two temporary built-in effects of Milestone 4: Reverb and Delay. Simple on purpose: the
// full set of effects (with better-sounding versions of these) is milestone M13.

#include "Effect.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>

namespace aether::dsp
{

namespace
{
    /** Dry/wet balance: at 50 % both are at full level; towards either end the other fades out. */
    float dryGainForMix (float mix) noexcept   { return std::min (1.0f, 2.0f * (1.0f - mix)); }
    float wetGainForMix (float mix) noexcept   { return std::min (1.0f, 2.0f * mix); }

    //==============================================================================
    /** JUCE's reverb (the well-known "Freeverb" design). */
    class ReverbEffect final : public Effect
    {
    public:
        void prepare (double sampleRate, int) override
        {
            reverb.setSampleRate (sampleRate);
            reverb.reset();
        }

        void process (float* left, float* right, int numSamples,
                      const EffectParameterValues& values, const EffectContext&) noexcept AETHER_NONBLOCKING override
        {
            const auto mix = values[3] / 100.0f;

            juce::Reverb::Parameters p;
            p.roomSize = values[0] / 100.0f;
            p.damping = values[1] / 100.0f;
            p.width = values[2] / 100.0f;
            p.wetLevel = wetGainForMix (mix) / 3.0f;   // juce::Reverb scales wet by 3 and dry by 2
            p.dryLevel = dryGainForMix (mix) / 2.0f;
            p.freezeMode = 0.0f;

            if (! sameAs (p, current))
            {
                reverb.setParameters (p);   // changes are smoothed inside juce::Reverb
                current = p;
            }

            reverb.processStereo (left, right, numSamples);
        }

    private:
        static bool sameAs (const juce::Reverb::Parameters& a, const juce::Reverb::Parameters& b) noexcept
        {
            return a.roomSize == b.roomSize && a.damping == b.damping && a.width == b.width
                && a.wetLevel == b.wetLevel && a.dryLevel == b.dryLevel;
        }

        juce::Reverb reverb;
        juce::Reverb::Parameters current { -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, 0.0f };
    };

    //==============================================================================
    /** A stereo echo in time with the tempo, with optional ping-pong, and a tone control that
        darkens each repeat a little more. */
    class DelayEffect final : public Effect
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
            toneLeft = toneRight = 0.0f;
            glide = 1.0 - std::exp (-1.0 / (0.08 * sampleRate));        // tempo changes glide over ~80 ms
            mixSmoothing = 1.0f - (float) std::exp (-1.0 / (0.01 * sampleRate));
        }

        void process (float* left, float* right, int numSamples,
                      const EffectParameterValues& values, const EffectContext& context) noexcept AETHER_NONBLOCKING override
        {
            if (size <= 4)
                return;

            const auto& lengths = delay::timeChoicesInQuarters;
            const auto choice = std::clamp ((int) std::lround (values[0]), 0, (int) lengths.size() - 1);
            const auto bpm = std::max (1.0, context.bpm);
            const auto target = std::clamp (lengths[(size_t) choice] * 60.0 / bpm * sampleRate, 1.0, (double) (size - 3));
            const auto feedback = std::clamp (values[1] / 100.0f, 0.0f, 0.95f);
            const auto toneCoefficient = (float) std::exp (-2.0 * juce::MathConstants<double>::pi
                                                           * std::clamp ((double) values[2], 20.0, sampleRate * 0.45) / sampleRate);
            const auto pingPong = values[3] >= 0.5f;
            const auto mix = std::clamp (values[4] / 100.0f, 0.0f, 1.0f);
            const auto dryTarget = dryGainForMix (mix), wetTarget = wetGainForMix (mix);

            if (delaySamples < 0.0)
            {
                delaySamples = target;   // first use: start at the right time, no glide
                dry = dryTarget;
                wet = wetTarget;
            }

            for (int i = 0; i < numSamples; ++i)
            {
                delaySamples += (target - delaySamples) * glide;
                dry += (dryTarget - dry) * mixSmoothing;
                wet += (wetTarget - wet) * mixSmoothing;

                const auto echoLeft = read (bufferLeft, delaySamples);
                const auto echoRight = read (bufferRight, delaySamples);

                // Each repeat goes through the tone filter before it is fed back.
                toneLeft = flush (echoLeft + toneCoefficient * (toneLeft - echoLeft));
                toneRight = flush (echoRight + toneCoefficient * (toneRight - echoRight));

                const auto inLeft = left[i], inRight = right[i];

                if (pingPong)
                {
                    // The input starts on the left; every repeat swaps sides.
                    bufferLeft[(size_t) writeIndex] = 0.5f * (inLeft + inRight) + toneRight * feedback;
                    bufferRight[(size_t) writeIndex] = toneLeft * feedback;
                }
                else
                {
                    bufferLeft[(size_t) writeIndex] = inLeft + toneLeft * feedback;
                    bufferRight[(size_t) writeIndex] = inRight + toneRight * feedback;
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

            const auto index = (int) position;
            const auto fraction = (float) (position - index);
            const auto next = index + 1 == size ? 0 : index + 1;
            return buffer[(size_t) index] + fraction * (buffer[(size_t) next] - buffer[(size_t) index]);
        }

        static float flush (float value) noexcept   { return std::abs (value) < 1.0e-15f ? 0.0f : value; }

        double sampleRate = 48000.0;
        int size = 0, writeIndex = 0;
        std::vector<float> bufferLeft, bufferRight;
        double delaySamples = -1.0, glide = 0.0;
        float toneLeft = 0.0f, toneRight = 0.0f;
        float dry = 1.0f, wet = 0.0f, mixSmoothing = 0.0f;
    };
}

std::unique_ptr<Effect> createEffect (const juce::String& typeId)
{
    if (typeId == "reverb") return std::make_unique<ReverbEffect>();
    if (typeId == "delay")  return std::make_unique<DelayEffect>();
    return nullptr;
}

} // namespace aether::dsp
