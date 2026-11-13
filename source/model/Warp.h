// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <vector>

#include "Clip.h"
#include "Project.h"
#include "time/TempoMap.h"

/** Audio clips that follow the song's tempo (Milestone 17).

    An audio clip normally plays its recording at its own speed: it starts on a beat, and the rest
    is in seconds, so if the song's tempo changes, the recording no longer lines up with the bars.

    A clip that follows the tempo is placed in beats instead. It knows the recording's own tempo
    (`warpBpm`) and, optionally, warp markers: moments of the file pinned to beat positions (e.g.
    each beat of a recording played without a metronome, put on the grid). Through them, every
    moment of the file has a beat position, "W". When the song's tempo isn't the recording's, or
    markers move things, the clip is stretched (dsp/TimeStretch) so every moment plays at its beat:
    the engine plays a stretched copy made in the background (engine/StretchCache).

    A clip can also be transposed (higher or lower, keeping its speed), following the tempo or not.

    Everything that places audio on the timeline (drawing, trimming, splitting, playing) goes
    through ClipTiming, so clips that follow the tempo and clips that don't behave the same way.
*/
namespace aether
{

namespace warp
{
    /** A moment of the clip's file (sample) pinned to a beat position (ticks). Only differences
        between beat positions matter. */
    struct Marker
    {
        juce::int64 sample = 0;
        Tick tick = 0;

        bool operator== (const Marker&) const = default;
    };

    bool followsTempo (const Clip& clip);
    double bpmOf (const Clip& clip);              // the recording's own tempo (120 if not set)
    std::vector<Marker> markersOf (const Clip& clip);   // in file order (samples and ticks rising)
    double transposeOf (const Clip& clip);        // semitones

    /** Where a clip's audio is on the song's timeline, worked out once (cheap to use many times). */
    class ClipTiming
    {
    public:
        explicit ClipTiming (const Clip& clip);

        bool followsTempo() const noexcept   { return follows; }

        /** The timeline position (ticks) where `played` samples into the clip play (counting loop
            repeats; 0 is the clip's start, its duration its end). */
        double tickAt (const TempoMap& tempo, double played) const;

        /** The other way: how many samples into the clip a timeline position is. */
        double playedAt (const TempoMap& tempo, double tick) const;

        /** Where the clip ends. */
        Tick end (const TempoMap& tempo) const;

        /** The file sample that plays `played` samples into the clip. */
        double fileSampleAt (double played) const;

        /** The beat position of a file sample (W), and the other way. */
        double beatOf (double fileSample) const;
        double fileSampleOf (double beat) const;

    private:
        double contentBeats (double x) const;      // W over the repeating content, x samples into it
        double contentSample (double beats) const; // ...and back

        bool follows = false, looping = false;
        Tick start = 0;
        double rate = 48000.0, offset = 0.0, contentLength = 1.0, phase = 0.0, duration = 1.0;
        double pace = 0.0;   // ticks per sample at the recording's own tempo
        std::vector<Marker> markers;
    };

    /** One point of a clip's time map: `seconds` after the clip's start, `played` samples into it
        play. */
    struct TimePoint
    {
        double seconds = 0.0;
        double played = 0.0;
    };

    /** The clip's time map at the song's tempo, from its start to its end (for the stretcher):
        exact at warp markers, loop repeats and tempo changes (straight lines between), and every
        16th note in tempo ramps. */
    std::vector<TimePoint> timeMap (const Clip& clip, const TempoMap& tempo);

    /** True if the clip can't simply play its recording: it follows the tempo and somewhere it
        must play faster or slower (by more than a sample), or it's transposed. */
    bool needsStretching (const Clip& clip, const TempoMap& tempo);

    /** A loop's tempo from its file name, if it says: "Drums 90 BPM", "beat_120bpm", or a bare
        number between 60 and 200 ("Funk_Loop_96_Am") if the recording is a whole number of bars
        at that tempo (`seconds` long). 0 if not. */
    double bpmFromFileName (const juce::String& name, double seconds);
}

namespace edit
{
    /** Makes audio clips follow the song's tempo (or not), one undo step. Turned on, a clip
        that already knows its own tempo (a loop, an earlier setting) goes back into the bars at
        it; otherwise it takes the tempo where it starts (with markers where the tempo changes
        inside it), so it sounds exactly as now and follows from now on. Turned off, it plays at
        its own speed from where it starts. Returns false if nothing changed. */
    bool setFollowTempo (Project& project, const juce::StringArray& clipIds, bool follow, bool newUndoStep = true);

    /** Makes a clip follow the tempo from now: its own tempo is the song's where it starts (with
        markers where the tempo changes inside it), whatever it knew before, so it sounds exactly
        as now. For a recording made with the metronome, or a sound effect just made. */
    bool followTempoFromNow (Project& project, const juce::String& clipId, bool newUndoStep = true);

    /** Sets the recording's own tempo (e.g. "this loop is 90 BPM"): a clip following the tempo
        plays it in time with the song. */
    bool setClipBpm (Project& project, const juce::String& clipId, double bpm);

    /** Transposes audio clips (semitones, -24 to +24, to the cent), one undo step. */
    bool setClipTranspose (Project& project, const juce::StringArray& clipIds, double semitones);

    /** Warp markers. Adding pins a moment of the file (`fileSample`) where it now plays, so
        nothing moves until it's dragged; a clip's first marker also pins its start. Moving one
        puts its moment at `tick` on the timeline (kept between its neighbours), stretching the
        audio either side. Each is one undo step (moving: `newUndoStep` false joins the last). */
    bool addWarpMarker (Project& project, const juce::String& clipId, juce::int64 fileSample);
    bool moveWarpMarker (Project& project, const juce::String& clipId, int index, Tick tick, bool newUndoStep = true);
    bool removeWarpMarker (Project& project, const juce::String& clipId, int index);
    bool clearWarpMarkers (Project& project, const juce::String& clipId);

    /** Replaces a clip's markers (and its own tempo), as one undo step (timing tools). */
    void setWarp (Project& project, const juce::String& clipId, double bpm, const std::vector<warp::Marker>& markers,
                  const juce::String& undoName, bool newUndoStep = true);
}

} // namespace aether
