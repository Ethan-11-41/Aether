// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <functional>

#include "MidiClips.h"
#include "time/AudioOnsets.h"
#include "time/Grid.h"
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

    Each fix starts from how the clips were played, not from the last fix: the first fix
    remembers, on each clip, when each moment of it was played (and the song's tempo before),
    so trying "steady" and then "follow" (or another tempo) gives the same as trying it first.
    Notes moved by hand since keep their place among the notes around them; a clip moved by hand
    is taken as it is now. putBackAsPlayed() undoes the fixes, however many there were.
*/
namespace aether
{

namespace miditiming
{
    /** An audio clip's hits (time/AudioOnsets), in samples of its file: given by whoever has its
        audio (empty if it isn't loaded). Without it, audio clips have nothing to find a beat in. */
    using AudioHits = std::function<std::vector<audioonsets::Hit> (const Clip& clip)>;

    /** The clips among `clipIds` the timing tools can change: MIDI clips that aren't repeating
        patterns, and audio clips that don't loop (Milestone 17: their hits count as notes, and
        fixes become warp markers, so the recording is stretched into time). */
    juce::StringArray usableClips (const Project& project, const juce::StringArray& clipIds);

    /** The notes the clips play (and audio clips' hits), as played: seconds from the start of the
        song. */
    std::vector<tempodetect::PlayedNote> playedNotes (const Project& project, const juce::StringArray& clipIds,
                                                      const AudioHits& hits = {});

    /** Finds the beat of the clips' notes, counting bars in the time signature where the first
        clip starts. `hintBpm` asks for a beat near that tempo (0: find it). */
    tempodetect::Result findBeat (const Project& project, const juce::StringArray& clipIds, double hintBpm = 0.0,
                                  const AudioHits& hits = {});

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
                        const SteadyOptions& options, const AudioHits& hits = {});
    int followStartBar (const Project& project, const juce::StringArray& clipIds, const tempodetect::Result& beat,
                        const AudioHits& hits = {});

    /** The song's tempo where the first of the clips starts (as it was before a timing fix
        changed it, if one did: a new fix starts from that). */
    double songTempoAt (const Project& project, const juce::StringArray& clipIds, const AudioHits& hits = {});

    /** How many tempo changes the song has, counted the same way. */
    int songTempoChanges (const Project& project, const juce::StringArray& clipIds, const AudioHits& hits = {});

    /** True if any of the clips remembers how it was played (an earlier timing fix). */
    bool remembersPlaying (const Project& project, const juce::StringArray& clipIds);

    /** True if a timing fix changed the clips or the song's tempo, and it can be put back. */
    bool wasFixed (const Project& project, const juce::StringArray& clipIds, const AudioHits& hits = {});
}

namespace edit
{
    /** Makes the clips' timing steady (see above). Returns false (changing nothing) if there's
        nothing to change or the beat wasn't found. */
    bool makeTimingSteady (Project& project, const juce::StringArray& clipIds, const tempodetect::Result& beat,
                           const miditiming::SteadyOptions& options, const miditiming::AudioHits& hits = {});

    /** Makes the song's tempo follow the clips' playing (see above). */
    bool makeTempoFollow (Project& project, const juce::StringArray& clipIds, const tempodetect::Result& beat,
                          const miditiming::FollowOptions& options, const miditiming::AudioHits& hits = {});

    /** Puts the clips back as they were played, and the song's tempo as it was then (if the last
        fix changed it and nothing has changed it since). One undo step; false if no fix is
        remembered. */
    bool putBackAsPlayed (Project& project, const juce::StringArray& clipIds, const miditiming::AudioHits& hits = {});

    /** Quantizes audio clips (Milestone 17): each hit (time/AudioOnsets) moves towards the
        song's grid by `strength` (with `swing`), through warp markers, so the recording is
        stretched between them. The clip's start and end stay. One undo step ("Quantize"). */
    bool quantizeAudioClips (Project& project, const juce::StringArray& clipIds, const miditiming::AudioHits& hits,
                             GridSize grid, double strength, double swing);
}

} // namespace aether
