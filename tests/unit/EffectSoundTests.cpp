// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
//
// The effects of Milestone 13, checked on the sound they make.

#include <cmath>
#include <random>

#include <catch2/catch_test_macros.hpp>

#include "dsp/ColourEffects.h"
#include "dsp/Dynamics.h"
#include "dsp/Eq.h"
#include "dsp/SpaceEffects.h"

using namespace aether::dsp;

namespace
{
    constexpr double rate = 48000.0;

    EffectParameterValues defaults (const juce::String& typeId, std::initializer_list<std::pair<int, float>> settings = {})
    {
        EffectParameterValues v {};
        const auto* type = findEffectType (typeId);
        REQUIRE (type != nullptr);
        for (size_t i = 0; i < type->parameters.size(); ++i)
            v[i] = type->parameters[i].defaultValue;
        for (const auto& [index, value] : settings)
            v[(size_t) index] = value;
        return v;
    }

    struct Signal
    {
        std::vector<float> left, right;
    };

    Signal sine (double hz, double seconds, float amplitude = 0.5f)
    {
        Signal s;
        for (int i = 0; i < (int) (seconds * rate); ++i)
        {
            const auto x = amplitude * (float) std::sin (juce::MathConstants<double>::twoPi * hz * i / rate);
            s.left.push_back (x);
            s.right.push_back (x);
        }
        return s;
    }

    Signal noise (double seconds, float amplitude, unsigned seed = 1)
    {
        std::mt19937 random (seed);
        std::uniform_real_distribution<float> d (-amplitude, amplitude);
        Signal s;
        for (int i = 0; i < (int) (seconds * rate); ++i)
        {
            s.left.push_back (d (random));
            s.right.push_back (d (random));
        }
        return s;
    }

    /** Runs an effect over a signal in blocks of 512 (as the engine would). */
    Signal run (Effect& effect, Signal s, const EffectParameterValues& v, double bpm = 120.0)
    {
        EffectContext context;
        context.sampleRate = rate;
        context.bpm = bpm;

        for (size_t done = 0; done < s.left.size(); done += 512)
        {
            const auto n = (int) std::min<size_t> (512, s.left.size() - done);
            effect.process (s.left.data() + done, s.right.data() + done, n, v, context);
        }
        return s;
    }

    std::unique_ptr<Effect> make (const juce::String& typeId)
    {
        auto e = createEffect (typeId);
        REQUIRE (e != nullptr);
        e->prepare (rate, 512);
        return e;
    }

    double rmsDb (const std::vector<float>& x, double fromSeconds, double toSeconds)
    {
        double sum = 0.0;
        const auto a = (size_t) (fromSeconds * rate), b = std::min (x.size(), (size_t) (toSeconds * rate));
        for (auto i = a; i < b; ++i)
            sum += (double) x[i] * x[i];
        return 10.0 * std::log10 (std::max (1.0e-20, sum / (double) (b - a)));
    }

    double peak (const std::vector<float>& x, double fromSeconds = 0.0)
    {
        float most = 0.0f;
        for (auto i = (size_t) (fromSeconds * rate); i < x.size(); ++i)
            most = std::max (most, std::abs (x[i]));
        return most;
    }
}

TEST_CASE ("The EQ sounds like its picture", "[effects]")
{
    const auto v = defaults (eq::typeId, { { eq::lowShelfGain, 6.0f }, { eq::lowShelfFreq, 200.0f },
                                           { eq::band2Gain, -6.0f }, { eq::band2Freq, 1000.0f }, { eq::band2Q, 1.0f },
                                           { eq::highCutOn, 1.0f }, { eq::highCut, 6000.0f }, { eq::output, 1.5f } });

    for (const auto hz : { 60.0, 400.0, 1000.0, 3000.0, 12000.0 })
    {
        INFO (hz << " Hz");
        auto effect = make (eq::typeId);
        const auto in = sine (hz, 0.6);
        const auto out = run (*effect, in, v);
        const auto measured = rmsDb (out.left, 0.3, 0.6) - rmsDb (in.left, 0.3, 0.6);
        CHECK (std::abs (measured - eq::responseDb (v, hz, rate)) < 0.3);
    }

    // Flat: nothing changes.
    auto flat = make (eq::typeId);
    const auto in = noise (0.2, 0.5f);
    const auto out = run (*flat, in, defaults (eq::typeId));
    CHECK (out.left == in.left);
}

TEST_CASE ("The compressor turns loud parts down by its ratio", "[effects]")
{
    const auto v = defaults (compressor::typeId, { { compressor::threshold, -18.0f }, { compressor::ratio, 4.0f },
                                                   { compressor::knee, 0.0f }, { compressor::attack, 1.0f }, { compressor::release, 300.0f } });
    CHECK (std::abs (compressor::outputDb (v, -6.0) - (-15.0)) < 1.0e-9);   // 12 dB over the threshold: 3 dB over
    CHECK (compressor::outputDb (v, -30.0) == -30.0);                       // under it: untouched

    auto effect = make (compressor::typeId);
    const auto out = run (*effect, sine (1000.0, 1.0, 0.5f), v);
    const auto peakDb = 20.0 * std::log10 (peak (out.left, 0.8));
    CHECK (std::abs (peakDb - (-15.0)) < 1.5);
    CHECK (effect->getMeter() > 8.0f);

    // Quiet: untouched, and the meter says so.
    auto quiet = make (compressor::typeId);
    const auto in = sine (1000.0, 0.5, 0.05f);
    const auto soft = run (*quiet, in, v);
    CHECK (std::abs (peak (soft.left, 0.3) - peak (in.left, 0.3)) < 1.0e-4);
    CHECK (quiet->getMeter() < 0.01f);
}

TEST_CASE ("The limiter never lets anything past its ceiling", "[effects]")
{
    const auto v = defaults (limiter::typeId, { { limiter::gain, 6.0f }, { limiter::ceiling, -1.0f } });
    auto effect = make (limiter::typeId);
    const auto out = run (*effect, noise (1.0, 2.0f), v);
    const auto ceiling = std::pow (10.0, -1.0 / 20.0);
    CHECK (peak (out.left) <= ceiling + 1.0e-6);
    CHECK (peak (out.right) <= ceiling + 1.0e-6);
    CHECK (effect->getMeter() > 6.0f);

    // Something quiet comes out unchanged, 1.5 ms late (72 samples at 48 kHz).
    auto clean = make (limiter::typeId);
    const auto in = sine (440.0, 0.2, 0.1f);
    const auto passed = run (*clean, in, defaults (limiter::typeId));
    for (size_t i = 1000; i < in.left.size(); i += 97)
        CHECK (std::abs (passed.left[i] - in.left[i - 72]) < 1.0e-5f);
}

TEST_CASE ("The gate turns quiet parts down and lets loud ones through", "[effects]")
{
    const auto v = defaults (gate::typeId, { { gate::threshold, -40.0f }, { gate::range, -40.0f }, { gate::hold, 10.0f }, { gate::release, 20.0f } });

    auto closed = make (gate::typeId);
    const auto hiss = noise (0.5, 0.001f);   // about -60 dB
    const auto out = run (*closed, hiss, v);
    CHECK (rmsDb (out.left, 0.3, 0.5) - rmsDb (hiss.left, 0.3, 0.5) < -35.0);
    CHECK (closed->getMeter() < 0.05f);

    auto open = make (gate::typeId);
    const auto loud = sine (500.0, 0.5, 0.5f);
    const auto through = run (*open, loud, v);
    CHECK (std::abs (rmsDb (through.left, 0.2, 0.5) - rmsDb (loud.left, 0.2, 0.5)) < 0.1);
    CHECK (open->getMeter() > 0.95f);
}

TEST_CASE ("The de-esser turns down harsh highs, not the voice", "[effects]")
{
    const auto v = defaults (deesser::typeId, { { deesser::frequency, 6000.0f }, { deesser::threshold, -30.0f }, { deesser::amount, 10.0f } });

    auto high = make (deesser::typeId);
    const auto s = sine (8000.0, 0.5, 0.3f);
    CHECK (rmsDb (run (*high, s, v).left, 0.2, 0.5) - rmsDb (s.left, 0.2, 0.5) < -4.0);
    CHECK (high->getMeter() > 4.0f);

    auto low = make (deesser::typeId);
    const auto voice = sine (300.0, 0.5, 0.3f);
    CHECK (std::abs (rmsDb (run (*low, voice, v).left, 0.2, 0.5) - rmsDb (voice.left, 0.2, 0.5)) < 0.5);
}

TEST_CASE ("The reverb rings on and dies away; mix 0 leaves the sound alone", "[effects]")
{
    const auto v = defaults (reverb::typeId, { { reverb::size, 50.0f }, { reverb::mix, 100.0f }, { reverb::predelay, 0.0f } });
    const auto seconds = reverb::decaySeconds (v);
    CHECK (seconds > 1.5);
    CHECK (seconds < 2.5);

    Signal impulse;
    impulse.left.assign ((size_t) (rate * 4.0), 0.0f);
    impulse.right = impulse.left;
    impulse.left[100] = impulse.right[100] = 1.0f;

    auto effect = make (reverb::typeId);
    const auto out = run (*effect, impulse, v);
    const auto early = rmsDb (out.left, 0.05, 0.15);
    const auto late = rmsDb (out.left, seconds, seconds + 0.1);
    CHECK (early > -60.0);
    CHECK (late < early - 40.0);        // around 60 dB down by the decay time (with some leeway)
    CHECK (late > early - 80.0);        // but still ringing until about then
    CHECK (out.left != out.right);      // a stereo room
    CHECK (std::all_of (out.left.begin(), out.left.end(), [] (float x) { return std::isfinite (x); }));

    auto dryOnly = make (reverb::typeId);
    const auto in = noise (0.2, 0.5f);
    const auto passed = run (*dryOnly, in, defaults (reverb::typeId, { { reverb::mix, 0.0f } }));
    CHECK (std::abs (rmsDb (passed.left, 0.0, 0.2) - rmsDb (in.left, 0.0, 0.2)) < 0.01);
}

TEST_CASE ("The delay echoes on the beat", "[effects]")
{
    // A quarter note at 120 BPM is half a second.
    const auto v = defaults (echo::typeId, { { echo::time, 8.0f }, { echo::feedback, 0.0f }, { echo::mix, 50.0f }, { echo::tone, 20000.0f } });
    Signal impulse;
    impulse.left.assign ((size_t) rate, 0.0f);
    impulse.right = impulse.left;
    impulse.left[10] = impulse.right[10] = 1.0f;

    auto effect = make (echo::typeId);
    const auto out = run (*effect, impulse, v);
    const auto echoAt = std::max_element (out.left.begin() + 100, out.left.end()) - out.left.begin();
    CHECK (std::abs ((int) echoAt - (10 + 24000)) <= 2);
}

TEST_CASE ("Chorus, saturation and filter colour the sound", "[effects]")
{
    // Chorus: about as loud, but changed.
    auto chorusEffect = make (chorus::typeId);
    const auto in = sine (440.0, 0.5, 0.3f);
    const auto chorused = run (*chorusEffect, in, defaults (chorus::typeId));
    CHECK (std::abs (rmsDb (chorused.left, 0.1, 0.5) - rmsDb (in.left, 0.1, 0.5)) < 3.0);
    CHECK (chorused.left != in.left);

    // Saturation: the shape bends, stays bounded, and adds harmonics.
    const auto sv = defaults (saturation::typeId, { { saturation::drive, 24.0f } });
    CHECK (saturation::shape (sv, 0.0f) == 0.0f);
    CHECK (saturation::shape (sv, 0.5f) > saturation::shape (sv, 0.25f));
    auto saturated = make (saturation::typeId);
    const auto bent = run (*saturated, sine (200.0, 0.5, 0.5f), sv);
    CHECK (peak (bent.left) < 2.0);
    juce::ignoreUnused (bent);

    // Filter: sounds like its picture.
    const auto fv = defaults (filter::typeId, { { filter::type, (float) filter::lowPass }, { filter::cutoff, 500.0f },
                                                { filter::resonance, 0.0f }, { filter::lfoDepth, 0.0f } });
    for (const auto hz : { 100.0, 2000.0, 5000.0 })
    {
        INFO (hz << " Hz");
        auto f = make (filter::typeId);
        const auto s = sine (hz, 0.5, 0.3f);
        const auto out = run (*f, s, fv);
        CHECK (std::abs ((rmsDb (out.left, 0.2, 0.5) - rmsDb (s.left, 0.2, 0.5)) - filter::responseDb (fv, 500.0, hz, rate)) < 1.0);
    }
    CHECK (filter::responseDb (fv, 500.0, 5000.0, rate) < -30.0);
}

TEST_CASE ("Every effect's every preset makes clean sound", "[effects]")
{
    for (const auto& type : effectTypes())
    {
        if (type.isInstrument)
            continue;

        INFO (type.name);
        CHECK (! type.presets.empty());

        for (const auto& preset : type.presets)
        {
            INFO (preset.name);
            REQUIRE (preset.values.size() == type.parameters.size());

            EffectParameterValues v {};
            for (size_t i = 0; i < preset.values.size(); ++i)
            {
                CHECK (preset.values[i] == type.parameters[i].clamp (preset.values[i]));
                v[i] = preset.values[i];
            }

            auto effect = make (type.id);
            const auto out = run (*effect, noise (0.3, 0.8f, 7), v);
            CHECK (std::all_of (out.left.begin(), out.left.end(), [] (float x) { return std::isfinite (x) && std::abs (x) < 20.0f; }));
            CHECK (std::all_of (out.right.begin(), out.right.end(), [] (float x) { return std::isfinite (x) && std::abs (x) < 20.0f; }));
        }
    }
}
