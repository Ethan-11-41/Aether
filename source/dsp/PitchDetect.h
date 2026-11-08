// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <optional>

#include <juce_audio_basics/juce_audio_basics.h>

/** Working out which note a recording is (Milestone 12), so a single sound (an 808, a bass note,
    a sung note) plays in tune across the keyboard. Uses the YIN method on a stretch after the
    attack (where most sounds have settled on their pitch). For the UI thread: it takes a few
    milliseconds. */
namespace aether::dsp::pitch
{

/** The recording's main frequency in Hz (25 Hz to 2 kHz), if it has a clear one. */
std::optional<double> detectFrequency (const juce::AudioBuffer<float>& sound, double sampleRate);

/** The nearest key to a frequency (A4 = 440 Hz = 69). */
int frequencyToKey (double hz) noexcept;

/** The nearest key, if the recording has a clear pitch. */
std::optional<int> detectKey (const juce::AudioBuffer<float>& sound, double sampleRate);

} // namespace aether::dsp::pitch
