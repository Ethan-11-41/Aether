// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
//
// The built-in drum sounds, synthesised. Each one is a short recipe: oscillators with falling
// pitch for kicks and toms, filtered noise for snares and claps, clusters of square waves for
// hi-hats and cymbals (as classic drum machines do), and a little processing for the "lo-fi" ones.

#include "DrumSounds.h"

#include <cmath>
#include <functional>
#include <map>

namespace aether::dsp::drums
{

namespace
{
    constexpr double twoPi = juce::MathConstants<double>::twoPi;

    /** A filter for shaping noise (RBJ biquad). */
    struct Biquad
    {
        enum Type { lowPass, highPass, bandPass };

        Biquad (Type type, double frequency, double q, double sampleRate)
        {
            const auto w = twoPi * std::min (frequency, sampleRate * 0.45) / sampleRate;
            const auto alpha = std::sin (w) / (2.0 * q), cosw = std::cos (w);
            double a0 = 1.0 + alpha;

            switch (type)
            {
                case lowPass:   b0 = (1.0 - cosw) * 0.5; b1 = 1.0 - cosw; b2 = b0; break;
                case highPass:  b0 = (1.0 + cosw) * 0.5; b1 = -(1.0 + cosw); b2 = b0; break;
                case bandPass:  b0 = alpha; b1 = 0.0; b2 = -alpha; break;
            }

            a1 = -2.0 * cosw;
            a2 = 1.0 - alpha;
            b0 /= a0; b1 /= a0; b2 /= a0; a1 /= a0; a2 /= a0;
        }

        double operator() (double x) noexcept
        {
            const auto y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
            x2 = x1; x1 = x; y2 = y1; y1 = y;
            return y;
        }

        double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0, x1 = 0.0, x2 = 0.0, y1 = 0.0, y2 = 0.0;
    };

    /** Writes a sound sample by sample: `sample (t)` for t in seconds. */
    juce::AudioBuffer<float> make (double seconds, double rate, const std::function<double (double)>& sample)
    {
        juce::AudioBuffer<float> out (1, std::max (1, (int) (seconds * rate)));
        auto* d = out.getWritePointer (0);

        for (int i = 0; i < out.getNumSamples(); ++i)
            d[i] = (float) sample (i / rate);

        return out;
    }

    /** Scales a sound so its loudest moment is `peak`, and fades out the last few milliseconds. */
    void finish (juce::AudioBuffer<float>& b, double rate, float peak)
    {
        const auto loudest = b.getMagnitude (0, 0, b.getNumSamples());
        if (loudest > 0.0f)
            b.applyGain (peak / loudest);

        const auto fade = std::min (b.getNumSamples(), (int) (0.005 * rate));
        b.applyGainRamp (b.getNumSamples() - fade, fade, 1.0f, 0.0f);
    }

    double envelope (double t, double tau) noexcept   { return std::exp (-t / tau); }

    /** A softly rounded attack, so sounds don't click. */
    double attack (double t, double seconds = 0.0008) noexcept   { return std::min (1.0, t / seconds); }

    /** Six square waves at inharmonic frequencies: the metallic shimmer of drum-machine cymbals. */
    struct Metal
    {
        explicit Metal (double frequencyScale) : scale (frequencyScale) {}

        double operator() (double t) const noexcept
        {
            static constexpr double freqs[] { 205.3, 304.4, 369.6, 522.7, 540.0, 800.0 };
            double v = 0.0;
            for (const auto f : freqs)
                v += std::fmod (t * f * scale, 1.0) < 0.5 ? 1.0 : -1.0;
            return v / 6.0;
        }

        double scale;
    };

    /** Lower sample rate and fewer bits: the sound of old samplers. */
    void lofi (juce::AudioBuffer<float>& b, double rate)
    {
        auto* d = b.getWritePointer (0);
        const auto hold = std::max (1, (int) std::round (rate / 11025.0));
        float held = 0.0f;
        double smooth = 0.0;
        const auto coefficient = 1.0 - std::exp (-twoPi * 6000.0 / rate);

        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            if (i % hold == 0)
                held = std::round (d[i] * 127.0f) / 127.0f;   // 8 bits

            smooth += (std::tanh (1.3 * held) - smooth) * coefficient;
            d[i] = (float) smooth;
        }
    }

    //==============================================================================
    juce::AudioBuffer<float> kick (double rate, double startHz, double endHz, double pitchTau, double decayTau,
                                   double click, double drive, double length)
    {
        double phase = 0.0;
        juce::Random random (11);
        Biquad clickFilter (Biquad::highPass, 2500.0, 0.7, rate);

        auto out = make (length, rate, [&] (double t)
        {
            const auto f = endHz + (startHz - endHz) * envelope (t, pitchTau);
            phase += f / rate;
            auto v = std::sin (twoPi * phase) * envelope (t, decayTau) * attack (t);
            v += click * clickFilter (random.nextDouble() * 2.0 - 1.0) * envelope (t, 0.003);
            return std::tanh (v * drive) / std::tanh (drive);
        });

        finish (out, rate, 0.95f);
        return out;
    }

    juce::AudioBuffer<float> snare (double rate, double tone1, double tone2, double toneTau, double noiseTau,
                                    double noiseLow, double noiseHigh, double toneMix, double length, int seed = 21)
    {
        double p1 = 0.0, p2 = 0.0;
        juce::Random random (seed);
        Biquad hp (Biquad::highPass, noiseLow, 0.7, rate), lp (Biquad::lowPass, noiseHigh, 0.7, rate);

        auto out = make (length, rate, [&] (double t)
        {
            const auto drop = 1.0 + 0.3 * envelope (t, 0.01);
            p1 += tone1 * drop / rate;
            p2 += tone2 * drop / rate;
            const auto tone = (std::sin (twoPi * p1) + 0.6 * std::sin (twoPi * p2)) * envelope (t, toneTau);
            const auto noise = lp (hp (random.nextDouble() * 2.0 - 1.0)) * envelope (t, noiseTau);
            return (toneMix * tone + (1.0 - toneMix) * 2.0 * noise) * attack (t);
        });

        finish (out, rate, 0.9f);
        return out;
    }

    juce::AudioBuffer<float> clap (double rate, double centre, int bursts, double spacing, double tailTau, double length)
    {
        juce::Random random (31);
        Biquad bp (Biquad::bandPass, centre, 1.4, rate), hp (Biquad::highPass, 600.0, 0.7, rate);

        auto out = make (length, rate, [&] (double t)
        {
            double env = 0.0;
            for (int i = 0; i < bursts; ++i)
                if (t >= i * spacing)
                    env = std::max (env, envelope (t - i * spacing, 0.006));

            const auto tailStart = (bursts - 1) * spacing;
            if (t >= tailStart)
                env = std::max (env, 0.6 * envelope (t - tailStart, tailTau));

            return hp (bp (random.nextDouble() * 2.0 - 1.0)) * env * attack (t);
        });

        finish (out, rate, 0.85f);
        return out;
    }

    juce::AudioBuffer<float> hat (double rate, double decayTau, double metal, double brightness, double length, int seed = 41,
                                  double softAttack = 0.0)
    {
        juce::Random random (seed);
        const Metal squares (1.0);
        Biquad bp (Biquad::bandPass, 10000.0 * brightness, 0.8, rate), hp (Biquad::highPass, 7000.0 * brightness, 0.7, rate);

        auto out = make (length, rate, [&] (double t)
        {
            const auto source = metal * squares (t) + (1.0 - metal) * (random.nextDouble() * 2.0 - 1.0);
            const auto env = envelope (t, decayTau) * (softAttack > 0.0 ? attack (t, softAttack) : attack (t));
            return hp (bp (source)) * env;
        });

        finish (out, rate, 0.55f);
        return out;
    }

    juce::AudioBuffer<float> cymbal (double rate, double decayTau, double scale, double brightness, double ping, double length)
    {
        juce::Random random (51);
        const Metal squares (scale);
        Biquad hp (Biquad::highPass, 4000.0 * brightness, 0.7, rate), lp (Biquad::lowPass, 14000.0, 0.7, rate);

        auto out = make (length, rate, [&] (double t)
        {
            const auto wash = 0.6 * squares (t) + 0.4 * (random.nextDouble() * 2.0 - 1.0);
            auto v = lp (hp (wash)) * (envelope (t, decayTau) + 0.8 * envelope (t, 0.02));

            // A ride's "ping": a few clear partials.
            if (ping > 0.0)
                v += ping * (std::sin (twoPi * 2140.0 * t) + 0.7 * std::sin (twoPi * 2963.0 * t) + 0.5 * std::sin (twoPi * 4300.0 * t))
                     / 2.2 * envelope (t, decayTau * 0.6);

            return v * attack (t);
        });

        finish (out, rate, 0.5f);
        return out;
    }

    juce::AudioBuffer<float> tom (double rate, double hz, double decayTau, double drop, double noise, double length)
    {
        double phase = 0.0;
        juce::Random random (61);
        Biquad lp (Biquad::lowPass, 3000.0, 0.7, rate);

        auto out = make (length, rate, [&] (double t)
        {
            phase += hz * (1.0 + drop * envelope (t, 0.04)) / rate;
            const auto body = std::sin (twoPi * phase) * envelope (t, decayTau);
            const auto skin = noise * lp (random.nextDouble() * 2.0 - 1.0) * envelope (t, 0.02);
            return (body + skin) * attack (t);
        });

        finish (out, rate, 0.85f);
        return out;
    }

    /** A band-limited square wave (odd harmonics up to the top of the sample rate). */
    double square (double t, double hz, double rate)
    {
        double v = 0.0;
        for (int k = 1; k * hz < rate * 0.45; k += 2)
            v += std::sin (twoPi * k * hz * t) / k;
        return v * 4.0 / juce::MathConstants<double>::pi;
    }

    juce::AudioBuffer<float> cowbell (double rate)
    {
        Biquad bp (Biquad::bandPass, 2640.0, 1.2, rate);
        auto out = make (0.5, rate, [&] (double t)
        {
            const auto env = 0.7 * envelope (t, 0.015) + 0.3 * envelope (t, 0.18);
            return bp (square (t, 540.0, rate) + square (t, 800.0, rate)) * env * attack (t);
        });
        finish (out, rate, 0.6f);
        return out;
    }

    juce::AudioBuffer<float> rim (double rate, double hz1, double hz2, double length)
    {
        juce::Random random (71);
        Biquad hp (Biquad::highPass, 3000.0, 0.7, rate);
        auto out = make (length, rate, [&] (double t)
        {
            return (std::sin (twoPi * hz1 * t) * envelope (t, 0.012) + 0.7 * std::sin (twoPi * hz2 * t) * envelope (t, 0.018)
                    + 0.5 * hp (random.nextDouble() * 2.0 - 1.0) * envelope (t, 0.006)) * attack (t);
        });
        finish (out, rate, 0.7f);
        return out;
    }

    juce::AudioBuffer<float> shaker (double rate, double rise, double decayTau, double length, int seed)
    {
        juce::Random random (seed);
        Biquad hp (Biquad::highPass, 5000.0, 0.7, rate), lp (Biquad::lowPass, 12000.0, 0.7, rate);
        auto out = make (length, rate, [&] (double t)
        {
            return lp (hp (random.nextDouble() * 2.0 - 1.0)) * std::min (1.0, t / rise) * envelope (std::max (0.0, t - rise), decayTau);
        });
        finish (out, rate, 0.45f);
        return out;
    }

    juce::AudioBuffer<float> tambourine (double rate)
    {
        juce::Random random (81);
        Biquad hp (Biquad::highPass, 6000.0, 0.7, rate);
        auto out = make (0.5, rate, [&] (double t)
        {
            const auto jingles = std::sin (twoPi * 6200.0 * t) + std::sin (twoPi * 7900.0 * t) + std::sin (twoPi * 9300.0 * t);
            return (hp (random.nextDouble() * 2.0 - 1.0) + 0.3 * jingles / 3.0) * envelope (t, 0.11) * attack (t, 0.002);
        });
        finish (out, rate, 0.45f);
        return out;
    }

    juce::AudioBuffer<float> conga (double rate, double hz)
    {
        double phase = 0.0;
        auto out = make (0.6, rate, [&] (double t)
        {
            phase += hz * (1.0 + 0.08 * envelope (t, 0.02)) / rate;
            return (std::sin (twoPi * phase) * envelope (t, 0.14) + 0.25 * std::sin (twoPi * phase * 2.6) * envelope (t, 0.04)) * attack (t);
        });
        finish (out, rate, 0.8f);
        return out;
    }

    juce::AudioBuffer<float> clave (double rate)
    {
        auto out = make (0.2, rate, [&] (double t) { return std::sin (twoPi * 2500.0 * t) * envelope (t, 0.025) * attack (t); });
        finish (out, rate, 0.6f);
        return out;
    }

    juce::AudioBuffer<float> snap (double rate)
    {
        juce::Random random (91);
        Biquad bp (Biquad::bandPass, 2600.0, 2.0, rate);
        auto out = make (0.25, rate, [&] (double t)
        {
            return (bp (random.nextDouble() * 2.0 - 1.0) * envelope (t, 0.018) + 0.4 * std::sin (twoPi * 2100.0 * t) * envelope (t, 0.008))
                   * attack (t);
        });
        finish (out, rate, 0.75f);
        return out;
    }

    juce::AudioBuffer<float> withLofi (juce::AudioBuffer<float> b, double rate, float peak)
    {
        lofi (b, rate);
        finish (b, rate, peak);
        return b;
    }

    using Recipe = std::function<juce::AudioBuffer<float> (double)>;

    struct Sound
    {
        SoundInfo info;
        Recipe recipe;
    };

    const std::vector<Sound>& sounds()
    {
        static const std::vector<Sound> list {
            { { "kick-punchy",   "Kick (punchy)",   "Kicks" },   [] (double r) { return kick (r, 140.0, 55.0, 0.012, 0.18, 0.5, 1.6, 0.6); } },
            { { "kick-808",      "Kick 808",        "Kicks" },   [] (double r) { return kick (r, 160.0, 48.0, 0.03, 0.45, 0.0, 1.4, 1.6); } },
            { { "kick-909",      "Kick 909",        "Kicks" },   [] (double r) { return kick (r, 230.0, 52.0, 0.018, 0.25, 0.7, 2.2, 0.8); } },
            { { "kick-deep",     "Kick (deep)",     "Kicks" },   [] (double r) { return kick (r, 110.0, 38.0, 0.04, 0.6, 0.15, 1.2, 1.8); } },
            { { "kick-lofi",     "Kick (lo-fi)",    "Kicks" },   [] (double r) { return withLofi (kick (r, 120.0, 50.0, 0.015, 0.22, 0.4, 2.0, 0.7), r, 0.9f); } },

            { { "snare-tight",   "Snare (tight)",   "Snares" },  [] (double r) { return snare (r, 190.0, 330.0, 0.05, 0.13, 1800.0, 9000.0, 0.35, 0.5); } },
            { { "snare-808",     "Snare 808",       "Snares" },  [] (double r) { return snare (r, 238.0, 476.0, 0.05, 0.11, 2000.0, 12000.0, 0.45, 0.45); } },
            { { "snare-909",     "Snare 909",       "Snares" },  [] (double r) { return snare (r, 200.0, 400.0, 0.04, 0.16, 1500.0, 11000.0, 0.3, 0.55, 22); } },
            { { "snare-brush",   "Snare (brush)",   "Snares" },  [] (double r) { return snare (r, 180.0, 300.0, 0.02, 0.2, 1200.0, 6000.0, 0.1, 0.6, 23); } },
            { { "snare-lofi",    "Snare (lo-fi)",   "Snares" },  [] (double r) { return withLofi (snare (r, 190.0, 330.0, 0.05, 0.15, 1500.0, 8000.0, 0.35, 0.5), r, 0.85f); } },

            { { "clap",          "Clap",            "Claps and snaps" },   [] (double r) { return clap (r, 1300.0, 4, 0.011, 0.13, 0.6); } },
            { { "clap-808",      "Clap 808",        "Claps and snaps" },   [] (double r) { return clap (r, 1100.0, 3, 0.009, 0.18, 0.7); } },
            { { "snap",          "Finger snap",     "Claps and snaps" },   [] (double r) { return snap (r); } },

            { { "rim",           "Rim",             "Rims" },    [] (double r) { return rim (r, 1700.0, 820.0, 0.2); } },
            { { "rim-808",       "Rim 808",         "Rims" },    [] (double r) { return rim (r, 1667.0, 455.0, 0.15); } },

            { { "hat-closed",    "Hi-hat (closed)", "Hi-hats" }, [] (double r) { return hat (r, 0.03, 0.4, 1.0, 0.3); } },
            { { "hat-pedal",     "Hi-hat (pedal)",  "Hi-hats" }, [] (double r) { return hat (r, 0.02, 0.4, 0.85, 0.25, 42, 0.004); } },
            { { "hat-open",      "Hi-hat (open)",   "Hi-hats" }, [] (double r) { return hat (r, 0.25, 0.4, 1.0, 1.2); } },
            { { "hat-808-closed","Hi-hat 808 (closed)", "Hi-hats" }, [] (double r) { return hat (r, 0.025, 0.9, 0.95, 0.25, 43); } },
            { { "hat-808-open",  "Hi-hat 808 (open)",   "Hi-hats" }, [] (double r) { return hat (r, 0.2, 0.9, 0.95, 1.0, 43); } },
            { { "hat-lofi-closed","Hi-hat lo-fi (closed)", "Hi-hats" }, [] (double r) { return withLofi (hat (r, 0.035, 0.5, 0.9, 0.3), r, 0.5f); } },
            { { "hat-lofi-open", "Hi-hat lo-fi (open)", "Hi-hats" }, [] (double r) { return withLofi (hat (r, 0.22, 0.5, 0.9, 1.0), r, 0.5f); } },

            { { "tom-low",       "Tom (low)",       "Toms" },    [] (double r) { return tom (r, 95.0, 0.28, 0.4, 0.3, 1.0); } },
            { { "tom-mid",       "Tom (mid)",       "Toms" },    [] (double r) { return tom (r, 140.0, 0.22, 0.4, 0.3, 0.8); } },
            { { "tom-high",      "Tom (high)",      "Toms" },    [] (double r) { return tom (r, 200.0, 0.18, 0.4, 0.3, 0.7); } },
            { { "tom-808-low",   "Tom 808 (low)",   "Toms" },    [] (double r) { return tom (r, 90.0, 0.35, 0.15, 0.0, 1.2); } },
            { { "tom-808-mid",   "Tom 808 (mid)",   "Toms" },    [] (double r) { return tom (r, 130.0, 0.3, 0.15, 0.0, 1.0); } },
            { { "tom-808-high",  "Tom 808 (high)",  "Toms" },    [] (double r) { return tom (r, 190.0, 0.25, 0.15, 0.0, 0.9); } },

            { { "crash",         "Crash",           "Cymbals" }, [] (double r) { return cymbal (r, 0.75, 1.6, 1.0, 0.0, 3.0); } },
            { { "ride",          "Ride",            "Cymbals" }, [] (double r) { return cymbal (r, 0.6, 2.2, 0.8, 0.35, 2.5); } },
            { { "cymbal-808",    "Cymbal 808",      "Cymbals" }, [] (double r) { return cymbal (r, 0.5, 1.0, 0.9, 0.0, 2.5); } },

            { { "cowbell",       "Cowbell",         "Percussion" }, [] (double r) { return cowbell (r); } },
            { { "shaker",        "Shaker",          "Percussion" }, [] (double r) { return shaker (r, 0.015, 0.05, 0.25, 101); } },
            { { "tambourine",    "Tambourine",      "Percussion" }, [] (double r) { return tambourine (r); } },
            { { "conga-low",     "Conga (low)",     "Percussion" }, [] (double r) { return conga (r, 190.0); } },
            { { "conga-high",    "Conga (high)",    "Percussion" }, [] (double r) { return conga (r, 290.0); } },
            { { "clave",         "Clave",           "Percussion" }, [] (double r) { return clave (r); } },
        };

        return list;
    }
}

const std::vector<SoundInfo>& builtInSounds()
{
    static const std::vector<SoundInfo> infos = []
    {
        std::vector<SoundInfo> list;
        for (const auto& s : sounds())
            list.push_back (s.info);
        return list;
    }();

    return infos;
}

const SoundInfo* findSound (const juce::String& id)
{
    for (const auto& s : builtInSounds())
        if (id == s.id)
            return &s;

    return nullptr;
}

juce::AudioBuffer<float> render (const juce::String& id, double sampleRate)
{
    for (const auto& s : sounds())
        if (id == s.info.id)
            return s.recipe (sampleRate > 0.0 ? sampleRate : 48000.0);

    return {};
}

//==============================================================================
const std::array<int, numPads>& padDisplayOrder()
{
    // Notes 36 kick, 38 snare, 40 snare 2, 39 clap, 37 rim, 42 closed hat, 44 pedal hat, 46 open
    // hat, 41/43/45/47/48/50 toms, 49 crash, 51 ride.
    static constexpr std::array<int, numPads> order { 0, 2, 4, 3, 1, 6, 8, 10, 5, 7, 9, 11, 12, 14, 13, 15 };
    return order;
}

const std::vector<Kit>& kits()
{
    // Pads by note: 36 kick, 37 rim, 38 snare, 39 clap, 40 snare 2, 41 low tom, 42 closed hat,
    // 43 low-mid tom, 44 pedal hat, 45 mid tom, 46 open hat, 47 mid-high tom, 48 high tom,
    // 49 crash, 50 highest tom, 51 ride. Hi-hats choke each other (group 1).
    static const std::vector<Kit> list {
        { "Tight kit", {{
            { "kick-punchy" }, { "rim" }, { "snare-tight" }, { "clap", 0, -3 }, { "snare-brush", 0, -4 },
            { "tom-low", 0, -2, -30 }, { "hat-closed", 0, -4, 15, 100, 1 }, { "tom-low", 3, -2, -20 },
            { "hat-pedal", 0, -6, 15, 100, 1 }, { "tom-mid", 0, -2, 0 }, { "hat-open", 0, -5, 15, 100, 1 },
            { "tom-mid", 3, -2, 15 }, { "tom-high", 0, -2, 25 }, { "crash", 0, -6, -25 }, { "tom-high", 3, -2, 35 },
            { "ride", 0, -6, 30 } }} },

        { "808", {{
            { "kick-808" }, { "rim-808", 0, -3 }, { "snare-808" }, { "clap-808", 0, -2 }, { "cowbell", 0, -6 },
            { "tom-808-low", 0, -2, -30 }, { "hat-808-closed", 0, -4, 10, 100, 1 }, { "tom-808-low", 3, -2, -20 },
            { "shaker", 0, -6, 20 }, { "tom-808-mid", 0, -2, 0 }, { "hat-808-open", 0, -5, 10, 100, 1 },
            { "tom-808-mid", 3, -2, 15 }, { "tom-808-high", 0, -2, 25 }, { "cymbal-808", 0, -6, -20 },
            { "conga-high", 0, -4, 30 }, { "clave", 0, -6, 20 } }} },

        { "Electronic", {{
            { "kick-909" }, { "snap", 0, -2 }, { "snare-909" }, { "clap", 0, -1 }, { "snare-808", 2, -3 },
            { "tom-808-low", -2, -3, -30 }, { "hat-808-closed", 2, -5, 10, 60, 1 }, { "tom-808-mid", -2, -3, -10 },
            { "shaker", 0, -5, 20 }, { "tom-808-high", -2, -3, 10 }, { "hat-808-open", 2, -6, 10, 70, 1 },
            { "conga-low", 0, -4, -20 }, { "conga-high", 0, -4, 20 }, { "crash", 0, -8, -25 }, { "tambourine", 0, -6, 30 },
            { "ride", 0, -8, 30 } }} },

        { "Lo-fi", {{
            { "kick-lofi" }, { "rim", 0, -4 }, { "snare-lofi" }, { "clap", -2, -4 }, { "snare-brush", 0, -5 },
            { "tom-low", -3, -4, -30 }, { "hat-lofi-closed", 0, -5, 10, 100, 1 }, { "tom-mid", -3, -4, -10 },
            { "shaker", 0, -8, 20 }, { "tom-high", -3, -4, 10 }, { "hat-lofi-open", 0, -6, 10, 80, 1 },
            { "conga-low", 0, -6, -20 }, { "conga-high", 0, -6, 20 }, { "crash", -2, -9, -25 }, { "tambourine", 0, -8, 30 },
            { "ride", -2, -9, 30 } }} },

        { "Deep", {{
            { "kick-deep" }, { "rim", 0, -4 }, { "snare-808", -2 }, { "clap-808", -1, -3 }, { "snap", 0, -4 },
            { "tom-808-low", -3, -3, -30 }, { "hat-closed", 0, -6, 10, 80, 1 }, { "tom-808-low", 0, -3, -15 },
            { "shaker", 0, -7, 20 }, { "tom-808-mid", -2, -3, 5 }, { "hat-open", 0, -7, 10, 70, 1 },
            { "tom-808-mid", 1, -3, 15 }, { "tom-808-high", -1, -3, 25 }, { "cymbal-808", -2, -8, -20 },
            { "conga-low", -2, -5, 25 }, { "ride", -2, -9, 30 } }} },
    };

    return list;
}

const Kit* findKit (const juce::String& name)
{
    for (const auto& kit : kits())
        if (name == kit.name)
            return &kit;

    return nullptr;
}

} // namespace aether::dsp::drums
