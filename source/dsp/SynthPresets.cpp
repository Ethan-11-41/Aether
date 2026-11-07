// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
//
// The Synth's settings and presets. Parameter ids are stored in project files: never rename them.

#include "Synth.h"

#include <initializer_list>

namespace aether::dsp::synth
{

std::vector<EffectParameter> parameters()
{
    const juce::StringArray waves { "Sine", "Triangle", "Saw", "Square" };
    const juce::StringArray lfoShapes { "Sine", "Triangle", "Saw", "Square", "Random" };
    const juce::StringArray lfoTargets { "Off", "Pitch", "Cutoff", "Volume", "Pan", "Shape", "Mix" };

    std::vector<EffectParameter> list {
        { "waveA",   "Wave A",      0.0f, 3.0f, 2.0f, {}, waves, false },
        { "octaveA", "Octave",    -3.0f, 3.0f, 0.0f, "oct", {}, false, true },
        { "semiA",   "Semitones", -12.0f, 12.0f, 0.0f, "st", {}, false, true },
        { "fineA",   "Fine",      -100.0f, 100.0f, 0.0f, "cents", {}, false },
        { "shapeA",  "Shape",     5.0f, 95.0f, 50.0f, "%", {}, false },

        { "waveB",   "Wave B",      0.0f, 3.0f, 2.0f, {}, waves, false },
        { "octaveB", "Octave",    -3.0f, 3.0f, 0.0f, "oct", {}, false, true },
        { "semiB",   "Semitones", -12.0f, 12.0f, 0.0f, "st", {}, false, true },
        { "fineB",   "Fine",      -100.0f, 100.0f, 7.0f, "cents", {}, false },
        { "shapeB",  "Shape",     5.0f, 95.0f, 50.0f, "%", {}, false },

        { "mix",     "Mix A/B",   0.0f, 100.0f, 50.0f, "%", {}, false },
        { "sub",     "Sub",       0.0f, 100.0f, 0.0f, "%", {}, false },
        { "subWave", "Sub wave",  0.0f, 1.0f, 0.0f, {}, { "Sine", "Square" }, false },
        { "noise",   "Noise",     0.0f, 100.0f, 0.0f, "%", {}, false },

        { "unison",  "Unison",    1.0f, 7.0f, 1.0f, "copies", {}, false, true },
        { "detune",  "Detune",    0.0f, 100.0f, 15.0f, "cents", {}, false },
        { "width",   "Width",     0.0f, 100.0f, 50.0f, "%", {}, false },

        { "filterType",     "Filter",      0.0f, 3.0f, 0.0f, {}, { "Low-pass 24", "Low-pass 12", "High-pass", "Band-pass" }, false },
        { "cutoff",         "Cutoff",      20.0f, 20000.0f, 8000.0f, "Hz", {}, true },
        { "resonance",      "Resonance",   0.0f, 100.0f, 10.0f, "%", {}, false },
        { "drive",          "Drive",       0.0f, 100.0f, 0.0f, "%", {}, false },
        { "keyTrack",       "Key track",   0.0f, 100.0f, 50.0f, "%", {}, false },
        { "filterEnv",      "Env amount",  -100.0f, 100.0f, 20.0f, "%", {}, false },
        { "filterVelocity", "Velocity",    0.0f, 100.0f, 30.0f, "%", {}, false },

        { "fAttack",  "Attack",  1.0f, 10000.0f, 2.0f, "ms", {}, true },
        { "fDecay",   "Decay",   1.0f, 10000.0f, 600.0f, "ms", {}, true },
        { "fSustain", "Sustain", 0.0f, 100.0f, 30.0f, "%", {}, false },
        { "fRelease", "Release", 1.0f, 10000.0f, 400.0f, "ms", {}, true },

        { "attack",   "Attack",   1.0f, 10000.0f, 3.0f, "ms", {}, true },
        { "decay",    "Decay",    1.0f, 10000.0f, 800.0f, "ms", {}, true },
        { "sustain",  "Sustain",  0.0f, 100.0f, 70.0f, "%", {}, false },
        { "release",  "Release",  1.0f, 10000.0f, 300.0f, "ms", {}, true },
        { "velocity", "Velocity", 0.0f, 100.0f, 60.0f, "%", {}, false },

        { "lfo1Shape",  "Shape",   0.0f, 4.0f, 0.0f, {}, lfoShapes, false },
        { "lfo1Rate",   "Rate",    0.05f, 20.0f, 5.0f, "Hz", {}, true },
        { "lfo1Target", "Moves",   0.0f, 6.0f, 0.0f, {}, lfoTargets, false },
        { "lfo1Amount", "Amount",  0.0f, 100.0f, 20.0f, "%", {}, false },
        { "lfo1Fade",   "Fade in", 0.0f, 5000.0f, 0.0f, "ms", {}, false },

        { "lfo2Shape",  "Shape",   0.0f, 4.0f, 1.0f, {}, lfoShapes, false },
        { "lfo2Rate",   "Rate",    0.05f, 20.0f, 0.5f, "Hz", {}, true },
        { "lfo2Target", "Moves",   0.0f, 6.0f, 0.0f, {}, lfoTargets, false },
        { "lfo2Amount", "Amount",  0.0f, 100.0f, 20.0f, "%", {}, false },
        { "lfo2Fade",   "Fade in", 0.0f, 5000.0f, 0.0f, "ms", {}, false },

        { "wheelTarget", "Mod wheel",  0.0f, 3.0f, 0.0f, {}, { "Vibrato", "Cutoff", "Tremolo", "Off" }, false },
        { "wheelAmount", "Wheel amount", 0.0f, 100.0f, 50.0f, "%", {}, false },

        { "glide",     "Glide",      0.0f, 2000.0f, 0.0f, "ms", {}, false },
        { "voiceMode", "Voices",     0.0f, 2.0f, 0.0f, {}, { "Poly", "Mono", "Legato" }, false },
        { "bendRange", "Bend range", 1.0f, 12.0f, 2.0f, "st", {}, false, true },

        { "chorus",     "Chorus",      0.0f, 100.0f, 0.0f, "%", {}, false },
        { "chorusRate", "Chorus rate", 0.1f, 5.0f, 0.8f, "Hz", {}, true },
        { "reverb",     "Reverb",      0.0f, 100.0f, 0.0f, "%", {}, false },
        { "reverbSize", "Room size",   0.0f, 100.0f, 60.0f, "%", {}, false },

        { "volume", "Volume", -24.0f, 6.0f, -6.0f, "dB", {}, false },
    };

    jassert ((int) list.size() == numParameters);
    return list;
}

namespace
{
    struct Setting
    {
        const char* id;
        float value;
    };

    using Settings = std::initializer_list<Setting>;

    /** A preset: the default settings, with these changed. */
    EffectPreset make (const std::vector<EffectParameter>& params, const char* name, const char* category, Settings changes)
    {
        EffectPreset preset;
        preset.name = name;
        preset.category = category;

        for (const auto& p : params)
            preset.values.push_back (p.defaultValue);

        for (const auto& [id, value] : changes)
        {
            bool found = false;

            for (size_t i = 0; i < params.size(); ++i)
                if (params[i].id.toString() == id)
                {
                    preset.values[i] = params[i].clamp (value);
                    found = true;
                }

            jassert (found);   // a misspelt setting
            juce::ignoreUnused (found);
        }

        return preset;
    }
}

std::vector<EffectPreset> presets()
{
    const auto p = parameters();
    constexpr float Sine = 0, Triangle = 1, Saw = 2, Square = 3;
    constexpr float LP12 = 1, BP = 3;
    constexpr float Off = 0, Pitch = 1, Cutoff = 2, Shape = 5, Mix = 6;

    return {
        // Keys ---------------------------------------------------------------------------------------
        make (p, "Soft keys", "Keys", {
            { "waveA", Sine }, { "waveB", Triangle }, { "octaveB", 1 }, { "fineB", 0 }, { "mix", 25 },
            { "filterType", LP12 }, { "cutoff", 3500 }, { "resonance", 5 }, { "keyTrack", 70 },
            { "filterEnv", 30 }, { "fAttack", 1 }, { "fDecay", 900 }, { "fSustain", 10 }, { "fRelease", 400 },
            { "filterVelocity", 50 }, { "attack", 2 }, { "decay", 2200 }, { "sustain", 15 }, { "release", 450 },
            { "velocity", 75 }, { "chorus", 30 }, { "chorusRate", 0.6f }, { "reverb", 18 }, { "volume", -8 } }),

        make (p, "Warm keys", "Keys", {
            { "waveA", Triangle }, { "waveB", Square }, { "fineB", 4 }, { "mix", 30 },
            { "cutoff", 2200 }, { "resonance", 10 }, { "filterEnv", 35 }, { "fDecay", 700 }, { "fSustain", 20 },
            { "attack", 3 }, { "decay", 1500 }, { "sustain", 35 }, { "release", 300 }, { "velocity", 60 },
            { "chorus", 35 }, { "reverb", 15 }, { "volume", -8 } }),

        make (p, "Bell keys", "Keys", {
            { "waveA", Sine }, { "waveB", Sine }, { "octaveB", 1 }, { "semiB", 7 }, { "fineB", 3 }, { "mix", 35 },
            { "filterType", LP12 }, { "cutoff", 9000 }, { "filterEnv", 0 }, { "attack", 1 }, { "decay", 2500 },
            { "sustain", 0 }, { "release", 900 }, { "velocity", 70 }, { "chorus", 15 }, { "reverb", 30 },
            { "volume", -8 } }),

        make (p, "Vintage keys", "Keys", {
            { "waveA", Saw }, { "waveB", Square }, { "shapeB", 30 }, { "fineB", 6 }, { "mix", 40 },
            { "cutoff", 1600 }, { "resonance", 20 }, { "filterEnv", 45 }, { "fDecay", 500 }, { "fSustain", 15 },
            { "attack", 4 }, { "decay", 1200 }, { "sustain", 40 }, { "release", 350 }, { "velocity", 55 },
            { "chorus", 55 }, { "chorusRate", 0.5f }, { "reverb", 20 }, { "volume", -9 } }),

        // Organ ---------------------------------------------------------------------------------------
        make (p, "Drawbar organ", "Organ", {
            { "waveA", Sine }, { "waveB", Sine }, { "octaveB", 1 }, { "fineB", 0 }, { "mix", 45 }, { "sub", 60 },
            { "cutoff", 12000 }, { "filterEnv", 0 }, { "filterVelocity", 0 }, { "attack", 3 }, { "decay", 100 },
            { "sustain", 100 }, { "release", 60 }, { "velocity", 0 }, { "chorus", 40 }, { "chorusRate", 5 },
            { "reverb", 15 }, { "volume", -9 } }),

        make (p, "Church organ", "Organ", {
            { "waveA", Square }, { "waveB", Sine }, { "octaveB", 1 }, { "fineB", 0 }, { "mix", 40 }, { "sub", 50 },
            { "filterType", LP12 }, { "cutoff", 4500 }, { "resonance", 0 }, { "keyTrack", 60 }, { "filterEnv", 0 },
            { "filterVelocity", 0 }, { "attack", 40 }, { "decay", 200 }, { "sustain", 100 }, { "release", 500 },
            { "velocity", 0 }, { "chorus", 10 }, { "reverb", 55 }, { "reverbSize", 90 }, { "volume", -11 } }),

        make (p, "Rock organ", "Organ", {
            { "waveA", Square }, { "shapeA", 35 }, { "waveB", Saw }, { "octaveB", 1 }, { "fineB", 0 }, { "mix", 30 },
            { "sub", 40 }, { "subWave", 1 }, { "filterType", LP12 }, { "cutoff", 5000 }, { "drive", 35 },
            { "filterEnv", 0 }, { "filterVelocity", 0 }, { "attack", 2 }, { "sustain", 100 }, { "release", 50 },
            { "velocity", 0 }, { "chorus", 50 }, { "chorusRate", 4.5f }, { "reverb", 12 }, { "volume", -10 } }),

        make (p, "Soft organ", "Organ", {
            { "waveA", Sine }, { "waveB", Triangle }, { "octaveB", 1 }, { "fineB", 0 }, { "mix", 30 }, { "sub", 30 },
            { "cutoff", 6000 }, { "filterEnv", 0 }, { "filterVelocity", 0 }, { "attack", 15 }, { "sustain", 100 },
            { "release", 150 }, { "velocity", 0 }, { "chorus", 25 }, { "chorusRate", 1.2f }, { "reverb", 25 },
            { "volume", -9 } }),

        // Strings -------------------------------------------------------------------------------------
        make (p, "String ensemble", "Strings", {
            { "waveA", Saw }, { "waveB", Saw }, { "fineB", 9 }, { "unison", 4 }, { "detune", 18 }, { "width", 70 },
            { "cutoff", 4500 }, { "resonance", 5 }, { "keyTrack", 40 }, { "filterEnv", 10 }, { "attack", 280 },
            { "decay", 1000 }, { "sustain", 90 }, { "release", 700 }, { "velocity", 30 },
            { "lfo1Target", Pitch }, { "lfo1Rate", 5.2f }, { "lfo1Amount", 8 }, { "lfo1Fade", 900 },
            { "chorus", 55 }, { "chorusRate", 0.7f }, { "reverb", 35 }, { "reverbSize", 75 }, { "volume", -10 } }),

        make (p, "Slow strings", "Strings", {
            { "waveA", Saw }, { "waveB", Saw }, { "fineB", 9 }, { "unison", 5 }, { "detune", 20 }, { "width", 80 },
            { "cutoff", 3200 }, { "resonance", 5 }, { "keyTrack", 40 }, { "filterEnv", 10 }, { "fAttack", 800 },
            { "attack", 900 }, { "decay", 1500 }, { "sustain", 90 }, { "release", 1500 }, { "velocity", 25 },
            { "lfo1Target", Pitch }, { "lfo1Rate", 5 }, { "lfo1Amount", 8 }, { "lfo1Fade", 1200 },
            { "chorus", 50 }, { "chorusRate", 0.6f }, { "reverb", 45 }, { "reverbSize", 80 }, { "volume", -10 } }),

        make (p, "String machine", "Strings", {
            { "waveA", Saw }, { "waveB", Square }, { "fineB", 3 }, { "mix", 30 }, { "filterType", LP12 },
            { "cutoff", 5500 }, { "filterEnv", 0 }, { "attack", 150 }, { "decay", 500 }, { "sustain", 100 },
            { "release", 600 }, { "velocity", 20 }, { "lfo1Target", Pitch }, { "lfo1Rate", 5.5f }, { "lfo1Amount", 4 },
            { "chorus", 85 }, { "chorusRate", 0.9f }, { "reverb", 30 }, { "volume", -9 } }),

        make (p, "Pizzicato", "Strings", {
            { "waveA", Saw }, { "waveB", Saw }, { "fineB", 6 }, { "unison", 2 }, { "detune", 10 },
            { "cutoff", 1800 }, { "filterEnv", 45 }, { "fAttack", 1 }, { "fDecay", 250 }, { "fSustain", 0 },
            { "attack", 2 }, { "decay", 350 }, { "sustain", 0 }, { "release", 250 }, { "velocity", 70 },
            { "reverb", 35 }, { "volume", -8 } }),

        // Pads ----------------------------------------------------------------------------------------
        make (p, "Warm pad", "Pads", {
            { "waveA", Saw }, { "waveB", Triangle }, { "octaveB", -1 }, { "fineB", 0 }, { "mix", 40 },
            { "unison", 5 }, { "detune", 22 }, { "width", 85 }, { "cutoff", 1600 }, { "resonance", 15 },
            { "filterEnv", 25 }, { "fAttack", 900 }, { "fDecay", 2000 }, { "fSustain", 40 }, { "fRelease", 1500 },
            { "attack", 700 }, { "decay", 1500 }, { "sustain", 85 }, { "release", 1800 }, { "velocity", 25 },
            { "lfo1Target", Cutoff }, { "lfo1Rate", 0.25f }, { "lfo1Amount", 20 }, { "chorus", 40 },
            { "reverb", 55 }, { "reverbSize", 85 }, { "volume", -11 } }),

        make (p, "Glass pad", "Pads", {
            { "waveA", Triangle }, { "waveB", Sine }, { "octaveB", 1 }, { "fineB", 0 }, { "mix", 45 },
            { "unison", 3 }, { "detune", 12 }, { "width", 80 }, { "filterType", LP12 }, { "cutoff", 7000 },
            { "resonance", 20 }, { "filterEnv", 0 }, { "attack", 500 }, { "decay", 1500 }, { "sustain", 80 },
            { "release", 2000 }, { "velocity", 25 }, { "lfo1Target", Mix }, { "lfo1Rate", 0.3f }, { "lfo1Amount", 30 },
            { "chorus", 30 }, { "reverb", 65 }, { "reverbSize", 90 }, { "volume", -10 } }),

        make (p, "Dark pad", "Pads", {
            { "waveA", Square }, { "shapeA", 40 }, { "waveB", Saw }, { "octaveB", -1 }, { "fineB", 0 }, { "mix", 35 },
            { "unison", 4 }, { "detune", 15 }, { "width", 75 }, { "cutoff", 700 }, { "resonance", 25 },
            { "filterEnv", 20 }, { "fAttack", 1500 }, { "fDecay", 3000 }, { "fSustain", 50 }, { "fRelease", 2000 },
            { "attack", 1200 }, { "decay", 2000 }, { "sustain", 90 }, { "release", 2500 }, { "velocity", 20 },
            { "lfo1Target", Shape }, { "lfo1Rate", 0.2f }, { "lfo1Amount", 40 }, { "chorus", 20 },
            { "reverb", 60 }, { "reverbSize", 85 }, { "volume", -10 } }),

        make (p, "Choir pad", "Pads", {
            { "waveA", Saw }, { "waveB", Saw }, { "fineB", 5 }, { "unison", 3 }, { "detune", 10 }, { "width", 70 },
            { "filterType", BP }, { "cutoff", 1100 }, { "resonance", 35 }, { "keyTrack", 30 }, { "filterEnv", 0 },
            { "attack", 400 }, { "decay", 1500 }, { "sustain", 90 }, { "release", 1500 }, { "velocity", 25 },
            { "lfo1Target", Pitch }, { "lfo1Rate", 5 }, { "lfo1Amount", 6 }, { "lfo1Fade", 600 },
            { "chorus", 60 }, { "reverb", 55 }, { "reverbSize", 85 }, { "volume", -6 } }),

        make (p, "Evolving pad", "Pads", {
            { "waveA", Saw }, { "waveB", Square }, { "shapeB", 30 }, { "mix", 50 }, { "unison", 5 }, { "detune", 25 },
            { "width", 100 }, { "cutoff", 1200 }, { "resonance", 30 }, { "filterEnv", 15 }, { "attack", 1500 },
            { "decay", 2000 }, { "sustain", 85 }, { "release", 3000 }, { "velocity", 20 },
            { "lfo1Shape", 1 }, { "lfo1Target", Cutoff }, { "lfo1Rate", 0.12f }, { "lfo1Amount", 45 },
            { "lfo2Target", Shape }, { "lfo2Rate", 0.3f }, { "lfo2Amount", 35 }, { "chorus", 25 },
            { "reverb", 70 }, { "reverbSize", 95 }, { "volume", -11 } }),

        // Bass ----------------------------------------------------------------------------------------
        make (p, "Sub bass", "Bass", {
            { "waveA", Sine }, { "mix", 0 }, { "filterType", LP12 }, { "cutoff", 600 }, { "filterEnv", 0 },
            { "attack", 3 }, { "decay", 300 }, { "sustain", 100 }, { "release", 80 }, { "velocity", 30 },
            { "voiceMode", 1 }, { "volume", -6 } }),

        make (p, "Analog bass", "Bass", {
            { "waveA", Saw }, { "waveB", Square }, { "octaveB", -1 }, { "fineB", 0 }, { "mix", 40 },
            { "cutoff", 400 }, { "resonance", 30 }, { "drive", 20 }, { "filterEnv", 55 }, { "fDecay", 280 },
            { "fSustain", 10 }, { "attack", 2 }, { "decay", 400 }, { "sustain", 80 }, { "release", 90 },
            { "velocity", 50 }, { "voiceMode", 2 }, { "glide", 40 }, { "volume", -8 } }),

        // Leads ---------------------------------------------------------------------------------------
        make (p, "Soft lead", "Leads", {
            { "waveA", Triangle }, { "waveB", Saw }, { "fineB", 5 }, { "mix", 30 }, { "cutoff", 3500 },
            { "resonance", 15 }, { "filterEnv", 10 }, { "attack", 15 }, { "sustain", 90 }, { "release", 200 },
            { "lfo1Target", Pitch }, { "lfo1Rate", 5.5f }, { "lfo1Amount", 10 }, { "lfo1Fade", 400 },
            { "voiceMode", 2 }, { "glide", 60 }, { "chorus", 15 }, { "reverb", 25 }, { "volume", -9 } }),

        make (p, "Saw lead", "Leads", {
            { "waveA", Saw }, { "waveB", Saw }, { "fineB", 10 }, { "unison", 3 }, { "detune", 15 }, { "width", 60 },
            { "cutoff", 6000 }, { "resonance", 20 }, { "drive", 20 }, { "filterEnv", 15 }, { "attack", 3 },
            { "sustain", 90 }, { "release", 150 }, { "voiceMode", 1 }, { "glide", 30 }, { "reverb", 20 },
            { "volume", -11 } }),

        // Plucks --------------------------------------------------------------------------------------
        make (p, "Pluck", "Plucks", {
            { "waveA", Saw }, { "waveB", Square }, { "octaveB", 1 }, { "fineB", 0 }, { "mix", 25 },
            { "cutoff", 900 }, { "resonance", 25 }, { "filterEnv", 70 }, { "fAttack", 1 }, { "fDecay", 220 },
            { "fSustain", 0 }, { "attack", 1 }, { "decay", 400 }, { "sustain", 0 }, { "release", 300 },
            { "velocity", 70 }, { "chorus", 15 }, { "reverb", 25 }, { "volume", -8 } }),

        make (p, "Marimba", "Plucks", {
            { "waveA", Sine }, { "waveB", Sine }, { "octaveB", 2 }, { "fineB", 0 }, { "mix", 15 },
            { "filterType", LP12 }, { "cutoff", 5000 }, { "filterEnv", 0 }, { "attack", 1 }, { "decay", 500 },
            { "sustain", 0 }, { "release", 300 }, { "velocity", 70 }, { "reverb", 20 }, { "volume", -6 } }),

        // A starting point ----------------------------------------------------------------------------
        make (p, "Basic saw", "Basic", { { "lfo1Target", Off } }),
    };
}

} // namespace aether::dsp::synth
