// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
//
// MIDI clips and notes, instruments, the sustain pedal, and importing MIDI files.

#include <catch2/catch_test_macros.hpp>

#include "dsp/EffectTypes.h"
#include "io/MidiImport.h"
#include "io/ProjectFormat.h"
#include "model/ClipEdits.h"
#include "model/EditCommands.h"
#include "model/MidiClips.h"
#include "model/Mixer.h"
#include "model/Recording.h"
#include "model/TimelineEdits.h"

using namespace aether;

namespace
{
    constexpr Tick q = ticksPerQuarter;

    struct Fixture
    {
        Fixture()
        {
            keys = edit::addTrack (project, TrackKind::midi, -1, true, "Keys").getId();
            keys2 = edit::addTrack (project, TrackKind::midi).getId();
            audio = edit::addTrack (project, TrackKind::audio).getId();

            // Four quarter notes C D E F, in a one-bar clip at bar 2.
            clip = edit::addMidiClip (project, keys, "Melody", 4 * q, 4 * q,
                                      { { 60, 100, 0, q }, { 62, 90, q, q }, { 64, 80, 2 * q, q }, { 65, 70, 3 * q, q } });
            project.getUndoManager().clearUndoHistory();
        }

        Clip c() const   { return clips::findClip (project, clip); }
        juce::String xml() const   { return project.getState().toXmlString(); }

        Project project;
        juce::String keys, keys2, audio, clip;
    };
}

TEST_CASE ("MIDI tracks get the Synth, and old projects get it too", "[midi][model]")
{
    Fixture f;
    const auto instrument = midi::instrumentOf (f.project, f.keys);
    REQUIRE (instrument.isValid());
    CHECK (instrument[ids::type].toString() == "subtractive");
    CHECK (instrument[ids::preset].toString() == "Soft keys");
    CHECK_FALSE (midi::instrumentOf (f.project, f.audio).isValid());

    // Its settings work like an effect's.
    const auto id = instrument[ids::id].toString();
    CHECK (mixer::findInsert (f.project, id) == instrument);
    REQUIRE (edit::setInsertParameter (f.project, id, "cutoff", 2000.0f));
    CHECK (mixer::getInsertParameter (instrument, "cutoff") == 2000.0f);

    // A preset changes everything in one step.
    REQUIRE (edit::setInstrumentPreset (f.project, f.keys, "Analog bass"));
    CHECK (mixer::getInsertParameter (instrument, "cutoff") == 400.0f);
    CHECK (instrument[ids::preset].toString() == "Analog bass");
    f.project.getUndoManager().undo();
    CHECK (mixer::getInsertParameter (instrument, "cutoff") == 2000.0f);

    // A MIDI track without an instrument (only possible in a hand-edited file) gets the default.
    // Projects saved since 0.7 keep whichever instrument they had (the starter synth, then).
    auto state = f.project.getState().createCopy();
    for (auto track : state.getChildWithName (ids::Tracks))
        track.removeChild (track.getChildWithName (ids::Instrument), nullptr);
    auto reopened = Project::fromState (state);
    CHECK (midi::instrumentOf (*reopened, f.keys)[ids::type].toString() == "subtractive");

    for (const auto& type : dsp::effectTypes())
        for (const auto& preset : type.presets)
            CHECK (preset.values.size() == type.parameters.size());
}

TEST_CASE ("A MIDI clip holds its notes, which follow the bars", "[midi][model]")
{
    Fixture f;
    const auto clip = f.c();
    CHECK (clip.isMidi());
    CHECK (clip.getStart() == 4 * q);
    CHECK (clip.getEnd ({}) == 8 * q);
    REQUIRE (midi::notesOf (clip).size() == 4);
    CHECK (midi::notesOf (clip)[1] == NoteData { 62, 90, q, q });
    CHECK (midi::noteName (60) == "C4");
    CHECK (midi::noteName (61) == "C#4");

    // MIDI clips can only go on MIDI tracks (and audio clips on audio tracks).
    REQUIRE (edit::setClipPosition (f.project, f.clip, 8 * q, f.audio));
    CHECK (clips::findClip (f.project, f.clip).getTrackState() == f.project.findTrack (f.keys).getState());
    REQUIRE (edit::setClipPosition (f.project, f.clip, 8 * q, f.keys2));
    CHECK (clips::findClip (f.project, f.clip).getTrackState() == f.project.findTrack (f.keys2).getState());
    CHECK (edit::addMidiClip (f.project, f.audio, "x", 0, q, {}).isEmpty());
}

TEST_CASE ("Trimming a MIDI clip hides notes without moving them", "[midi][model]")
{
    Fixture f;
    REQUIRE (edit::trimClipStart (f.project, f.clip, 5 * q));   // hide the first note
    auto notes = midi::audibleNotes (f.c());
    REQUIRE (notes.size() == 3);
    CHECK (notes[0].pitch == 62);
    CHECK (f.c().getStart() + notes[0].start == 5 * q);         // D still plays at beat 5

    REQUIRE (edit::trimClipEnd (f.project, f.clip, 7 * q + q / 2));   // half of the last note
    notes = midi::audibleNotes (f.c());
    REQUIRE (notes.size() == 3);
    CHECK (notes[2].pitch == 65);
    CHECK (notes[2].length == q / 2);

    // Trimming can't make it shorter than a 64th note.
    edit::trimClipEnd (f.project, f.clip, 0);
    CHECK (f.c().getLengthTicks() == midi::minimumClipLength);
}

TEST_CASE ("Splitting a MIDI clip shares the notes out by where they start", "[midi][model]")
{
    Fixture f;
    const auto before = f.xml();
    const auto right = edit::splitClip (f.project, f.clip, 6 * q);
    REQUIRE (right.isNotEmpty());

    const auto leftNotes = midi::audibleNotes (f.c());
    const auto rightNotes = midi::audibleNotes (clips::findClip (f.project, right));
    REQUIRE (leftNotes.size() == 2);
    REQUIRE (rightNotes.size() == 2);
    CHECK (rightNotes[0].pitch == 64);
    CHECK (clips::findClip (f.project, right).getStart() + rightNotes[0].start == 6 * q);   // E still at beat 6

    // Duplicating and deleting work as for audio clips.
    const auto copies = edit::duplicateClips (f.project, { f.clip, right });
    REQUIRE (copies.size() == 2);
    CHECK (clips::findClip (f.project, copies[0]).getStart() == 8 * q);
    CHECK (midi::audibleNotes (clips::findClip (f.project, copies[1])).size() == 2);

    f.project.getUndoManager().undo();
    f.project.getUndoManager().undo();
    CHECK (f.xml() == before);
}

TEST_CASE ("Recording over a MIDI clip cuts it like an audio clip", "[midi][model]")
{
    Fixture f;
    REQUIRE (edit::clearRange (f.project, f.keys, 5 * q, 6 * q));
    const auto pieces = clips::clipsOnTrack (f.project.findTrack (f.keys).getState());
    REQUIRE (pieces.size() == 2);

    int notes = 0;
    for (const auto& piece : pieces)
        notes += (int) midi::audibleNotes (piece).size();

    CHECK (notes == 3);   // D (beat 5) is gone
}

TEST_CASE ("The sustain pedal makes notes last until it's released", "[midi][model]")
{
    midi::NoteCollector collector;
    collector.noteOn (0, 60, 100);
    collector.sustain (100, true);
    collector.noteOff (200, 60);      // released while the pedal is down: keeps sounding
    collector.noteOn (300, 64, 80);
    collector.noteOff (400, 64);
    collector.sustain (1000, false);  // pedal up: both stop here
    collector.noteOn (1100, 67, 60);
    collector.noteOn (1200, 67, 70);  // the same key again: the first one ends
    const auto notes = collector.finish (2000);

    REQUIRE (notes.size() == 4);
    CHECK (notes[0] == NoteData { 60, 100, 0, 1000 });
    CHECK (notes[1] == NoteData { 64, 80, 300, 700 });
    CHECK (notes[2] == NoteData { 67, 60, 1100, 100 });
    CHECK (notes[3] == NoteData { 67, 70, 1200, 800 });   // still held at the end
}

TEST_CASE ("MIDI files import as tracks with clips, in one undo step", "[midi][import]")
{
    // A file at 480 ticks per quarter, 90 BPM: a melody on channel 1 and drums on channel 10,
    // both in one track (as "type 0" files do), with a sustain pedal.
    juce::MidiFile file;
    file.setTicksPerQuarterNote (480);
    juce::MidiMessageSequence track;
    track.addEvent (juce::MidiMessage::tempoMetaEvent (666667), 0);   // 90 BPM
    track.addEvent (juce::MidiMessage::textMetaEvent (3, "Piano"), 0);
    track.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
    track.addEvent (juce::MidiMessage::controllerEvent (1, 64, 127), 240);
    track.addEvent (juce::MidiMessage::noteOff (1, 60), 480);
    track.addEvent (juce::MidiMessage::controllerEvent (1, 64, 0), 960);
    track.addEvent (juce::MidiMessage::noteOn (10, 36, (juce::uint8) 120), 1920);
    track.addEvent (juce::MidiMessage::noteOff (10, 36), 2000);
    file.addTrack (track);

    const auto temp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("aether-import-test.mid");
    {
        temp.deleteFile();
        juce::FileOutputStream out (temp);
        REQUIRE (file.writeTo (out));
    }

    SECTION ("Reading")
    {
        const auto contents = readMidiFile (temp);
        CHECK (contents.problem.isEmpty());
        CHECK (std::abs (contents.firstTempo - 90.0) < 0.01);
        REQUIRE (contents.parts.size() == 2);
        CHECK (contents.parts[0].name == "Piano (channel 1)");
        CHECK (contents.parts[1].name == "Drums");   // channel 10 is drums in General MIDI
        REQUIRE (contents.parts[0].notes.size() == 1);
        CHECK (contents.parts[0].notes[0] == NoteData { 60, 100, 0, 2 * q });   // held by the pedal until beat 2
        CHECK (contents.parts[1].notes[0].start == 4 * q);
    }

    SECTION ("Importing")
    {
        Project project;
        const auto result = importMidiFile (project, temp, 4 * q);
        CHECK (result.problem.isEmpty());
        REQUIRE (result.trackIds.size() == 2);
        CHECK (project.getNumTracks() == 2);
        CHECK (project.getTrack (0).getKind() == TrackKind::midi);

        const auto clip = clips::findClip (project, result.clipIds[0]);
        CHECK (clip.getStart() == 4 * q);
        CHECK (clip.getEnd ({}) == 8 * q);                                 // to the end of the bar
        CHECK (timeline::buildTempoMap (project).tempoAt (0) == 90.0);    // an empty project takes the file's tempo
        CHECK (project.getUndoManager().getUndoDescription() == "Import aether-import-test.mid");

        project.getUndoManager().undo();
        CHECK (project.getNumTracks() == 0);
        CHECK (timeline::buildTempoMap (project).tempoAt (0) == 120.0);
    }

    SECTION ("A project with clips keeps its tempo (the caller asks about it)")
    {
        Project project;
        const auto audio = edit::addTrack (project, TrackKind::audio).getId();
        edit::addAudioClip (project, audio, { "Audio/a.wav", "a", 48000.0, 48000 }, 0);

        const auto result = importMidiFile (project, temp, 0);
        CHECK (std::abs (result.fileTempo - 90.0) < 0.01);
        CHECK_FALSE (result.tempoTaken);
        CHECK (timeline::buildTempoMap (project).tempoAt (0) == 120.0);
    }

    temp.deleteFile();

    // Not a MIDI file.
    const auto notMidi = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("aether-not-midi.mid");
    notMidi.replaceWithText ("hello");
    CHECK (readMidiFile (notMidi).problem.contains ("isn't a MIDI file"));
    notMidi.deleteFile();
}

TEST_CASE ("MIDI clips and instruments are saved and loaded", "[midi][io]")
{
    Fixture f;
    edit::setInstrumentPreset (f.project, f.keys, "Church organ");
    const auto loaded = projectformat::read (projectformat::write (f.project));
    REQUIRE (loaded.result.wasOk());
    CHECK (loaded.state.isEquivalentTo (f.project.getState()));

    auto reopened = Project::fromState (loaded.state);
    const auto clip = clips::findClip (*reopened, f.clip);
    CHECK (clip.isMidi());
    CHECK (midi::notesOf (clip).size() == 4);
    CHECK (midi::instrumentOf (*reopened, f.keys)[ids::preset].toString() == "Church organ");
}

TEST_CASE ("A one-part MIDI file dropped on a MIDI track goes onto that track", "[midi][import]")
{
    juce::MidiFile file;
    file.setTicksPerQuarterNote (96);
    juce::MidiMessageSequence track;
    track.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
    track.addEvent (juce::MidiMessage::noteOff (1, 60), 96);
    file.addTrack (track);

    const auto temp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("aether-one-part.mid");
    {
        temp.deleteFile();
        juce::FileOutputStream out (temp);
        REQUIRE (file.writeTo (out));
    }

    Project project;
    const auto keys = edit::addTrack (project, TrackKind::midi, -1, true, "My keys").getId();
    const auto audio = edit::addTrack (project, TrackKind::audio).getId();

    auto result = importMidiFile (project, temp, 0, -1, keys);
    CHECK (result.trackIds.isEmpty());
    CHECK (project.getNumTracks() == 2);
    REQUIRE (result.clipIds.size() == 1);
    CHECK (clips::findClip (project, result.clipIds[0]).getTrackState() == project.findTrack (keys).getState());

    // Dropped on an audio track: a new MIDI track instead.
    result = importMidiFile (project, temp, 0, -1, audio);
    CHECK (result.trackIds.size() == 1);
    CHECK (project.getNumTracks() == 3);
    temp.deleteFile();
}

TEST_CASE ("A track's instrument can be changed, and the starter synth still works in old projects", "[midi][model]")
{
    Fixture f;
    REQUIRE (edit::setInstrumentType (f.project, f.keys, "synth", "Organ"));
    auto instrument = midi::instrumentOf (f.project, f.keys);
    CHECK (instrument[ids::type].toString() == "synth");
    CHECK (instrument[ids::preset].toString() == "Organ");
    CHECK (f.project.getUndoManager().getUndoDescription() == "Use Starter synth");

    f.project.getUndoManager().undo();
    CHECK (midi::instrumentOf (f.project, f.keys)[ids::type].toString() == "subtractive");

    // Without a preset name: the type's first preset.
    REQUIRE (edit::setInstrumentType (f.project, f.keys, "subtractive"));
    CHECK (midi::instrumentOf (f.project, f.keys)[ids::preset].toString() == "Soft keys");

    // Not an instrument, or not a MIDI track.
    CHECK_FALSE (edit::setInstrumentType (f.project, f.keys, "reverb"));
    CHECK_FALSE (edit::setInstrumentType (f.project, f.audio, "subtractive"));
}
