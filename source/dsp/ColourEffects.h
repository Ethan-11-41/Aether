// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <memory>
#include <vector>

#include "Effect.h"

/** Effects that colour the sound (Milestone 13). */
namespace aether::dsp
{

/** Chorus: a slightly delayed, gently wavering copy mixed in, for a wider, fuller, shimmering
    sound (pads, pianos, backing vocals). */
namespace chorus
{
    inline const juce::String typeId { "chorus" };
    enum Parameter : int { rate, depth, delay, width, mix, numParameters };

    std::vector<EffectParameter> parameters();
    std::vector<EffectPreset> presets();
    std::unique_ptr<Effect> create();
}

/** Saturation: warmth and grit, from a gentle tape glow to fuzz. */
namespace saturation
{
    inline const juce::String typeId { "saturation" };
    enum Parameter : int { drive, type, tone, mix, output, numParameters };
    enum Type : int { tape, tube, fuzz, hardClip };

    std::vector<EffectParameter> parameters();
    std::vector<EffectPreset> presets();

    /** What a sample becomes (drive and type included, before tone, mix and output): its picture. */
    float shape (const EffectParameterValues& values, float input) noexcept;

    std::unique_ptr<Effect> create();
}

/** Filter: low-pass (darker), high-pass (thinner), band-pass or notch, which can sweep by itself
    (an LFO) for movement. */
namespace filter
{
    inline const juce::String typeId { "filter" };
    enum Parameter : int { type, cutoff, resonance, lfoRate, lfoDepth, mix, numParameters };
    enum Type : int { lowPass, highPass, bandPass, notch };

    std::vector<EffectParameter> parameters();
    std::vector<EffectPreset> presets();

    /** The filter's gain at a frequency for a cutoff, in dB (the LFO aside): its picture. */
    double responseDb (const EffectParameterValues& values, double cutoffHz, double hz, double sampleRate) noexcept;

    std::unique_ptr<Effect> create();
}

} // namespace aether::dsp
