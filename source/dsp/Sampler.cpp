// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "Sampler.h"

#include <algorithm>
#include <cmath>

namespace aether::dsp::sampler
{

std::vector<EffectParameter> parameters()
{
    std::vector<EffectParameter> list {
        { "attack",    "Attack",     1.0f, 10000.0f, 2.0f,    "ms", {}, true },
        { "decay",     "Decay",      10.0f, 20000.0f, 2000.0f, "ms", {}, true },
        { "sustain",   "Sustain",    0.0f, 100.0f, 100.0f,    "%",  {}, false },
        { "release",   "Release",    5.0f, 10000.0f, 300.0f,  "ms", {}, true },
        { "tone",      "Tone",       200.0f, 20000.0f, 20000.0f, "Hz", {}, true },
        { "touch",     "Touch",      0.0f, 100.0f, 70.0f,     "%",  {}, false },
        { "tune",      "Tune",       -24.0f, 24.0f, 0.0f,     "st", {}, false, true },
        { "fine",      "Fine",       -100.0f, 100.0f, 0.0f,   "cents", {}, false },
        { "voiceMode", "Voices",     0.0f, 2.0f, 0.0f, {}, { "Poly", "Mono", "Slide" }, false },
        { "glide",     "Glide",      1.0f, 2000.0f, 80.0f,    "ms", {}, true },
        { "volume",    "Volume",     -40.0f, 12.0f, 0.0f,     "dB", {}, false },
        { "pan",       "Pan",        -100.0f, 100.0f, 0.0f,   "pan", {}, false },
        { "bendRange", "Bend range", 1.0f, 12.0f, 2.0f,       "st", {}, false, true },
    };

    jassert ((int) list.size() == numParameters);
    return list;
}

//==============================================================================
namespace
{
    constexpr int controlInterval = 16;     // pitch and glide are worked out every 16 samples
    constexpr int scratchSize = 1024;

    /** Where to read sample `i`: past a loop's end it carries on from the loop's start (so the
        interpolation is smooth across the loop point); otherwise kept within the sound. */
    struct Reader
    {
        std::int64_t last = 0, loopStart = -1, loopEnd = -1;

        std::int64_t at (std::int64_t i) const noexcept
        {
            if (loopEnd > loopStart && loopStart >= 0 && i >= loopEnd)
                i -= loopEnd - loopStart;
            return std::clamp<std::int64_t> (i, 0, last);
        }
    };

    /** 4-point Hermite interpolation between d[i] and the next sample. */
    inline float hermite (const float* d, std::int64_t i, float t, const Reader& reader) noexcept
    {
        const auto xm1 = d[reader.at (i - 1)];
        const auto x0 = d[reader.at (i)];
        const auto x1 = d[reader.at (i + 1)];
        const auto x2 = d[reader.at (i + 2)];
        const auto c = (x1 - xm1) * 0.5f;
        const auto v = x0 - x1;
        const auto w = c + v;
        const auto a = w + v + (x2 - x0) * 0.5f;
        const auto b = w + a;
        return ((a * t - b) * t + c) * t + x0;
    }

    struct Envelope
    {
        enum Stage : std::uint8_t { idle, attack, decay, sustain, release };

        Stage stage = idle;
        double level = 0.0;

        void trigger() noexcept   { stage = attack; }
        void letGo() noexcept     { if (stage != idle) stage = release; }

        double next (double attackStep, double decayCoefficient, double sustainLevel, double releaseCoefficient) noexcept
        {
            switch (stage)
            {
                case attack:
                    level += attackStep;
                    if (level >= 1.0) { level = 1.0; stage = decay; }
                    break;

                case decay:
                    level = sustainLevel + (level - sustainLevel) * decayCoefficient;
                    if (std::abs (level - sustainLevel) < 1.0e-4) { level = sustainLevel; stage = sustain; }
                    break;

                case sustain:
                    level = sustainLevel;
                    break;

                case release:
                    level *= releaseCoefficient;
                    if (level < 1.0e-4) { level = 0.0; stage = idle; }
                    break;

                case idle:
                    level = 0.0;
                    break;
            }

            return level;
        }
    };

    class Sampler final : public Instrument
    {
    public:
        void prepare (double newSampleRate, int) override
        {
            sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
            for (auto& v : voices)
                v = {};
            currentMap = nullptr;
            numHeld = 0;
            pedal = false;
            bend = 0.0;
            roundRobin.fill (0);
        }

        int getNumSounding() const noexcept override
        {
            return (int) std::count_if (voices.begin(), voices.end(), [] (const Voice& v) { return v.active; });
        }

        void process (float* left, float* right, int numSamples, const EffectParameterValues& p,
                      const NoteEvent* events, int numEvents, const InstrumentSounds* sounds) noexcept AETHER_NONBLOCKING override
        {
            const auto* map = sounds != nullptr ? sounds->map : nullptr;

            // Another map (other recordings, or the old ones gone): the notes it was playing stop.
            if (map != currentMap)
            {
                for (auto& v : voices)
                    v.active = false;
                currentMap = map;
            }

            update (p);

            int done = 0, next = 0;

            while (done < numSamples)
            {
                while (next < numEvents && events[next].offset <= done)
                    handle (events[next++]);

                const auto until = next < numEvents ? std::clamp (events[next].offset, done + 1, numSamples) : numSamples;

                for (int from = done; from < until; from += scratchSize)
                    renderAndMix (left + from, right + from, std::min (scratchSize, until - from));

                done = until;
            }

            while (next < numEvents)
                handle (events[next++]);
        }

    private:
        //==============================================================================
        struct Settings
        {
            double attackStep = 1.0, decayCoefficient = 0.0, sustainLevel = 1.0, releaseCoefficient = 0.0;
            double glideCoefficient = 1.0, tuneOffset = 0.0, bendRange = 2.0, touch = 0.7;
            float gainLeft = 1.0f, gainRight = 1.0f;
            bool filtered = false;
            double a1 = 1.0, a2 = 0.0, a3 = 0.0;
            int mode = poly;
        };

        struct Voice
        {
            bool active = false;
            const SampleZone* zone = nullptr;
            int note = 60;
            bool fromClip = false, keyDown = false, sustained = false, mono = false;
            std::int64_t remaining = -1;          // notes from clips: samples until they end
            double position = 0.0, step = 1.0, pitch = 60.0, targetPitch = 60.0;
            float gainLeft = 1.0f, gainRight = 1.0f;
            float fade = 1.0f, fadeStep = 0.0f;   // a quick fade-out when cut off
            Envelope amp;
            double ic1[2] {}, ic2[2] {};
            int control = 0;
            std::uint64_t startedAt = 0;

            void letGo() noexcept
            {
                keyDown = false;
                sustained = false;

                if (zone != nullptr && ! zone->oneShot)
                    amp.letGo();
            }
        };

        double perSample (double milliseconds) const noexcept   { return std::max (1.0, milliseconds * 0.001 * sampleRate); }

        void update (const EffectParameterValues& p) noexcept
        {
            auto& s = settings;
            s.attackStep = 1.0 / perSample (p[attack]);
            s.decayCoefficient = std::exp (-4.6 / perSample (p[decay]));
            s.sustainLevel = std::clamp ((double) p[sustain] / 100.0, 0.0, 1.0);
            s.releaseCoefficient = std::exp (-4.6 / perSample (p[release]));
            s.glideCoefficient = 1.0 - std::exp (-4.6 * controlInterval / perSample (std::max (1.0f, p[glide])));
            s.tuneOffset = std::round ((double) p[tune]) + (double) p[fine] / 100.0;
            s.bendRange = std::clamp ((double) std::round (p[bendRange]), 1.0, 12.0);
            s.touch = std::clamp ((double) p[touch] / 100.0, 0.0, 1.0);
            s.mode = std::clamp ((int) std::lround (p[voiceMode]), 0, 2);

            const auto gain = juce::Decibels::decibelsToGain ((double) p[volume], -100.0);
            const auto angle = (std::clamp ((double) p[pan], -100.0, 100.0) / 100.0 + 1.0) * juce::MathConstants<double>::pi * 0.25;
            s.gainLeft = (float) (gain * std::cos (angle) * juce::MathConstants<double>::sqrt2);
            s.gainRight = (float) (gain * std::sin (angle) * juce::MathConstants<double>::sqrt2);

            // The tone control: a gentle 12 dB low-pass, off when fully open.
            const auto cutoff = std::clamp ((double) p[tone], 20.0, sampleRate * 0.45);
            s.filtered = p[tone] < 19900.0f;
            const auto g = std::tan (juce::MathConstants<double>::pi * cutoff / sampleRate);
            const auto k = juce::MathConstants<double>::sqrt2;
            s.a1 = 1.0 / (1.0 + g * (g + k));
            s.a2 = g * s.a1;
            s.a3 = g * s.a2;
        }

        //==============================================================================
        void handle (const NoteEvent& e) noexcept
        {
            switch (e.type)
            {
                case NoteEvent::Type::noteOn:     noteOn (e.pitch, e.velocity, e.fromClip, e.length); break;
                case NoteEvent::Type::noteOff:    noteOff (e.pitch); break;
                case NoteEvent::Type::pitchBend:  bend = std::clamp ((double) e.value, -1.0, 1.0); break;
                case NoteEvent::Type::modWheel:   break;

                case NoteEvent::Type::sustain:
                    pedal = e.value >= 0.5f;

                    if (! pedal)
                        for (auto& v : voices)
                            if (v.active && v.sustained)
                                v.letGo();
                    break;

                case NoteEvent::Type::stopClipNotes:
                    for (auto& v : voices)
                        if (v.active && v.fromClip)
                            v.letGo();
                    break;

                case NoteEvent::Type::allNotesOff:
                    for (auto& v : voices)
                        if (v.active)
                            v.letGo();
                    pedal = false;
                    numHeld = 0;
                    break;
            }
        }

        void noteOn (int note, int velocity, bool fromClip, std::int64_t length) noexcept
        {
            if (currentMap == nullptr)
                return;

            if (settings.mode != poly)
            {
                if (! fromClip)
                    pushHeld (note);

                // Slide: a note that starts while the last one is still held glides to the new
                // pitch, the sound carrying on.
                if (settings.mode == slide && slideTo (note, fromClip, length))
                    return;

                for (auto& v : voices)
                    if (v.active && v.mono)
                        cut (v);
            }
            else
            {
                // The same key again: its old sound is let go (it rings on into its release).
                for (auto& v : voices)
                    if (v.active && v.note == note && v.keyDown && v.fromClip == fromClip)
                        v.letGo();
            }

            startNote (note, velocity, fromClip, length, settings.mode != poly);
        }

        void startNote (int note, int velocity, bool fromClip, std::int64_t length, bool mono) noexcept
        {
            // Round robin: each key counts its hits, and zones play on their turn.
            const auto hit = roundRobin[(size_t) std::clamp (note, 0, 127)]++;
            const auto vel = std::clamp (velocity, 1, 127);
            const auto v01 = (double) vel / 127.0;
            const auto velocityGain = 1.0 - settings.touch + settings.touch * v01 * v01;

            for (const auto& zone : currentMap->zones)
            {
                if (zone.sample == nullptr || ! zone.matches (note, vel)
                    || (int) (hit % (std::uint32_t) std::max (1, zone.sequenceLength)) + 1 != std::max (1, zone.sequencePosition))
                    continue;

                if (zone.lastSample() - zone.start < 4)
                    continue;

                auto& v = freeVoice();
                v = {};
                v.active = true;
                v.zone = &zone;
                v.note = note;
                v.fromClip = fromClip;
                v.keyDown = true;
                v.mono = mono;
                v.remaining = fromClip ? std::max<std::int64_t> (1, length) : -1;
                v.position = (double) zone.start;
                v.pitch = v.targetPitch = note;
                v.startedAt = ++clock;
                v.amp.trigger();

                const auto angle = (std::clamp ((double) zone.pan, -1.0, 1.0) + 1.0) * juce::MathConstants<double>::pi * 0.25;
                const auto gain = (double) zone.gain * velocityGain;
                v.gainLeft = (float) (gain * std::cos (angle) * juce::MathConstants<double>::sqrt2);
                v.gainRight = (float) (gain * std::sin (angle) * juce::MathConstants<double>::sqrt2);
                updateStep (v);
            }
        }

        bool slideTo (int note, bool fromClip, std::int64_t length) noexcept
        {
            bool slid = false;

            for (auto& v : voices)
            {
                if (! v.active || ! v.mono || ! v.keyDown || v.fade < 1.0f)
                    continue;

                v.note = note;
                v.targetPitch = note;
                v.fromClip = fromClip;
                v.remaining = fromClip ? std::max<std::int64_t> (1, length) : -1;
                slid = true;
            }

            return slid;
        }

        void noteOff (int note) noexcept
        {
            if (settings.mode != poly)
            {
                removeHeld (note);

                bool playing = false;
                for (auto& v : voices)
                    playing = playing || (v.active && v.mono && ! v.fromClip && v.note == note && v.keyDown);

                if (! playing)
                    return;

                // Back to the key still held before it.
                if (numHeld > 0)
                {
                    const auto previous = held[(size_t) numHeld - 1];

                    if (settings.mode == slide)
                    {
                        slideTo (previous, false, 0);
                    }
                    else
                    {
                        for (auto& v : voices)
                            if (v.active && v.mono)
                                cut (v);
                        startNote (previous, 100, false, 0, true);
                    }
                    return;
                }
            }

            for (auto& v : voices)
                if (v.active && ! v.fromClip && v.note == note && v.keyDown)
                {
                    if (pedal) { v.keyDown = false; v.sustained = true; }
                    else       { v.letGo(); }
                }
        }

        void cut (Voice& v) noexcept
        {
            // 3 ms, so it doesn't click.
            v.fadeStep = (float) (1.0 / perSample (3.0));
            v.keyDown = false;
        }

        Voice& freeVoice() noexcept
        {
            for (auto& v : voices)
                if (! v.active)
                    return v;

            auto* oldest = &voices[0];
            for (auto& v : voices)
                if (v.startedAt < oldest->startedAt)
                    oldest = &v;

            return *oldest;
        }

        void pushHeld (int note) noexcept
        {
            removeHeld (note);
            if (numHeld < (int) held.size())
                held[(size_t) numHeld++] = note;
        }

        void removeHeld (int note) noexcept
        {
            for (int i = 0; i < numHeld; ++i)
                if (held[(size_t) i] == note)
                {
                    for (int j = i; j < numHeld - 1; ++j)
                        held[(size_t) j] = held[(size_t) j + 1];
                    --numHeld;
                    return;
                }
        }

        void updateStep (Voice& v) const noexcept
        {
            const auto semitones = v.pitch - v.zone->rootKey + v.zone->tune + settings.tuneOffset + bend * settings.bendRange;
            v.step = std::exp2 (semitones / 12.0);
        }

        //==============================================================================
        void renderAndMix (float* left, float* right, int numSamples) noexcept
        {
            std::fill (scratchLeft.begin(), scratchLeft.begin() + numSamples, 0.0f);
            std::fill (scratchRight.begin(), scratchRight.begin() + numSamples, 0.0f);

            for (auto& v : voices)
                if (v.active)
                    renderVoice (v, numSamples);

            for (int i = 0; i < numSamples; ++i)
            {
                left[i] += scratchLeft[(size_t) i] * settings.gainLeft;
                right[i] += scratchRight[(size_t) i] * settings.gainRight;
            }
        }

        void renderVoice (Voice& v, int numSamples) noexcept
        {
            const auto& zone = *v.zone;
            const auto& sound = *zone.sample;
            const auto* l = sound.getReadPointer (0);
            const auto* r = sound.getReadPointer (sound.getNumChannels() > 1 ? 1 : 0);
            const auto stereo = sound.getNumChannels() > 1;
            const auto last = zone.lastSample() - 1;
            const auto loops = zone.loops();
            const auto loopLength = (double) (zone.loopEnd - zone.loopStart);
            const Reader reader { last, loops ? zone.loopStart : -1, loops ? zone.loopEnd : -1 };
            const auto& s = settings;

            for (int i = 0; i < numSamples; ++i)
            {
                if (--v.control <= 0)
                {
                    v.control = controlInterval;
                    if (v.pitch != v.targetPitch)
                    {
                        v.pitch += (v.targetPitch - v.pitch) * s.glideCoefficient;
                        if (std::abs (v.targetPitch - v.pitch) < 1.0e-3)
                            v.pitch = v.targetPitch;
                    }
                    updateStep (v);
                }

                // Notes from clips end by themselves.
                if (v.remaining > 0 && --v.remaining == 0)
                {
                    if (pedal) { v.keyDown = false; v.sustained = true; }
                    else       { v.letGo(); }
                }

                // A loop (held notes): back to its start when it reaches its end, which may be the
                // very end of the recording. Without one, the note ends with the recording.
                while (loops && v.position >= (double) zone.loopEnd)
                    v.position -= loopLength;

                const auto index = (std::int64_t) v.position;
                if (! loops && index >= last)
                {
                    v.active = false;
                    return;
                }

                const auto t = (float) (v.position - (double) index);
                auto sl = hermite (l, index, t, reader);
                auto sr = stereo ? hermite (r, index, t, reader) : sl;

                const auto env = (float) v.amp.next (s.attackStep, s.decayCoefficient, s.sustainLevel, s.releaseCoefficient) * v.fade;

                if (s.filtered)
                {
                    sl = (float) filter (v, 0, sl);
                    sr = stereo ? (float) filter (v, 1, sr) : sl;
                }

                scratchLeft[(size_t) i] += sl * v.gainLeft * env;
                scratchRight[(size_t) i] += sr * v.gainRight * env;

                v.position += v.step;

                if (v.fadeStep > 0.0f)
                    v.fade -= v.fadeStep;

                // Finished: the envelope has died away, or a cut has faded out.
                if (v.fade <= 0.0f || v.amp.stage == Envelope::idle
                    || (v.amp.stage == Envelope::sustain && s.sustainLevel <= 0.0))
                {
                    v.active = false;
                    return;
                }
            }
        }

        double filter (Voice& v, int channel, double x) const noexcept
        {
            auto& ic1 = v.ic1[channel];
            auto& ic2 = v.ic2[channel];
            const auto v3 = x - ic2;
            const auto v1 = settings.a1 * ic1 + settings.a2 * v3;
            const auto v2 = ic2 + settings.a2 * ic1 + settings.a3 * v3;
            ic1 = 2.0 * v1 - ic1;
            ic2 = 2.0 * v2 - ic2;
            return v2;
        }

        double sampleRate = 48000.0;
        Settings settings;
        std::array<Voice, maxVoices> voices {};
        std::array<float, scratchSize> scratchLeft {}, scratchRight {};
        std::array<std::uint32_t, 128> roundRobin {};
        std::array<int, 16> held {};
        int numHeld = 0;
        bool pedal = false;
        double bend = 0.0;
        const SampleMap* currentMap = nullptr;
        std::uint64_t clock = 0;
    };
}

std::unique_ptr<Instrument> create()
{
    return std::make_unique<Sampler>();
}

} // namespace aether::dsp::sampler
