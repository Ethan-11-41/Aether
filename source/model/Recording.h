// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "ClipEdits.h"
#include "MidiClips.h"
#include "Project.h"

/** The recording side of the project: which tracks are armed, what they record from, punch
    in/out, and turning a finished recording into a clip.

    How a recording becomes a clip:
    - The engine records one continuous file per armed track, from the moment recording starts
      until Stop (even through loop jumps).
    - The sound the musician played in time with what they heard arrives late by the round-trip
      latency (output + input). So the clip uses the file from `latency` samples in: that puts
      every note back where it was played.
    - With punch on, only the part inside the loop region becomes the clip (the file keeps the
      rest).
    - With the loop on, every pass round the loop is a "take" in the same file. The clip covers
      the loop region and plays the last take; the others can be chosen from the clip's menu.
    - Whatever was on the track where the new clip goes is cut away (in the same undo step), so
      you hear the new recording, not both.
*/
namespace aether
{

namespace recording
{
    /** The facts about a finished recording. All positions are in samples at the engine's rate
        (which is also the file's rate), measured on the timeline. */
    struct Facts
    {
        std::int64_t startSample = 0;      // where the playhead was when the file's first sample was captured
        std::int64_t lengthInSamples = 0;  // the file's length
        int latencySamples = 0;            // round trip: how late the recorded sound arrives
        bool loopEnabled = false;          // the loop, as it was when recording began
        std::int64_t loopStart = 0, loopEnd = 0;
        bool punch = false;                // punch in/out on the loop region
    };

    /** Where (and which part of the file) the clip goes. */
    struct Placement
    {
        std::int64_t timelineStart = 0;    // samples
        std::int64_t offset = 0;           // where in the file the clip begins
        std::int64_t duration = 0;
        int takes = 1;                     // loop recording: how many passes the file holds
        std::int64_t takeLength = 0;       // samples between one take and the next in the file
        int take = 0;                      // the take the clip plays (the last one)
    };

    /** Works out where a recording goes. Nothing (no clip) if too little was recorded to keep:
        less than the minimum clip length, or nothing inside the punch range. */
    std::optional<Placement> place (const Facts& facts, double sampleRate);

    /** Where recorded MIDI goes. */
    struct MidiPlacement
    {
        std::int64_t timelineStart = 0, duration = 0;   // the clip, in samples
        std::vector<NoteData> notes;                    // start and length in samples, from timelineStart
    };

    /** Places recorded notes, whose `start` and `length` count samples from the start of the
        recording (as an audio file's would). `facts.latencySamples` is MIDI's latency (how late
        you heard what you played along to). With the loop on, every pass goes into the same
        clip (overdub). Nothing if no notes landed inside the clip's range. */
    std::optional<MidiPlacement> placeMidi (const Facts& facts, const std::vector<NoteData>& notes, double sampleRate);

    /** A track's input as text, e.g. "Input 1", "Inputs 1+2" or "No input". */
    juce::String describeInput (int firstChannel, bool stereo);

    /** Tracks that will record when you press Record: armed audio tracks, and armed MIDI tracks. */
    juce::StringArray armedTracks (const Project& project);
    juce::StringArray armedMidiTracks (const Project& project);
}

namespace edit
{
    /** Arms tracks for recording (or disarms them). Arming isn't an undo step: like selection,
        it's a switch, not a change to the song. Audio and MIDI tracks can be armed (not buses). */
    void setArmed (Project& project, const juce::StringArray& trackIds, bool shouldBeArmed);

    /** Lets you hear the tracks' input through them (with their effects). Not an undo step either. */
    void setMonitoring (Project& project, const juce::StringArray& trackIds, bool shouldMonitor);

    /** What the tracks record from: a channel of the sound card (0 = input 1, -1 = none), alone
        or with the next channel as a stereo pair. Undoable. */
    bool setInput (Project& project, const juce::StringArray& trackIds, int firstChannel, bool stereo);

    /** Punch in/out: when on, recording only keeps what's inside the loop region. Undoable. */
    bool setPunch (Project& project, bool shouldPunch);

    /** Removes everything on a track between two positions, cutting clips that cross them. */
    bool clearRange (Project& project, const juce::String& trackId, Tick from, Tick to, bool newUndoStep = true);

    /** A finished recording for one track: audio (a file and where it goes) or MIDI (notes). */
    struct RecordedTake
    {
        juce::String trackId;
        AudioSourceInfo source;
        recording::Placement placement;

        bool isMidi = false;
        Tick midiStart = 0, midiLength = 0;
        std::vector<NoteData> notes {};  // MIDI: start in ticks from midiStart
    };

    /** Adds the clips of a recording (all tracks in one undo step), replacing whatever was on
        those tracks in that range. Returns the new clips' ids. */
    juce::StringArray addRecordings (Project& project, const std::vector<RecordedTake>& takes);

    /** Chooses which take a loop-recorded clip plays (0 = the first). */
    bool setClipTake (Project& project, const juce::String& clipId, int take);
}

} // namespace aether
