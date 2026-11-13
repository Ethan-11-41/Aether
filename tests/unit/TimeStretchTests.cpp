// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
//
// Time-stretching and pitch-shifting (Milestone 17): the stretcher keeps pitch while changing
// speed, changes pitch while keeping speed, and puts each moment where its time map says.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include "dsp/TimeStretch.h"

using namespace aether::dsp;

namespace
{
    constexpr double rate = 48000.0;

    juce::AudioBuffer<float> sine (double hz, double seconds, float level = 0.5f)
    {
        juce::AudioBuffer<float> b (2, (int) (seconds * rate));
        for (int i = 0; i < b.getNumSamples(); ++i)
            for (int c = 0; c < 2; ++c)
                b.setSample (c, i, level * (float) std::sin (juce::MathConstants<double>::twoPi * hz * i / rate));
        return b;
    }

    /** Short bursts of noise (drum-like hits) at the given times, silence between. */
    juce::AudioBuffer<float> hits (const std::vector<double>& times, double seconds)
    {
        juce::AudioBuffer<float> b (2, (int) (seconds * rate));
        b.clear();
        juce::Random random (5);
        for (const auto t : times)
            for (int i = 0; i < 2400; ++i)   // 50 ms, dying away
            {
                const auto at = (int) (t * rate) + i;
                if (at < b.getNumSamples())
                    for (int c = 0; c < 2; ++c)
                        b.setSample (c, at, (random.nextFloat() * 2.0f - 1.0f) * 0.8f * std::exp ((float) -i / 400.0f));
            }
        return b;
    }

    double frequency (const juce::AudioBuffer<float>& b, int from, int to)
    {
        int crossings = 0;
        for (int i = from + 1; i < to; ++i)
            if ((b.getSample (0, i - 1) < 0.0f) != (b.getSample (0, i) < 0.0f))
                ++crossings;
        return crossings / 2.0 / ((to - from) / rate);
    }

    /** Where each hit starts: the first moment its level reaches half its loudest, after a
        quiet stretch (a stretched hit is spread out a little, so it isn't as loud). */
    std::vector<double> onsets (const juce::AudioBuffer<float>& b)
    {
        constexpr int window = 96;   // 2 ms
        std::vector<float> levels;
        for (int i = 0; i + window < b.getNumSamples(); i += window)
            levels.push_back (b.getRMSLevel (0, i, window));

        std::vector<double> result;

        for (size_t k = 0; k < levels.size(); ++k)
        {
            // Loud, after 100 ms of quiet that ended at most 30 ms before (a stretched hit has a
            // faint smear just before it).
            bool quietBefore = k >= 15;
            for (size_t j = k >= 65 ? k - 65 : 0; j + 15 < k; ++j)
                quietBefore = quietBefore && levels[j] < 0.02f;

            if (levels[k] > 0.05f && (quietBefore || result.empty()))
            {
                // This hit's loudest moment (within 100 ms), and where it first gets to half that.
                float loudest = 0.0f;
                for (size_t j = k; j < std::min (levels.size(), k + 50); ++j)
                    loudest = std::max (loudest, levels[j]);

                auto at = k;
                while (levels[at] < 0.5f * loudest)
                    ++at;

                result.push_back ((double) at * window / rate);
                k = at + 50;
            }
        }

        return result;
    }

    timestretch::Options options (double semitones = 0.0)
    {
        timestretch::Options o;
        o.sampleRate = rate;
        o.semitones = semitones;
        return o;
    }
}

TEST_CASE ("Stretching keeps the pitch and changes the length", "[stretch]")
{
    const auto a440 = sine (440.0, 2.0);
    const auto source = timestretch::Source::of (a440);

    // 1.5 times as long (slower), and 0.75 times (faster).
    for (const auto factor : { 1.5, 0.75 })
    {
        INFO ("factor " << factor);
        const auto length = (juce::int64) (96000 * factor);
        const auto out = timestretch::render (source, { { 0.0, 0.0 }, { (double) length, 96000.0 } }, length, options());
        REQUIRE (out.getNumSamples() == length);
        REQUIRE (out.getNumChannels() == 2);

        const auto middle = (int) (length / 2);
        CHECK (frequency (out, middle - 12000, middle + 12000) == Catch::Approx (440.0).margin (3.0));
        CHECK (out.getRMSLevel (0, middle - 12000, 24000) == Catch::Approx (0.5 / std::sqrt (2.0)).margin (0.06));
    }
}

TEST_CASE ("Pitch-shifting changes the pitch and keeps the length", "[stretch]")
{
    const auto a440 = sine (440.0, 2.0);
    const auto source = timestretch::Source::of (a440);

    const auto up = timestretch::render (source, { { 0.0, 0.0 }, { 96000.0, 96000.0 } }, 96000, options (12.0));
    REQUIRE (up.getNumSamples() == 96000);
    CHECK (frequency (up, 36000, 60000) == Catch::Approx (880.0).margin (6.0));

    const auto down = timestretch::render (source, { { 0.0, 0.0 }, { 96000.0, 96000.0 } }, 96000, options (-7.0));
    CHECK (frequency (down, 36000, 60000) == Catch::Approx (440.0 * std::pow (2.0, -7.0 / 12.0)).margin (4.0));
}

TEST_CASE ("Each moment lands where the time map puts it, also when the speed changes", "[stretch]")
{
    // Hits every half second for 4 seconds.
    std::vector<double> times;
    for (int k = 0; k < 8; ++k)
        times.push_back (0.25 + 0.5 * k);

    const auto drums = hits (times, 4.0);
    const auto source = timestretch::Source::of (drums);

    // The first 2 seconds as they are, the next 2 seconds twice as long.
    const std::vector<timestretch::MapPoint> map { { 0.0, 0.0 }, { 96000.0, 96000.0 }, { 288000.0, 192000.0 } };
    const auto out = timestretch::render (source, map, 288000, options());
    REQUIRE (out.getNumSamples() == 288000);

    const auto found = onsets (out);
    REQUIRE (found.size() == times.size());

    for (size_t k = 0; k < times.size(); ++k)
    {
        const auto expected = times[k] < 2.0 ? times[k] : 2.0 + 2.0 * (times[k] - 2.0);
        INFO ("hit " << k);
        CHECK (found[k] == Catch::Approx (expected).margin (0.006));
    }

    // A map that doesn't start at the input's start: from 1 second in.
    const auto later = timestretch::render (source, { { 0.0, 48000.0 }, { 96000.0, 144000.0 } }, 96000, options());
    const auto laterHits = onsets (later);
    REQUIRE (! laterHits.empty());
    CHECK (laterHits[0] == Catch::Approx (0.25).margin (0.006));   // the hit at 1.25 s
}

TEST_CASE ("Stretching is always the same, and can be stopped", "[stretch]")
{
    const auto drums = hits ({ 0.1, 0.6, 1.1 }, 1.5);
    const auto source = timestretch::Source::of (drums);
    const std::vector<timestretch::MapPoint> map { { 0.0, 0.0 }, { 90000.0, 72000.0 } };

    const auto a = timestretch::render (source, map, 90000, options (3.0));
    const auto b = timestretch::render (source, map, 90000, options (3.0));
    REQUIRE (a.getNumSamples() == b.getNumSamples());
    bool same = true;
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < a.getNumSamples(); ++i)
            same = same && a.getSample (c, i) == b.getSample (c, i);
    CHECK (same);

    auto stopping = options();
    stopping.shouldStop = [] { return true; };
    CHECK (timestretch::render (source, map, 90000, stopping).getNumSamples() == 0);

    // Nothing to do.
    CHECK (timestretch::render (source, map, 0, options()).getNumSamples() == 0);
    CHECK (timestretch::inputAt (map, 45000.0) == Catch::Approx (36000.0));
    CHECK (timestretch::inputAt (map, 180000.0) == Catch::Approx (144000.0));   // carries on past the end
}

TEST_CASE ("Stretching is quick enough to do in the background", "[stretch][!benchmark]")
{
    // Continuous sound (a chord over hits), so nothing is skipped as silence.
    auto music = hits ({ 0.1, 0.6, 1.1, 1.6, 2.1, 2.6, 3.1 }, 10.0);
    for (const auto hz : { 220.0, 277.2, 329.6 })
    {
        const auto tone = sine (hz, 10.0, 0.2f);
        for (int c = 0; c < 2; ++c)
            music.addFrom (c, 0, tone, c, 0, music.getNumSamples());
    }

    const auto source = timestretch::Source::of (music);
    const auto started = juce::Time::getMillisecondCounterHiRes();
    const auto out = timestretch::render (source, { { 0.0, 0.0 }, { 600000.0, 480000.0 } }, 600000, options());
    const auto seconds = (juce::Time::getMillisecondCounterHiRes() - started) / 1000.0;
    INFO ("12.5 seconds of stereo took " << seconds << " s");
    WARN ("12.5 seconds of stereo audio stretched in " << seconds << " s");
    CHECK (out.getNumSamples() == 600000);
    CHECK (seconds < 12.5);   // at least faster than it plays, even on a slow test machine
}
