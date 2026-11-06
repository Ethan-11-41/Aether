// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <vector>

#include "model/MidiClips.h"
#include "model/Project.h"

namespace aether
{

/** One part of a MIDI file: the notes of one track (or one channel, for files that keep
    everything in one track). Note starts are in Aether ticks from the start of the file. */
struct MidiPart
{
    juce::String name;
    int channel = 1;
    std::vector<NoteData> notes;
};

struct MidiFileContents
{
    std::vector<MidiPart> parts;   // only parts with notes
    double firstTempo = 0.0;       // the file's first tempo in BPM, or 0 if it has none
    juce::String problem;          // why it couldn't be read, if it couldn't
};

/** Reads the notes from a standard MIDI file (.mid). The sustain pedal is turned into note
    lengths. Tempo changes after the first aren't used: notes keep their musical positions. */
MidiFileContents readMidiFile (const juce::File& file);
MidiFileContents readMidiFile (const juce::MidiFile& midiFile, const juce::String& fallbackName);

struct MidiImportResult
{
    juce::StringArray trackIds, clipIds;   // trackIds: the new tracks (none if it went onto an existing one)
    juce::String problem;
    double fileTempo = 0.0;                // the file's tempo (0 if it has none)
    bool tempoTaken = false;               // true if the project's tempo was set to it
};

/** Imports a MIDI file, starting at `position`, in one undo step:
    - a file with one part dropped on a MIDI track (`targetTrackId`) goes onto that track;
    - otherwise each part becomes a new MIDI track with one clip (inserted at `insertIndex`).
    An empty project (no clips yet) also takes the file's tempo. A project that already has clips
    keeps its own: the caller can ask whether to use the file's (see `fileTempo`). */
MidiImportResult importMidiFile (Project& project, const juce::File& file, Tick position, int insertIndex = -1,
                                 const juce::String& targetTrackId = {});

/** True for .mid / .midi files. */
bool isMidiFile (const juce::File& file);

} // namespace aether
