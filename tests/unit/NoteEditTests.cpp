// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
//
// Editing notes in a MIDI clip (the piano roll's model): adding, moving, resizing, velocity,
// quantize with swing, humanize, duplicating, scales.

#include <catch2/catch_test_macros.hpp>

#include "model/ClipEdits.h"
#include "model/EditCommands.h"
#include "model/NoteEdits.h"
#include "model/Scales.h"

using namespace aether;

namespace
{
    constexpr Tick q = ticksPerQuarter;
    constexpr Tick bar = 4 * q;

    struct Fixture
    {
        Fixture()
        {
            track = edit::addTrack (project, TrackKind::midi).getId();
            clipId = edit::addMidiClip (project, track, "Chords", 0, bar, {});
        }

        Clip clip() const            { return clips::findClip (project, clipId); }
        NoteNodes nodes() const      { return notes::nodesOf (clip()); }
        NoteData note (int i) const  { return notes::read (nodes()[(size_t) i]); }

        Project project;
        juce::String track, clipId;
    };
}

TEST_CASE ("Notes are added, deleted and undone", "[notes][model]")
{
    Fixture f;
    const auto node = edit::addNote (f.project, f.clipId, { 60, 100, q, q });
    REQUIRE (node.isValid());
    REQUIRE (f.nodes().size() == 1);
    CHECK (f.note (0) == NoteData { 60, 100, q, q });
    CHECK (notes::clipOf (node).getId() == f.clipId);
    CHECK (f.project.getUndoManager().getUndoDescription() == "Add note");

    edit::deleteNotes (f.project, { node });
    CHECK (f.nodes().empty());

    f.project.getUndoManager().undo();
    REQUIRE (f.nodes().size() == 1);
    CHECK (f.nodes()[0] == node);   // the same node comes back, so a selection stays valid
}

TEST_CASE ("A note past the end of the clip makes the clip longer, to the end of that bar", "[notes][model]")
{
    Fixture f;
    edit::addNote (f.project, f.clipId, { 64, 90, bar + q, q / 2 });   // in bar 2
    CHECK (f.clip().getLengthTicks() == 2 * bar);

    // One undo step removes the note and puts the length back.
    f.project.getUndoManager().undo();
    CHECK (f.clip().getLengthTicks() == bar);
}

TEST_CASE ("Moving notes keeps them together, on the keyboard and after the clip's start", "[notes][model]")
{
    Fixture f;
    const auto a = edit::addNote (f.project, f.clipId, { 60, 100, q, q });
    const auto b = edit::addNote (f.project, f.clipId, { 126, 100, 2 * q, q });

    edit::moveNotes (f.project, { a, b }, q / 2, 1);
    CHECK (notes::read (a) == NoteData { 61, 100, q + q / 2, q });
    CHECK (notes::read (b) == NoteData { 127, 100, 2 * q + q / 2, q });

    // Too high, and too early: limited so the chord keeps its shape.
    edit::moveNotes (f.project, { a, b }, -10 * q, 5);
    CHECK (notes::read (a).start == 0);
    CHECK (notes::read (b).start == q);
    CHECK (notes::read (b).pitch == 127);
    CHECK (notes::read (a).pitch == 61);

    // A drag is one undo step.
    f.project.getUndoManager().beginNewTransaction();
    edit::moveNotes (f.project, { a }, q, 0, true);
    edit::moveNotes (f.project, { a }, q, 0, false);
    edit::moveNotes (f.project, { a }, q, 0, false);
    CHECK (notes::read (a).start == 3 * q);
    f.project.getUndoManager().undo();
    CHECK (notes::read (a).start == 0);
}

TEST_CASE ("Note lengths and velocities change, within limits", "[notes][model]")
{
    Fixture f;
    const auto a = edit::addNote (f.project, f.clipId, { 60, 100, 0, q });
    const auto b = edit::addNote (f.project, f.clipId, { 64, 120, 0, q / 2 });

    edit::resizeNotes (f.project, { a, b }, -q, q / 8);
    CHECK (notes::read (a).length == q / 8);
    CHECK (notes::read (b).length == q / 8);

    edit::resizeNotes (f.project, { a }, q);
    CHECK (notes::read (a).length == q + q / 8);

    edit::setNoteVelocity (f.project, { a, b }, 200);
    CHECK (notes::read (a).velocity == 127);

    edit::setNoteVelocity (f.project, { a }, 60);
    edit::changeNoteVelocity (f.project, { a, b }, 10);
    CHECK (notes::read (a).velocity == 70);
    CHECK (notes::read (b).velocity == 127);
}

TEST_CASE ("Quantizing moves notes towards the grid, with strength and swing", "[notes][model][quantize]")
{
    const auto s = q / 4;   // a sixteenth

    // Full strength: onto the nearest step.
    CHECK (notes::quantizeTick (s + 100, s, 1.0, 0.0) == s);
    CHECK (notes::quantizeTick (2 * s - 100, s, 1.0, 0.0) == 2 * s);

    // Half strength: half way there.
    CHECK (notes::quantizeTick (s + 1000, s, 0.5, 0.0) == s + 500);
    CHECK (notes::quantizeTick (s + 1000, s, 0.0, 0.0) == s + 1000);

    // Swing pushes every second step later (full swing: a triplet feel).
    CHECK (notes::quantizeTick (2 * s, s, 1.0, 1.0) == 2 * s);        // on the beat: unchanged
    CHECK (notes::quantizeTick (s, s, 1.0, 1.0) == s + s / 3);        // the off-beat: later
    CHECK (notes::quantizeTick (s, s, 1.0, 0.5) == s + s / 6);

    Fixture f;
    const auto a = edit::addNote (f.project, f.clipId, { 60, 100, q + 3000, q - 5000 });
    const auto b = edit::addNote (f.project, f.clipId, { 62, 100, 2 * q - 4000, q / 2 });

    edit::quantizeNotes (f.project, { a, b }, q / 2, 1.0, 0.0, false);
    CHECK (notes::read (a).start == q);
    CHECK (notes::read (a).length == q - 5000);   // ends follow (lengths kept)
    CHECK (notes::read (b).start == 2 * q);
    CHECK (f.project.getUndoManager().getUndoDescription() == "Quantize");

    // Quantizing the ends too.
    edit::quantizeNotes (f.project, { a }, q / 2, 1.0, 0.0, true);
    CHECK (notes::read (a).length == q);
}

TEST_CASE ("Humanize moves notes a little, at random, within the amounts", "[notes][model]")
{
    Fixture f;
    NoteNodes all;
    for (int i = 0; i < 32; ++i)
        all.push_back (edit::addNote (f.project, f.clipId, { 60, 80, i * q / 4 + q, q / 4 }));

    juce::Random random (1234);
    edit::humanizeNotes (f.project, all, 1000, 10, random);

    int moved = 0;
    for (int i = 0; i < 32; ++i)
    {
        const auto n = notes::read (all[(size_t) i]);
        CHECK (std::abs (n.start - (i * q / 4 + q)) <= 1000);
        CHECK (std::abs (n.velocity - 80) <= 10);
        moved += n.start != i * q / 4 + q ? 1 : 0;
    }

    CHECK (moved > 16);
    f.project.getUndoManager().undo();
    CHECK (notes::read (all[5]).start == 5 * q / 4 + q);
}

TEST_CASE ("Duplicating notes puts copies right after them", "[notes][model]")
{
    Fixture f;
    const auto a = edit::addNote (f.project, f.clipId, { 60, 100, 0, q });
    const auto b = edit::addNote (f.project, f.clipId, { 64, 100, q, q / 2 });   // ends at 1.5 beats

    const auto copies = edit::duplicateNotes (f.project, { a, b }, q);
    REQUIRE (copies.size() == 2);
    CHECK (notes::read (copies[0]).start == 2 * q);   // 1.5 beats rounded up to 2
    CHECK (notes::read (copies[1]).start == 3 * q);
    CHECK (f.nodes().size() == 4);

    // Pasting adds several in one step.
    const auto pasted = edit::addNotes (f.project, f.clipId, { { 67, 90, 0, q }, { 71, 90, 0, q } });
    CHECK (pasted.size() == 2);
    f.project.getUndoManager().undo();
    CHECK (f.nodes().size() == 4);
}

TEST_CASE ("Scales say which notes belong to them", "[notes][scales]")
{
    const auto major = scales::find ("major");
    CHECK (scales::contains (major, 0, 60));    // C in C major
    CHECK_FALSE (scales::contains (major, 0, 61));
    CHECK (scales::contains (major, 0, 71));    // B
    CHECK (scales::contains (major, 7, 66));    // F# in G major
    CHECK_FALSE (scales::contains (major, 7, 65));

    const auto minor = scales::find ("minor");
    CHECK (scales::contains (minor, 9, 60));    // C in A minor
    CHECK_FALSE (scales::contains (minor, 9, 61));

    CHECK_FALSE (scales::highlights (scales::find ("off")));
    CHECK_FALSE (scales::highlights (scales::find ("nonsense")));
    CHECK (scales::highlights (scales::find ("blues")));
    CHECK (std::string (scales::keyName (10)) == "A#");

    // Each scale has the expected number of notes.
    CHECK (juce::BigInteger ((int) major.notes).countNumberOfSetBits() == 7);
    CHECK (juce::BigInteger ((int) scales::find ("pentatonicMinor").notes).countNumberOfSetBits() == 5);
    CHECK (juce::BigInteger ((int) scales::find ("blues").notes).countNumberOfSetBits() == 6);

    Project project;
    edit::setScale (project, 14, "minor");
    CHECK ((int) project.getState()[ids::songKey] == 2);
    CHECK (project.getState()[ids::scale].toString() == "minor");
}
