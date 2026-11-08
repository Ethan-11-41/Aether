// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
//
// Selecting several tracks, and the mixer panel, driven without opening a window.

#include <catch2/catch_test_macros.hpp>

#include "model/EditCommands.h"
#include "model/Mixer.h"
#include "ui/DelayedTooltipWindow.h"
#include "ui/EffectEditor.h"
#include "ui/MixerComponent.h"
#include "ui/TrackListComponent.h"

using namespace aether;

namespace
{
    constexpr auto ctrl = juce::ModifierKeys::commandModifier;
    constexpr auto shift = juce::ModifierKeys::shiftModifier;

    struct Fixture
    {
        Fixture()
        {
            for (int i = 0; i < 4; ++i)
                ids.add (edit::addTrack (project, TrackKind::audio).getId());

            list.setSize (300, list.getIdealHeight());
            mixer.setSize (900, 340);
            project.getUndoManager().clearUndoHistory();
        }

        juce::ScopedJuceInitialiser_GUI juce;
        ui::Theme theme = ui::Theme::dark();
        Project project;
        juce::StringArray ids;
        ui::TrackListComponent list { theme, project };
        ui::MixerComponent mixer { theme, project, list };
    };
}

TEST_CASE ("Ctrl+click and Shift+click select several tracks", "[ui][tracks][selection]")
{
    Fixture f;
    f.list.selectTrack (f.ids[0]);
    f.list.selectTrack (f.ids[2], juce::ModifierKeys (ctrl));
    CHECK (f.list.getSelectedTrackIds() == juce::StringArray { f.ids[0], f.ids[2] });
    CHECK (f.list.getSelectedTrackId() == f.ids[2]);

    // Ctrl+click again takes it out.
    f.list.selectTrack (f.ids[2], juce::ModifierKeys (ctrl));
    CHECK (f.list.getSelectedTrackIds() == juce::StringArray { f.ids[0] });

    // Shift+click: everything from the last clicked track (as in Windows Explorer, that's the
    // last one Ctrl+clicked, even if that click took it out).
    f.list.selectTrack (f.ids[3], juce::ModifierKeys (shift));
    CHECK (f.list.getSelectedTrackIds() == juce::StringArray { f.ids[2], f.ids[3] });

    f.list.selectTrack (f.ids[0]);
    f.list.selectTrack (f.ids[3], juce::ModifierKeys (shift));
    CHECK (f.list.getSelectedTrackIds() == juce::StringArray { f.ids[0], f.ids[1], f.ids[2], f.ids[3] });

    // A plain click selects just one again.
    f.list.selectTrack (f.ids[1], juce::ModifierKeys());
    CHECK (f.list.getSelectedTrackIds() == juce::StringArray { f.ids[1] });

    // Shift+Down extends the selection.
    f.list.keyPressed (juce::KeyPress (juce::KeyPress::downKey, juce::ModifierKeys (shift), 0));
    CHECK (f.list.getSelectedTrackIds() == juce::StringArray { f.ids[1], f.ids[2] });
}

TEST_CASE ("A control on a selected track changes every selected track", "[ui][tracks][selection][mixer]")
{
    Fixture f;
    f.list.selectTrack (f.ids[0]);
    f.list.selectTrack (f.ids[1], juce::ModifierKeys (ctrl));

    CHECK (f.list.tracksToEdit (f.ids[0]) == juce::StringArray { f.ids[0], f.ids[1] });
    CHECK (f.list.tracksToEdit (f.ids[3]) == juce::StringArray { f.ids[3] });   // not selected: just itself

    auto* strip = f.mixer.findStrip (f.ids[1]);
    REQUIRE (strip != nullptr);

    strip->getMuteButton().onClick();
    CHECK (f.project.findTrack (f.ids[0]).isMuted());
    CHECK (f.project.findTrack (f.ids[1]).isMuted());
    CHECK_FALSE (f.project.findTrack (f.ids[2]).isMuted());

    // A fader drag on one moves both by the same amount, as one undo step.
    edit::setVolume (f.project, f.ids[0], -4.0);
    f.project.getUndoManager().clearUndoHistory();

    auto& fader = strip->getFader();
    fader.onGestureStart();
    fader.onValueChange (-3.0);
    fader.onValueChange (-6.0);
    fader.onGestureEnd();

    CHECK (f.project.findTrack (f.ids[1]).getVolumeDb() == -6.0);
    CHECK (f.project.findTrack (f.ids[0]).getVolumeDb() == -10.0);
    CHECK (f.project.findTrack (f.ids[2]).getVolumeDb() == 0.0);

    f.project.getUndoManager().undo();
    CHECK (f.project.findTrack (f.ids[0]).getVolumeDb() == -4.0);
    CHECK_FALSE (f.project.getUndoManager().canUndo());
}

TEST_CASE ("The mixer has a strip for every track, plus the master", "[ui][mixer]")
{
    Fixture f;
    CHECK (f.mixer.getNumStrips() == 4);
    CHECK (f.mixer.findStrip (mixer::masterId) != nullptr);

    const auto bus = edit::addTrack (f.project, TrackKind::bus).getId();
    CHECK (f.mixer.getNumStrips() == 5);
    CHECK (f.mixer.findStrip (bus) != nullptr);

    edit::deleteTracks (f.project, { f.ids[0], f.ids[1] });
    CHECK (f.mixer.getNumStrips() == 3);
    CHECK (f.mixer.findStrip (f.ids[0]) == nullptr);

    // Adding effects or sends doesn't rebuild the strips (which would interrupt a drag).
    auto* strip = f.mixer.findStrip (f.ids[2]);
    edit::addInsert (f.project, f.ids[2], "reverb");
    edit::addSend (f.project, f.ids[2], bus);
    CHECK (f.mixer.findStrip (f.ids[2]) == strip);
}

TEST_CASE ("Meters on the track headers and the mixer show the levels they are given", "[ui][mixer][meters]")
{
    Fixture f;
    auto peaks = [&f] (const juce::String& id) -> std::pair<float, float>
    {
        return id == f.ids[0] ? std::pair { 0.5f, 0.25f } : std::pair { 0.0f, 0.0f };
    };

    f.mixer.updateMeters (peaks, 0.03f);
    auto& meter = f.mixer.findStrip (f.ids[0])->getMeter();
    CHECK (meter.getDisplayedDb (0) > -7.0f);
    CHECK (meter.getDisplayedDb (0) < -5.0f);
    CHECK (meter.getDisplayedDb (1) < meter.getDisplayedDb (0));
    CHECK (f.mixer.findStrip (f.ids[1])->getMeter().getDisplayedDb (0) == ui::StereoMeter::minimumDb);

    f.list.updateMeters (peaks, 0.03f);   // and the track headers don't crash with them
}

TEST_CASE ("The effect editor has a control for every setting", "[ui][mixer][effects]")
{
    Fixture f;
    const auto reverb = edit::addInsert (f.project, f.ids[0], "reverb");
    const auto echo = edit::addInsert (f.project, f.ids[0], "delay");

    ui::EffectEditor reverbEditor (f.theme, f.project, reverb);
    CHECK (reverbEditor.getNumKnobs() == 6);   // size, damping, width, mix, pre-delay, low cut

    ui::EffectEditor delayEditor (f.theme, f.project, echo);
    CHECK (delayEditor.getNumKnobs() == 4);   // plus two choice boxes: Time and Ping-pong
    CHECK (delayEditor.getWidth() > 0);

    // The editor follows undo without trouble, and survives its effect being removed.
    edit::setInsertParameter (f.project, echo, "mix", 80.0f);
    f.project.getUndoManager().undo();
    edit::removeInsert (f.project, echo);
}

TEST_CASE ("Tooltips wait for the mouse to rest on each control, every time", "[ui][tooltips]")
{
    ui::TooltipDelay delay (700);
    int record = 0, loop = 0;   // two controls

    CHECK (delay.filter (&record, "Record", 1000).isEmpty());   // just arrived
    CHECK (delay.filter (&record, "Record", 1500).isEmpty());
    CHECK (delay.filter (&record, "Record", 1700) == "Record"); // rested long enough

    // Moving straight on to the next button: it waits again, instead of showing at once.
    CHECK (delay.filter (&loop, "Loop", 1750).isEmpty());
    CHECK (delay.filter (&loop, "Loop", 2449).isEmpty());
    CHECK (delay.filter (&loop, "Loop", 2450) == "Loop");

    // A different part of the same control, with a different tip: waits too.
    CHECK (delay.filter (&loop, "Loop region", 2500).isEmpty());
}
