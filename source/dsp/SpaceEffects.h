// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <memory>
#include <vector>

#include "Effect.h"

/** Effects that add space (Milestone 13; the type ids are those of the temporary versions from
    Milestone 4, so older projects keep their settings). */
namespace aether::dsp
{

/** Reverb: the sound of a room, from a small room to a big hall. A feedback delay network (eight
    delay lines mixed into each other) with diffusion, gentle modulation, damping, pre-delay and a
    low cut on the reverb only. */
namespace reverb
{
    inline const juce::String typeId { "reverb" };
    enum Parameter : int { size, damping, width, mix, predelay, lowCut, numParameters };

    std::vector<EffectParameter> parameters();
    std::vector<EffectPreset> presets();

    /** How long the reverb takes to die away by 60 dB, in seconds: its picture. */
    double decaySeconds (const EffectParameterValues& values) noexcept;

    std::unique_ptr<Effect> create();
}

/** Delay: echoes in time with the song's tempo. */
namespace echo
{
    inline const juce::String typeId { "delay" };
    enum Parameter : int { time, feedback, tone, pingPong, mix, lowCut, numParameters };

    std::vector<EffectParameter> parameters();
    std::vector<EffectPreset> presets();
    std::unique_ptr<Effect> create();
}

} // namespace aether::dsp
