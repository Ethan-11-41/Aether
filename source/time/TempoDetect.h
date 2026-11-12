// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <vector>

/** Finding the beat in music played freely, without a metronome (Milestone 15).

    Given the notes of a performance (when each started, in seconds, its length, pitch and
    velocity), this finds where the beats are: one time per beat, following the player as they
    speed up and slow down. It works in three steps:

      1. Onsets: notes starting within 35 ms of each other count as one "onset" (a chord). Each
         onset gets a weight: louder, longer and lower (bass) notes weigh more, since they tend to
         fall on beats.
      2. Tempo: in windows a few seconds long, how often each distance between two onsets
         occurs. A beat length fits when onsets are that far apart, and two, three and four
         times that far (so half a beat, which isn't repeated over one and a half beats, doesn't
         fit as well). The windows' answers, leaning towards ordinary tempos (about 105 BPM, or
         near a tempo you ask for), give the typical beat length; the path through the windows
         that fits best while changing gradually gives the tempo at each moment, so a player who
         drifts from 75 to 105 BPM is followed.
      3. Beats: dynamic programming (as in D. Ellis, "Beat tracking by dynamic programming",
         2007) picks the chain of beats that lands on the strongest onsets while keeping each
         beat close to the tempo at that moment. If every other beat carries much heavier notes
         (a chord on each beat, a light note between), the real beat is taken to be half as fast.
         The beats are tracked again following the first chain, then moved exactly onto the
         onsets they sit on; beats where nothing was played (rests, held chords) are spread
         evenly in between.

    Some music has two good answers: a slow ballad with fast arpeggios can be found at twice its
    tempo, and music in 6/8 often at the speed of two eighth notes. The window that uses this
    offers "half as fast", "twice as fast" and typing the tempo meant (`Options::hintBpm`).

    It also guesses which beat is the first beat of a bar (bass notes and long chords tend to
    start bars; when unsure it takes the first beat played), how steady the playing was, and
    how clearly the beat stood out.

    Everything here is plain arithmetic on the UI thread: no JUCE, no allocation rules.
*/
namespace aether::tempodetect
{

/** One note, as played. */
struct PlayedNote
{
    double seconds = 0.0;   // when it started
    double length = 0.1;    // how long it lasted, in seconds
    int pitch = 60;
    int velocity = 100;
};

struct Options
{
    /** 0: find the tempo. Otherwise, look for a beat near this tempo (e.g. twice the tempo that
        was found, when the player counted twice as fast). */
    double hintBpm = 0.0;

    /** Beats in a bar, for guessing where bars start. */
    int beatsPerBar = 4;
};

struct Result
{
    /** False if there was too little to go on (fewer than 6 onsets, or under 2 seconds). */
    bool found = false;

    /** The beats, in seconds, in time order. They cover every note: the first beat is at or
        before the first note, the last at or after the last note's start. */
    std::vector<double> beats;

    /** The same beats with the player's beat-to-beat unevenness smoothed out: a line through
        their neighbours, so slow changes of tempo remain. */
    std::vector<double> smoothBeats;

    /** The index (in `beats`) of the first beat that starts a bar. Beats before it are a
        pick-up. */
    int downbeat = 0;

    /** The typical tempo (the median beat length), and the range the tempo moved in as played
        (the 10th and 90th percentiles of the smoothed tempo). */
    double bpm = 0.0, slowestBpm = 0.0, fastestBpm = 0.0;

    /** 0 to 1: how much of the playing (by weight) sits on the beats or halfway between them.
        Music played to a clear beat scores 0.8 or more; notes at random times about 0.3. */
    double clarity = 0.0;

    int beatsPerBar = 4;
};

/** Finds the beat (see above). The notes don't need to be sorted. */
Result analyse (const std::vector<PlayedNote>& notes, const Options& options = {});

/** Where a time falls among the beats, counting in beats from the first: 2.5 is halfway
    between beats 2 and 3. Before the first beat and after the last, the nearest beat's length
    carries on. `beats` needs at least two entries. */
double beatPosition (const std::vector<double>& beats, double seconds) noexcept;

} // namespace aether::tempodetect
