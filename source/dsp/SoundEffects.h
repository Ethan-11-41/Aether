// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>

/** Sound effects for arrangements (Milestone 16): risers, reverse swells, downlifters, impacts,
    booms and whooshes, made by Aether's own code (so they're free to use in anything).

    Build-ups (risers, reverse swells) are made to any length, so they can last exactly as many
    bars as wanted at the song's tempo and reach their peak on a bar line. The others are hits
    (impact, boom, whoosh: the drum sounds of the same names) or fall away from a bar line
    (downlifter). Everything is stereo and the same every time (seeded noise).
*/
namespace aether::dsp::soundfx
{

struct Info
{
    const char* id;                 // saved in file names, e.g. "riser-noise"
    const char* name;               // shown in menus
    bool buildsUp = false;          // ends at its bar line (risers) rather than starting there
    bool hasLength = false;         // made to a number of bars (otherwise its own length)
    const char* description = "";
};

/** Every sound effect, in menu order. */
const std::vector<Info>& all();
const Info* find (const juce::String& id);

/** Renders an effect (stereo). `seconds` is its length for effects made to a length (ignored
    for the others). Peaks at about -1 dB; starts and ends without a click. Empty if unknown. */
juce::AudioBuffer<float> render (const juce::String& id, double seconds, double sampleRate);

} // namespace aether::dsp::soundfx
