// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
//
// Drives the piano roll the way the mouse and keyboard would, without opening a window.

#include <catch2/catch_test_macros.hpp>

#include "model/ClipEdits.h"
#include "model/EditCommands.h"
#include "model/NoteEdits.h"
#include "ui/PianoRoll.h"

using namespace aether;

namespace
{
    constexpr Tick q = ticksPerQuarter;
    using Mods = juce::ModifierKeys;

    struct Fixture
    {
        Fixture()
        {
            track = edit::addTrack (project, TrackKind::midi).getId();
            clipId = edit::addMidiClip (project, track, "Tune", 0, 4 * q, {});
            roll.setSize (1000, 640);
            roll.previewNote = [this] (int pitch, int, bool on) { previews.push_back ({ pitch, on }); };
            roll.setClip (clipId);
            roll.scrollToPitch (62);
        }

        juce::Component& grid()   { return roll.getNoteGrid(); }

        /** The middle of the cell for a note position, in the note grid. */
        juce::Point<float> at (Tick tick, int pitch)
        {
            return roll.positionOf (tick, pitch).toFloat() + juce::Point<float> (2.0f, (float) roll.getRowHeight() / 2.0f);
        }

        static juce::MouseEvent event (juce::Component& c, juce::Point<float> position, juce::Point<float> down,
                                       Mods mods = Mods::leftButtonModifier, bool dragged = false)
        {
            return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), position, mods,
                                     juce::MouseInputSource::defaultPressure, 0.0f, 0.0f, 0.0f, 0.0f, &c, &c,
                                     juce::Time::getCurrentTime(), down, juce::Time::getCurrentTime(), 1, dragged);
        }

        void click (juce::Component& c, juce::Point<float> p, Mods mods = Mods::leftButtonModifier)
        {
            c.mouseDown (event (c, p, p, mods));
            c.mouseUp (event (c, p, p, mods));
        }

        void drag (juce::Component& c, juce::Point<float> from, juce::Point<float> to, Mods mods = Mods::leftButtonModifier)
        {
            c.mouseDown (event (c, from, from, mods));
            c.mouseDrag (event (c, (from + to) / 2.0f, from, mods, true));
            c.mouseDrag (event (c, to, from, mods, true));
            c.mouseUp (event (c, to, from, mods, true));
        }

        NoteNodes nodes() const        { return notes::nodesOf (clips::findClip (project, clipId)); }
        NoteData note (int i) const    { return notes::read (nodes()[(size_t) i]); }
        juce::String undoName()        { return project.getUndoManager().getUndoDescription(); }

        juce::ScopedJuceInitialiser_GUI juceInit;
        ui::Theme theme = ui::Theme::dark();
        Project project;
        AudioEngine engine;
        juce::String track, clipId;
        ui::PianoRoll roll { theme, project, engine };
        std::vector<std::pair<int, bool>> previews;
    };
}

TEST_CASE ("Clicking an empty place adds a note there, and you hear it", "[ui][pianoroll]")
{
    Fixture f;
    REQUIRE (f.roll.getClipId() == f.clipId);

    // A click a little way into beat 2 puts a note at the start of that sixteenth.
    f.click (f.grid(), f.at (q + q / 8, 64));
    REQUIRE (f.nodes().size() == 1);
    CHECK (f.note (0) == NoteData { 64, 100, q, q / 4 });   // a sixteenth, the grid size
    CHECK (f.undoName() == "Add note");
    CHECK (f.roll.getSelection().size() == 1);

    // Heard while the mouse button is down.
    REQUIRE (f.previews.size() == 2);
    CHECK (f.previews[0] == std::pair<int, bool> { 64, true });
    CHECK (f.previews[1] == std::pair<int, bool> { 64, false });

    // With the sound switched off: silent.
    f.roll.setPreviewEnabled (false);
    f.click (f.grid(), f.at (2 * q, 60));
    CHECK (f.previews.size() == 2);
    CHECK (f.nodes().size() == 2);
}

TEST_CASE ("Dragging while adding a note makes it longer, and the next note is as long", "[ui][pianoroll]")
{
    Fixture f;
    f.drag (f.grid(), f.at (0, 60), f.at (q, 60));
    REQUIRE (f.nodes().size() == 1);
    CHECK (f.note (0).length == q + q / 4);

    // One undo step for adding and stretching.
    f.project.getUndoManager().undo();
    CHECK (f.nodes().empty());
    f.project.getUndoManager().redo();

    f.click (f.grid(), f.at (2 * q, 62));
    CHECK (f.note (1).length == q + q / 4);   // "last used" length

    // A fixed length instead.
    f.roll.setFixedNoteLength (q / 2);
    f.click (f.grid(), f.at (3 * q, 64));
    CHECK (f.note (2).length == q / 2);
}

TEST_CASE ("Dragging a note moves it; dragging its right edge changes its length", "[ui][pianoroll]")
{
    Fixture f;
    edit::addNote (f.project, f.clipId, { 60, 90, q, q });

    // Grab the middle, move a beat later and two semitones up.
    f.drag (f.grid(), f.at (q + q / 2, 60), f.at (2 * q + q / 2, 62));
    CHECK (f.note (0) == NoteData { 62, 90, 2 * q, q });
    CHECK (f.undoName() == "Move note");

    f.project.getUndoManager().undo();
    CHECK (f.note (0) == NoteData { 60, 90, q, q });

    // The right edge: half a beat longer.
    const auto edge = f.roll.positionOf (2 * q, 60).toFloat() + juce::Point<float> (-2.0f, 5.0f);
    f.drag (f.grid(), edge, edge + juce::Point<float> ((float) (f.roll.getView().tickToX (q / 2) - f.roll.getView().tickToX (0)), 0.0f));
    CHECK (f.note (0).length == q + q / 2);
    CHECK (f.undoName() == "Change note length");
}

TEST_CASE ("Right-click deletes a note; Ctrl+drag selects with a box", "[ui][pianoroll]")
{
    Fixture f;
    edit::addNote (f.project, f.clipId, { 60, 100, 0, q / 2 });
    edit::addNote (f.project, f.clipId, { 62, 100, q, q / 2 });
    edit::addNote (f.project, f.clipId, { 64, 100, 2 * q, q / 2 });

    f.click (f.grid(), f.at (q, 62), Mods (Mods::rightButtonModifier));
    CHECK (f.nodes().size() == 2);

    // A box from before the first note to past the last one, over both their rows.
    f.drag (f.grid(), f.at (0, 65) - juce::Point<float> (1.0f, 0.0f), f.at (2 * q + q / 4, 59),
            Mods (Mods::leftButtonModifier | Mods::commandModifier));
    CHECK (f.roll.getSelection().size() == 2);
    CHECK (f.nodes().size() == 2);   // no note was added

    // Ctrl+click takes one away again.
    f.click (f.grid(), f.at (0, 60), Mods (Mods::leftButtonModifier | Mods::commandModifier));
    CHECK (f.roll.getSelection().size() == 1);
}

TEST_CASE ("The piano roll's keys edit the selected notes", "[ui][pianoroll]")
{
    Fixture f;
    const auto a = edit::addNote (f.project, f.clipId, { 60, 100, q + 3000, q });
    const auto b = edit::addNote (f.project, f.clipId, { 64, 100, 2 * q, q });
    f.roll.setSelection ({ a, b });

    using KP = juce::KeyPress;
    CHECK (f.roll.keyPressed (KP (KP::upKey)));
    CHECK (notes::read (a).pitch == 61);
    CHECK (f.roll.keyPressed (KP (KP::downKey, Mods::shiftModifier, 0)));
    CHECK (notes::read (b).pitch == 53);

    CHECK (f.roll.keyPressed (KP ('q', 0, 'q')));
    CHECK (notes::read (a).start == q);   // quantized to the sixteenth grid

    CHECK (f.roll.keyPressed (KP (KP::rightKey)));
    CHECK (notes::read (a).start == q + q / 4);

    CHECK (f.roll.keyPressed (KP ('d', Mods::commandModifier, 'd')));
    CHECK (f.nodes().size() == 4);
    CHECK (f.roll.getSelection().size() == 2);   // the copies

    CHECK (f.roll.keyPressed (KP (KP::deleteKey)));
    CHECK (f.nodes().size() == 2);

    // Copy and paste at the playhead.
    f.roll.setSelection ({ a });
    CHECK (f.roll.keyPressed (KP ('c', Mods::commandModifier, 'c')));
    f.engine.locate (3 * q);
    f.roll.paste();
    CHECK (f.nodes().size() == 3);
    CHECK (notes::read (f.roll.getSelection()[0]).start == 3 * q);

    CHECK (f.roll.keyPressed (KP ('a', Mods::commandModifier, 'a')));
    CHECK (f.roll.getSelection().size() == 3);
    CHECK (f.roll.keyPressed (KP (KP::escapeKey)));
    CHECK (f.roll.getSelection().empty());

    // Keys the piano roll doesn't use go on to Aether's shortcuts.
    CHECK_FALSE (f.roll.keyPressed (KP (KP::spaceKey)));
    CHECK_FALSE (f.roll.keyPressed (KP ('z', Mods::commandModifier, 'z')));
}

TEST_CASE ("Dragging in the velocity lane changes how hard notes are played", "[ui][pianoroll]")
{
    Fixture f;
    const auto a = edit::addNote (f.project, f.clipId, { 60, 100, q, q });
    edit::addNote (f.project, f.clipId, { 64, 100, 2 * q, q });

    auto& lane = f.roll.getVelocityLane();
    const auto x = (float) f.roll.positionOf (q, 60).x;
    f.drag (lane, { x, 50.0f }, { x, (float) lane.getHeight() - 1.0f });   // to the bottom: very soft

    CHECK (notes::read (a).velocity < 10);
    CHECK (f.note (1).velocity == 100);   // the other note is untouched
    CHECK (f.undoName() == "Change velocity");

    f.drag (lane, { x, 50.0f }, { x, 0.0f });   // to the top: as hard as possible
    CHECK (notes::read (a).velocity == 127);
}

TEST_CASE ("Clicking the piano roll's keys plays them, even with the sound switched off", "[ui][pianoroll]")
{
    Fixture f;
    f.roll.setPreviewEnabled (false);
    f.click (f.roll.getKeys(), f.at (0, 67).withX (10.0f));
    REQUIRE (f.previews.size() == 2);
    CHECK (f.previews[0] == std::pair<int, bool> { 67, true });
}

TEST_CASE ("The piano roll empties when its clip is deleted", "[ui][pianoroll]")
{
    Fixture f;
    bool told = false;
    f.roll.onClipChanged = [&] { told = true; };

    edit::deleteClips (f.project, { f.clipId });
    CHECK (f.roll.getClipId().isEmpty());
    CHECK (told);

    // Clicking does nothing now.
    f.click (f.grid(), { 100.0f, 100.0f });

    // Undoing the delete doesn't reopen it by itself.
    f.project.getUndoManager().undo();
    CHECK (f.roll.getClipId().isEmpty());
}
