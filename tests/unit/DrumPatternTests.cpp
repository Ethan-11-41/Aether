// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
//
// Drum tracks and patterns: steps, step size, swing and pattern length.

#include <catch2/catch_test_macros.hpp>

#include "dsp/DrumKit.h"
#include "io/AudioImport.h"
#include "io/ProjectFiles.h"
#include "model/ClipEdits.h"
#include "model/DrumPatterns.h"
#include "model/EditCommands.h"
#include "model/NoteEdits.h"

using namespace aether;

namespace
{
    constexpr Tick q = ticksPerQuarter;
    constexpr Tick sixteenth = q / 4;
    constexpr int kick = 0, snare = 2, hat = 6;

    struct Fixture
    {
        Fixture()
        {
            track = edit::addDrumTrack (project).getId();
            clip = edit::addPattern (project, track, 4 * q, 4 * q);   // one bar, at bar 2
            project.getUndoManager().clearUndoHistory();
        }

        Clip c() const   { return clips::findClip (project, clip); }

        Project project;
        juce::String track, clip;
    };
}

TEST_CASE ("A drum track has the drum kit, and its patterns repeat", "[drums][model]")
{
    Project project;
    const auto before = project.getState().toXmlString();
    const auto track = edit::addDrumTrack (project);
    CHECK (track.getName() == "Drums");
    CHECK (drumpattern::isDrumTrack (project, track.getId()));
    CHECK (midi::instrumentOf (project, track.getId())[ids::preset].toString() == juce::String (dsp::drums::kits().front().name));
    CHECK (project.getUndoManager().getUndoDescription() == "Add drum track");
    CHECK_FALSE (drumpattern::isDrumTrack (project, edit::addTrack (project, TrackKind::midi).getId()));

    const auto clip = edit::addPattern (project, track.getId(), 0, 4 * q);
    CHECK (drumpattern::isDrumClip (project, clips::findClip (project, clip)));
    CHECK (clips::findClip (project, clip).isLooping());
    CHECK (clips::findClip (project, clip).getPatternLength() == 4 * q);
    CHECK (drumpattern::numSteps (clips::findClip (project, clip)) == 16);
    CHECK (project.getUndoManager().getUndoDescription() == "Add pattern");

    while (project.getUndoManager().canUndo())
        project.getUndoManager().undo();
    CHECK (project.getState().toXmlString() == before);
}

TEST_CASE ("Steps turn on and off, as notes on the pad's note", "[drums][model]")
{
    Fixture f;
    REQUIRE (edit::setStep (f.project, f.clip, kick, 0, drumpattern::defaultVelocity));
    REQUIRE (edit::setStep (f.project, f.clip, kick, 8, drumpattern::defaultVelocity));
    REQUIRE (edit::setStep (f.project, f.clip, snare, 4, drumpattern::accentVelocity));
    CHECK (f.project.getUndoManager().getUndoDescription() == "Add step");

    auto notes = midi::audibleNotes (f.c());
    REQUIRE (notes.size() == 3);
    CHECK (notes[0] == NoteData { 36, 100, 0, sixteenth });
    CHECK (notes[1] == NoteData { 38, 127, 4 * sixteenth, sixteenth });   // in time order
    CHECK (notes[2] == NoteData { 36, 100, 8 * sixteenth, sixteenth });
    CHECK (drumpattern::stepVelocity (f.c(), snare, 4) == 127);
    CHECK (drumpattern::stepVelocity (f.c(), snare, 5) == 0);

    // Changing how hard a step is hit, and turning it off.
    REQUIRE (edit::setStep (f.project, f.clip, snare, 4, 60));
    CHECK (drumpattern::stepVelocity (f.c(), snare, 4) == 60);
    CHECK_FALSE (edit::setStep (f.project, f.clip, snare, 4, 60));
    REQUIRE (edit::setStep (f.project, f.clip, kick, 8, 0));
    CHECK (drumpattern::stepVelocity (f.c(), kick, 8) == 0);
    CHECK_FALSE (edit::setStep (f.project, f.clip, kick, 8, 0));
    CHECK (f.project.getUndoManager().getUndoDescription() == "Clear step");

    // Steps past the pattern, and pads that don't exist, can't be set.
    CHECK_FALSE (edit::setStep (f.project, f.clip, kick, 16, 100));
    CHECK_FALSE (edit::setStep (f.project, f.clip, 16, 0, 100));

    // A note played in (or drawn in the piano roll) close to a step shows on that step.
    edit::addNote (f.project, f.clip, { 42, 90, 2 * sixteenth + 30, 50 });
    CHECK (drumpattern::stepVelocity (f.c(), hat, 2) == 90);

    // The pattern repeats on the timeline.
    edit::trimClipEnd (f.project, f.clip, 12 * q);
    CHECK (midi::audibleNotes (f.c()).size() == 2 * 3);
}

TEST_CASE ("The step size changes how many steps a pattern has", "[drums][model]")
{
    Fixture f;
    edit::setStep (f.project, f.clip, hat, 1, 100);             // the second 1/16
    REQUIRE (edit::setStepLength (f.project, f.clip, q / 2));   // 1/8 steps
    CHECK (drumpattern::numSteps (f.c()) == 8);
    CHECK (drumpattern::stepVelocity (f.c(), hat, 0) == 0);    // between steps now (still plays)
    CHECK (midi::audibleNotes (f.c()).size() == 1);

    REQUIRE (edit::setStepLength (f.project, f.clip, q / 8));   // 1/32 steps
    CHECK (drumpattern::numSteps (f.c()) == 32);
    CHECK (drumpattern::stepVelocity (f.c(), hat, 2) == 100);
    CHECK_FALSE (edit::setStepLength (f.project, f.clip, q / 8));

    f.project.getUndoManager().undo();
    CHECK (drumpattern::stepLengthOf (f.c()) == q / 2);
}

TEST_CASE ("Swing moves every second step later, and back", "[drums][model]")
{
    Fixture f;
    for (int step = 0; step < 4; ++step)
        edit::setStep (f.project, f.clip, hat, step, 100);

    REQUIRE (edit::setSwing (f.project, f.clip, 0.5));
    auto notes = midi::audibleNotes (f.c());
    REQUIRE (notes.size() == 4);
    CHECK (notes[0].start == 0);
    CHECK (notes[1].start == sixteenth + sixteenth / 6);   // half way to a triplet shuffle
    CHECK (notes[2].start == 2 * sixteenth);
    CHECK (notes[3].start == 3 * sixteenth + sixteenth / 6);

    // The steps still show as on, and new ones are swung too.
    CHECK (drumpattern::stepVelocity (f.c(), hat, 3) == 100);
    edit::setStep (f.project, f.clip, snare, 5, 100);
    CHECK (drumpattern::findStep (f.c(), snare, 5)[ids::tick].operator juce::int64() == 5 * sixteenth + sixteenth / 6);

    REQUIRE (edit::setSwing (f.project, f.clip, 0.0));
    CHECK (midi::audibleNotes (f.c())[1].start == sixteenth);

    f.project.getUndoManager().undo();
    CHECK (drumpattern::swingOf (f.c()) == 0.5);
    CHECK (midi::audibleNotes (f.c())[1].start == sixteenth + sixteenth / 6);
}

TEST_CASE ("A longer pattern starts as copies of the old one; a shorter one loses its end", "[drums][model]")
{
    Fixture f;
    edit::setStep (f.project, f.clip, kick, 0, 100);
    edit::setStep (f.project, f.clip, snare, 12, 100);

    REQUIRE (edit::setPatternLooping (f.project, f.clip, true, 8 * q));   // two bars
    CHECK (drumpattern::numSteps (f.c()) == 32);
    CHECK (drumpattern::stepVelocity (f.c(), kick, 16) == 100);
    CHECK (drumpattern::stepVelocity (f.c(), snare, 28) == 100);
    CHECK (f.c().getLengthTicks() == 8 * q);   // the clip grew to hold the whole pattern

    edit::setStep (f.project, f.clip, snare, 30, 100);
    REQUIRE (edit::setPatternLooping (f.project, f.clip, true, 4 * q));
    CHECK (midi::notesOf (f.c()).size() == 2);

    f.project.getUndoManager().undo();
    CHECK (drumpattern::stepVelocity (f.c(), snare, 30) == 100);
}

TEST_CASE ("An audio file can be put on a drum pad", "[drums][io]")
{
    juce::TemporaryFile folder;
    folder.getFile().createDirectory();
    ProjectLocation location;
    REQUIRE (projectfiles::createProjectFolder (folder.getFile(), "Song", location).wasOk());

    // A short WAV file somewhere else.
    const auto source = folder.getFile().getChildFile ("My kick.wav");
    {
        juce::WavAudioFormat wav;
        auto stream = source.createOutputStream();
        REQUIRE (stream != nullptr);
        std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor (stream.release(), 48000.0, 1, 16, {}, 0));
        REQUIRE (writer != nullptr);
        juce::AudioBuffer<float> b (1, 4800);
        b.clear();
        writer->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
    }

    Fixture f;
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    CHECK (importPadSound (f.project, location, formats, source, f.track, 0).isEmpty());

    const auto reference = midi::instrumentOf (f.project, f.track)[dsp::drumkit::soundProperty (0)].toString();
    CHECK (reference == "Audio/My kick.wav");
    CHECK (location.getAudioFolder().getChildFile ("My kick.wav").existsAsFile());

    // Not audio: nothing changes.
    const auto text = folder.getFile().getChildFile ("notes.txt");
    text.replaceWithText ("hello");
    CHECK (importPadSound (f.project, location, formats, text, f.track, 1).isNotEmpty());
    CHECK (midi::instrumentOf (f.project, f.track)[dsp::drumkit::soundProperty (1)].toString().startsWith ("builtin:"));
}
