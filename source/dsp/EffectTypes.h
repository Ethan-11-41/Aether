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
    bool wholeNumbers = false;    // only whole values (octaves, semitones, counts)

    bool isChoice() const noexcept   { return ! choices.isEmpty(); }
    float clamp (float value) const noexcept;
    juce::String format (float value) const;
};

/** The most parameters any built-in effect or instrument has (the engine keeps them in a
    fixed-size array). */
constexpr int maxEffectParameters = 128;
using EffectParameterValues = std::array<float, maxEffectParameters>;

/** A named set of settings, e.g. the synth's "Electric piano". */
struct EffectPreset
{
    juce::String name;
    std::vector<float> values;    // in the order of the type's parameters
    juce::String category;        // e.g. "Pads" (empty: none)
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

/** Every built-in effect and instrument: the effects in the order the mixer lists them (EQ,
    Compressor, De-esser, Gate, Saturation, Filter, Chorus, Delay, Reverb, Limiter), then the
    instruments (the starter synth, the Synth, the Drum kit and the Sampler). */
const std::vector<EffectType>& effectTypes();

/** The instrument new MIDI tracks get (the Synth, since 0.11.0), and its starting preset. Projects
    made earlier keep the starter synth ("synth") on their tracks. */
inline const juce::String defaultInstrument { "subtractive" };
inline const juce::String defaultInstrumentPreset { "Soft keys" };

/** The effect type with this id, or null. */
const EffectType* findEffectType (const juce::String& id);

/** A ready-made set of effects for a common job, e.g. "Vocal: clear and up front": effects, each
    with one of its presets, in the order the sound goes through them. */
struct EffectChain
{
    struct Step
    {
        juce::String typeId;      // e.g. "eq"
        juce::String presetName;  // one of that effect's presets
    };

    juce::String name;
    juce::String category;        // "Vocals", "Instruments" or "Master" (the whole song)
    juce::String description;
    std::vector<Step> steps;
};

/** The built-in chains, by category (vocals, instruments, the whole song). */
const std::vector<EffectChain>& effectChains();

/** The chain with this name, or null. */
const EffectChain* findEffectChain (const juce::String& name);

/** The index of a type's preset with this name, or -1. */
int findPresetIndex (const EffectType& type, const juce::String& presetName);

namespace delay
{
    /** The Delay's "Time" choices, as lengths in quarter notes. (A fixed table, so the audio
        thread can read it without anything being set up the first time.) */
    inline constexpr std::array<double, 11> timeChoicesInQuarters { 0.125, 1.0 / 6.0, 0.25, 1.0 / 3.0, 0.375, 0.5,
                                                                    2.0 / 3.0, 0.75, 1.0, 1.5, 2.0 };
}

} // namespace aether::dsp
