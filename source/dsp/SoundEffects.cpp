// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "SoundEffects.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "DrumSounds.h"

namespace aether::dsp::soundfx
{

namespace
{
    constexpr double pi = juce::MathConstants<double>::pi;
    constexpr float peakLevel = 0.8f;   // about -2 dB: loud, with room left in the mix

    /** A two-pole state-variable filter (topology-preserving: stable while its cutoff sweeps,
        up to near half the sample rate). */
    struct Sweeper
    {
        double ic1 = 0.0, ic2 = 0.0;

        /** Returns {low-pass, band-pass}. */
        std::pair<double, double> process (double x, double cutoff, double q, double rate) noexcept
        {
            const auto g = std::tan (pi * std::min (cutoff, rate * 0.45) / rate);
            const auto k = 1.0 / q;
            const auto a1 = 1.0 / (1.0 + g * (g + k)), a2 = g * a1, a3 = g * a2;
            const auto v3 = x - ic2;
            const auto v1 = a1 * ic1 + a2 * v3;
            const auto v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0 * v1 - ic1;
            ic2 = 2.0 * v2 - ic2;
            return { v2, v1 };
        }
    };

    /** A sawtooth with polyBLEP corrections at its jumps, so it doesn't alias (no harsh tones
        folding back, even high up). */
    struct Saw
    {
        double phase = 0.0;

        static double blep (double t, double dt) noexcept
        {
            if (t < dt)       { t /= dt; return t + t - t * t - 1.0; }
            if (t > 1.0 - dt) { t = (t - 1.0) / dt; return t * t + t + t + 1.0; }
            return 0.0;
        }

        double next (double hz, double rate) noexcept
        {
            const auto dt = hz / rate;
            phase += dt;
            if (phase >= 1.0)
                phase -= 1.0;
            return 2.0 * phase - 1.0 - blep (phase, dt);
        }
    };

    /** Sets the peak, and fades the very start and end so neither clicks. */
    void finish (juce::AudioBuffer<float>& b, double rate, double fadeInSeconds, double fadeOutSeconds)
    {
        const auto n = b.getNumSamples();
        const auto loudest = b.getMagnitude (0, n);

        if (loudest > 0.0f)
            b.applyGain (peakLevel / loudest);

        const auto in = std::min (n, (int) (fadeInSeconds * rate));
        const auto out = std::min (n - in, (int) (fadeOutSeconds * rate));
        b.applyGainRamp (0, in, 0.0f, 1.0f);
        b.applyGainRamp (n - out, out, 1.0f, 0.0f);
    }

    juce::AudioBuffer<float> make (double seconds, double rate)
    {
        juce::AudioBuffer<float> b (2, std::max (1, (int) std::lround (seconds * rate)));
        b.clear();
        return b;
    }

    /** Noise rising: a low-pass opening from 200 Hz to 12 kHz, more resonant and louder towards
        the end, which it reaches at full strength. Left and right are partly different noise, so
        it's wide. */
    juce::AudioBuffer<float> noiseRiser (double seconds, double rate)
    {
        auto b = make (seconds, rate);
        juce::Random a (201), c (202);
        Sweeper left, right;
        const auto n = b.getNumSamples();

        for (int i = 0; i < n; ++i)
        {
            const auto p = (double) i / (double) n;
            const auto cutoff = 200.0 * std::pow (60.0, std::pow (p, 1.4));
            const auto q = 0.7 + 2.3 * p * p;
            const auto level = juce::Decibels::decibelsToGain (-30.0 + 30.0 * std::pow (p, 1.2));
            const auto x = a.nextDouble() * 2.0 - 1.0, y = c.nextDouble() * 2.0 - 1.0;
            const auto [ll, lb] = left.process (0.75 * x + 0.25 * y, cutoff, q, rate);
            const auto [rl, rb] = right.process (0.75 * y + 0.25 * x, cutoff, q, rate);
            b.setSample (0, i, (float) ((ll + 0.5 * lb) * level));
            b.setSample (1, i, (float) ((rl + 0.5 * rb) * level));
        }

        finish (b, rate, 0.02, 0.005);
        return b;
    }

    /** A synth riser: detuned saws an octave apart, rising an octave in pitch, through a filter
        opening up, getting louder. */
    juce::AudioBuffer<float> synthRiser (double seconds, double rate)
    {
        auto b = make (seconds, rate);
        constexpr double cents[] { 0.0, 9.0, -9.0, 4.0, -4.0 };
        constexpr double pans[] { 0.0, -0.8, 0.8, 0.4, -0.4 };
        Saw saws[5], octaves[5];
        Sweeper left, right;
        const auto n = b.getNumSamples();

        for (int i = 0; i < n; ++i)
        {
            const auto p = (double) i / (double) n;
            const auto hz = 110.0 * std::pow (2.0, p);   // A2 up to A3
            double l = 0.0, r = 0.0;

            for (int v = 0; v < 5; ++v)
            {
                const auto f = hz * std::pow (2.0, cents[v] / 1200.0);
                const auto s = saws[v].next (f, rate) + 0.5 * octaves[v].next (2.0 * f, rate);
                l += s * (1.0 - pans[v]) * 0.5;
                r += s * (1.0 + pans[v]) * 0.5;
            }

            const auto cutoff = 400.0 * std::pow (22.0, std::pow (p, 1.5));   // 400 Hz to 8.8 kHz
            const auto level = juce::Decibels::decibelsToGain (-24.0 + 24.0 * std::pow (p, 1.3));
            b.setSample (0, i, (float) (left.process (l, cutoff, 1.2, rate).first * level));
            b.setSample (1, i, (float) (right.process (r, cutoff, 1.2, rate).first * level));
        }

        finish (b, rate, 0.02, 0.005);
        return b;
    }

    /** A cymbal wash dying away over its length, backwards: it swells up into its end. */
    juce::AudioBuffer<float> reverseSwell (double seconds, double rate)
    {
        auto b = make (seconds, rate);
        juce::Random a (211), c (212);
        Sweeper left, right, leftLow, rightLow;
        const auto n = b.getNumSamples();
        const auto decay = std::max (0.05, seconds / 5.0);

        for (int i = 0; i < n; ++i)
        {
            // Forwards in time, written backwards.
            const auto t = (double) i / rate;
            const auto env = std::exp (-t / decay) + 0.6 * std::exp (-t / 0.03);
            const auto lband = left.process (a.nextDouble() * 2.0 - 1.0, 9000.0, 0.8, rate).second;
            const auto rband = right.process (c.nextDouble() * 2.0 - 1.0, 9500.0, 0.8, rate).second;
            const auto lmid = leftLow.process (lband, 5000.0, 1.5, rate).second;
            const auto rmid = rightLow.process (rband, 5300.0, 1.5, rate).second;
            b.setSample (0, n - 1 - i, (float) ((lband + 0.5 * lmid) * env));
            b.setSample (1, n - 1 - i, (float) ((rband + 0.5 * rmid) * env));
        }

        finish (b, rate, 0.02, 0.005);
        return b;
    }

    /** Noise falling away: a low-pass closing from 9 kHz to 150 Hz, fading, over a sub that
        falls an octave. */
    juce::AudioBuffer<float> downlifter (double seconds, double rate)
    {
        auto b = make (seconds, rate);
        juce::Random a (221), c (222);
        Sweeper left, right;
        double phase = 0.0;
        const auto n = b.getNumSamples();

        for (int i = 0; i < n; ++i)
        {
            const auto p = (double) i / (double) n;
            const auto cutoff = 9000.0 * std::pow (150.0 / 9000.0, std::pow (p, 0.7));
            const auto level = juce::Decibels::decibelsToGain (-40.0 * std::pow (p, 0.8));
            const auto x = a.nextDouble() * 2.0 - 1.0, y = c.nextDouble() * 2.0 - 1.0;
            phase += (80.0 * std::pow (0.5, p)) / rate;
            const auto sub = 0.35 * std::sin (2.0 * pi * phase);
            b.setSample (0, i, (float) ((left.process (0.75 * x + 0.25 * y, cutoff, 1.5, rate).first + sub) * level));
            b.setSample (1, i, (float) ((right.process (0.75 * y + 0.25 * x, cutoff, 1.5, rate).first + sub) * level));
        }

        finish (b, rate, 0.004, 0.05);
        return b;
    }

    /** One of the cinematic drum sounds, in stereo. */
    juce::AudioBuffer<float> fromDrum (const char* id, double rate)
    {
        const auto mono = drums::render (id, rate);
        juce::AudioBuffer<float> b (2, std::max (1, mono.getNumSamples()));
        b.clear();

        for (int c = 0; c < 2; ++c)
            b.copyFrom (c, 0, mono, 0, 0, mono.getNumSamples());

        finish (b, rate, 0.0, 0.005);
        return b;
    }
}

const std::vector<Info>& all()
{
    static const std::vector<Info> list {
        { "riser-noise",   "Riser (noise)",     true,  true,  "Rushing noise rising into the bar: the classic build-up before a drop or a chorus." },
        { "riser-synth",   "Riser (synth)",     true,  true,  "A synth chord rising an octave and opening up, into the bar." },
        { "reverse-swell", "Reverse swell",     true,  true,  "A cymbal played backwards, swelling into the bar." },
        { "downlifter",    "Downlifter",        false, true,  "Noise falling away from the bar: after a drop, or into a quiet part." },
        { "impact",        "Impact",            false, false, "A big hit with a deep boom, on the bar." },
        { "boom",          "Boom (sub drop)",   false, false, "A deep sub falling in pitch: weight under a drop." },
        { "whoosh",        "Whoosh",            false, false, "Something rushing past: for transitions." },
    };

    return list;
}

const Info* find (const juce::String& id)
{
    for (const auto& info : all())
        if (id == info.id)
            return &info;

    return nullptr;
}

juce::AudioBuffer<float> render (const juce::String& id, double seconds, double sampleRate)
{
    const auto rate = sampleRate > 0.0 ? sampleRate : 48000.0;
    seconds = std::clamp (seconds, 0.05, 120.0);

    if (id == "riser-noise")    return noiseRiser (seconds, rate);
    if (id == "riser-synth")    return synthRiser (seconds, rate);
    if (id == "reverse-swell")  return reverseSwell (seconds, rate);
    if (id == "downlifter")     return downlifter (seconds, rate);
    if (id == "impact")         return fromDrum ("impact", rate);
    if (id == "boom")           return fromDrum ("boom", rate);
    if (id == "whoosh")         return fromDrum ("whoosh", rate);

    return {};
}

} // namespace aether::dsp::soundfx
