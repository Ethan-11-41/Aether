// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "MidiImport.h"

#include <map>

#include "model/ClipEdits.h"
#include "model/EditCommands.h"
#include "model/TimelineEdits.h"

namespace aether
{

bool isMidiFile (const juce::File& file)
{
    return file.hasFileExtension (".mid;.midi;.smf");
}

MidiFileContents readMidiFile (const juce::File& file)
{
    juce::FileInputStream stream (file);
    juce::MidiFile midiFile;

    if (! stream.openedOk() || ! midiFile.readFrom (stream, true))
    {
        MidiFileContents contents;
        contents.problem = file.getFileName() + " isn't a MIDI file Aether can read.";
        return contents;
    }

    return readMidiFile (midiFile, file.getFileNameWithoutExtension());
}

MidiFileContents readMidiFile (const juce::MidiFile& midiFile, const juce::String& fallbackName)
{
    MidiFileContents contents;
    const auto timeFormat = midiFile.getTimeFormat();

    if (timeFormat <= 0)
    {
        contents.problem = "This MIDI file uses timecode (SMPTE) timing, which Aether can't import yet.";
        return contents;
    }

    // File ticks to Aether ticks: both count in quarter notes.
    auto toTicks = [timeFormat] (double fileTicks) { return (std::int64_t) std::llround (fileTicks * (double) ticksPerQuarter / timeFormat); };

    for (int t = 0; t < midiFile.getNumTracks(); ++t)
    {
        const auto* track = midiFile.getTrack (t);
        juce::String trackName;
        std::map<int, midi::NoteCollector> channels;   // a file that keeps everything in one track: one part per channel
        std::int64_t lastTime = 0;

        for (const auto* holder : *track)
        {
            const auto& message = holder->message;
            const auto time = toTicks (message.getTimeStamp());
            lastTime = std::max (lastTime, time);

            if (message.isTrackNameEvent() && trackName.isEmpty())
                trackName = message.getTextFromTextMetaEvent().trim();
            else if (message.isTempoMetaEvent() && contents.firstTempo <= 0.0 && message.getTempoSecondsPerQuarterNote() > 0.0)
                contents.firstTempo = 60.0 / message.getTempoSecondsPerQuarterNote();
            else if (message.getChannel() > 0)
                channels[message.getChannel()].add (time, message);
        }

        for (auto& [channel, collector] : channels)
        {
            MidiPart part;
            part.channel = channel;
            part.notes = collector.finish (lastTime);

            if (part.notes.empty())
                continue;

            part.name = trackName.isNotEmpty() ? trackName : fallbackName;

            // Channel 10 is drums in General MIDI files.
            if (channel == 10 && (channels.size() > 1 || trackName.isEmpty()))
                part.name = "Drums";
            else if (channels.size() > 1)
                part.name << " (channel " << channel << ")";

            contents.parts.push_back (std::move (part));
        }
    }

    if (contents.parts.empty() && contents.problem.isEmpty())
        contents.problem = "The MIDI file has no notes in it.";

    return contents;
}

MidiImportResult importMidiFile (Project& project, const juce::File& file, Tick position, int insertIndex,
                                 const juce::String& targetTrackId)
{
    MidiImportResult result;
    const auto contents = readMidiFile (file);
    result.fileTempo = std::round (contents.firstTempo * 100.0) / 100.0;

    if (contents.parts.empty())
    {
        result.problem = contents.problem;
        return result;
    }

    auto& undo = project.getUndoManager();
    undo.beginNewTransaction ("Import " + file.getFileName());

    // A new project takes the song's tempo (one with clips in already keeps its own).
    if (result.fileTempo > 0.0 && clips::allClips (project).isEmpty())
    {
        edit::setTempo (project, 0, result.fileTempo, false);
        result.tempoTaken = true;
    }

    // One part, dropped on a MIDI track: it goes there.
    const auto target = project.findTrack (targetTrackId);
    const auto ontoTarget = contents.parts.size() == 1 && target.getKind() == TrackKind::midi;

    const auto tempo = timeline::buildTempoMap (project);
    position = std::max<Tick> (0, position);

    for (const auto& part : contents.parts)
    {
        // The clip runs to the end of the bar in which the last note ends.
        Tick end = 0;
        for (const auto& n : part.notes)
            end = std::max (end, n.start + n.length);

        const auto lastBar = tempo.ticksToBarBeat (position + end).bar;
        auto clipEnd = tempo.barToTicks (lastBar);
        if (clipEnd < position + end)
            clipEnd = tempo.barToTicks (lastBar + 1);

        auto trackId = target.getId();

        if (! ontoTarget)
        {
            trackId = edit::addTrack (project, TrackKind::midi, insertIndex, false, part.name).getId();
            if (insertIndex >= 0)
                ++insertIndex;

            result.trackIds.add (trackId);
        }

        result.clipIds.add (edit::addMidiClip (project, trackId, part.name, position, clipEnd - position, part.notes, false));
    }

    return result;
}

} // namespace aether
