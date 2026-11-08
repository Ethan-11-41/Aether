// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <cstdint>
#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>

/** What a sampler plays (Milestone 12): recorded sounds laid out over the keyboard.

    Each zone is one recording with the keys and velocities it answers to, the key it was recorded
    at (played higher or lower, it's pitched up or down), and optional round-robin position and
    loop. The audio is already at the engine's sample rate. A map is built on the UI thread and is
    read-only afterwards, so the audio thread can use it without locks. */
namespace aether::dsp
{

struct SampleZone
{
    const juce::AudioBuffer<float>* sample = nullptr;   // mono or stereo, at the engine's rate

    int lowKey = 0, highKey = 127;        // the keys it plays on
    int lowVelocity = 1, highVelocity = 127;
    int rootKey = 60;                     // the key it was recorded at
    float tune = 0.0f;                    // semitones (from tune and transpose)
    float gain = 1.0f;                    // linear
    float pan = 0.0f;                     // -1 (left) .. +1 (right)

    int sequenceLength = 1, sequencePosition = 1;   // round robin: plays on every Nth hit, at this one

    std::int64_t start = 0;               // first sample played
    std::int64_t end = -1;                // one past the last sample (-1: the end of the recording)
    std::int64_t loopStart = -1, loopEnd = -1;      // a loop while the note is held (-1: none)
    bool oneShot = false;                 // plays to its end even when the key is let go

    bool matches (int key, int velocity) const noexcept
    {
        return key >= lowKey && key <= highKey && velocity >= lowVelocity && velocity <= highVelocity;
    }

    std::int64_t lastSample() const noexcept
    {
        const auto length = sample != nullptr ? (std::int64_t) sample->getNumSamples() : 0;
        return end > 0 ? std::min (end, length) : length;
    }

    bool loops() const noexcept   { return loopStart >= 0 && loopEnd > loopStart + 16 && loopEnd <= lastSample(); }
};

struct SampleMap
{
    std::vector<SampleZone> zones;

    bool isEmpty() const noexcept   { return zones.empty(); }
};

} // namespace aether::dsp
