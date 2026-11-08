// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <memory>
#include <vector>

#include "DrumSounds.h"
#include "EffectTypes.h"
#include "Instrument.h"

/** The drum kit (Milestone 11): an instrument with 16 pads, each playing a sound once when its
    note arrives (General MIDI drum notes 36 to 51, see DrumSounds.h).

    Each pad has a tune, volume, pan, attack, decay (how much of the sound plays) and choke group
    (pads in the same group cut each other off, like a closed hi-hat stopping an open one). A pad
    hit again cuts off its own previous hit (with a 1 ms fade, covered by the new hit), as a real
    drum would; choke groups cut with a 5 ms fade. The
    sounds themselves aren't settings: the Instrument node stores each pad's sound as a property
    ("pad1Sound" ... "pad16Sound"), either a built-in sound ("builtin:kick-808") or a file in the
    project ("Audio/My kick.wav"), and the engine hands the decoded sounds to process(). */
namespace aether::dsp::drumkit
{

inline const juce::String typeId { "drums" };

enum PadSetting : int { tune, volume, pan, attack, decay, choke, numPadSettings };

/** The index of a pad's setting in the instrument's parameters; the kit's overall volume is last. */
constexpr int parameterIndex (int pad, PadSetting setting) noexcept   { return pad * numPadSettings + setting; }
constexpr int masterVolume = drums::numPads * numPadSettings;
constexpr int numParameters = masterVolume + 1;

static_assert (numParameters <= maxEffectParameters);

/** The kit's settings, in parameterIndex() order. */
std::vector<EffectParameter> parameters();

/** One preset per kit (its settings; the sounds come from the kit itself, see drums::kits()). */
std::vector<EffectPreset> presets();

/** The property that stores a pad's sound, e.g. "pad1Sound". */
juce::Identifier soundProperty (int pad);

/** A built-in sound as stored in a pad's sound property, and back. */
inline juce::String builtInReference (const juce::String& soundId)   { return "builtin:" + soundId; }
inline bool isBuiltIn (const juce::String& reference)               { return reference.startsWith ("builtin:"); }
inline juce::String builtInId (const juce::String& reference)       { return reference.fromFirstOccurrenceOf ("builtin:", false, false); }

/** What a pad's sound is called, for showing on it ("Kick 808", "My kick"). */
juce::String describeSound (const juce::String& reference);

/** Makes a drum kit (it still needs prepare()). */
std::unique_ptr<Instrument> create();

} // namespace aether::dsp::drumkit
