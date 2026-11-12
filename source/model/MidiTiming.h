// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include "MidiClips.h"
#include "time/TempoDetect.h"

/** Timing for MIDI played freely, without the metronome (Milestone 15).

    The beat of some MIDI clips is found (time/TempoDetect) from their notes as they sound, in
    seconds. Then one of two things happens, each one undo step:

    - Make it steady: the notes move so that every beat found lands on a beat of the song's grid
      (bar 1 of the playing on a bar line). Between beats, notes keep their place in the beat, so
      the feel inside each beat stays; then the song plays it at one steady tempo. Optionally the
      song's tempo becomes the tempo played (one steady tempo for the whole song). A strength
      below 100% keeps some of the player's beat-to-beat push and pull: each beat lands that much
      of the way from where it was played (measured from the player's smoothed tempo, so slow
      drifts are always removed) to its place on the grid.

    - Make the song's tempo follow the playing: the notes keep exactly the time they were played
      at, and the song gets tempo changes (one per bar, or one per beat) so that its bars and
      beats line up with the beats found. The metronome, the grid, quantize and other tracks then
      follow the player. Tempo changes the song had where the playing is are replaced; before
      it, one "lead-in" change makes the first bar of the playing start on a bar line.

    Clips are changed as a group (e.g. a piano part and a bass part played over it); repeating
    patterns are left alone. Other clips keep their place in the bars, as with any tempo change.
*/
namespace aether
{

namespace miditiming
{
    /** The clips among `clipIds` the timing tools can change: MIDI clips that aren't repeating
        patterns. */
    juce::StringArray usableClips (const Project& project, const juce::StringArray& clipIds);

    /** The notes the clips play, as they sound: seconds from the start of the song. */
    std::vector<tempodetect::PlayedNote> playedNotes (const Project& project, const juce::StringArray& clipIds);

    /** Finds the beat of the clips' notes, counting bars in the time signature where the first
        clip starts. `hintBpm` asks for a beat near that tempo (0: find it). */
    tempodetect::Result findBeat (const Project& project, const juce::StringArray& clipIds, double hintBpm = 0.0);

    struct SteadyOptions
    {
        double strength = 1.0;   // 1: every beat exactly on the grid; 0.5: half way
        double songBpm = 0.0;    // > 0: the song gets this one steady tempo; 0: the song's tempo stays
    };

    struct FollowOptions
    {
        bool everyBeat = false;  // a tempo change on every beat (closer), or on every bar (smoother)
    };

    /** The song's bar (0 = the first) where bar 1 of the playing will be. -1 if it can't be done. */
    int steadyStartBar (const Project& project, const juce::StringArray& clipIds, const tempodetect::Result& beat,
                        const SteadyOptions& options);
    int followStartBar (const Project& project, const juce::StringArray& clipIds, const tempodetect::Result& beat);

    /** The song's tempo where the first of the clips starts. */
    double songTempoAt (const Project& project, const juce::StringArray& clipIds);
}

namespace edit
{
    /** Makes the clips' timing steady (see above). Returns false (changing nothing) if there's
        nothing to change or the beat wasn't found. */
    bool makeTimingSteady (Project& project, const juce::StringArray& clipIds, const tempodetect::Result& beat,
                           const miditiming::SteadyOptions& options);

    /** Makes the song's tempo follow the clips' playing (see above). */
    bool makeTempoFollow (Project& project, const juce::StringArray& clipIds, const tempodetect::Result& beat,
                          const miditiming::FollowOptions& options);
}

} // namespace aether
