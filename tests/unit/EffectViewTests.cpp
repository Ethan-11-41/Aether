// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
//
// The effects' own windows (Milestone 13): every setting has a control, the presets menu, the
// pictures (drawn for every effect and preset), dragging the EQ's points, and the live meters.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "dsp/Eq.h"
#include "engine/MixerResources.h"
#include "model/EditCommands.h"
#include "model/Mixer.h"
#include "ui/EffectView.h"

using namespace aether;
using Catch::Matchers::WithinAbs;

namespace
{
    using Mods = juce::ModifierKeys;

    struct Fixture
    {
        Fixture()
        {
            track = edit::addTrack (project, TrackKind::audio, -1, true, "Vocals").getId();
            project.getUndoManager().clearUndoHistory();
        }

        static juce::MouseEvent event (juce::Component& c, juce::Point<float> position, juce::Point<float> down, bool dragged = false)
        {
            return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), position, Mods::leftButtonModifier,
                                     juce::MouseInputSource::defaultPressure, 0.0f, 0.0f, 0.0f, 0.0f, &c, &c,
                                     juce::Time::getCurrentTime(), down, juce::Time::getCurrentTime(), 1, dragged);
        }

        void drag (juce::Component& c, juce::Point<float> from, juce::Point<float> to)
        {
            c.mouseDown (event (c, from, from));
            c.mouseDrag (event (c, (from + to) / 2.0f, from, true));
            c.mouseDrag (event (c, to, from, true));
            c.mouseUp (event (c, to, from, true));
        }

        float setting (const juce::String& insertId, const juce::Identifier& id) const
        {
            return mixer::getInsertParameter (mixer::findInsert (project, insertId), id);
        }

        juce::ScopedJuceInitialiser_GUI juceInit;
        ui::Theme theme = ui::Theme::dark();
        Project project;
        juce::String track;
    };
}

TEST_CASE ("Every effect's window has a control for every setting, and draws with every preset", "[ui][effects]")
{
    Fixture f;
    juce::Image image (juce::Image::ARGB, ui::EffectView::idealWidth, 700, true);

    for (const auto& type : dsp::effectTypes())
    {
        if (type.isInstrument)
            continue;

        INFO (type.name);
        auto& undo = f.project.getUndoManager();
        undo.beginNewTransaction();
        const auto insert = edit::addInsert (f.project, mixer::masterId, type.id);
        REQUIRE (insert.isNotEmpty());

        ui::EffectView view (f.theme, f.project, insert);
        CHECK (view.getNumControls() == (int) type.parameters.size());
        CHECK (view.getNumGroups() >= 1);
        CHECK (view.getTitle() == type.name + " - Master");
        CHECK (view.getHeight() == view.getIdealHeight());
        CHECK (view.getHeight() < 700);
        CHECK (view.getPresetBox().getNumItems() == (int) type.presets.size());
        CHECK (view.getPresetText() == type.presets.front().name);   // a new effect is on its first preset
        CHECK (view.getPicture().getHeight() >= 180);

        view.meter = [] { return 6.0f; };
        view.getPicture().tick();

        // Every preset draws (and the switched-off look too).
        for (const auto& preset : type.presets)
        {
            REQUIRE (edit::setInsertPreset (f.project, insert, preset.name));
            CHECK (view.getPresetText() == preset.name);
            juce::Graphics g (image);
            view.paintEntireComponent (g, false);
        }

        edit::setInsertBypassed (f.project, insert, true);
        {
            juce::Graphics g (image);
            view.paintEntireComponent (g, false);
        }

        // A smaller window still fits everything (the picture shrinks, the knobs wrap).
        view.setSize (660, 480);
        juce::Graphics g (image);
        view.paintEntireComponent (g, false);

        edit::removeInsert (f.project, insert);
    }
}

TEST_CASE ("An effect's window shows its preset, moves through them, and follows undo", "[ui][effects]")
{
    Fixture f;
    const auto insert = edit::addInsert (f.project, f.track, "compressor");
    ui::EffectView view (f.theme, f.project, insert);
    CHECK (view.getTitle() == "Compressor - Vocals");
    CHECK (view.getPresetText() == "Gentle (start here)");

    // The next preset in the menu: the first vocal one.
    view.choosePresetOffset (1);
    CHECK (view.getPresetText() == "Vocal: even");
    CHECK (f.project.getUndoManager().getUndoDescription() == "Choose \"Vocal: even\" (Compressor)");
    view.choosePresetOffset (-1);
    CHECK (view.getPresetText() == "Gentle (start here)");
    view.choosePresetOffset (-1);   // wraps around to the last
    CHECK (view.getPresetText() == "Master: punchy");

    // Choosing from the menu.
    const auto* type = dsp::findEffectType ("compressor");
    view.getPresetBox().setSelectedId (dsp::findPresetIndex (*type, "Vocal: rap / spoken") + 1, juce::sendNotificationSync);
    CHECK (view.getPresetText() == "Vocal: rap / spoken");
    CHECK (f.setting (insert, "ratio") == 6.0f);

    // Changing a setting: "(changed)"; undo brings the preset back.
    edit::setInsertParameter (f.project, insert, "ratio", 10.0f);
    CHECK (view.getPresetText() == "Vocal: rap / spoken (changed)");
    f.project.getUndoManager().undo();
    CHECK (view.getPresetText() == "Vocal: rap / spoken");

    // The track's name in the title.
    int titleChanges = 0;
    view.onTitleChanged = [&] { ++titleChanges; };
    edit::renameTrack (f.project, f.track, "Lead vocal");
    CHECK (titleChanges == 1);
    CHECK (view.getTitle() == "Compressor - Lead vocal");
}

TEST_CASE ("An effect's window asks to close when its effect is removed", "[ui][effects]")
{
    Fixture f;
    const auto insert = edit::addInsert (f.project, f.track, "reverb");
    ui::EffectView view (f.theme, f.project, insert);
    int closes = 0;
    view.onEffectRemoved = [&] { ++closes; };

    edit::addInsert (f.project, f.track, "delay");   // another effect: stays open
    CHECK (closes == 0);

    edit::removeInsert (f.project, insert);
    CHECK (closes == 1);
    f.project.getUndoManager().undo();   // (already closing)
    CHECK (closes == 1);
}

TEST_CASE ("Dragging the EQ's points changes the bands, one undo step per drag", "[ui][effects][eq]")
{
    Fixture f;
    const auto insert = edit::addInsert (f.project, f.track, "eq");
    ui::EffectView view (f.theme, f.project, insert);
    auto& picture = view.getPicture();
    REQUIRE (picture.getNumHandles() == 7);

    // The mids point (the fourth): up and to the right.
    const auto from = picture.getHandlePosition (3);
    f.drag (picture, from, from + juce::Point<float> (60.0f, -40.0f));
    CHECK (f.setting (insert, "band2Gain") > 3.0f);
    CHECK (f.setting (insert, "band2Freq") > 1300.0f);
    CHECK (f.project.getUndoManager().getUndoDescription() == "Change EQ mids");
    CHECK (view.getPresetText() == "Flat (start here) (changed)");

    // The point follows the setting.
    CHECK (picture.getHandlePosition (3).getDistanceFrom (from + juce::Point<float> (60.0f, -40.0f)) < 2.0f);

    // Double-click: back to 0 dB.
    const auto p = picture.getHandlePosition (3);
    picture.mouseDoubleClick (Fixture::event (picture, p, p));
    CHECK (f.setting (insert, "band2Gain") == 0.0f);

    // Dragging the low cut (off at first) switches it on.
    CHECK (f.setting (insert, "lowCutOn") == 0.0f);
    const auto cut = picture.getHandlePosition (0);
    f.drag (picture, cut, cut + juce::Point<float> (30.0f, 0.0f));
    CHECK (f.setting (insert, "lowCutOn") == 1.0f);
    CHECK (f.setting (insert, "lowCut") > 80.0f);

    // One drag, one step.
    f.project.getUndoManager().undo();
    CHECK (f.setting (insert, "lowCutOn") == 0.0f);
    CHECK_THAT (f.setting (insert, "lowCut"), WithinAbs (80.0, 0.01));

    // Scrolling over a bell band makes it wider or narrower.
    const auto band = picture.getHandlePosition (2);
    juce::MouseWheelDetails wheel {};
    wheel.deltaY = 0.25f;
    picture.mouseWheelMove (Fixture::event (picture, band, band), wheel);
    CHECK (f.setting (insert, "band1Q") < 1.0f);

    // Clicking where there's no point changes nothing.
    const auto before = f.project.getUndoManager().getNumActionsInCurrentTransaction();
    f.drag (picture, { 2.0f, 2.0f }, { 50.0f, 50.0f });
    CHECK (f.project.getUndoManager().getNumActionsInCurrentTransaction() == before);
}

TEST_CASE ("The engine tells an effect's window what the effect is doing", "[engine][effects]")
{
    MixerResources resources;
    resources.beginBuild();
    auto compressor = resources.effect ("insert-1", "compressor", 48000.0);
    resources.endBuild();
    REQUIRE (compressor != nullptr);
    CHECK (resources.effectMeter ("insert-1") == 0.0f);
    CHECK (resources.effectMeter ("no-such-insert") == 0.0f);

    // A loud sine, well above the threshold.
    const auto* type = dsp::findEffectType ("compressor");
    dsp::EffectParameterValues values {};
    for (size_t i = 0; i < type->parameters.size(); ++i)
        values[i] = type->parameters[i].defaultValue;

    std::vector<float> left (256), right (256);
    for (int block = 0; block < 200; ++block)
    {
        for (size_t i = 0; i < left.size(); ++i)
            left[i] = right[i] = 0.9f * (float) std::sin (0.05 * (double) (block * 256 + (int) i));

        compressor->process (left.data(), right.data(), (int) left.size(), values, {});
    }

    CHECK (resources.effectMeter ("insert-1") > 3.0f);
}
