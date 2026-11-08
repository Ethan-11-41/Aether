// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <array>
#include <memory>
#include <vector>

#include "Biquad.h"
#include "Effect.h"

/** The EQ (Milestone 13): turns frequency ranges up or down.

    Seven bands, left to right on its picture: a low cut (takes away rumble below a frequency), a
    low shelf (more or less bass), three bell bands (more or less around a frequency; Q is how
    narrow), a high shelf (more or less treble) and a high cut, then the output level. */
namespace aether::dsp::eq
{

inline const juce::String typeId { "eq" };

enum Parameter : int
{
    lowCutOn, lowCut, lowShelfFreq, lowShelfGain,
    band1Freq, band1Gain, band1Q, band2Freq, band2Gain, band2Q, band3Freq, band3Gain, band3Q,
    highShelfFreq, highShelfGain, highCutOn, highCut, output,
    numParameters
};

constexpr int numFilters = 7;

std::vector<EffectParameter> parameters();
std::vector<EffectPreset> presets();

/** The seven filters for a set of settings (those switched off or at 0 dB are flat). */
std::array<BiquadCoefficients, numFilters> design (const EffectParameterValues& values, double sampleRate);

/** The EQ's gain at a frequency, in dB, output level included: its picture. */
double responseDb (const EffectParameterValues& values, double hz, double sampleRate);

std::unique_ptr<Effect> create();

} // namespace aether::dsp::eq
