// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
//
// The Synth's sound (see Synth.h for what it does). Everything here runs on the audio thread,
// except prepare(): no memory is allocated and nothing waits.

#include "Synth.h"

#include <algorithm>
#include <array>

#include <juce_audio_basics/juce_audio_basics.h>

namespace aether::dsp::synth
{

namespace
{
    constexpr int numVoices = 24;
    constexpr int subBlock = 256;          // the most samples handled in one go
    constexpr int controlInterval = 16;    // pitch, filter and LFOs are updated this often
    constexpr int maxSlices = subBlock / controlInterval;
    constexpr double outputScale = 0.3;    // leaves room for chords

    /** Smooths the corners of saw and square waves, so high notes don't alias (whistle). */
    inline double polyBlep (double t, double dt) noexcept
    {
        if (t < dt)        { t /= dt; return t + t - t * t - 1.0; }
        if (t > 1.0 - dt)  { t = (t - 1.0) / dt; return t * t + t + t + 1.0; }
        return 0.0;
    }

    /** One oscillator sample (the same shapes as waveValue(), anti-aliased). */
    inline double oscillator (int wave, double phase, double dt, double pulseWidth) noexcept
    {
        switch (wave)
        {
            case triangle:
                return phase < pulseWidth ? -1.0 + 2.0 * phase / pulseWidth
                                          : 1.0 - 2.0 * (phase - pulseWidth) / (1.0 - pulseWidth);
            case saw:
                return 2.0 * phase - 1.0 - polyBlep (phase, dt);
            case square:
            {
                auto t = phase - pulseWidth;
                if (t < 0.0)
                    t += 1.0;
                return (phase < pulseWidth ? 1.0 : -1.0) + polyBlep (phase, dt) - polyBlep (t, dt);
            }
            default:
                return sin2pi (phase);
        }
    }

    /** A smooth limit to -1..1 (an approximation of tanh), for the filter's drive. */
    inline double softClip (double x) noexcept
    {
        x = std::clamp (x, -3.0, 3.0);
        return x * (27.0 + x * x) / (27.0 + 9.0 * x * x);
    }

    inline double coefficientFor (double milliseconds, double sampleRate) noexcept
    {
        // A one-pole that covers ~99% of the way in the given time.
        return 1.0 - std::exp (-4.6 / std::max (1.0, milliseconds * 0.001 * sampleRate));
    }

    //==============================================================================
    struct Envelope
    {
        enum Stage : std::uint8_t { idle, attack, decay, sustain, release };
        Stage stage = idle;
        double level = 0.0;

        void trigger() noexcept         { stage = attack; }
        void letGo() noexcept           { if (stage != idle) stage = release; }

        double next (double attackStep, double decayCoefficient, double sustainLevel, double releaseCoefficient) noexcept
        {
            switch (stage)
            {
                case attack:
                    level += attackStep;
                    if (level >= 1.0) { level = 1.0; stage = decay; }
                    break;
                case decay:
                    level += (sustainLevel - level) * decayCoefficient;
                    if (std::abs (level - sustainLevel) < 1.0e-4) { level = sustainLevel; stage = sustain; }
                    break;
                case sustain:
                    level = sustainLevel;   // follows the knob
                    break;
                case release:
                    level -= level * releaseCoefficient;
                    if (level < 1.0e-5) { level = 0.0; stage = idle; }
                    break;
                case idle:
                    level = 0.0;
                    break;
            }

            return level;
        }
    };

    /** Settings worked out from the parameters, once per block. */
    struct Settings
    {
        int waveA = saw, waveB = saw, subWave = 0, unison = 1, filterType = lowPass24, voiceMode = poly;
        double ratioA = 1.0, ratioB = 1.0, shapeA = 0.5, shapeB = 0.5, mix = 0.5, sub = 0.0, noise = 0.0;
        std::array<double, maxUnison> unisonRatio {}, gainLeft {}, gainRight {};
        double cutoff = 8000.0, k = 2.0, drive = 0.0, keyTrack = 0.5, envAmount = 0.2, velocityToCutoff = 0.3;
        double fAttackStep = 1.0, fDecay = 1.0, fSustain = 0.3, fRelease = 1.0;
        double attackStep = 1.0, decayCoefficient = 1.0, sustainLevel = 0.7, releaseCoefficient = 1.0, velocitySensitivity = 0.6;
        int lfoTarget[2] { targetOff, targetOff };
        double lfoAmount[2] {}, lfoFadeSeconds[2] {};
        int wheelTarget = wheelVibrato;
        double wheelAmount = 0.5, glideCoefficient = 1.0, bendRange = 2.0;
        double gain = 0.0;

        void update (const EffectParameterValues& p, double sampleRate) noexcept
        {
            const auto choice = [&p] (int index, int last) { return std::clamp ((int) std::lround (p[(size_t) index]), 0, last); };

            waveA = choice (synth::waveA, 3);
            waveB = choice (synth::waveB, 3);
            ratioA = std::pow (2.0, (double) p[octaveA] + ((double) p[semiA] + (double) p[fineA] / 100.0) / 12.0);
            ratioB = std::pow (2.0, (double) p[octaveB] + ((double) p[semiB] + (double) p[fineB] / 100.0) / 12.0);
            shapeA = std::clamp ((double) p[synth::shapeA] / 100.0, 0.05, 0.95);
            shapeB = std::clamp ((double) p[synth::shapeB] / 100.0, 0.05, 0.95);
            mix = std::clamp ((double) p[synth::mix] / 100.0, 0.0, 1.0);
            sub = (double) p[synth::sub] / 100.0;
            subWave = choice (synth::subWave, 1);
            noise = (double) p[synth::noise] / 100.0 * 0.5;

            // Unison: copies spread evenly across the detune range and the stereo field; their
            // level is shared so more copies don't get louder.
            unison = choice (synth::unison, maxUnison - 1) + 1;
            const auto spread = std::clamp ((double) p[synth::width] / 100.0, 0.0, 1.0);
            const auto norm = std::sqrt (2.0 / (double) unison);

            for (int j = 0; j < unison; ++j)
            {
                const auto position = unison == 1 ? 0.0 : (double) j / (double) (unison - 1) * 2.0 - 1.0;   // -1 .. 1
                unisonRatio[(size_t) j] = std::pow (2.0, position * 0.5 * (double) p[detune] / 1200.0);
                const auto angle = (1.0 + position * spread) * juce::MathConstants<double>::pi * 0.25;
                gainLeft[(size_t) j] = std::cos (angle) * norm;
                gainRight[(size_t) j] = std::sin (angle) * norm;
            }

            filterType = choice (synth::filterType, 3);
            cutoff = std::clamp ((double) p[synth::cutoff], 20.0, 20000.0);
            k = 2.0 - 1.9 * std::clamp ((double) p[resonance] / 100.0, 0.0, 1.0);
            drive = (double) p[synth::drive] / 100.0;
            keyTrack = (double) p[synth::keyTrack] / 100.0;
            envAmount = (double) p[filterEnv] / 100.0;
            velocityToCutoff = (double) p[filterVelocity] / 100.0;

            fAttackStep = 1.0 / std::max (1.0, (double) p[fAttack] * 0.001 * sampleRate);
            fDecay = coefficientFor (p[synth::fDecay], sampleRate);
            fSustain = std::clamp ((double) p[synth::fSustain] / 100.0, 0.0, 1.0);
            fRelease = coefficientFor (p[synth::fRelease], sampleRate);

            attackStep = 1.0 / std::max (1.0, (double) p[synth::attack] * 0.001 * sampleRate);
            decayCoefficient = coefficientFor (p[synth::decay], sampleRate);
            sustainLevel = std::clamp ((double) p[synth::sustain] / 100.0, 0.0, 1.0);
            releaseCoefficient = coefficientFor (p[synth::release], sampleRate);
            velocitySensitivity = std::clamp ((double) p[synth::velocity] / 100.0, 0.0, 1.0);

            lfoTarget[0] = choice (lfo1Target, 6);
            lfoTarget[1] = choice (lfo2Target, 6);
            lfoAmount[0] = (double) p[lfo1Amount] / 100.0;
            lfoAmount[1] = (double) p[lfo2Amount] / 100.0;
            lfoFadeSeconds[0] = (double) p[lfo1Fade] / 1000.0;
            lfoFadeSeconds[1] = (double) p[lfo2Fade] / 1000.0;

            wheelTarget = choice (synth::wheelTarget, 3);
            wheelAmount = (double) p[synth::wheelAmount] / 100.0;
            glideCoefficient = p[glide] < 1.0f ? 1.0 : 1.0 - std::exp (-4.6 * controlInterval / ((double) p[glide] * 0.001 * sampleRate));
            voiceMode = choice (synth::voiceMode, 2);
            bendRange = std::clamp ((double) std::lround (p[synth::bendRange]), 1.0, 12.0);

            gain = juce::Decibels::decibelsToGain ((double) p[volume]) * outputScale;
        }
    };

    //==============================================================================
    struct Voice
    {
        bool active = false;
        int note = 60;
        double velocity = 0.8;
        bool fromClip = false, keyDown = false, sustained = false;
        std::int64_t remaining = -1;       // clip notes: samples until they're let go
        std::uint64_t startedAt = 0;

        double pitch = 60.0, targetPitch = 60.0;   // in semitones; glide moves pitch towards the target
        Envelope amp, filter;
        std::array<double, maxUnison> phaseA {}, phaseB {};
        double subPhase = 0.0, age = 0.0;
        std::uint32_t noiseState = 0x9e3779b9u;

        // The filter: up to two stages, for each side.
        std::array<double, 4> ic1 {}, ic2 {};

        // Worked out every controlInterval samples.
        std::array<double, maxUnison> dtA {}, dtB {};
        double dtSub = 0.0, a1 = 1.0, a2 = 0.0, a3 = 0.0, b1 = 1.0, b2 = 0.0, b3 = 0.0;
        double pulseA = 0.5, pulseB = 0.5, mix = 0.5, panLeft = 1.0, panRight = 1.0, tremolo = 1.0;

        void letGo() noexcept
        {
            keyDown = false;
            sustained = false;
            amp.letGo();
            filter.letGo();
        }

        double nextNoise() noexcept
        {
            noiseState ^= noiseState << 13;
            noiseState ^= noiseState >> 17;
            noiseState ^= noiseState << 5;
            return (double) noiseState / 2147483648.0 - 1.0;
        }
    };

    //==============================================================================
    /** A stereo chorus: two short delays whose length moves slowly, a quarter of a cycle apart. */
    class Chorus
    {
    public:
        void prepare (double rate)
        {
            sampleRate = rate;
            size = 1;
            while (size < (int) (0.05 * rate))
                size *= 2;

            left.assign ((size_t) size, 0.0f);
            right.assign ((size_t) size, 0.0f);
            write = 0;
            phase = 0.0;
        }

        void process (float* l, float* r, int numSamples, double amount, double rateHz) noexcept
        {
            if (size <= 1)
                return;

            const auto base = 0.012 * sampleRate, depth = 0.004 * sampleRate;
            const auto step = rateHz / sampleRate;
            const auto wet = 0.7 * amount, dry = 1.0 - 0.35 * amount;
            const auto mask = size - 1;

            for (int i = 0; i < numSamples; ++i)
            {
                left[(size_t) write] = l[i];
                right[(size_t) write] = r[i];

                const auto read = [&] (const std::vector<float>& buffer, double delay)
                {
                    const auto position = (double) write - delay;
                    const auto whole = (int) std::floor (position);
                    const auto frac = (float) (position - whole);
                    const auto a = buffer[(size_t) (whole & mask)], b = buffer[(size_t) ((whole + 1) & mask)];
                    return a + (b - a) * frac;
                };

                const auto wetL = read (left, base + depth * sin2pi (phase));
                const auto wetR = read (right, base + depth * sin2pi (phase + 0.25));
                l[i] = (float) (dry * l[i] + wet * wetL);
                r[i] = (float) (dry * r[i] + wet * wetR);

                write = (write + 1) & mask;
                phase += step;
                if (phase >= 1.0)
                    phase -= 1.0;
            }
        }

    private:
        std::vector<float> left, right;
        int size = 1, write = 0;
        double sampleRate = 48000.0, phase = 0.0;
    };

    //==============================================================================
    class Synth final : public Instrument
    {
    public:
        void prepare (double newSampleRate, int) override
        {
            sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;

            for (auto& v : voices)
                v = {};

            for (int i = 0; i < numVoices; ++i)
                voices[(size_t) i].noiseState = 0x9e3779b9u + (std::uint32_t) i * 7919u;

            chorus.prepare (sampleRate);
            reverb.reset();
            reverb.setSampleRate (sampleRate);
            reverbParameters = {};
            pedal = false;
            bend = wheel = 0.0;
            lfoPhase[0] = lfoPhase[1] = vibratoPhase = 0.0;
            lfoHeld[0] = lfoHeld[1] = 0.0;
            numHeld = 0;
            lastPitch = -1.0;
        }

        int getNumSounding() const noexcept override
        {
            return (int) std::count_if (voices.begin(), voices.end(), [] (const Voice& v) { return v.active; });
        }

        void process (float* left, float* right, int numSamples, const EffectParameterValues& p,
                      const NoteEvent* events, int numEvents) noexcept AETHER_NONBLOCKING override
        {
            settings.update (p, sampleRate);
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
        //==============================================================================
        void handle (const NoteEvent& e) noexcept
        {
            switch (e.type)
            {
                case NoteEvent::Type::noteOn:        noteOn (e.pitch, e.velocity / 127.0, e.fromClip, e.length); break;
                case NoteEvent::Type::noteOff:       noteOff (e.pitch); break;
                case NoteEvent::Type::pitchBend:     bend = std::clamp ((double) e.value, -1.0, 1.0); break;
                case NoteEvent::Type::modWheel:      wheel = std::clamp ((double) e.value, 0.0, 1.0); break;

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

        void start (Voice& v, int note, double velocity, bool fromClip, std::int64_t length, bool retrigger) noexcept
        {
            const auto sounding = v.active && v.amp.stage != Envelope::idle && v.amp.level > 0.001;
            const auto from = settings.glideCoefficient < 1.0 ? (sounding ? v.pitch : lastPitch) : -1.0;

            v.active = true;
            v.note = note;
            v.velocity = velocity;
            v.fromClip = fromClip;
            v.keyDown = true;
            v.sustained = false;
            v.remaining = fromClip ? std::max<std::int64_t> (1, length) : -1;
            v.startedAt = ++clock;
            v.targetPitch = note;
            v.pitch = from >= 0.0 ? from : (double) note;

            if (retrigger || ! sounding)
            {
                v.amp.trigger();
                v.filter.trigger();
            }

            if (! sounding)
            {
                // A fresh start (a stolen voice keeps its level and phases, so it doesn't click).
                v.amp.level = v.filter.level = 0.0;
                v.ic1 = {};
                v.ic2 = {};
                v.subPhase = 0.0;
                v.age = 0.0;

                // With unison, the copies start at different places so they don't all line up.
                for (int j = 0; j < maxUnison; ++j)
                {
                    v.phaseA[(size_t) j] = settings.unison > 1 ? (v.nextNoise() + 1.0) * 0.5 : 0.0;
                    v.phaseB[(size_t) j] = settings.unison > 1 ? (v.nextNoise() + 1.0) * 0.5 : 0.0;
                }
            }

            lastPitch = note;
        }

        void noteOn (int note, double velocity, bool fromClip, std::int64_t length) noexcept
        {
            if (settings.voiceMode != poly)
            {
                // One voice. Keys held down are remembered, so letting go of one returns to the last.
                auto& v = voices[0];

                if (! fromClip)
                    pushHeld (note);

                const auto playing = v.active && v.keyDown;
                start (v, note, velocity, fromClip, length, settings.voiceMode == mono || ! playing);
                return;
            }

            // The same key again: its old voice stops (quickly) and a new one starts.
            for (auto& v : voices)
                if (v.active && v.note == note && v.keyDown && v.fromClip == fromClip)
                    v.letGo();

            start (freeVoice(), note, velocity, fromClip, length, true);
        }

        void noteOff (int note) noexcept
        {
            if (settings.voiceMode != poly)
            {
                removeHeld (note);
                auto& v = voices[0];

                if (! v.active || v.fromClip || v.note != note || ! v.keyDown)
                    return;

                if (numHeld > 0)
                {
                    // Back to the previous key still held (legato: without starting the sound again).
                    start (v, held[(size_t) numHeld - 1], v.velocity, false, 0, settings.voiceMode == mono);
                    return;
                }

                v.keyDown = false;
                if (pedal) v.sustained = true; else v.letGo();
                return;
            }

            for (auto& v : voices)
                if (v.active && ! v.fromClip && v.note == note && v.keyDown)
                {
                    v.keyDown = false;
                    if (pedal) v.sustained = true; else v.letGo();
                }
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

        Voice& freeVoice() noexcept
        {
            // A free voice; otherwise the quietest one fading out; otherwise the oldest.
            for (auto& v : voices)
                if (! v.active)
                    return v;

            Voice* best = nullptr;

            for (auto& v : voices)
                if (v.amp.stage == Envelope::release && (best == nullptr || v.amp.level < best->amp.level))
                    best = &v;

            if (best == nullptr)
                for (auto& v : voices)
                    if (best == nullptr || v.startedAt < best->startedAt)
                        best = &v;

            return *best;
        }

        //==============================================================================
        double lfoValue (int index, int shape) noexcept
        {
            const auto ph = lfoPhase[index];

            switch (shape)
            {
                case lfoTriangle:  return 1.0 - 4.0 * std::abs (ph - 0.5);
                case lfoSaw:       return 1.0 - 2.0 * ph;
                case lfoSquare:    return ph < 0.5 ? 1.0 : -1.0;
                case lfoRandom:    return lfoHeld[index];
                default:           return sin2pi (ph);
            }
        }

        void advanceLfo (int index, double rateHz, int samples) noexcept
        {
            lfoPhase[index] += rateHz * samples / sampleRate;

            if (lfoPhase[index] >= 1.0)
            {
                lfoPhase[index] -= std::floor (lfoPhase[index]);
                randomState = randomState * 1664525u + 1013904223u;   // a new random value each cycle
                lfoHeld[index] = (double) (randomState >> 8) / 8388608.0 - 1.0;
            }
        }

        void render (float* left, float* right, int numSamples, const EffectParameterValues& p) noexcept
        {
            for (int offset = 0; offset < numSamples; offset += subBlock)
            {
                const auto length = std::min (subBlock, numSamples - offset);
                std::fill (dryLeft.begin(), dryLeft.begin() + length, 0.0f);
                std::fill (dryRight.begin(), dryRight.begin() + length, 0.0f);

                // The LFOs and the mod wheel's vibrato, once per slice of the block.
                const auto slices = (length + controlInterval - 1) / controlInterval;

                for (int s = 0; s < slices; ++s)
                {
                    const auto sliceLength = std::min (controlInterval, length - s * controlInterval);
                    lfoSlices[0][(size_t) s] = lfoValue (0, (int) std::lround (p[lfo1Shape]));
                    lfoSlices[1][(size_t) s] = lfoValue (1, (int) std::lround (p[lfo2Shape]));
                    vibratoSlices[(size_t) s] = sin2pi (vibratoPhase);
                    advanceLfo (0, (double) p[lfo1Rate], sliceLength);
                    advanceLfo (1, (double) p[lfo2Rate], sliceLength);
                    vibratoPhase += 5.5 * sliceLength / sampleRate;
                    vibratoPhase -= std::floor (vibratoPhase);
                }

                for (auto& v : voices)
                    if (v.active)
                        renderVoice (v, length);

                // The effects (they keep running while their echoes fade, even with no notes).
                if (p[synth::chorus] > 0.5f)
                    chorus.process (dryLeft.data(), dryRight.data(), length, (double) p[synth::chorus] / 100.0, (double) p[chorusRate]);

                if (p[synth::reverb] > 0.5f)
                {
                    juce::Reverb::Parameters wanted;
                    wanted.roomSize = 0.3f + 0.69f * p[reverbSize] / 100.0f;
                    wanted.damping = 0.45f;
                    wanted.wetLevel = 0.25f * p[synth::reverb] / 100.0f;
                    wanted.dryLevel = 0.5f;   // juce::Reverb doubles it: unchanged
                    wanted.width = 1.0f;

                    if (wanted.roomSize != reverbParameters.roomSize || wanted.wetLevel != reverbParameters.wetLevel)
                    {
                        reverbParameters = wanted;
                        reverb.setParameters (wanted);
                    }

                    reverb.processStereo (dryLeft.data(), dryRight.data(), length);
                }

                const auto g = (float) settings.gain;
                for (int i = 0; i < length; ++i)
                {
                    left[offset + i] += dryLeft[(size_t) i] * g;
                    right[offset + i] += dryRight[(size_t) i] * g;
                }
            }
        }

        /** Pitch, filter and modulation for the next slice of a voice. */
        void updateVoice (Voice& v, int slice) noexcept
        {
            const auto& s = settings;

            if (s.glideCoefficient < 1.0)
                v.pitch += (v.targetPitch - v.pitch) * s.glideCoefficient;
            else
                v.pitch = v.targetPitch;

            double pitchShift = bend * s.bendRange, cutoffOctaves = 0.0, shapeShift = 0.0, mixShift = 0.0, pan = 0.0;
            double tremolo = 1.0;

            for (int i = 0; i < 2; ++i)
            {
                if (s.lfoTarget[i] == targetOff || s.lfoAmount[i] <= 0.0)
                    continue;

                const auto fade = s.lfoFadeSeconds[i] > 0.0 ? std::min (1.0, v.age / s.lfoFadeSeconds[i]) : 1.0;
                const auto value = lfoSlices[(size_t) i][(size_t) slice] * s.lfoAmount[i] * fade;

                switch (s.lfoTarget[i])
                {
                    case targetPitch:   pitchShift += 2.0 * value; break;
                    case targetCutoff:  cutoffOctaves += 3.0 * value; break;
                    case targetVolume:  tremolo *= 1.0 - s.lfoAmount[i] * fade * (0.5 + 0.5 * lfoSlices[(size_t) i][(size_t) slice]); break;
                    case targetPan:     pan += value; break;
                    case targetShape:   shapeShift += 0.45 * value; break;
                    case targetMix:     mixShift += 0.5 * value; break;
                    default:            break;
                }
            }

            const auto w = wheel * s.wheelAmount;
            if (w > 0.0)
            {
                switch (s.wheelTarget)
                {
                    case wheelVibrato:  pitchShift += 0.5 * w * vibratoSlices[(size_t) slice]; break;
                    case wheelCutoff:   cutoffOctaves += 4.0 * w; break;
                    case wheelTremolo:  tremolo *= 1.0 - w * (0.5 + 0.5 * vibratoSlices[(size_t) slice]); break;
                    default:            break;
                }
            }

            // Oscillator steps (the fraction of a cycle per sample).
            const auto hz = 440.0 * std::pow (2.0, (v.pitch + pitchShift - 69.0) / 12.0);
            const auto stepA = hz * s.ratioA / sampleRate, stepB = hz * s.ratioB / sampleRate;

            for (int j = 0; j < s.unison; ++j)
            {
                v.dtA[(size_t) j] = std::min (0.45, stepA * s.unisonRatio[(size_t) j]);
                v.dtB[(size_t) j] = std::min (0.45, stepB * s.unisonRatio[(size_t) j]);
            }

            v.dtSub = std::min (0.45, hz * 0.5 / sampleRate);
            v.pulseA = std::clamp (s.shapeA + shapeShift, 0.05, 0.95);
            v.pulseB = std::clamp (s.shapeB + shapeShift, 0.05, 0.95);
            v.mix = std::clamp (s.mix + mixShift, 0.0, 1.0);
            v.tremolo = tremolo;

            const auto angle = (1.0 + std::clamp (pan, -1.0, 1.0)) * juce::MathConstants<double>::pi * 0.25;
            v.panLeft = std::cos (angle) * juce::MathConstants<double>::sqrt2;
            v.panRight = std::sin (angle) * juce::MathConstants<double>::sqrt2;

            // The filter: its cutoff follows the key, its envelope, the velocity and the LFOs.
            cutoffOctaves += s.keyTrack * (v.pitch - 60.0) / 12.0 + s.envAmount * 6.0 * v.filter.level
                           + s.velocityToCutoff * 2.5 * (v.velocity - 0.7);
            const auto fc = std::clamp (s.cutoff * std::pow (2.0, cutoffOctaves), 20.0, sampleRate * 0.45);
            const auto g = std::tan (juce::MathConstants<double>::pi * fc / sampleRate);

            v.a1 = 1.0 / (1.0 + g * (g + s.k));
            v.a2 = g * v.a1;
            v.a3 = g * v.a2;

            constexpr auto k2 = juce::MathConstants<double>::sqrt2;   // the second stage: no resonance
            v.b1 = 1.0 / (1.0 + g * (g + k2));
            v.b2 = g * v.b1;
            v.b3 = g * v.b2;
        }

        void renderVoice (Voice& v, int length) noexcept
        {
            const auto& s = settings;
            const auto velocityGain = (1.0 - s.velocitySensitivity) + s.velocitySensitivity * v.velocity * std::sqrt (v.velocity);
            const auto driveGain = 1.0 + 4.0 * s.drive;

            for (int i = 0; i < length; ++i)
            {
                if (i % controlInterval == 0)
                {
                    updateVoice (v, i / controlInterval);
                    v.age += (double) std::min (controlInterval, length - i) / sampleRate;
                }

                // Clip notes end by themselves after their length.
                if (v.remaining > 0 && --v.remaining == 0 && v.keyDown)
                    v.letGo();

                const auto level = v.amp.next (s.attackStep, s.decayCoefficient, s.sustainLevel, s.releaseCoefficient);
                v.filter.next (s.fAttackStep, s.fDecay, s.fSustain, s.fRelease);

                if (v.amp.stage == Envelope::idle || (v.amp.stage == Envelope::sustain && s.sustainLevel <= 0.0))
                {
                    v.active = false;
                    return;
                }

                // The oscillators.
                double l = 0.0, r = 0.0;

                for (int j = 0; j < s.unison; ++j)
                {
                    auto& pa = v.phaseA[(size_t) j];
                    auto& pb = v.phaseB[(size_t) j];
                    const auto a = oscillator (s.waveA, pa, v.dtA[(size_t) j], v.pulseA);
                    const auto b = oscillator (s.waveB, pb, v.dtB[(size_t) j], v.pulseB);
                    const auto mixed = a + (b - a) * v.mix;
                    l += mixed * s.gainLeft[(size_t) j];
                    r += mixed * s.gainRight[(size_t) j];

                    pa += v.dtA[(size_t) j];
                    if (pa >= 1.0) pa -= 1.0;
                    pb += v.dtB[(size_t) j];
                    if (pb >= 1.0) pb -= 1.0;
                }

                if (s.sub > 0.0 || s.noise > 0.0)
                {
                    const auto subValue = s.subWave == 0 ? sin2pi (v.subPhase) : oscillator (square, v.subPhase, v.dtSub, 0.5);
                    const auto centre = s.sub * subValue + (s.noise > 0.0 ? s.noise * v.nextNoise() : 0.0);
                    l += centre;
                    r += centre;
                    v.subPhase += v.dtSub;
                    if (v.subPhase >= 1.0) v.subPhase -= 1.0;
                }

                if (s.drive > 0.0)
                {
                    l = softClip (l * driveGain) * (1.0 + s.drive);
                    r = softClip (r * driveGain) * (1.0 + s.drive);
                }

                l = filterSample (v, 0, l);
                r = filterSample (v, 1, r);

                const auto out = level * velocityGain * v.tremolo;
                dryLeft[(size_t) i] += (float) (l * out * v.panLeft);
                dryRight[(size_t) i] += (float) (r * out * v.panRight);
            }
        }

        /** One sample through the filter, for one side (0 = left, 1 = right). */
        double filterSample (Voice& v, int side, double x) noexcept
        {
            const auto stage = [] (double in, double& c1, double& c2, double a1, double a2, double a3, double& band)
            {
                const auto v3 = in - c2;
                const auto v1 = a1 * c1 + a2 * v3;
                const auto v2 = c2 + a2 * c1 + a3 * v3;
                c1 = 2.0 * v1 - c1;
                c2 = 2.0 * v2 - c2;
                band = v1;
                return v2;   // low-pass
            };

            double band = 0.0;
            const auto low = stage (x, v.ic1[(size_t) side], v.ic2[(size_t) side], v.a1, v.a2, v.a3, band);

            switch (settings.filterType)
            {
                case lowPass24:
                {
                    double unused = 0.0;
                    return stage (low, v.ic1[(size_t) side + 2], v.ic2[(size_t) side + 2], v.b1, v.b2, v.b3, unused);
                }
                case highPass:   return x - settings.k * band - low;
                case bandPass:   return settings.k * band;
                default:         return low;
            }
        }

        //==============================================================================
        double sampleRate = 48000.0;
        Settings settings;
        std::array<Voice, numVoices> voices {};
        std::array<int, 16> held {};
        int numHeld = 0;
        bool pedal = false;
        double bend = 0.0, wheel = 0.0, lastPitch = -1.0;
        std::uint64_t clock = 0;

        double lfoPhase[2] {}, lfoHeld[2] {}, vibratoPhase = 0.0;
        std::uint32_t randomState = 12345u;
        std::array<std::array<double, maxSlices>, 2> lfoSlices {};
        std::array<double, maxSlices> vibratoSlices {};
        std::array<float, subBlock> dryLeft {}, dryRight {};

        Chorus chorus;
        juce::Reverb reverb;
        juce::Reverb::Parameters reverbParameters;
    };
}

std::unique_ptr<Instrument> create()
{
    return std::make_unique<Synth>();
}

} // namespace aether::dsp::synth
