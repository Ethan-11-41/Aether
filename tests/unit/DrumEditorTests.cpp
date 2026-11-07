// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
//
// Drives the drum editor the way the mouse would, without opening a window.

#include <catch2/catch_test_macros.hpp>

#include "model/ClipEdits.h"
#include "model/EditCommands.h"
#include "model/Mixer.h"
#include "ui/DrumEditor.h"

using namespace aether;
namespace kit = aether::dsp::drumkit;

namespace
{
    constexpr Tick q = ticksPerQuarter;
    using Mods = juce::ModifierKeys;
    constexpr int kick = 0, snare = 2, closedHat = 6;

    struct Fixture
    {
        Fixture()
        {
            track = edit::addDrumTrack (project).getId();
            clip = edit::addPattern (project, track, 0, 4 * q, "Beat");
            project.getUndoManager().clearUndoHistory();
            editor.playPad = [this] (int note, int velocity) { heard.push_back ({ note, velocity }); };
            editor.setClip (clip);
        }

        Clip c() const   { return clips::findClip (project, clip); }
        int velocity (int pad, int step) const   { return drumpattern::stepVelocity (c(), pad, step); }

        static juce::MouseEvent event (juce::Component& comp, juce::Point<float> position, juce::Point<float> down,
                                       Mods mods = Mods::leftButtonModifier, bool dragged = false)
        {
            return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), position, mods,
                                     juce::MouseInputSource::defaultPressure, 0.0f, 0.0f, 0.0f, 0.0f, &comp, &comp,
                                     juce::Time::getCurrentTime(), down, juce::Time::getCurrentTime(), 1, dragged);
        }

        void click (juce::Component& comp, juce::Point<float> p, Mods mods = Mods::leftButtonModifier)
        {
            comp.mouseDown (event (comp, p, p, mods));
            comp.mouseUp (event (comp, p, p, mods));
        }

        void drag (juce::Component& comp, juce::Point<float> from, juce::Point<float> to, Mods mods = Mods::leftButtonModifier)
        {
            comp.mouseDown (event (comp, from, from, mods));
            for (int i = 1; i <= 8; ++i)
                comp.mouseDrag (event (comp, from + (to - from) * ((float) i / 8.0f), from, mods, true));
            comp.mouseUp (event (comp, to, from, mods, true));
        }

        juce::ScopedJuceInitialiser_GUI juceInit;
        Project project;
        AudioEngine engine;
        ui::Theme theme = ui::Theme::dark();
        ui::DrumEditor editor { theme, project, engine };
        juce::String track, clip;
        std::vector<std::pair<int, int>> heard;
    };
}

TEST_CASE ("The drum editor shows a pattern's steps and the kit's pads", "[drums][ui]")
{
    Fixture f;
    CHECK (f.editor.isShowingSteps());
    CHECK (f.editor.getNumSteps() == 16);
    CHECK (f.editor.getTitle() == "Drums - Beat");
    CHECK (f.editor.getPadName (kick) == juce::String (dsp::drums::findSound (dsp::drums::kits().front().pads[0].sound)->name));
    CHECK (f.editor.getKitBox().getText() == juce::String (dsp::drums::kits().front().name));
    CHECK (f.editor.getLengthBox().getText() == "1 bar");
    CHECK (f.editor.getStepBox().getText() == "1/16 steps");

    // Every step is on screen, and the pads line up with their rows.
    auto& area = f.editor.getStepArea();
    CHECK (f.editor.stepPosition (kick, 15).x < (float) area.getWidth());
    CHECK (f.editor.padPosition (snare).y == f.editor.stepPosition (snare, 0).y);
}

TEST_CASE ("Clicking steps turns them on and off; dragging does several in one undo step", "[drums][ui]")
{
    Fixture f;
    auto& area = f.editor.getStepArea();

    f.click (area, f.editor.stepPosition (kick, 0));
    CHECK (f.velocity (kick, 0) == drumpattern::defaultVelocity);
    REQUIRE (f.heard.size() == 1);
    CHECK (f.heard[0].first == 36);   // you hear the pad as you add it

    f.click (area, f.editor.stepPosition (kick, 0));
    CHECK (f.velocity (kick, 0) == 0);

    // Drag along the hi-hat row: every step from 0 to 7.
    f.drag (area, f.editor.stepPosition (closedHat, 0), f.editor.stepPosition (closedHat, 7));
    for (int step = 0; step < 8; ++step)
        CHECK (f.velocity (closedHat, step) > 0);
    CHECK (f.velocity (closedHat, 8) == 0);
    CHECK (f.editor.getSelectedPad() == closedHat);

    f.project.getUndoManager().undo();
    CHECK (midi::notesOf (f.c()).empty());
    f.project.getUndoManager().redo();

    // Shift+click: an accent. Right-click: clear.
    f.click (area, f.editor.stepPosition (snare, 4), Mods (Mods::leftButtonModifier | Mods::shiftModifier));
    CHECK (f.velocity (snare, 4) == drumpattern::accentVelocity);
    f.click (area, f.editor.stepPosition (closedHat, 3), Mods::rightButtonModifier);
    CHECK (f.velocity (closedHat, 3) == 0);

    // Dragging over steps that are on (starting on an empty one) leaves them on.
    f.drag (area, f.editor.stepPosition (closedHat, 3), f.editor.stepPosition (closedHat, 5));
    CHECK (f.velocity (closedHat, 3) > 0);
    CHECK (f.velocity (closedHat, 4) > 0);
}

TEST_CASE ("The velocity lane changes how hard the selected pad's steps are hit", "[drums][ui]")
{
    Fixture f;
    edit::setStep (f.project, f.clip, snare, 4, 100);
    edit::setStep (f.project, f.clip, snare, 12, 100);
    f.editor.selectPad (snare);
    auto& area = f.editor.getStepArea();

    f.drag (area, f.editor.lanePosition (4, 100), f.editor.lanePosition (4, 40));
    CHECK (std::abs (f.velocity (snare, 4) - 40) <= 2);
    CHECK (f.velocity (snare, 12) == 100);
    CHECK (f.project.getUndoManager().getUndoDescription() == "Change step velocity");

    // Steps that are off stay off.
    f.drag (area, f.editor.lanePosition (6, 100), f.editor.lanePosition (6, 30));
    CHECK (f.velocity (snare, 6) == 0);
}

TEST_CASE ("The drum editor's length, step size, swing and kit", "[drums][ui]")
{
    Fixture f;
    edit::setStep (f.project, f.clip, kick, 0, 100);

    f.editor.getLengthBox().setSelectedId (2, juce::sendNotificationSync);   // 2 bars
    CHECK (f.c().getPatternLength() == 8 * q);
    CHECK (f.editor.getNumSteps() == 32);
    CHECK (f.velocity (kick, 16) == 100);         // the copy

    f.editor.getStepBox().setSelectedId (3, juce::sendNotificationSync);     // 1/32 steps
    CHECK (f.editor.getNumSteps() == 64);
    f.project.getUndoManager().undo();
    CHECK (f.editor.getStepBox().getText() == "1/16 steps");   // follows undo

    edit::setStep (f.project, f.clip, closedHat, 1, 100);
    f.editor.getSwingSlider().setValue (60.0, juce::sendNotificationSync);
    CHECK (drumpattern::swingOf (f.c()) == 0.6);
    CHECK (f.velocity (closedHat, 1) == 100);

    f.editor.getKitBox().setSelectedId (2, juce::sendNotificationSync);
    const auto& second = dsp::drums::kits()[1];
    CHECK (midi::instrumentOf (f.project, f.track)[ids::preset].toString() == juce::String (second.name));
    CHECK (f.editor.getPadName (kick) == juce::String (dsp::drums::findSound (second.pads[0].sound)->name));
}

TEST_CASE ("The pad panel edits the selected pad", "[drums][ui]")
{
    Fixture f;
    f.editor.selectPad (snare);
    CHECK (f.editor.getSelectedPad() == snare);

    auto* tune = f.editor.findKnob (kit::tune);
    REQUIRE (tune != nullptr);
    CHECK (tune->getName() == "pad3Tune");

    tune->onGestureStart();
    tune->onValueChange (5.0);
    const auto instrument = midi::instrumentOf (f.project, f.track);
    CHECK (mixer::getInsertParameter (instrument, "pad3Tune") == 5.0f);
    CHECK (f.project.getUndoManager().getUndoDescription() == "Change pad 3 tune");

    f.editor.getChokeBox().setSelectedId (3, juce::sendNotificationSync);   // group 2
    CHECK (mixer::getInsertParameter (instrument, "pad3Choke") == 2.0f);

    f.editor.chooseSound (snare, "builtin:clap");
    CHECK (f.editor.getPadName (snare) == "Clap");
    CHECK (! f.heard.empty());
    f.project.getUndoManager().undo();
    CHECK (f.editor.getPadName (snare) != "Clap");

    f.editor.getKitVolumeKnob()->onValueChange (-6.0);
    CHECK (mixer::getInsertParameter (instrument, "volume") == -6.0f);
}

TEST_CASE ("The drum editor offers a pattern, or the drum kit, when there's nothing to show", "[drums][ui]")
{
    Fixture f;
    const auto empty = edit::addDrumTrack (f.project).getId();
    f.editor.setTrack (empty);
    CHECK_FALSE (f.editor.isShowingSteps());
    CHECK (f.editor.getAddPatternButton().isVisible());

    f.editor.getAddPatternButton().onClick();
    CHECK (f.editor.isShowingSteps());
    CHECK (f.editor.getClipId().isNotEmpty());
    CHECK (clips::findClip (f.project, f.editor.getClipId()).isLooping());

    // Showing a drum track finds its first pattern.
    f.editor.setTrack (f.track);
    CHECK (f.editor.getClipId() == f.clip);

    // A track that doesn't play the drum kit (e.g. after undoing): offer the kit.
    const auto keys = edit::addTrack (f.project, TrackKind::midi).getId();
    f.editor.setTrack (keys);
    CHECK_FALSE (f.editor.isShowingSteps());
    f.editor.getAddPatternButton().onClick();
    CHECK (drumpattern::isDrumTrack (f.project, keys));

    // The track goes (e.g. its creation is undone): nothing to offer.
    edit::deleteTracks (f.project, { keys });
    CHECK_FALSE (f.editor.isShowingSteps());
    CHECK_FALSE (f.editor.getAddPatternButton().isVisible());
}
