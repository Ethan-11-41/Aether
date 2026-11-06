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

    if (unit == "ms")
        return value >= 1000.0f ? juce::String (value / 1000.0f, 2) + " s" : juce::String (juce::roundToInt (value)) + " ms";

    if (unit == "cents" || unit == "%")
        return juce::String (juce::roundToInt (value)) + " " + unit;

    if (unit == "dB")
        return (value > 0.0f ? "+" : "") + juce::String (value, 1) + " dB";

    return juce::String (value, 1) + (unit.isNotEmpty() ? " " + unit : juce::String());
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

        EffectType synth;
        synth.id = "synth";
        synth.name = "Starter synth";
        synth.description = "A simple synthesizer for MIDI tracks. A fuller synth comes in M10.";
        synth.isInstrument = true;
        synth.parameters = {
            { "tone",      "Tone",       0.0f, 4.0f, 4.0f, {},
              { "Soft (sine)", "Warm (triangle)", "Bright (saw)", "Hollow (square)", "Electric piano" }, false },
            { "detune",    "Detune",     0.0f, 50.0f, 0.0f,        "cents", {}, false },
            { "sub",       "Sub",        0.0f, 100.0f, 0.0f,       "%",     {}, false },
            { "cutoff",    "Cutoff",     80.0f, 18000.0f, 9000.0f, "Hz",    {}, true },
            { "resonance", "Resonance",  0.0f, 100.0f, 0.0f,       "%",     {}, false },
            { "envelope",  "Filter env", 0.0f, 100.0f, 20.0f,      "%",     {}, false },
            { "attack",    "Attack",     1.0f, 3000.0f, 2.0f,      "ms",    {}, true },
            { "decay",     "Decay",      10.0f, 5000.0f, 1800.0f,  "ms",    {}, true },
            { "sustain",   "Sustain",    0.0f, 100.0f, 25.0f,      "%",     {}, false },
            { "release",   "Release",    10.0f, 5000.0f, 400.0f,   "ms",    {}, true },
            { "touch",     "Touch",      0.0f, 100.0f, 80.0f,      "%",     {}, false },
            { "volume",    "Volume",     -24.0f, 6.0f, -6.0f,      "dB",    {}, false },
        };
        //                                tone detune sub cutoff  res  env attack decay sustain release touch volume
        synth.presets = {
            { "Electric piano", {         4,   0,     0,  9000,   0,   20,   2,    1800,  25,     400,    80,   -6 } },
            { "Soft pad",       {         2,   12,    20, 1800,   15,  30,   600,  1500,  80,     1500,   30,   -9 } },
            { "Bass",           {         2,   0,     60, 700,    30,  50,   2,    300,   60,     120,    60,   -6 } },
            { "Lead",           {         3,   8,     0,  4000,   25,  25,   5,    400,   80,     200,    40,   -9 } },
            { "Organ",          {         1,   0,     50, 8000,   0,   0,    5,    100,   100,    60,     0,    -9 } },
            { "Pluck",          {         2,   0,     0,  600,    20,  70,   1,    350,   0,      250,    70,   -6 } },
        };
        list.push_back (synth);

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
