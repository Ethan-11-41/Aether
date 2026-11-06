// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "EffectTypes.h"

#include <cmath>

namespace aether::dsp
{

float EffectParameter::clamp (float value) const noexcept
{
    value = juce::jlimit (minimum, maximum, value);
    return isChoice() ? std::round (value) : value;
}

juce::String EffectParameter::format (float value) const
{
    if (isChoice())
        return choices[juce::jlimit (0, choices.size() - 1, (int) std::lround (value))];

    if (unit == "%")
        return juce::String (juce::roundToInt (value)) + " %";

    if (unit == "Hz")
        return value >= 1000.0f ? juce::String (value / 1000.0f, 1) + " kHz" : juce::String (juce::roundToInt (value)) + " Hz";

    return juce::String (value, 1) + (unit.isNotEmpty() ? " " + unit : juce::String());
}

namespace delay
{
    const std::vector<double>& timeChoicesInQuarters()
    {
        static const std::vector<double> lengths { 0.125, 1.0 / 6.0, 0.25, 1.0 / 3.0, 0.375, 0.5, 2.0 / 3.0, 0.75, 1.0, 1.5, 2.0 };
        return lengths;
    }
}

const std::vector<EffectType>& effectTypes()
{
    static const std::vector<EffectType> types = []
    {
        std::vector<EffectType> list;

        EffectType reverb;
        reverb.id = "reverb";
        reverb.name = "Reverb";
        reverb.description = "Adds the sound of a room. Temporary simple version: a full reverb comes in M13.";
        reverb.parameters = {
            { "size",    "Size",    0.0f, 100.0f, 50.0f,  "%", {}, false },
            { "damping", "Damping", 0.0f, 100.0f, 50.0f,  "%", {}, false },
            { "width",   "Width",   0.0f, 100.0f, 100.0f, "%", {}, false },
            { "mix",     "Mix",     0.0f, 100.0f, 30.0f,  "%", {}, false },
        };
        list.push_back (reverb);

        EffectType echo;
        echo.id = "delay";
        echo.name = "Delay";
        echo.description = "Echoes in time with the tempo. Temporary simple version: a full delay comes in M13.";
        echo.parameters = {
            { "time",     "Time",      0.0f, 10.0f, 5.0f, {},
              { "1/32", "1/16 T", "1/16", "1/8 T", "1/16 D", "1/8", "1/4 T", "1/8 D", "1/4", "1/4 D", "1/2" }, false },
            { "feedback", "Feedback",  0.0f, 90.0f, 35.0f,     "%",  {}, false },
            { "tone",     "Tone",      500.0f, 20000.0f, 8000.0f, "Hz", {}, true },
            { "pingPong", "Ping-pong", 0.0f, 1.0f, 0.0f, {}, { "Off", "On" }, false },
            { "mix",      "Mix",       0.0f, 100.0f, 25.0f,    "%",  {}, false },
        };
        list.push_back (echo);

        return list;
    }();

    return types;
}

const EffectType* findEffectType (const juce::String& id)
{
    for (const auto& type : effectTypes())
        if (type.id == id)
            return &type;

    return nullptr;
}

} // namespace aether::dsp
