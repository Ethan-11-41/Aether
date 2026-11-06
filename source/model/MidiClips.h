// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <array>
#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>

#include "Clip.h"
#include "Project.h"

/** MIDI clips and instruments.

    A MIDI clip is a Clip node with kind = "midi". Its position (`start`) and `length` are in
    ticks, so it follows the bars when the tempo changes. Its notes are Note child nodes, each
    with a `tick` (from the start of the clip's content), a `length` (ticks), a `pitch` and a
    `velocity`. `contentStart` says where in the notes the clip starts playing, which lets the
    clip's left edge be trimmed without moving the notes.

    Every MIDI track has an Instrument node: which instrument plays its notes, and its settings
    (stored like an effect's, see dsp/EffectTypes.cpp).
*/
namespace aether
{

/** One note. `start` is in ticks from the start of the clip's content. */
struct NoteData
{
    int pitch = 60;
    int velocity = 100;
    Tick start = 0;
    Tick length = ticksPerQuarter;

    bool operator== (const NoteData& other) const = default;
};

namespace midi
{
    /** The shortest a MIDI clip can be made: a 64th note. */
    constexpr Tick minimumClipLength = ticksPerQuarter / 16;

    /** A clip's notes, sorted by start (then pitch). */
    std::vector<NoteData> notesOf (const Clip& clip);

    /** The notes a clip actually plays (inside its range), with `start` measured from the clip's
        start on the timeline. Notes running past the end are shortened to it. */
    std::vector<NoteData> audibleNotes (const Clip& clip);

    /** A MIDI track's instrument node (every MIDI track has one). Invalid for other tracks. */
    juce::ValueTree instrumentOf (const Project& project, const juce::String& trackId);

    /** A new instrument node with a type's default settings and the given preset. */
    juce::ValueTree makeInstrument (const juce::String& type, const juce::String& preset);

    /** "C4", "F#2"... (middle C, 60, is "C4"). */
    juce::String noteName (int pitch);

    /** Turns a stream of MIDI events (note on/off, sustain pedal) into notes with lengths.
        Times can be in any unit (ticks or samples), as long as they only go forwards. Notes
        held by the sustain pedal last until the pedal is released, so they sound the same when
        played back without the pedal. */
    class NoteCollector
    {
    public:
        void noteOn (std::int64_t time, int pitch, int velocity);
        void noteOff (std::int64_t time, int pitch);
        void sustain (std::int64_t time, bool down);

        /** Ends any notes still sounding at `time`, and returns every note (sorted by start). */
        std::vector<NoteData> finish (std::int64_t time);

        /** Feeds a JUCE MIDI message in (other kinds of message are ignored). */
        void add (std::int64_t time, const juce::MidiMessage& message);

    private:
        void end (std::int64_t time, int pitch);

        struct Held { std::int64_t start = -1; int velocity = 0; bool released = false; };
        std::array<Held, 128> held {};
        bool pedal = false;
        std::vector<NoteData> notes;
    };
}

namespace edit
{
    /** Adds a MIDI clip to a MIDI track. Returns its id (empty if the track isn't a MIDI track). */
    juce::String addMidiClip (Project& project, const juce::String& trackId, const juce::String& name,
                              Tick start, Tick length, const std::vector<NoteData>& notes, bool newUndoStep = true);

    /** Chooses an instrument preset for a track (all its settings change, in one undo step). */
    bool setInstrumentPreset (Project& project, const juce::String& trackId, const juce::String& presetName);
}

} // namespace aether
