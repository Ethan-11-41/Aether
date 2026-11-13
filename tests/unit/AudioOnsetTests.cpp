// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
//
// Finding hits in recordings (Milestone 17), for the beat finder.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include "time/AudioOnsets.h"

using namespace aether;

namespace
{
    constexpr double rate = 48000.0;

    void addKick (juce::AudioBuffer<float>& b, double at)
    {
        const auto start = (int) std::lround (at * rate);
        double phase = 0.0;
        for (int i = 0; i < 12000 && start + i < b.getNumSamples(); ++i)
        {
            const auto t = i / rate;
            phase += (50.0 + 90.0 * std::exp (-t * 40.0)) / rate;
            const auto fade = std::min (1.0, (12000 - i) / 1000.0);   // no click at the end
            const auto v = 0.9f * (float) (std::sin (juce::MathConstants<double>::twoPi * phase) * std::exp (-t * 12.0) * fade);
            for (int c = 0; c < b.getNumChannels(); ++c)
                b.addSample (c, start + i, v);
        }
    }

    void addHat (juce::AudioBuffer<float>& b, double at, juce::Random& random, float level = 0.3f)
    {
        const auto start = (int) std::lround (at * rate);
        juce::IIRFilter high;
        high.setCoefficients (juce::IIRCoefficients::makeHighPass (rate, 6000.0));
        for (int i = 0; i < 3000 && start + i < b.getNumSamples(); ++i)
        {
            const auto v = high.processSingleSampleRaw ((random.nextFloat() * 2.0f - 1.0f)) * level * std::exp ((float) -i / 500.0f);
            for (int c = 0; c < b.getNumChannels(); ++c)
                b.addSample (c, start + i, v);
        }
    }

    void addSnare (juce::AudioBuffer<float>& b, double at, juce::Random& random)
    {
        const auto start = (int) std::lround (at * rate);
        juce::IIRFilter band;
        band.setCoefficients (juce::IIRCoefficients::makeBandPass (rate, 1500.0, 0.7));
        for (int i = 0; i < 9000 && start + i < b.getNumSamples(); ++i)
        {
            const auto t = (float) i / (float) rate;
            const auto v = (band.processSingleSampleRaw (random.nextFloat() * 2.0f - 1.0f) * 1.2f
                            + 0.3f * std::sin (juce::MathConstants<float>::twoPi * 190.0f * t)) * std::exp (-t * 25.0f)
                           * std::min (1.0f, (float) (9000 - i) / 800.0f);
            for (int c = 0; c < b.getNumChannels(); ++c)
                b.addSample (c, start + i, v);
        }
    }

    /** A plucked note: a quick rise, then dying away (like a guitar or piano). */
    void addNote (juce::AudioBuffer<float>& b, double at, double hz)
    {
        const auto start = (int) std::lround (at * rate);
        for (int i = 0; i < 30000 && start + i < b.getNumSamples(); ++i)
        {
            const auto t = i / rate;
            const auto env = std::min (1.0, t / 0.002) * std::exp (-t * 3.0) * std::min (1.0, (30000 - i) / 2000.0);
            const auto v = (float) (0.3 * env * (std::sin (juce::MathConstants<double>::twoPi * hz * t)
                                                 + 0.4 * std::sin (juce::MathConstants<double>::twoPi * 2.0 * hz * t)));
            for (int c = 0; c < b.getNumChannels(); ++c)
                b.addSample (c, start + i, v);
        }
    }

    /** The hit nearest `seconds`, or null. */
    const audioonsets::Hit* near (const std::vector<audioonsets::Hit>& hits, double seconds, double within = 0.01)
    {
        for (const auto& h : hits)
            if (std::abs ((double) h.sample / rate - seconds) <= within)
                return &h;
        return nullptr;
    }
}

TEST_CASE ("Drum hits are found where they start, and kicks are low", "[onsets]")
{
    // 4 bars at 100 BPM (0.6 s a beat): kick on 1 and 3, hats on every eighth, quiet noise floor.
    juce::AudioBuffer<float> drums (2, (int) (10.0 * rate));
    drums.clear();
    juce::Random random (9);
    for (int i = 0; i < drums.getNumSamples(); ++i)
        for (int c = 0; c < 2; ++c)
            drums.setSample (c, i, (random.nextFloat() * 2.0f - 1.0f) * 0.0005f);

    std::vector<double> kicks, hats;
    for (int beat = 0; beat < 16; ++beat)
    {
        const auto t = 0.25 + beat * 0.6;
        if (beat % 2 == 0)
        {
            addKick (drums, t);
            kicks.push_back (t);
        }
        else
        {
            addSnare (drums, t, random);
            hats.push_back (t);
        }
        addHat (drums, t + 0.3, random, 0.15f);
        hats.push_back (t + 0.3);
    }

    const auto hits = audioonsets::detect (drums, rate);
    CHECK (hits.size() == kicks.size() + hats.size());

    for (const auto t : kicks)
    {
        const auto* h = near (hits, t, 0.004);
        INFO ("kick at " << t);
        REQUIRE (h != nullptr);
        CHECK (h->low > 0.45f);
    }

    for (const auto t : hats)
    {
        const auto* h = near (hits, t, 0.004);
        INFO ("snare or hat at " << t);
        REQUIRE (h != nullptr);
        CHECK (h->low < 0.45f);
    }

    // As notes for the beat finder, the beat comes out at 100 BPM.
    const auto beat = tempodetect::analyse (audioonsets::asNotes (hits, rate), {});
    REQUIRE (beat.found);
    CHECK (beat.bpm == Catch::Approx (100.0).margin (1.5));
}

TEST_CASE ("Plucked notes are found, also when they ring on into each other", "[onsets]")
{
    juce::AudioBuffer<float> guitar (1, (int) (6.0 * rate));
    guitar.clear();
    const double times[] { 0.3, 0.8, 1.2, 1.75, 2.3, 2.6, 3.1, 3.8, 4.4, 5.0 };
    const double pitches[] { 196.0, 247.0, 294.0, 196.0, 330.0, 294.0, 247.0, 220.0, 196.0, 147.0 };

    for (size_t k = 0; k < std::size (times); ++k)
        addNote (guitar, times[k], pitches[k]);

    const auto hits = audioonsets::detect (guitar, rate);
    CHECK (hits.size() == std::size (times));
    for (const auto t : times)
    {
        INFO ("note at " << t);
        CHECK (near (hits, t, 0.006) != nullptr);
    }

    // Silence, or too short: nothing.
    juce::AudioBuffer<float> silence (2, 48000);
    silence.clear();
    CHECK (audioonsets::detect (silence, rate).empty());
    CHECK (audioonsets::detect (juce::AudioBuffer<float> (2, 100), rate).empty());
}
