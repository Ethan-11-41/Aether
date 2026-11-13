// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>

#include "TempoDetect.h"

/** Finding where things start in a recording (Milestone 17): drum hits, plucked or struck notes,
    strummed chords. Their times go to the same beat finder as MIDI (time/TempoDetect), so a
    recording played without the metronome can be made steady, or the song's tempo can follow it.

    How: the recording is split into 6 frequency bands; every 5 ms, how much louder each band got
    is added up (a sudden jump in any band is a hit). Peaks that stand out from their surroundings
    are hits; each is then moved to the exact sample where its sound starts rising. A hit where the
    lowest band jumps most (a kick drum, a bass note) is marked as low: low hits count more for
    where bars start, as bass notes do in MIDI.

    Plain C++, not for the audio thread (a 3-minute recording takes a fraction of a second).
*/
namespace aether::audioonsets
{

struct Hit
{
    juce::int64 sample = 0;   // where it starts in the recording
    float strength = 0.0f;    // 0..1 (1: the strongest hits)
    float low = 0.0f;         // 0..1: how much of it is low (kick drum, bass)
};

/** The hits in `audio` (both channels together), in order. */
std::vector<Hit> detect (const juce::AudioBuffer<float>& audio, double sampleRate);

/** The hits as "notes" for the beat finder: `seconds` from the recording's start (plus
    `startSeconds`), loud hits as hard notes, low hits as low notes. */
std::vector<tempodetect::PlayedNote> asNotes (const std::vector<Hit>& hits, double sampleRate, double startSeconds = 0.0);

} // namespace aether::audioonsets
