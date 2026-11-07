// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
//
// The Synth (Milestone 10), checked on the sound it makes.

#include <cmath>
#include <set>

#include <catch2/catch_test_macros.hpp>

#include "dsp/EffectTypes.h"
#include "dsp/Synth.h"

using namespace aether;
using namespace aether::dsp;
namespace sy = aether::dsp::synth;

namespace
{
    constexpr double rate = 48000.0;

    /** The Synth with its default settings, made quiet and plain (no effects, filter open) so
        tests can measure what they need. */
    struct Fixture
    {
        Fixture()
        {
            instrument = createInstrument (sy::typeId);
            REQUIRE (instrument != nullptr);
            instrument->prepare (rate, 512);

            const auto* type = findEffectType (sy::typeId);
            REQUIRE (type != nullptr);
            for (size_t i = 0; i < type->parameters.size(); ++i)
                params[i] = type->parameters[i].defaultValue;

            set (sy::filterType, sy::lowPass12);
            set (sy::cutoff, 20000.0f);
            set (sy::filterEnv, 0.0f);
            set (sy::keyTrack, 0.0f);
            set (sy::filterVelocity, 0.0f);
            set (sy::sustain, 100.0f);
            set (sy::velocity, 0.0f);
            set (sy::chorus, 0.0f);
            set (sy::reverb, 0.0f);
        }

        void set (int index, float value)   { params[(size_t) index] = value; }

        void usePreset (const juce::String& name)
        {
            for (const auto& preset : findEffectType (sy::typeId)->presets)
                if (preset.name == name)
                    for (size_t i = 0; i < preset.values.size(); ++i)
                        params[i] = preset.values[i];
        }

        static NoteEvent on (int pitch, int velocity = 100)
        {
            NoteEvent e;
            e.type = NoteEvent::Type::noteOn;
            e.pitch = (std::uint8_t) pitch;
            e.velocity = (std::uint8_t) velocity;
            return e;
        }

        static NoteEvent off (int pitch)
        {
            NoteEvent e;
            e.type = NoteEvent::Type::noteOff;
            e.pitch = (std::uint8_t) pitch;
            return e;
        }

        /** Plays `samples` of sound, with `events` at the start. Returns left (and right in `rightOut`). */
        std::vector<float> run (int samples, std::vector<NoteEvent> events = {}, std::vector<float>* rightOut = nullptr)
        {
            std::vector<float> l ((size_t) samples, 0.0f), r ((size_t) samples, 0.0f);

            for (int done = 0; done < samples; done += 512)
            {
                const auto n = std::min (512, samples - done);
                instrument->process (l.data() + done, r.data() + done, n, params,
                                     done == 0 ? events.data() : nullptr, done == 0 ? (int) events.size() : 0);
            }

            if (rightOut != nullptr)
                *rightOut = r;

            return l;
        }

        /** The frequency of a sound, from how often it crosses zero going up. */
        static double frequency (const std::vector<float>& v, size_t from, size_t to)
        {
            int crossings = 0;
            size_t first = 0, last = 0;

            for (auto i = from + 1; i < to; ++i)
                if (v[i - 1] <= 0.0f && v[i] > 0.0f)
                {
                    if (crossings == 0) first = i;
                    last = i;
                    ++crossings;
                }

            return crossings < 2 ? 0.0 : (crossings - 1) * rate / (double) (last - first);
        }

        static float peak (const std::vector<float>& v, size_t from, size_t to)
        {
            float m = 0.0f;
            for (auto i = from; i < std::min (to, v.size()); ++i)
                m = std::max (m, std::abs (v[i]));
            return m;
        }

        /** How much of the sound's energy is in fast changes (high frequencies). */
        static double brightness (const std::vector<float>& v, size_t from, size_t to)
        {
            double change = 0.0, energy = 0.0;
            for (auto i = from + 1; i < to; ++i)
            {
                const auto d = (double) v[i] - (double) v[i - 1];
                change += d * d;
                energy += (double) v[i] * (double) v[i];
            }
            return change / std::max (1.0e-12, energy);
        }

        std::unique_ptr<Instrument> instrument;
        EffectParameterValues params {};
    };
}

TEST_CASE ("The Synth's settings and presets are complete and in range", "[synth]")
{
    const auto* type = findEffectType (sy::typeId);
    REQUIRE (type != nullptr);
    CHECK (type->isInstrument);
    CHECK (type->name == "Synth");
    CHECK ((int) type->parameters.size() == sy::numParameters);
    CHECK ((int) type->parameters.size() <= maxEffectParameters);

    std::set<juce::String> ids, names, categories;
    for (const auto& p : type->parameters)
        CHECK (ids.insert (p.id.toString()).second);   // every id once

    CHECK (type->presets.size() >= 20);

    for (const auto& preset : type->presets)
    {
        INFO (preset.name);
        CHECK (names.insert (preset.name).second);
        CHECK (preset.category.isNotEmpty());
        categories.insert (preset.category);
        REQUIRE (preset.values.size() == type->parameters.size());

        for (size_t i = 0; i < preset.values.size(); ++i)
            CHECK (type->parameters[i].clamp (preset.values[i]) == preset.values[i]);
    }

    for (const auto* c : { "Pads", "Strings", "Keys", "Organ", "Bass", "Leads", "Plucks" })
        CHECK (categories.count (c) == 1);

    CHECK (defaultInstrument == sy::typeId);
}

TEST_CASE ("Every Synth preset makes a sound, without blowing up", "[synth]")
{
    for (const auto& preset : findEffectType (sy::typeId)->presets)
    {
        INFO (preset.name);
        Fixture f;
        f.usePreset (preset.name);

        std::vector<float> right;
        const auto left = f.run (48000, { Fixture::on (48), Fixture::on (55), Fixture::on (60), Fixture::on (64) }, &right);
        const auto loudest = std::max (Fixture::peak (left, 0, left.size()), Fixture::peak (right, 0, right.size()));

        CHECK (loudest > 0.01f);
        CHECK (loudest < 2.0f);
        CHECK (std::all_of (left.begin(), left.end(), [] (float x) { return std::isfinite (x); }));
    }
}

TEST_CASE ("The Synth plays in tune, and the oscillators' octave and tuning work", "[synth]")
{
    Fixture f;
    f.set (sy::waveA, sy::sine);
    f.set (sy::mix, 0.0f);   // only oscillator A

    auto out = f.run (48000, { Fixture::on (69) });
    CHECK (std::abs (Fixture::frequency (out, 4800, 48000) - 440.0) < 1.0);

    Fixture g;
    g.set (sy::waveA, sy::sine);
    g.set (sy::mix, 0.0f);
    g.set (sy::octaveA, 1.0f);
    g.set (sy::semiA, -12.0f);   // up an octave and down twelve semitones: the same note
    g.set (sy::fineA, 0.0f);
    out = g.run (48000, { Fixture::on (57) });
    CHECK (std::abs (Fixture::frequency (out, 4800, 48000) - 220.0) < 1.0);

    // Pitch bend: up two semitones (the default range).
    Fixture h;
    h.set (sy::waveA, sy::sine);
    h.set (sy::mix, 0.0f);
    NoteEvent bend;
    bend.type = NoteEvent::Type::pitchBend;
    bend.value = 1.0f;
    out = h.run (48000, { Fixture::on (69), bend });
    CHECK (std::abs (Fixture::frequency (out, 4800, 48000) - 440.0 * std::pow (2.0, 2.0 / 12.0)) < 1.5);
}

TEST_CASE ("The Synth's filter takes away high frequencies", "[synth]")
{
    Fixture open;
    open.set (sy::mix, 0.0f);   // a saw
    const auto bright = Fixture::brightness (open.run (24000, { Fixture::on (48) }), 4800, 24000);

    Fixture closed;
    closed.set (sy::mix, 0.0f);
    closed.set (sy::cutoff, 300.0f);
    closed.set (sy::filterType, sy::lowPass24);
    const auto dark = Fixture::brightness (closed.run (24000, { Fixture::on (48) }), 4800, 24000);
    CHECK (dark < bright * 0.4);

    // High-pass: the reverse.
    Fixture high;
    high.set (sy::mix, 0.0f);
    high.set (sy::cutoff, 3000.0f);
    high.set (sy::filterType, sy::highPass);
    CHECK (Fixture::brightness (high.run (24000, { Fixture::on (48) }), 4800, 24000) > bright);

    // The picture in the window agrees: well below the cutoff a low-pass changes nothing,
    // two octaves above it a 24 dB one takes away about 48 dB.
    CHECK (std::abs (sy::filterGainDb (sy::lowPass24, 1000.0, 0.0, 100.0)) < 0.5);
    CHECK (sy::filterGainDb (sy::lowPass24, 1000.0, 0.0, 4000.0) < -40.0);
    CHECK (sy::filterGainDb (sy::lowPass12, 1000.0, 0.9, 1000.0) > 10.0);   // resonance: a peak
}

TEST_CASE ("The Synth's envelope: a slow attack fades in, and the release fades out and stops", "[synth]")
{
    Fixture f;
    f.set (sy::attack, 500.0f);
    f.set (sy::release, 100.0f);

    const auto out = f.run (48000, { Fixture::on (60) });
    CHECK (Fixture::peak (out, 4000, 5000) < Fixture::peak (out, 30000, 31000) * 0.4f);   // 0.1 s vs 0.6 s

    const auto after = f.run (24000, { Fixture::off (60) });
    CHECK (Fixture::peak (after, 0, 500) > 0.01f);           // still fading
    CHECK (Fixture::peak (after, 14400, 24000) < 1.0e-4f);   // gone after 0.3 s
    CHECK (f.instrument->getNumSounding() == 0);
}

TEST_CASE ("Unison and width make the Synth stereo", "[synth]")
{
    std::vector<float> right;

    Fixture narrow;
    const auto left1 = narrow.run (24000, { Fixture::on (60) }, &right);
    CHECK (left1 == right);   // one copy, in the middle

    Fixture wide;
    wide.set (sy::unison, 4.0f);   // five copies
    wide.set (sy::width, 100.0f);
    wide.set (sy::detune, 30.0f);
    const auto left2 = wide.run (24000, { Fixture::on (60) }, &right);

    double difference = 0.0;
    for (size_t i = 4800; i < left2.size(); ++i)
        difference += std::abs (left2[i] - right[i]);
    CHECK (difference / 19200.0 > 0.01);
}

TEST_CASE ("Mono and legato play one note at a time, gliding between them", "[synth]")
{
    Fixture f;
    f.set (sy::waveA, sy::sine);
    f.set (sy::mix, 0.0f);
    f.set (sy::voiceMode, sy::legato);

    f.run (4800, { Fixture::on (57) });
    auto out = f.run (24000, { Fixture::on (69) });
    CHECK (f.instrument->getNumSounding() == 1);
    CHECK (std::abs (Fixture::frequency (out, 2400, 24000) - 440.0) < 1.5);

    // Letting go of the top note goes back to the one still held.
    out = f.run (24000, { Fixture::off (69) });
    CHECK (f.instrument->getNumSounding() == 1);
    CHECK (std::abs (Fixture::frequency (out, 2400, 24000) - 220.0) < 1.0);

    // With glide, the pitch slides: half way through, it's between the two notes.
    Fixture g;
    g.set (sy::waveA, sy::sine);
    g.set (sy::mix, 0.0f);
    g.set (sy::voiceMode, sy::legato);
    g.set (sy::glide, 1000.0f);
    g.run (4800, { Fixture::on (57) });
    out = g.run (9600, { Fixture::on (69) });
    const auto sliding = Fixture::frequency (out, 2400, 7200);
    CHECK (sliding > 240.0);
    CHECK (sliding < 420.0);
}

TEST_CASE ("The mod wheel opens the filter when set to Cutoff", "[synth]")
{
    Fixture f;
    f.set (sy::mix, 0.0f);
    f.set (sy::cutoff, 300.0f);
    f.set (sy::wheelTarget, sy::wheelCutoff);
    f.set (sy::wheelAmount, 100.0f);

    const auto dark = Fixture::brightness (f.run (24000, { Fixture::on (48) }), 4800, 24000);

    NoteEvent wheel;
    wheel.type = NoteEvent::Type::modWheel;
    wheel.value = 1.0f;
    const auto bright = Fixture::brightness (f.run (24000, { wheel }), 4800, 24000);
    CHECK (bright > dark * 2.0);
}

TEST_CASE ("Notes from clips end by themselves, and stopping releases them", "[synth]")
{
    Fixture f;
    f.set (sy::release, 20.0f);

    auto note = Fixture::on (60);
    note.fromClip = true;
    note.length = 4800;
    const auto out = f.run (24000, { note });
    CHECK (Fixture::peak (out, 0, 4800) > 0.01f);
    CHECK (Fixture::peak (out, 12000, 24000) < 1.0e-4f);

    note.length = 480000;
    f.run (4800, { note });
    CHECK (f.instrument->getNumSounding() == 1);

    NoteEvent stop;
    stop.type = NoteEvent::Type::stopClipNotes;
    f.run (24000, { stop });
    CHECK (f.instrument->getNumSounding() == 0);
}

TEST_CASE ("More notes than voices: the oldest are taken, nothing breaks", "[synth]")
{
    Fixture f;
    std::vector<NoteEvent> chord;
    for (int i = 0; i < 40; ++i)
        chord.push_back (Fixture::on (30 + i));

    const auto out = f.run (9600, chord);
    CHECK (f.instrument->getNumSounding() <= 24);
    CHECK (std::all_of (out.begin(), out.end(), [] (float x) { return std::isfinite (x); }));
}
