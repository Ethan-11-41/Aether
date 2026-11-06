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
    juce::StringArray trackIds, clipIds;
    juce::String problem;
};

/** Imports a MIDI file as new MIDI tracks (one per part) with one clip each, starting at
    `position`, in one undo step. If the project has no clips yet, it also takes the file's tempo. */
MidiImportResult importMidiFile (Project& project, const juce::File& file, Tick position, int insertIndex = -1);

/** True for .mid / .midi files. */
bool isMidiFile (const juce::File& file);

} // namespace aether
