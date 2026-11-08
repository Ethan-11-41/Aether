// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <initializer_list>
#include <vector>

#include "EffectTypes.h"

namespace aether::dsp
{

/** One setting in a preset: its id and value. (A plain struct rather than a std::pair, so whole
    numbers like { "drive", 6 } turn into floats without a compiler warning.) */
struct PresetSetting
{
    const char* id;
    float value;
};

/** A preset written as "these settings, everything else at its default". */
inline EffectPreset makePreset (const std::vector<EffectParameter>& parameters, const juce::String& name, const juce::String& category,
                                std::initializer_list<PresetSetting> settings)
{
    EffectPreset preset;
    preset.name = name;
    preset.category = category;

    for (const auto& p : parameters)
        preset.values.push_back (p.defaultValue);

    for (const auto& [id, value] : settings)
    {
        bool found = false;
        for (size_t i = 0; i < parameters.size(); ++i)
            if (parameters[i].id.toString() == id)
            {
                preset.values[i] = parameters[i].clamp (value);
                found = true;
            }
        jassert (found);   // a typo in a preset
        juce::ignoreUnused (found);
    }

    return preset;
}

} // namespace aether::dsp
