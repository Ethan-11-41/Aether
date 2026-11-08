// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <memory>
#include <vector>

#include "Effect.h"

/** The effects that change how loud things are as they play (Milestone 13). */
namespace aether::dsp
{

/** The compressor: turns loud parts down (above the threshold, by the ratio), so a vocal or an
    instrument sits evenly in the mix; makeup gain brings the whole back up. */
namespace compressor
{
    inline const juce::String typeId { "compressor" };
    enum Parameter : int { threshold, ratio, attack, release, knee, makeup, mix, numParameters };

    std::vector<EffectParameter> parameters();
    std::vector<EffectPreset> presets();

    /** The level that comes out for a steady level going in (both dB), makeup included: its picture. */
    double outputDb (const EffectParameterValues& values, double inputDb) noexcept;

    std::unique_ptr<Effect> create();
}

/** The limiter: a ceiling nothing goes above, for the end of the master chain. It looks 1.5 ms
    ahead so it can turn down smoothly before a peak arrives (so the channel is 1.5 ms late). */
namespace limiter
{
    inline const juce::String typeId { "limiter" };
    enum Parameter : int { gain, ceiling, release, numParameters };

    std::vector<EffectParameter> parameters();
    std::vector<EffectPreset> presets();
    std::unique_ptr<Effect> create();
}

/** The gate: silences (or turns down) what's quieter than the threshold, e.g. room noise and
    breaths between sung phrases. */
namespace gate
{
    inline const juce::String typeId { "gate" };
    enum Parameter : int { threshold, range, attack, hold, release, numParameters };

    std::vector<EffectParameter> parameters();
    std::vector<EffectPreset> presets();

    /** The level that comes out for a steady level going in (both dB): its picture. */
    double outputDb (const EffectParameterValues& values, double inputDb) noexcept;

    std::unique_ptr<Effect> create();
}

/** The de-esser: turns down harsh "s" and "t" sounds in a voice, only in the high frequencies
    where they are, and only when they're there. */
namespace deesser
{
    inline const juce::String typeId { "deesser" };
    enum Parameter : int { frequency, threshold, amount, numParameters };

    std::vector<EffectParameter> parameters();
    std::vector<EffectPreset> presets();
    std::unique_ptr<Effect> create();
}

} // namespace aether::dsp
