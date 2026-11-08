// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <memory>
#include <vector>

#include "EffectTypes.h"
#include "Instrument.h"

/** The sampler (Milestone 12): plays recorded sounds (a SampleMap, handed to process() by the
    engine) across the keyboard.

    - Several recordings per key range and loudness ("velocity layers"), alternating takes
      ("round robin") so repeated notes don't sound mechanical, loops for held notes.
    - A volume envelope (attack, decay, sustain, release), a tone control (a low-pass filter),
      tuning, volume, pan and how much playing harder makes it louder.
    - Voices: Poly (chords), Mono (one note at a time), or Slide: one note at a time, and when a
      note starts while the previous one is still held, the sound glides to the new pitch without
      starting again (how 808 bass lines slide).

    Which recordings it plays isn't a setting: the Instrument node stores where they come from
    (see model/SamplerSounds.h) and the engine builds the map. */
namespace aether::dsp::sampler
{

inline const juce::String typeId { "sampler" };

enum Parameter : int
{
    attack, decay, sustain, release, tone, touch, tune, fine, voiceMode, glide, volume, pan, bendRange,
    numParameters
};

enum VoiceMode : int { poly, mono, slide };

constexpr int maxVoices = 64;

/** The sampler's settings, in Parameter order. */
std::vector<EffectParameter> parameters();

/** Makes a sampler (it still needs prepare()). */
std::unique_ptr<Instrument> create();

} // namespace aether::dsp::sampler
