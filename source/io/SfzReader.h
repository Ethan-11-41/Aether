// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <optional>
#include <vector>

#include <juce_core/juce_core.h>

/** Reading SFZ files (Milestone 12): the common text format for sampled instruments, used by
    Aether's own recorded instruments and by many free ones online.

    An SFZ file lists regions: a recording, the keys and velocities it plays on, and how. Settings
    can be given once for a group of regions (<global>, <master>, <group>) and are inherited.
    Aether reads the parts its sampler can play:

    - sample, default_path (<control>), #define $NAME value, #include "file"
    - key, lokey, hikey, pitch_keycenter (numbers, or note names: c4 = 60, c#4, db4),
      lovel, hivel, tune (cents), transpose, volume (dB), pan
    - seq_length, seq_position (round robin)
    - offset, end, loop_mode (no_loop, one_shot, loop_continuous, loop_sustain), loop_start,
      loop_end (also loopstart, loopend)
    - ampeg_attack, ampeg_decay, ampeg_sustain, ampeg_release and amp_veltrack (from the first
      region: the sampler uses one envelope and one velocity response for the whole instrument)

    Regions that play when a key is let go (trigger=release) are skipped. Anything else is
    ignored, and listed in `ignored` (so a strange-sounding instrument can be explained). */
namespace aether::sfz
{

struct Region
{
    juce::String sample;                 // as written, relative to the SFZ file's folder, with "/"
    int lowKey = 0, highKey = 127, rootKey = 60;
    int lowVelocity = 1, highVelocity = 127;
    float tune = 0.0f;                   // semitones (tune and transpose together)
    float volume = 0.0f;                 // dB
    float pan = 0.0f;                    // -100 .. 100
    int sequenceLength = 1, sequencePosition = 1;
    std::int64_t offset = 0, end = -1;   // in samples of the file
    std::int64_t loopStart = -1, loopEnd = -1;
    bool loop = false, oneShot = false;
};

struct Instrument
{
    std::vector<Region> regions;
    std::optional<float> attack, decay, sustain, release;   // seconds (sustain: %)
    std::optional<float> velocityTracking;                  // amp_veltrack: how much velocity changes loudness, %
    juce::StringArray ignored;                              // opcodes Aether doesn't use
};

/** Reads SFZ text. `includeFolder` is where #include files are looked for (none if empty). */
Instrument parse (const juce::String& text, const juce::File& includeFolder = {});

/** Reads an SFZ file (its #includes are relative to it). */
Instrument read (const juce::File& file);

/** A note name or number as SFZ writes it ("c4" = 60, "f#2", "eb5", "60"), or -1. */
int parseKey (const juce::String& text);

/** Writes an instrument as SFZ text (for instruments Aether makes from a set of files). */
juce::String write (const Instrument& instrument);

} // namespace aether::sfz
