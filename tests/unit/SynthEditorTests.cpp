// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
//
// The Synth's window, driven the way the mouse would, without opening a window.

#include <catch2/catch_test_macros.hpp>

#include "dsp/Synth.h"
#include "model/EditCommands.h"
#include "model/MidiClips.h"
#include "model/Mixer.h"
#include "ui/SynthEditor.h"

using namespace aether;

namespace
{
    struct Fixture
    {
        Fixture()
        {
            track = edit::addTrack (project, TrackKind::midi, -1, true, "Keys").getId();
            editor = std::make_unique<ui::SynthEditor> (theme, project, track);
            editor->setSize (1120, 760);
        }

        float value (const juce::String& id) const
        {
            return mixer::getInsertParameter (midi::instrumentOf (project, track), id);
        }

        static juce::MouseEvent event (juce::Component& c, juce::Point<float> position, juce::Point<float> down)
        {
            return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), position, juce::ModifierKeys::leftButtonModifier,
                                     juce::MouseInputSource::defaultPressure, 0.0f, 0.0f, 0.0f, 0.0f, &c, &c,
                                     juce::Time::getCurrentTime(), down, juce::Time::getCurrentTime(), 1, true);
        }

        juce::ScopedJuceInitialiser_GUI juceInit;
        ui::Theme theme = ui::Theme::dark();
        Project project;
        juce::String track;
        std::unique_ptr<ui::SynthEditor> editor;
    };
}

TEST_CASE ("The Synth window shows the track's preset and its settings", "[ui][synth]")
{
    Fixture f;
    CHECK (f.editor->getTitle() == "Synth - Keys");
    CHECK (f.editor->getPresetText() == "Soft keys");
    CHECK (f.editor->getPage() == ui::SynthEditor::Page::simple);

    REQUIRE (f.editor->findKnob ("cutoff") != nullptr);
    CHECK (f.editor->findKnob ("lfo1Rate") == nullptr);   // only on the Advanced page
    REQUIRE (f.editor->findChoice ("waveA") != nullptr);
    CHECK (f.editor->findChoice ("waveA")->getText() == "Sine");   // Soft keys

    f.editor->showPage (ui::SynthEditor::Page::advanced);
    CHECK (f.editor->findKnob ("lfo1Rate") != nullptr);

    // Every setting is on the Advanced page, with an explanation.
    for (const auto& p : dsp::findEffectType (dsp::synth::typeId)->parameters)
    {
        INFO (p.id.toString());
        if (p.isChoice())
        {
            auto* box = f.editor->findChoice (p.id.toString());
            REQUIRE (box != nullptr);
            CHECK (box->getTooltip().length() > 20);
        }
        else
        {
            auto* knob = f.editor->findKnob (p.id.toString());
            REQUIRE (knob != nullptr);
            CHECK (knob->getTooltip().length() > 40);
        }
    }
}

TEST_CASE ("Turning a Synth knob changes the sound, undoably, and marks the preset as changed", "[ui][synth]")
{
    Fixture f;
    auto* knob = f.editor->findKnob ("cutoff");
    REQUIRE (knob != nullptr);
    knob->setBounds (0, 0, 72, 86);
    const auto before = f.value ("cutoff");

    const juce::Point<float> start (36.0f, 24.0f);
    knob->mouseDown (Fixture::event (*knob, start, start));
    knob->mouseDrag (Fixture::event (*knob, start - juce::Point<float> (0.0f, 30.0f), start));
    knob->mouseUp (Fixture::event (*knob, start - juce::Point<float> (0.0f, 30.0f), start));

    CHECK (f.value ("cutoff") > before);
    CHECK (f.project.getUndoManager().getUndoDescription() == "Change Synth cutoff");
    CHECK (f.editor->getPresetText() == "Soft keys (changed)");

    f.project.getUndoManager().undo();
    CHECK (f.value ("cutoff") == before);
    CHECK (f.editor->getPresetText() == "Soft keys");

    // A drop-down.
    auto* wave = f.editor->findChoice ("waveA");
    REQUIRE (wave != nullptr);
    wave->setSelectedId (3, juce::sendNotificationSync);   // Saw
    CHECK (f.value ("waveA") == 2.0f);
}

TEST_CASE ("Choosing presets in the Synth window", "[ui][synth]")
{
    Fixture f;
    const auto& presets = dsp::findEffectType (dsp::synth::typeId)->presets;

    f.editor->choosePresetOffset (1);
    CHECK (f.editor->getPresetText() == presets[1].name);
    f.editor->choosePresetOffset (-2);   // past the first: wraps round to the last
    CHECK (f.editor->getPresetText() == presets.back().name);

    // From the menu.
    for (size_t i = 0; i < presets.size(); ++i)
        if (presets[i].name == "Warm pad")
            f.editor->getPresetBox().setSelectedId ((int) i + 1, juce::sendNotificationSync);

    CHECK (midi::instrumentOf (f.project, f.track)[ids::preset].toString() == "Warm pad");
    CHECK (f.value ("attack") == 700.0f);
}

TEST_CASE ("A track with the starter synth can switch to the Synth from its window", "[ui][synth]")
{
    Fixture f;
    edit::setInstrumentType (f.project, f.track, "synth", "Organ");
    f.editor->setTrack (f.track);
    const auto* knob = f.editor->findKnob ("cutoff");
    CHECK ((knob == nullptr || ! knob->isVisible() || ! knob->getParentComponent()->getParentComponent()->isVisible()));

    // The window offers the Synth instead.
    bool offered = false;
    for (auto* child : f.editor->getChildren())
        if (auto* button = dynamic_cast<juce::TextButton*> (child); button != nullptr && button->isVisible()
            && button->getButtonText().contains ("Use the Synth"))
        {
            offered = true;
            button->onClick();
        }

    CHECK (offered);
    CHECK (midi::instrumentOf (f.project, f.track)[ids::type].toString() == dsp::synth::typeId);
}

TEST_CASE ("The Synth window draws both pages", "[ui][synth]")
{
    Fixture f;
    edit::setInstrumentPreset (f.project, f.track, "Evolving pad");

    for (const auto page : { ui::SynthEditor::Page::simple, ui::SynthEditor::Page::advanced })
    {
        f.editor->showPage (page);
        juce::Image image (juce::Image::ARGB, f.editor->getWidth(), f.editor->getHeight(), true);
        juce::Graphics g (image);
        f.editor->paintEntireComponent (g, false);
        CHECK (image.getPixelAt (f.editor->getWidth() / 2, f.editor->getHeight() / 2).getAlpha() > 0);
    }
}
