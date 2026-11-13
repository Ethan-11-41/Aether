// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
//
// The sampler (Milestone 12), checked on the sound it makes from small made-up recordings.

#include <cmath>

#include <catch2/catch_test_macros.hpp>

#include "dsp/EffectTypes.h"
#include "dsp/Sampler.h"

using namespace aether;
using namespace aether::dsp;
namespace sm = aether::dsp::sampler;

namespace
{
    constexpr double rate = 48000.0;

    /** A sine wave recording (mono). */
    juce::AudioBuffer<float> sine (double hz, double seconds, float level = 0.5f)
    {
        juce::AudioBuffer<float> b (1, (int) (seconds * rate));
        for (int i = 0; i < b.getNumSamples(); ++i)
            b.setSample (0, i, level * (float) std::sin (juce::MathConstants<double>::twoPi * hz * i / rate));
        return b;
    }

    /** A steady level (mono): easy to tell recordings apart by their level. */
    juce::AudioBuffer<float> steady (float level, double seconds = 1.0)
    {
        juce::AudioBuffer<float> b (1, (int) (seconds * rate));
        for (int i = 0; i < b.getNumSamples(); ++i)
            b.setSample (0, i, level);
        return b;
    }

    double frequency (const std::vector<float>& x, int from, int to)
    {
        int crossings = 0;
        for (int i = from + 1; i < to; ++i)
            if ((x[(size_t) i - 1] < 0.0f) != (x[(size_t) i] < 0.0f))
                ++crossings;
        return crossings / 2.0 / ((to - from) / rate);
    }

    struct Fixture
    {
        Fixture()
        {
            instrument = createInstrument (sm::typeId);
            REQUIRE (instrument != nullptr);
            instrument->prepare (rate, 512);

            const auto* type = findEffectType (sm::typeId);
            REQUIRE (type != nullptr);
            for (size_t i = 0; i < type->parameters.size(); ++i)
                params[i] = type->parameters[i].defaultValue;

            set (sm::attack, 1.0f);
            set (sm::touch, 0.0f);   // every note at full level, unless a test says otherwise
            sounds.map = &map;
        }

        void set (int index, float value)   { params[(size_t) index] = value; }

        SampleZone& addZone (const juce::AudioBuffer<float>& sound, int root = 60, int lowKey = 0, int highKey = 127)
        {
            SampleZone z;
            z.sample = &sound;
            z.rootKey = root;
            z.lowKey = lowKey;
            z.highKey = highKey;
            map.zones.push_back (z);
            return map.zones.back();
        }

        static NoteEvent on (int pitch, int velocity = 100, int offset = 0)
        {
            NoteEvent e;
            e.type = NoteEvent::Type::noteOn;
            e.pitch = (std::uint8_t) pitch;
            e.velocity = (std::uint8_t) velocity;
            e.offset = offset;
            return e;
        }

        static NoteEvent clipNote (int pitch, std::int64_t length, int offset = 0)
        {
            auto e = on (pitch, 100, offset);
            e.fromClip = true;
            e.length = length;
            return e;
        }

        static NoteEvent off (int pitch, int offset = 0)
        {
            NoteEvent e;
            e.type = NoteEvent::Type::noteOff;
            e.pitch = (std::uint8_t) pitch;
            e.offset = offset;
            return e;
        }

        std::vector<float> run (int samples, std::vector<NoteEvent> events = {})
        {
            std::vector<float> l ((size_t) samples, 0.0f), r ((size_t) samples, 0.0f);
            for (int done = 0; done < samples; done += 512)
            {
                std::vector<NoteEvent> now;
                for (auto e : events)
                    if (e.offset >= done && e.offset < done + 512)
                    {
                        e.offset -= done;
                        now.push_back (e);
                    }

                const auto n = std::min (512, samples - done);
                instrument->process (l.data() + done, r.data() + done, n, params, now.data(), (int) now.size(), &sounds);
            }
            right = r;
            return l;
        }

        std::unique_ptr<Instrument> instrument;
        EffectParameterValues params {};
        SampleMap map;
        InstrumentSounds sounds;
        std::vector<float> right;
    };
}

TEST_CASE ("The sampler plays a recording higher or lower across the keyboard", "[sampler]")
{
    const auto a440 = sine (440.0, 2.0);
    Fixture f;
    f.map.zones.reserve (4);
    f.addZone (a440, 69);   // recorded at A4

    auto l = f.run (24000, { Fixture::on (69) });
    CHECK (std::abs (frequency (l, 2400, 24000) - 440.0) < 3.0);

    Fixture g;
    g.addZone (a440, 69);
    l = g.run (24000, { Fixture::on (81) });   // an octave up
    CHECK (std::abs (frequency (l, 2400, 24000) - 880.0) < 5.0);

    Fixture h;
    h.addZone (a440, 69);
    h.set (sm::tune, -12.0f);                   // tuned down an octave
    l = h.run (24000, { Fixture::on (69) });
    CHECK (std::abs (frequency (l, 2400, 24000) - 220.0) < 3.0);

    // Keys outside every zone are silent.
    Fixture k;
    k.addZone (a440, 69, 60, 72);
    l = k.run (4800, { Fixture::on (40) });
    CHECK (*std::max_element (l.begin(), l.end()) == 0.0f);
}

TEST_CASE ("Velocity layers and round robin pick the right recording", "[sampler]")
{
    const auto soft = steady (0.2f), loud = steady (0.8f), first = steady (0.3f), second = steady (0.6f);

    Fixture f;
    f.map.zones.reserve (4);
    auto& s = f.addZone (soft);
    s.highVelocity = 64;
    auto& l = f.addZone (loud);
    l.lowVelocity = 65;

    auto out = f.run (2400, { Fixture::on (60, 40) });
    CHECK (std::abs (out[1000] - 0.2f) < 0.01f);
    f.run (24000, { Fixture::off (60) });
    out = f.run (2400, { Fixture::on (60, 110) });
    CHECK (std::abs (out[1000] - 0.8f) < 0.01f);

    // Two takes that alternate.
    Fixture g;
    g.map.zones.reserve (4);
    auto& a = g.addZone (first);
    a.sequenceLength = 2;
    a.sequencePosition = 1;
    auto& b = g.addZone (second);
    b.sequenceLength = 2;
    b.sequencePosition = 2;

    std::vector<float> levels;
    for (int i = 0; i < 4; ++i)
    {
        out = g.run (2400, { Fixture::on (60) });
        levels.push_back (out[1000]);
        g.run (24000, { Fixture::off (60) });
    }
    CHECK (std::abs (levels[0] - 0.3f) < 0.01f);
    CHECK (std::abs (levels[1] - 0.6f) < 0.01f);
    CHECK (std::abs (levels[2] - 0.3f) < 0.01f);
    CHECK (std::abs (levels[3] - 0.6f) < 0.01f);

    // Touch: softer notes are quieter.
    Fixture h;
    h.addZone (loud);
    h.set (sm::touch, 100.0f);
    out = h.run (2400, { Fixture::on (60, 64) });
    CHECK (out[1000] < 0.3f);
}

TEST_CASE ("The sampler's envelope, loops and one-shots", "[sampler]")
{
    const auto level = steady (0.5f, 0.5);

    // Attack ramps up; release fades after the key is let go.
    Fixture f;
    f.addZone (level);
    f.set (sm::attack, 100.0f);
    f.set (sm::release, 100.0f);
    auto out = f.run (9600, { Fixture::on (60), Fixture::off (60, 7200) });
    CHECK (out[2400] < 0.35f);                    // half way through the attack
    CHECK (std::abs (out[6000] - 0.5f) < 0.01f);  // held
    CHECK (out[9500] < 0.25f);                    // fading
    CHECK (f.instrument->getNumSounding() == 1);
    f.run (24000);
    CHECK (f.instrument->getNumSounding() == 0);

    // Without a loop, a held note stops at the end of its recording; with one, it carries on.
    Fixture g;
    g.addZone (level);
    out = g.run (48000, { Fixture::on (60) });
    CHECK (out[30000] == 0.0f);

    Fixture h;
    auto& z = h.addZone (level);
    z.loopStart = 4800;
    z.loopEnd = 19200;
    out = h.run (48000, { Fixture::on (60) });
    CHECK (std::abs (out[40000] - 0.5f) < 0.01f);

    // A one-shot plays to its end even when the key is let go at once.
    Fixture k;
    auto& o = k.addZone (level);
    o.oneShot = true;
    out = k.run (24000, { Fixture::on (60), Fixture::off (60, 100) });
    CHECK (std::abs (out[20000] - 0.5f) < 0.01f);
}

TEST_CASE ("Slide: overlapping notes glide without starting again; separate notes don't", "[sampler]")
{
    const auto a440 = sine (440.0, 4.0);

    Fixture f;
    f.addZone (a440, 69);
    f.set (sm::voiceMode, (float) sm::slide);
    f.set (sm::glide, 50.0f);

    // Two clip notes: the second (an octave up) starts while the first still plays.
    auto out = f.run (48000, { Fixture::clipNote (69, 24000), Fixture::clipNote (81, 20000, 19200) });
    CHECK (f.instrument->getNumSounding() == 1);           // one sound, gliding
    CHECK (std::abs (frequency (out, 2400, 19000) - 440.0) < 4.0);
    CHECK (std::abs (frequency (out, 26000, 38000) - 880.0) < 8.0);
    CHECK (*std::max_element (out.begin() + 19200, out.begin() + 19500) > 0.4f);   // no gap where the new note starts

    // Separate notes: the second starts at its own pitch straight away.
    Fixture g;
    g.addZone (a440, 69);
    g.set (sm::voiceMode, (float) sm::slide);
    g.set (sm::glide, 500.0f);
    out = g.run (48000, { Fixture::clipNote (69, 9600), Fixture::clipNote (81, 20000, 24000) });
    CHECK (std::abs (frequency (out, 24500, 26000) - 880.0) < 40.0);

    // Mono: a new note cuts the last one off.
    Fixture h;
    h.addZone (a440, 69);
    h.set (sm::voiceMode, (float) sm::mono);
    h.run (4800, { Fixture::on (60), Fixture::on (64, 100, 2400) });
    CHECK (h.instrument->getNumSounding() == 1);

    // Poly: chords.
    Fixture p;
    p.addZone (a440, 69);
    p.run (4800, { Fixture::on (60), Fixture::on (64), Fixture::on (67) });
    CHECK (p.instrument->getNumSounding() == 3);
}

TEST_CASE ("The tone control darkens the sound; another map stops the notes", "[sampler]")
{
    const auto high = sine (6000.0, 1.0);
    auto rms = [] (const std::vector<float>& x, int from, int to)
    {
        double sum = 0.0;
        for (int i = from; i < to; ++i)
            sum += (double) x[(size_t) i] * x[(size_t) i];
        return std::sqrt (sum / (to - from));
    };

    Fixture f;
    f.addZone (high, 60);
    const auto open = rms (f.run (9600, { Fixture::on (60) }), 2400, 9600);

    Fixture g;
    g.addZone (high, 60);
    g.set (sm::tone, 800.0f);
    const auto dark = rms (g.run (9600, { Fixture::on (60) }), 2400, 9600);
    CHECK (dark < open * 0.1);

    // Volume and pan.
    Fixture h;
    const auto level = steady (0.5f);
    h.addZone (level);
    h.set (sm::volume, -6.0206f);
    h.set (sm::pan, -100.0f);
    auto out = h.run (2400, { Fixture::on (60) });
    CHECK (std::abs (out[1000] - 0.25f * std::sqrt (2.0f)) < 0.01f);
    CHECK (std::abs (h.right[1000]) < 1.0e-4f);

    // The engine hands over another map (other recordings): what was playing stops.
    SampleMap other;
    h.sounds.map = &other;
    h.run (512);
    CHECK (h.instrument->getNumSounding() == 0);
}

TEST_CASE ("A loop that runs to the very end of its recording holds the note smoothly", "[sampler]")
{
    // A 480 Hz sine (100 samples a cycle) whose loop is whole cycles up to its last sample, as
    // Aether's recorded instruments are made: held for 3 seconds, it never stops or jumps.
    const auto tone = sine (480.0, 0.5);
    Fixture f;
    auto& z = f.addZone (tone, 60);
    z.loopStart = 4800;
    z.loopEnd = tone.getNumSamples();   // the very end

    const auto out = f.run (144000, { Fixture::on (60) });
    CHECK (f.instrument->getNumSounding() == 1);
    CHECK (*std::max_element (out.begin() + 140000, out.end()) > 0.45f);

    float biggestStep = 0.0f;
    for (size_t i = 1000; i < out.size(); ++i)
        biggestStep = std::max (biggestStep, std::abs (out[i] - out[i - 1]));
    CHECK (biggestStep < 0.033f);   // a 480 Hz sine at 0.5 moves up to 0.0314 per sample; a click would be far more

    // Played higher (faster through the recording), the same.
    Fixture g;
    auto& y = g.addZone (tone, 60);
    y.loopStart = 4800;
    y.loopEnd = tone.getNumSamples();
    const auto higher = g.run (144000, { Fixture::on (67) });
    CHECK (*std::max_element (higher.begin() + 140000, higher.end()) > 0.45f);
}

TEST_CASE ("Loudness layers blend, so playing harder changes the sound gradually", "[sampler]")
{
    const auto soft = steady (0.2f), loud = steady (0.8f);
    auto levelAt = [&] (int velocity)
    {
        Fixture f;
        f.map.zones.reserve (2);
        auto& s = f.addZone (soft);
        s.highVelocity = 63;
        auto& l = f.addZone (loud);
        l.lowVelocity = 64;
        return f.run (2400, { Fixture::on (60, velocity) })[1000];
    };

    // Far from where they meet: one recording each.
    CHECK (std::abs (levelAt (30) - 0.2f) < 0.005f);
    CHECK (std::abs (levelAt (110) - 0.8f) < 0.005f);

    // Where they meet, half of each; and the level only ever rises, a little at a time.
    CHECK (std::abs (levelAt (64) - 0.5f) < 0.04f);

    float previous = levelAt (40);
    for (int velocity = 41; velocity <= 90; ++velocity)
    {
        const auto level = levelAt (velocity);
        INFO ("velocity " << velocity);
        CHECK (level >= previous - 1.0e-4f);
        CHECK (level - previous < 0.04f);
        previous = level;
    }
}

TEST_CASE ("A recording played far above its pitch doesn't fold back into harsh tones", "[sampler]")
{
    const auto rms = [] (const std::vector<float>& x, size_t from, size_t to)
    {
        double sum = 0.0;
        for (auto i = from; i < to; ++i)
            sum += (double) x[i] * x[i];
        return std::sqrt (sum / (double) (to - from));
    };

    // A 9 kHz tone played two octaves up would be 36 kHz: more than the output can hold (24 kHz
    // at 48 kHz). Without filtering it would come out as a 12 kHz tone that isn't in the
    // recording; filtered, it's (almost) silent.
    const auto high = sine (9000.0, 1.0);
    Fixture f;
    f.addZone (high, 60);
    const auto folded = f.run (24000, { Fixture::on (84) });
    CHECK (rms (folded, 4800, 24000) < 0.01 * 0.5 / std::sqrt (2.0));

    // What fits is kept: a 500 Hz tone two octaves up is a 2 kHz tone at its full level.
    const auto low = sine (500.0, 2.0);
    Fixture g;
    g.addZone (low, 60);
    const auto up = g.run (24000, { Fixture::on (84) });
    CHECK (std::abs (frequency (up, 4800, 24000) - 2000.0) < 20.0);
    CHECK (rms (up, 4800, 24000) > 0.93 * 0.5 / std::sqrt (2.0));

    // Within 3 semitones nothing changes: the recording is read exactly as before.
    Fixture h, k;
    h.addZone (high, 60);
    k.addZone (high, 60);
    const auto near = h.run (4800, { Fixture::on (62) });
    CHECK (rms (near, 1000, 4800) > 0.3);

    // A held note through a loop stays smooth when filtered (the filter carries on across the
    // jump back to the loop's start).
    const auto tone = sine (480.0, 0.5);
    Fixture m;
    auto& z = m.addZone (tone, 60);
    z.loopStart = 4800;
    z.loopEnd = tone.getNumSamples();
    const auto held = m.run (96000, { Fixture::on (67) });   // 7 semitones up: filtered
    float biggestStep = 0.0f;
    for (size_t i = 2000; i < held.size(); ++i)
        biggestStep = std::max (biggestStep, std::abs (held[i] - held[i - 1]));
    CHECK (biggestStep < 0.033f * 1.5f + 0.002f);   // a 719 Hz sine at 0.5 moves up to 0.047 per sample
    CHECK (*std::max_element (held.begin() + 90000, held.end()) > 0.45f);
}

TEST_CASE ("Legato: an overlapping note takes over smoothly, notes played together still make a chord", "[sampler]")
{
    const auto a440 = sine (440.0, 4.0, 0.4f);

    // Two clip notes: the second starts while the first still plays.
    Fixture f;
    f.addZone (a440, 69);
    f.set (sm::voiceMode, (float) sm::legato);
    const auto out = f.run (48000, { Fixture::clipNote (69, 24000), Fixture::clipNote (76, 20000, 19200) });

    // After the hand-over (40 ms), only the new note sounds, at its own pitch...
    CHECK (f.instrument->getNumSounding() == 1);
    CHECK (std::abs (frequency (out, 24000, 38000) - 659.3) < 8.0);

    // ...and the two never add up to much more than one note.
    CHECK (*std::max_element (out.begin() + 19200, out.begin() + 22000) < 0.4f * 1.45f);

    // Played together, they're a chord.
    Fixture g;
    g.addZone (a440, 69);
    g.set (sm::voiceMode, (float) sm::legato);
    g.run (4800, { Fixture::on (60), Fixture::on (64, 100, 200), Fixture::on (67, 100, 400) });
    CHECK (g.instrument->getNumSounding() == 3);

    // A note after the chord takes over from all of it.
    g.run (9600, { Fixture::on (72, 100, 4800) });
    CHECK (g.instrument->getNumSounding() == 1);

    // In the sampler's list of voice modes.
    const auto voices = sm::parameters()[(size_t) sm::voiceMode];
    CHECK (voices.choices[sm::legato] == "Legato");
    CHECK (voices.maximum == (float) sm::legato);
}
