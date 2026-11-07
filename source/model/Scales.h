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

/** True for scales with a minor third (minor, dorian, blues...), which are named like minor keys. */
inline bool isMinorLike (const Scale& scale) noexcept
{
    return ((scale.notes >> 3) & 1) && ! ((scale.notes >> 4) & 1);
}

/** The key's name as musicians write it. Major keys: C, Db, D, Eb, E, F, F#/Gb, G, Ab, A, Bb, B.
    Minor keys: C, C#, D, D#/Eb, E, F, F#, G, G#, A, Bb, B. */
inline const char* keyName (int key, bool minor = false) noexcept
{
    static constexpr const char* major[] { "C", "Db", "D", "Eb", "E", "F", "F#/Gb", "G", "Ab", "A", "Bb", "B" };
    static constexpr const char* minorNames[] { "C", "C#", "D", "D#/Eb", "E", "F", "F#", "G", "G#", "A", "Bb", "B" };
    const auto k = ((key % 12) + 12) % 12;
    return minor ? minorNames[k] : major[k];
}

/** True if notes in this key are written with flats (Bb rather than A#): F, Bb, Eb, Ab and Db
    major, and D, G, C, F, Bb and Eb minor. */
inline bool usesFlats (int key, bool minor) noexcept
{
    const auto k = ((key % 12) + 12) % 12;
    return minor ? (k == 2 || k == 7 || k == 0 || k == 5 || k == 10 || k == 3)
                 : (k == 5 || k == 10 || k == 3 || k == 8 || k == 1);
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
