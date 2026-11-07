// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <cmath>
#include <memory>
#include <vector>

#include "EffectTypes.h"
#include "FastMath.h"
#include "Instrument.h"

/** The Synth (Milestone 10): a polyphonic subtractive synthesizer.

    Two oscillators (sine, triangle, saw, square/pulse) with unison (up to seven slightly detuned
    copies, spread across the stereo field), a sub oscillator and noise; a filter (low-pass 24 or
    12 dB, high-pass, band-pass) with its own envelope, key tracking and drive; an amplitude
    envelope; two LFOs that can move pitch, cutoff, volume, pan, pulse width or the oscillator
    mix; the mod wheel; glide and mono/legato modes; and a chorus and reverb on its output.

    Its settings are stored like any instrument's (an Instrument node in the track, see
    EffectTypes.h); the indices below are their order in parameters(). */
namespace aether::dsp::synth
{

inline const juce::String typeId { "subtractive" };

enum Parameter : int
{
    waveA, octaveA, semiA, fineA, shapeA,
    waveB, octaveB, semiB, fineB, shapeB,
    mix, sub, subWave, noise,
    unison, detune, width,
    filterType, cutoff, resonance, drive, keyTrack, filterEnv, filterVelocity,
    fAttack, fDecay, fSustain, fRelease,
    attack, decay, sustain, release, velocity,
    lfo1Shape, lfo1Rate, lfo1Target, lfo1Amount, lfo1Fade,
    lfo2Shape, lfo2Rate, lfo2Target, lfo2Amount, lfo2Fade,
    wheelTarget, wheelAmount,
    glide, voiceMode, bendRange,
    chorus, chorusRate, reverb, reverbSize,
    volume,
    numParameters
};

static_assert (numParameters <= maxEffectParameters);

enum Wave { sine, triangle, saw, square };
enum FilterType { lowPass24, lowPass12, highPass, bandPass };
enum LfoShape { lfoSine, lfoTriangle, lfoSaw, lfoSquare, lfoRandom };
enum LfoTarget { targetOff, targetPitch, targetCutoff, targetVolume, targetPan, targetShape, targetMix };
enum WheelTarget { wheelVibrato, wheelCutoff, wheelTremolo, wheelOff };
enum VoiceMode { poly, mono, legato };

constexpr int maxUnison = 7;

/** The Synth's settings, in Parameter order. */
std::vector<EffectParameter> parameters();

/** Its presets, each with a category ("Pads", "Strings", "Keys", "Organ", "Bass", "Leads",
    "Plucks", "Basic"). */
std::vector<EffectPreset> presets();

/** Makes a Synth (it still needs prepare()). */
std::unique_ptr<Instrument> create();

//==============================================================================
// The shapes, as drawn in the synth's window (the sound uses the same maths, with the corners
// smoothed so high notes don't alias).

/** One cycle of a wave, -1..1. `shape` (0.05..0.95): a square's pulse width, a triangle's tilt. */
inline float waveValue (int wave, double phase, double shape) noexcept
{
    phase -= std::floor (phase);

    switch (wave)
    {
        case triangle:
        {
            const auto peak = std::clamp (shape, 0.05, 0.95);
            return (float) (phase < peak ? -1.0 + 2.0 * phase / peak : 1.0 - 2.0 * (phase - peak) / (1.0 - peak));
        }
        case saw:     return (float) (2.0 * phase - 1.0);
        case square:  return phase < std::clamp (shape, 0.05, 0.95) ? 1.0f : -1.0f;
        default:      return (float) sin2pi (phase);
    }
}

/** How a filter type changes the level of a frequency, in dB (0 = unchanged). */
inline double filterGainDb (int type, double cutoffHz, double resonance01, double frequencyHz) noexcept
{
    const auto w = frequencyHz / std::max (1.0, cutoffHz);
    const auto k = 2.0 - 1.9 * std::clamp (resonance01, 0.0, 1.0);
    const auto stage = [w] (double kk) { return (1.0 - w * w) * (1.0 - w * w) + (kk * w) * (kk * w); };

    double power = 1.0;

    switch (type)
    {
        case lowPass24:  power = 1.0 / (stage (k) * stage (std::sqrt (2.0))); break;
        case lowPass12:  power = 1.0 / stage (k); break;
        case highPass:   power = (w * w * w * w) / stage (k); break;
        case bandPass:   power = (k * w) * (k * w) / stage (k); break;
        default:         break;
    }

    return 10.0 * std::log10 (std::max (1.0e-12, power));
}

} // namespace aether::dsp::synth
