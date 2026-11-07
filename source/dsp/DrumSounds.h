// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <array>
#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>

/** Aether's built-in drum sounds (Milestone 11).

    Every sound is made by Aether's own code (synthesised: no recordings), so it belongs to the
    project and can be shipped freely. They're rendered when needed, at the engine's sample rate,
    always identically (no randomness from one run to the next). */
namespace aether::dsp::drums
{

struct SoundInfo
{
    const char* id;      // stored in project files: never rename
    const char* name;    // shown on the pads
    const char* group;   // "Kicks", "Snares"... for the sound menu
};

/** Every built-in sound, grouped. */
const std::vector<SoundInfo>& builtInSounds();

/** The sound with this id, or null. */
const SoundInfo* findSound (const juce::String& id);

/** Renders a built-in sound (mono, -1..1). Empty if the id is unknown. */
juce::AudioBuffer<float> render (const juce::String& id, double sampleRate);

//==============================================================================
/** The 16 pads play these MIDI notes (General MIDI drums, so drum parts in MIDI files play the
    right sounds): pad 0 is note 36 (kick) ... pad 15 is note 51 (ride). */
constexpr int numPads = 16;
constexpr int firstPadNote = 36;

inline int noteForPad (int pad) noexcept   { return firstPadNote + pad; }
inline int padForNote (int note) noexcept  { return note >= firstPadNote && note < firstPadNote + numPads ? note - firstPadNote : -1; }

/** A kit: which sound each pad plays, and its settings. */
struct PadSetup
{
    const char* sound = "";
    float tune = 0.0f, volume = 0.0f, pan = 0.0f;   // semitones, dB, -100..100
    float decay = 100.0f;                           // % of the sound's own length
    int choke = 0;                                  // 0: none; pads in the same group cut each other off
};

struct Kit
{
    const char* name;
    std::array<PadSetup, numPads> pads;
};

const std::vector<Kit>& kits();
const Kit* findKit (const juce::String& name);

/** The order the drum editor lists the pads in (most used first): kick, snares, clap, hats... */
const std::array<int, numPads>& padDisplayOrder();

} // namespace aether::dsp::drums
