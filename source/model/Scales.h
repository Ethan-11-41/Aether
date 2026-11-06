// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <array>
#include <cstdint>

#include <juce_core/juce_core.h>

namespace aether::scales
{

/** A scale: which of the 12 notes from its key note belong to it. */
struct Scale
{
    const char* id;      // stored in the project, e.g. "minor"
    const char* name;    // shown in menus
    std::uint16_t notes; // bit n set: n semitones above the key note is in the scale
};

inline constexpr std::array<Scale, 10> allScales {{
    { "off",             "No scale",          0x0fff },
    { "major",           "Major",             0b101010110101 },   // 0 2 4 5 7 9 11
    { "minor",           "Minor",             0b010110101101 },   // 0 2 3 5 7 8 10
    { "harmonicMinor",   "Harmonic minor",    0b100110101101 },   // 0 2 3 5 7 8 11
    { "dorian",          "Dorian",            0b011010101101 },   // 0 2 3 5 7 9 10
    { "mixolydian",      "Mixolydian",        0b011010110101 },   // 0 2 4 5 7 9 10
    { "pentatonicMajor", "Pentatonic major",  0b001010010101 },   // 0 2 4 7 9
    { "pentatonicMinor", "Pentatonic minor",  0b010010101001 },   // 0 3 5 7 10
    { "blues",           "Blues",             0b010011101001 },   // 0 3 5 6 7 10
    { "chromatic",       "Chromatic (all)",   0x0fff },
}};

/** "C", "C#"... for key notes 0 to 11. */
inline const char* keyName (int key) noexcept
{
    static constexpr const char* names[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return names[((key % 12) + 12) % 12];
}

/** The scale with this id ("No scale" if unknown). */
inline Scale find (const juce::String& id) noexcept
{
    for (const auto& scale : allScales)
        if (id == scale.id)
            return scale;

    return allScales[0];
}

/** True if `pitch` (MIDI note) belongs to `scale` in `key` (0 = C). */
inline bool contains (const Scale& scale, int key, int pitch) noexcept
{
    const auto degree = (((pitch - key) % 12) + 12) % 12;
    return (scale.notes >> degree) & 1;
}

/** True when a scale actually narrows the notes down ("No scale" and chromatic don't). */
inline bool highlights (const Scale& scale) noexcept   { return scale.notes != 0x0fff; }

} // namespace aether::scales
