// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <array>
#include <vector>

#include <juce_core/juce_core.h>

namespace aether::dsp
{

/** One setting of an effect, e.g. a reverb's "Size". */
struct EffectParameter
{
    juce::Identifier id;          // stored in project files: never rename
    juce::String name;            // shown in the interface
    float minimum = 0.0f, maximum = 1.0f, defaultValue = 0.0f;
    juce::String unit;            // "%", "Hz", ...
    juce::StringArray choices;    // if not empty, the value is an index into these
    bool logarithmic = false;     // knob travel is logarithmic (frequencies)

    bool isChoice() const noexcept   { return ! choices.isEmpty(); }
    float clamp (float value) const noexcept;
    juce::String format (float value) const;
};

/** A kind of effect Aether has built in. */
struct EffectType
{
    juce::String id;              // stored in project files: never rename
    juce::String name;
    juce::String description;
    std::vector<EffectParameter> parameters;
};

/** The most parameters any built-in effect has (the engine keeps them in a fixed-size array). */
constexpr int maxEffectParameters = 8;
using EffectParameterValues = std::array<float, maxEffectParameters>;

/** Every built-in effect. Milestone 4 has two simple temporary ones (Reverb and Delay); the full
    set of effects arrives in M13. */
const std::vector<EffectType>& effectTypes();

/** The effect type with this id, or null. */
const EffectType* findEffectType (const juce::String& id);

namespace delay
{
    /** The Delay's "Time" choices, as lengths in quarter notes. */
    const std::vector<double>& timeChoicesInQuarters();
}

} // namespace aether::dsp
