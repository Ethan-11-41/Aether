// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "EffectTypes.h"

#include "ColourEffects.h"
#include "Dynamics.h"
#include "Eq.h"
#include "SpaceEffects.h"
#include "DrumKit.h"
#include "Sampler.h"
#include "Synth.h"

#include <cmath>

namespace aether::dsp
{

float EffectParameter::clamp (float value) const noexcept
{
    value = juce::jlimit (minimum, maximum, value);
    return isChoice() || wholeNumbers ? std::round (value) : value;
}

juce::String EffectParameter::format (float value) const
{
    if (isChoice())
        return choices[juce::jlimit (0, choices.size() - 1, (int) std::lround (value))];

    if (unit == "%")
        return juce::String (juce::roundToInt (value)) + " %";

    if (unit == "Hz")
        return value >= 1000.0f ? juce::String (value / 1000.0f, 1) + " kHz"
             : value < 10.0f    ? juce::String (value, 2) + " Hz"
             : value < 100.0f   ? juce::String (value, 1) + " Hz"
                                : juce::String (juce::roundToInt (value)) + " Hz";

    if (unit == "copies")
    {
        const auto whole = juce::roundToInt (value);
        return whole <= 1 ? juce::String ("1 (off)") : juce::String (whole) + " copies";
    }

        if (unit == "pan")
    {
        const auto whole = juce::roundToInt (value);
        return whole == 0 ? juce::String ("Centre") : (whole < 0 ? "L " : "R ") + juce::String (std::abs (whole));
    }

        if (unit == "oct" || unit == "st")
    {
        const auto whole = juce::roundToInt (value);
        return (whole > 0 ? "+" : "") + juce::String (whole) + (unit == "oct" ? " oct" : " st");
    }

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

        // The effects (Milestone 13), in the order a chain usually has them.
        auto effect = [&list] (const juce::String& id, const juce::String& name, const juce::String& description,
                               std::vector<EffectParameter> parameters, std::vector<EffectPreset> presets)
        {
            EffectType t;
            t.id = id;
            t.name = name;
            t.description = description;
            t.parameters = std::move (parameters);
            t.presets = std::move (presets);
            list.push_back (std::move (t));
        };

        effect (eq::typeId, "EQ", "Turns frequency ranges up or down: less mud, more clarity, more or less bass and treble.",
                eq::parameters(), eq::presets());
        effect (compressor::typeId, "Compressor", "Turns the loud parts down so it sits evenly in the mix, then brings it all back up.",
                compressor::parameters(), compressor::presets());
        effect (deesser::typeId, "De-esser", "Tames harsh \"s\" and \"t\" sounds in a voice.",
                deesser::parameters(), deesser::presets());
        effect (gate::typeId, "Gate", "Silences what's quieter than the threshold: room noise and breaths between phrases.",
                gate::parameters(), gate::presets());
        effect (saturation::typeId, "Saturation", "Warmth and grit, from a gentle tape glow to fuzz.",
                saturation::parameters(), saturation::presets());
        effect (filter::typeId, "Filter", "Makes it darker or thinner, or sweeps by itself for movement.",
                filter::parameters(), filter::presets());
        effect (chorus::typeId, "Chorus", "A wavering copy mixed in: wider, fuller, shimmering.",
                chorus::parameters(), chorus::presets());
        effect (echo::typeId, "Delay", "Echoes in time with the tempo.", echo::parameters(), echo::presets());
        effect (reverb::typeId, "Reverb", "The sound of a room, from a small room to a big hall.", reverb::parameters(), reverb::presets());
        effect (limiter::typeId, "Limiter", "A ceiling nothing goes above: makes the whole song louder without clipping. Last in the master.",
                limiter::parameters(), limiter::presets());

        EffectType synth;
        synth.id = "synth";
        synth.name = "Starter synth";
        synth.description = "The simple synthesizer from Aether 0.7 (still used by older projects). The Synth can do much more.";
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
            { "Electric piano", {         4,   0,     0,  9000,   0,   20,   2,    1800,  25,     400,    80,   -6 }, "Starter" },
            { "Soft pad",       {         2,   12,    20, 1800,   15,  30,   600,  1500,  80,     1500,   30,   -9 }, "Starter" },
            { "Bass",           {         2,   0,     60, 700,    30,  50,   2,    300,   60,     120,    60,   -6 }, "Starter" },
            { "Lead",           {         3,   8,     0,  4000,   25,  25,   5,    400,   80,     200,    40,   -9 }, "Starter" },
            { "Organ",          {         1,   0,     50, 8000,   0,   0,    5,    100,   100,    60,     0,    -9 }, "Starter" },
            { "Pluck",          {         2,   0,     0,  600,    20,  70,   1,    350,   0,      250,    70,   -6 }, "Starter" },
        };
        list.push_back (synth);

        EffectType subtractive;
        subtractive.id = synth::typeId;
        subtractive.name = "Synth";
        subtractive.description = "A synthesizer for pads, strings, keys, organ, bass and leads: two oscillators with "
                                  "unison, a filter, envelopes, LFOs, chorus and reverb.";
        subtractive.isInstrument = true;
        subtractive.parameters = synth::parameters();
        subtractive.presets = synth::presets();
        list.push_back (subtractive);

        EffectType kit;
        kit.id = drumkit::typeId;
        kit.name = "Drum kit";
        kit.description = "16 pads of drum sounds (Aether's own, or your samples), each with its tune, volume, pan, "
                          "attack, length and choke group.";
        kit.isInstrument = true;
        kit.parameters = drumkit::parameters();
        kit.presets = drumkit::presets();
        list.push_back (kit);

        EffectType sampler;
        sampler.id = sampler::typeId;
        sampler.name = "Sampler";
        sampler.description = "Plays recorded instruments (pianos, strings, woodwinds, brass, mallets...) or your own "
                              "sounds across the keyboard, with a volume envelope, tone, tuning and slides.";
        sampler.isInstrument = true;
        sampler.parameters = sampler::parameters();
        list.push_back (sampler);

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
