// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
//
// The starter synth (Milestone 6): a small polyphonic synthesizer so MIDI tracks make a sound.
// A fuller synth is milestone M10.

#include "FastMath.h"
#include "Instrument.h"
#include "DrumKit.h"
#include "Synth.h"

#include <algorithm>
#include <array>
#include <cmath>

#include <juce_core/juce_core.h>

namespace aether::dsp
{

namespace
{
    constexpr int numVoices = 16;
    constexpr double twoPi = juce::MathConstants<double>::twoPi;

    enum Parameter { tone, detune, sub, cutoff, resonance, envelope, attack, decay, sustainLevel, release, touch, volume };
    enum Tone { sine, triangle, saw, square, electricPiano };

    /** Smooths the corners of saw and square waves, so they don't alias (whistle) at high notes. */
    double polyBlep (double t, double dt) noexcept AETHER_NONBLOCKING
    {
        if (t < dt)        { t /= dt; return t + t - t * t - 1.0; }
        if (t > 1.0 - dt)  { t = (t - 1.0) / dt; return t * t + t + t + 1.0; }
        return 0.0;
    }

    double oscillator (int shape, double phase, double dt) noexcept AETHER_NONBLOCKING
    {
        switch (shape)
        {
            case triangle: return 1.0 - 4.0 * std::abs (phase - 0.5);
            case saw:      return 2.0 * phase - 1.0 - polyBlep (phase, dt);
            case square:
            {
                auto v = phase < 0.5 ? 1.0 : -1.0;
                v += polyBlep (phase, dt);
                v -= polyBlep (std::fmod (phase + 0.5, 1.0), dt);
                return v;
            }
            default:       return sin2pi (phase);
        }
    }

    struct Voice
    {
        bool active = false;
        int pitch = 0;
        float velocity = 0.0f;
        bool fromClip = false, keyDown = false, sustained = false;
        std::int64_t remaining = -1;     // clip notes: samples until they're released
        std::uint64_t startedAt = 0;     // for choosing which voice to take when all are busy

        enum class Stage { attack, decay, sustain, release } stage = Stage::attack;
        double level = 0.0;              // amplitude envelope
        double filterEnv = 0.0, fmEnv = 0.0;
        double phase1 = 0.0, phase2 = 0.0, subPhase = 0.0, modPhase = 0.0;
        double ic1 = 0.0, ic2 = 0.0;     // filter state
        double a1 = 1.0, a2 = 0.0, a3 = 0.0;   // filter coefficients (updated every 16 samples)
        int sinceCoefficients = 0;

        void start (int notePitch, float noteVelocity, bool isClip, std::int64_t length, std::uint64_t now) noexcept AETHER_NONBLOCKING
        {
            const auto wasSounding = active && level > 0.001;
            active = true;
            pitch = notePitch;
            velocity = noteVelocity;
            fromClip = isClip;
            keyDown = true;
            sustained = false;
            remaining = isClip ? std::max<std::int64_t> (1, length) : -1;
            startedAt = now;
            stage = Stage::attack;
            filterEnv = 1.0;
            fmEnv = 1.0;

            if (! wasSounding)
            {
                // A fresh start; a stolen voice keeps its level and phases so it doesn't click.
                level = 0.0;
                phase1 = phase2 = subPhase = modPhase = 0.0;
                ic1 = ic2 = 0.0;
            }

            sinceCoefficients = 0;
        }

        void releaseNote() noexcept AETHER_NONBLOCKING
        {
            keyDown = false;
            sustained = false;
            stage = Stage::release;
        }
    };

    class StarterSynth final : public Instrument
    {
    public:
        void prepare (double newSampleRate, int) override
        {
            sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
            for (auto& v : voices)
                v = {};
            pedal = false;
            bend = 0.0;
        }

        int getNumSounding() const noexcept override
        {
            return (int) std::count_if (voices.begin(), voices.end(), [] (const Voice& v) { return v.active; });
        }

        void process (float* left, float* right, int numSamples, const EffectParameterValues& p,
                      const NoteEvent* events, int numEvents, const InstrumentSounds*) noexcept AETHER_NONBLOCKING override
        {
            int done = 0, next = 0;

            while (done < numSamples)
            {
                // Handle every event due now, then render up to the next one.
                while (next < numEvents && events[next].offset <= done)
                    handle (events[next++]);

                const auto until = next < numEvents ? std::clamp (events[next].offset, done + 1, numSamples) : numSamples;
                render (left + done, right + done, until - done, p);
                done = until;
            }

            while (next < numEvents)
                handle (events[next++]);
        }

    private:
        void handle (const NoteEvent& e) noexcept AETHER_NONBLOCKING
        {
            switch (e.type)
            {
                case NoteEvent::Type::noteOn:
                {
                    // The same key again: its old voice stops (quickly) and a new one starts.
                    for (auto& v : voices)
                        if (v.active && v.pitch == e.pitch && v.keyDown && v.fromClip == e.fromClip)
                            v.releaseNote();

                    voiceFor (e.pitch).start (e.pitch, (float) e.velocity / 127.0f, e.fromClip, e.length, ++clock);
                    break;
                }

                case NoteEvent::Type::noteOff:
                    for (auto& v : voices)
                        if (v.active && ! v.fromClip && v.pitch == e.pitch && v.keyDown)
                        {
                            v.keyDown = false;

                            if (pedal)
                                v.sustained = true;
                            else
                                v.releaseNote();
                        }
                    break;

                case NoteEvent::Type::sustain:
                    pedal = e.value >= 0.5f;

                    if (! pedal)
                        for (auto& v : voices)
                            if (v.active && v.sustained)
                                v.releaseNote();
                    break;

                case NoteEvent::Type::pitchBend:
                    bend = std::clamp ((double) e.value, -1.0, 1.0);
                    break;

                case NoteEvent::Type::stopClipNotes:
                    for (auto& v : voices)
                        if (v.active && v.fromClip)
                            v.releaseNote();
                    break;

                case NoteEvent::Type::allNotesOff:
                    for (auto& v : voices)
                        if (v.active)
                            v.releaseNote();
                    pedal = false;
                    break;

                case NoteEvent::Type::modWheel:
                    break;   // the starter synth has no use for it
            }
        }

        Voice& voiceFor (int pitch) noexcept AETHER_NONBLOCKING
        {
            // A free voice if there is one; otherwise one already releasing; otherwise the oldest.
            for (auto& v : voices)
                if (! v.active)
                    return v;

            Voice* best = nullptr;

            for (auto& v : voices)
                if (v.stage == Voice::Stage::release && (best == nullptr || v.level < best->level))
                    best = &v;

            if (best == nullptr)
                for (auto& v : voices)
                    if (best == nullptr || v.startedAt < best->startedAt)
                        best = &v;

            juce::ignoreUnused (pitch);
            return *best;
        }

        double timeToCoefficient (double milliseconds) const noexcept AETHER_NONBLOCKING
        {
            // A one-pole that covers ~99% of the way in the given time.
            return 1.0 - std::exp (-4.6 / std::max (1.0, milliseconds * 0.001 * sampleRate));
        }

        void render (float* left, float* right, int numSamples, const EffectParameterValues& p) noexcept AETHER_NONBLOCKING
        {
            const auto shape = std::clamp ((int) std::lround (p[tone]), 0, 4);
            const auto detuneRatio = std::pow (2.0, p[detune] / 1200.0);
            const auto subLevel = p[sub] / 100.0;
            const auto res = std::clamp (p[resonance] / 100.0, 0.0, 1.0);
            const auto envAmount = p[envelope] / 100.0;
            const auto attackSamples = std::max (1.0, p[attack] * 0.001 * sampleRate);
            const auto decayCoefficient = timeToCoefficient (p[decay]);
            const auto releaseCoefficient = timeToCoefficient (p[release]);
            const auto filterDecay = timeToCoefficient (std::max (30.0, (double) p[decay]));
            const auto fmDecay = timeToCoefficient (400.0);
            const auto sustainTarget = std::clamp (p[sustainLevel] / 100.0, 0.0, 1.0);
            const auto touchAmount = std::clamp (p[touch] / 100.0, 0.0, 1.0);
            const auto outputGain = 0.25 * std::pow (10.0, p[volume] / 20.0);
            const auto bendRatio = std::pow (2.0, bend * 2.0 / 12.0);
            const auto twoOscillators = shape != electricPiano && p[detune] > 0.5f;

            for (auto& v : voices)
            {
                if (! v.active)
                    continue;

                const auto frequency = 440.0 * std::pow (2.0, (v.pitch - 69) / 12.0) * bendRatio;
                const auto dt = std::min (0.45, frequency / sampleRate);
                const auto loudness = (1.0 - touchAmount) + touchAmount * std::pow ((double) v.velocity, 1.6);
                const auto baseCutoff = (double) p[cutoff];
                const auto gain = loudness * outputGain;

                for (int i = 0; i < numSamples; ++i)
                {
                    // Clip notes end by themselves after their length.
                    if (v.remaining > 0 && --v.remaining == 0 && v.stage != Voice::Stage::release)
                        v.releaseNote();

                    // Amplitude envelope.
                    switch (v.stage)
                    {
                        case Voice::Stage::attack:
                            v.level += 1.0 / attackSamples;
                            if (v.level >= 1.0) { v.level = 1.0; v.stage = Voice::Stage::decay; }
                            break;
                        case Voice::Stage::decay:
                            v.level += (sustainTarget - v.level) * decayCoefficient;
                            if (std::abs (v.level - sustainTarget) < 1.0e-4) v.stage = Voice::Stage::sustain;
                            break;
                        case Voice::Stage::sustain:
                            v.level = sustainTarget;
                            break;
                        case Voice::Stage::release:
                            v.level -= v.level * releaseCoefficient;
                            break;
                    }

                    if (v.stage == Voice::Stage::release && v.level < 1.0e-4)
                    {
                        v.active = false;
                        break;
                    }

                    if (v.stage == Voice::Stage::sustain && sustainTarget <= 0.0)
                    {
                        v.active = false;   // decayed to silence while held
                        break;
                    }

                    v.filterEnv -= v.filterEnv * filterDecay;
                    v.fmEnv -= v.fmEnv * fmDecay;

                    // Oscillators.
                    double sample;

                    if (shape == electricPiano)
                    {
                        // Two-operator FM: a bright, bell-like start that mellows, like an electric piano's tines.
                        const auto index = (0.6 + 2.4 * v.velocity) * v.fmEnv + 0.15;
                        const auto modulator = sin2pi (v.modPhase);
                        sample = sin2pi (v.phase1 + index * modulator * (1.0 / twoPi));
                        v.modPhase += dt;
                        if (v.modPhase >= 1.0) v.modPhase -= 1.0;
                    }
                    else
                    {
                        sample = oscillator (shape, v.phase1, dt);

                        if (twoOscillators)
                        {
                            sample = 0.5 * (sample + oscillator (shape, v.phase2, dt * detuneRatio));
                            v.phase2 += dt * detuneRatio;
                            if (v.phase2 >= 1.0) v.phase2 -= 1.0;
                        }
                    }

                    if (subLevel > 0.0)
                    {
                        sample += subLevel * sin2pi (v.subPhase);
                        v.subPhase += dt * 0.5;
                        if (v.subPhase >= 1.0) v.subPhase -= 1.0;
                    }

                    v.phase1 += dt;
                    if (v.phase1 >= 1.0) v.phase1 -= 1.0;

                    // A resonant low-pass filter, opened by its envelope (more for harder notes).
                    if (--v.sinceCoefficients <= 0)
                    {
                        const auto open = envAmount * v.filterEnv * (0.5 + 0.5 * v.velocity);
                        const auto fc = std::clamp (baseCutoff * std::pow (2.0, open * 6.0), 20.0, sampleRate * 0.45);
                        const auto g = std::tan (juce::MathConstants<double>::pi * fc / sampleRate);
                        const auto k = 2.0 - 1.9 * res;
                        v.a1 = 1.0 / (1.0 + g * (g + k));
                        v.a2 = g * v.a1;
                        v.a3 = g * v.a2;
                        v.sinceCoefficients = 16;
                    }

                    const auto v3 = sample - v.ic2;
                    const auto v1 = v.a1 * v.ic1 + v.a2 * v3;
                    const auto v2 = v.ic2 + v.a2 * v.ic1 + v.a3 * v3;
                    v.ic1 = 2.0 * v1 - v.ic1;
                    v.ic2 = 2.0 * v2 - v.ic2;

                    const auto out = (float) (v2 * v.level * gain);
                    left[i] += out;
                    right[i] += out;
                }
            }
        }

        double sampleRate = 48000.0;
        std::array<Voice, numVoices> voices {};
        bool pedal = false;
        double bend = 0.0;
        std::uint64_t clock = 0;
    };
}

std::unique_ptr<Instrument> createInstrument (const juce::String& typeId)
{
    if (typeId == "synth")
        return std::make_unique<StarterSynth>();

    if (typeId == synth::typeId)
        return synth::create();

    if (typeId == drumkit::typeId)
        return drumkit::create();

    return nullptr;
}

} // namespace aether::dsp
