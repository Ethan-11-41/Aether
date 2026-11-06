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

/** The most parameters any built-in effect or instrument has (the engine keeps them in a
    fixed-size array). */
constexpr int maxEffectParameters = 16;
using EffectParameterValues = std::array<float, maxEffectParameters>;

/** A named set of settings, e.g. the synth's "Electric piano". */
struct EffectPreset
{
    juce::String name;
    std::vector<float> values;    // in the order of the type's parameters
};

/** A kind of effect (or instrument) Aether has built in. */
struct EffectType
{
    juce::String id;              // stored in project files: never rename
    juce::String name;
    juce::String description;
    std::vector<EffectParameter> parameters;
    bool isInstrument = false;    // plays notes (on a MIDI track) rather than changing sound
    std::vector<EffectPreset> presets;
};

/** Every built-in effect and instrument. Milestone 4 has two simple temporary effects (Reverb
    and Delay), the full set arrives in M13; Milestone 6 adds the starter synth ("synth"). */
const std::vector<EffectType>& effectTypes();

/** The instrument new MIDI tracks get, and its starting preset. */
inline const juce::String defaultInstrument { "synth" };
inline const juce::String defaultInstrumentPreset { "Electric piano" };

/** The effect type with this id, or null. */
const EffectType* findEffectType (const juce::String& id);

namespace delay
{
    /** The Delay's "Time" choices, as lengths in quarter notes. (A fixed table, so the audio
        thread can read it without anything being set up the first time.) */
    inline constexpr std::array<double, 11> timeChoicesInQuarters { 0.125, 1.0 / 6.0, 0.25, 1.0 / 3.0, 0.375, 0.5,
                                                                    2.0 / 3.0, 0.75, 1.0, 1.5, 2.0 };
}

} // namespace aether::dsp
