// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
//
// The built-in drum sounds and kits.

#include <cmath>
#include <set>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "dsp/DrumSounds.h"

using namespace aether::dsp;

namespace
{
    /** Zero crossings per second: a rough measure of how high a sound is. */
    double crossingsPerSecond (const juce::AudioBuffer<float>& b, double rate, double seconds)
    {
        const auto* d = b.getReadPointer (0);
        const auto n = std::min (b.getNumSamples(), (int) (seconds * rate));
        int crossings = 0;
        for (int i = 1; i < n; ++i)
            if ((d[i - 1] < 0.0f) != (d[i] < 0.0f))
                ++crossings;
        return crossings / (n / rate);
    }
}

TEST_CASE ("Every built-in drum sound is clean, quiet at the end and always the same", "[drums]")
{
    std::set<juce::String> ids;

    for (const auto& info : drums::builtInSounds())
    {
        INFO (info.id);
        CHECK (ids.insert (info.id).second);
        CHECK (juce::String (info.name).isNotEmpty());
        CHECK (drums::findSound (info.id) == &info);

        const auto sound = drums::render (info.id, 48000.0);
        REQUIRE (sound.getNumSamples() > 2400);
        CHECK (sound.getNumSamples() <= 48000 * 4);

        const auto peak = sound.getMagnitude (0, 0, sound.getNumSamples());
        CHECK (peak > 0.3f);
        CHECK (peak <= 1.0f);

        const auto* d = sound.getReadPointer (0);
        CHECK (std::all_of (d, d + sound.getNumSamples(), [] (float x) { return std::isfinite (x); }));
        CHECK (std::abs (d[sound.getNumSamples() - 1]) < 1.0e-3f);   // no click at the end

        const auto again = drums::render (info.id, 48000.0);
        CHECK (std::equal (d, d + sound.getNumSamples(), again.getReadPointer (0)));
    }

    CHECK (ids.size() >= 30);
    CHECK (drums::render ("no such sound", 48000.0).getNumSamples() == 0);
}

TEST_CASE ("Kicks are low and hi-hats are high", "[drums]")
{
    const auto kick = drums::render ("kick-808", 48000.0);
    const auto hat = drums::render ("hat-closed", 48000.0);
    CHECK (crossingsPerSecond (kick, 48000.0, 0.2) < 300.0);
    CHECK (crossingsPerSecond (hat, 48000.0, 0.05) > 5000.0);

    // Rendered at another sample rate, the same sound lasts as long.
    const auto at44 = drums::render ("kick-808", 44100.0);
    CHECK (std::abs (at44.getNumSamples() / 44100.0 - kick.getNumSamples() / 48000.0) < 0.001);
}

TEST_CASE ("Every kit uses real sounds, and its hi-hats cut each other off", "[drums]")
{
    REQUIRE (drums::kits().size() >= 4);

    for (const auto& kit : drums::kits())
    {
        INFO (kit.name);
        CHECK (drums::findKit (kit.name) == &kit);

        for (const auto& pad : kit.pads)
            CHECK (drums::findSound (pad.sound) != nullptr);

        // Closed (42), pedal (44) and open (46) hi-hat: pads 6, 8 and 10.
        CHECK (kit.pads[6].choke == 1);
        CHECK (kit.pads[10].choke == 1);
    }

    // Pads follow General MIDI drums: note 36 is the first pad (kick), 51 the last.
    CHECK (drums::noteForPad (0) == 36);
    CHECK (drums::padForNote (51) == 15);
    CHECK (drums::padForNote (35) == -1);
    CHECK (drums::padForNote (52) == -1);

    // The display order lists every pad once.
    std::set<int> shown (drums::padDisplayOrder().begin(), drums::padDisplayOrder().end());
    CHECK (shown.size() == 16);
}

TEST_CASE ("The lo-fi sounds are warm, without fizz or folded-back tones", "[drums]")
{
    // How much of a sound is above about 10 kHz, in dB against the whole sound: a high-pass made
    // of four second differences (each cuts the lows much more than the highs).
    const auto highs = [] (const char* id)
    {
        const auto sound = drums::render (id, 48000.0);
        const auto* d = sound.getReadPointer (0);
        std::vector<double> h (d, d + sound.getNumSamples());

        for (int pass = 0; pass < 4; ++pass)
            for (size_t i = h.size(); i-- > 2;)
                h[i] = (h[i] - 2.0 * h[i - 1] + h[i - 2]) / 4.0;

        double all = 0.0, high = 0.0;
        for (size_t i = 8; i < h.size(); ++i)
        {
            all += (double) d[i] * d[i];
            high += h[i] * h[i];
        }

        return 10.0 * std::log10 (high / std::max (1.0e-30, all));
    };

    // Each lo-fi sound is a clean recipe made much darker, with nothing harsh left at the top
    // (until 0.16.1 the sample rate was lowered without filtering first, which folded the
    // highs back down as tones that clash).
    for (const auto& [clean, lofi] : std::vector<std::pair<const char*, const char*>> {
             { "snare-tight", "snare-lofi" }, { "hat-closed", "hat-lofi-closed" }, { "hat-open", "hat-lofi-open" },
             { "rim", "rim-lofi" }, { "clap", "clap-lofi" }, { "snare-brush", "snare-brush-lofi" }, { "tom-low", "tom-lofi-low" },
             { "shaker", "shaker-lofi" }, { "tambourine", "tambourine-lofi" }, { "conga-high", "conga-lofi-high" },
             { "crash", "crash-lofi" }, { "ride", "ride-lofi" } })
    {
        INFO (lofi);
        CHECK (highs (lofi) < highs (clean) - 25.0);
        CHECK (highs (lofi) < -40.0);
    }

    CHECK (highs ("kick-lofi") < -60.0);

    // The whole Lo-fi kit is lo-fi.
    const auto* kit = drums::findKit ("Lo-fi");
    REQUIRE (kit != nullptr);
    for (const auto& pad : kit->pads)
        CHECK (juce::String (pad.sound).contains ("lofi"));
}
