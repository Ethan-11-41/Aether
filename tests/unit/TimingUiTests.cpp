// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
//
// The "Fix timing" window and the quantize pop-up (Milestone 15), driven without opening a window.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <optional>

#include "model/ClipEdits.h"
#include "model/EditCommands.h"
#include "model/MidiTiming.h"
#include "model/TimelineEdits.h"
#include "ui/QuantizePanel.h"
#include "ui/TimingPanel.h"

using namespace aether;

namespace
{
    constexpr Tick q = ticksPerQuarter;

    /** A song at 120 BPM with a clip played freely at about 90 BPM, getting a little faster:
        chords on every beat, a bass note on each bar, a melody note between beats. */
    struct Fixture
    {
        Fixture()
        {
            track = edit::addTrack (project, TrackKind::midi, -1, true, "Piano").getId();
            std::vector<NoteData> notes;
            const auto at = [] (double beat) { return 1.2 + beat * 60.0 / 88.0 - beat * beat * 0.0006; };
            const auto ticks = [] (double seconds) { return (Tick) std::llround (seconds * 2.0 * (double) q); };

            for (int b = 0; b < 40; ++b)
            {
                if (b % 4 == 0)
                    notes.push_back ({ 41, 100, ticks (at (b)), ticks (1.8) });

                for (const auto pitch : { 60, 65, 69 })
                    notes.push_back ({ pitch, 85, ticks (at (b)), ticks (0.5) });

                notes.push_back ({ 72, 55, ticks (at (b + 0.5)), ticks (0.2) });
            }

            clipId = edit::addMidiClip (project, track, "Take 1", 0, ticks (at (41)), notes);
            panel = std::make_unique<ui::TimingPanel> (theme, project, juce::StringArray { clipId }, false);
            panel->onClose = [this] { ++closed; };
            panel->onOfferChanged = [this] (bool offer) { offered = offer; };
        }

        juce::ScopedJuceInitialiser_GUI juceInit;
        ui::Theme theme = ui::Theme::dark();
        Project project;
        juce::String track, clipId;
        std::unique_ptr<ui::TimingPanel> panel;
        int closed = 0;
        bool offered = false;
    };

    /** Every button, slider, box and field in a component (and inside it) has a tooltip. */
    void checkTooltips (juce::Component& parent)
    {
        for (auto* child : parent.getChildren())
        {
            if (dynamic_cast<juce::Button*> (child) != nullptr || dynamic_cast<juce::Slider*> (child) != nullptr
                || dynamic_cast<juce::ComboBox*> (child) != nullptr || dynamic_cast<ui::NumberDragField*> (child) != nullptr)
            {
                auto* client = dynamic_cast<juce::TooltipClient*> (child);
                REQUIRE (client != nullptr);
                INFO (child->getName() << " " << (dynamic_cast<juce::Button*> (child) != nullptr ? dynamic_cast<juce::Button*> (child)->getButtonText() : juce::String()));
                CHECK (client->getTooltip().length() > 15);
            }

            checkTooltips (*child);
        }
    }
}

TEST_CASE ("The timing window says what it found and where bar 1 will go", "[ui][timing]")
{
    Fixture f;
    const auto& beat = f.panel->getBeat();

    REQUIRE (beat.found);
    CHECK (beat.bpm > 85.0);
    CHECK (beat.bpm < 96.0);
    CHECK (f.panel->getSummary().startsWith ("You played at about "));
    CHECK (f.panel->getMode() == ui::TimingPanel::Mode::steady);
    CHECK (f.panel->getSteadyTempo() == Catch::Approx (std::round (beat.bpm)));
    CHECK (f.panel->getOutcome().startsWith ("Bar 1 of your playing will be at bar 1 of the song, and the song's tempo will be "));

    // Bar 1 one beat later: there's a pick-up, so it needs another bar.
    f.panel->moveBarStart (1);
    CHECK (f.panel->getBeat().downbeat == 1);
    CHECK (f.panel->getOutcome().startsWith ("Bar 1 of your playing will be at bar 2"));
    f.panel->moveBarStart (-5);
    CHECK (f.panel->getBeat().downbeat == 0);

    // The song's own tempo instead.
    f.panel->setUseSongTempo (true);
    CHECK (f.panel->getSteadyTempo() == Catch::Approx (120.0));
    CHECK_FALSE (f.panel->getOutcome().contains ("tempo will be"));

    f.panel->setMode (ui::TimingPanel::Mode::follow);
    CHECK (f.panel->getOutcome().contains ("follow you bar by bar"));
    f.panel->setEveryBeat (true);
    CHECK (f.panel->getOutcome().contains ("beat by beat"));
}

TEST_CASE ("Half as fast and twice as fast look for the beat again", "[ui][timing]")
{
    Fixture f;
    const auto found = f.panel->getBeat().bpm;
    const auto beats = f.panel->getBeat().beats.size();

    f.panel->findBeatNear (found * 2.0);
    CHECK (f.panel->getBeat().bpm == Catch::Approx (found * 2.0).epsilon (0.05));
    CHECK (f.panel->getBeat().beats.size() > beats + beats / 2);

    f.panel->findBeatNear (found / 2.0);
    CHECK (f.panel->getBeat().bpm == Catch::Approx (found / 2.0).epsilon (0.05));

    f.panel->findBeatNear (0.0);
    CHECK (f.panel->getBeat().bpm == Catch::Approx (found));
}

TEST_CASE ("Applying makes the playing steady, in one undo step", "[ui][timing]")
{
    Fixture f;
    const auto tempo = f.panel->getSteadyTempo();

    REQUIRE (f.panel->apply());
    CHECK (f.closed == 1);
    CHECK (timeline::buildTempoMap (f.project).tempoAt (0) == Catch::Approx (tempo));
    CHECK (f.project.getUndoManager().getUndoDescription() == "Make timing steady");

    // The first chord is on bar 1.
    const auto clip = clips::findClip (f.project, f.clipId);
    const auto notes = midi::audibleNotes (clip);
    CHECK (clip.getStart() + notes.front().start == 0);
}

TEST_CASE ("Applying can make the song's tempo follow the playing", "[ui][timing]")
{
    Fixture f;
    f.panel->setMode (ui::TimingPanel::Mode::follow);
    REQUIRE (f.panel->apply());
    CHECK (timeline::getNumTempoChanges (f.project) > 5);
    CHECK (f.project.getUndoManager().getUndoDescription() == "Make the tempo follow the playing");
}

TEST_CASE ("The timing window starts from how it was played, and can put it back", "[ui][timing]")
{
    Fixture f;
    const auto found = f.panel->getBeat().bpm;
    CHECK_FALSE (f.panel->canPutBack());
    REQUIRE (f.panel->apply());   // steady

    // Opened again: the same beat as played (not the steady one), and it says so.
    ui::TimingPanel again (f.theme, f.project, { f.clipId }, false);
    again.onClose = [&f] { ++f.closed; };
    CHECK (again.getBeat().bpm == Catch::Approx (found).margin (0.01));
    CHECK (again.getSummary().contains ("how you played it"));
    CHECK (again.canPutBack());

    // Following after making it steady: the tempo follows the playing (it goes between tempos).
    again.setMode (ui::TimingPanel::Mode::follow);
    REQUIRE (again.apply());
    CHECK (timeline::getNumTempoChanges (f.project) > 5);

    // Put back: the song's tempo is as it was, in one undo step.
    ui::TimingPanel last (f.theme, f.project, { f.clipId }, false);
    REQUIRE (last.putBack());
    CHECK (timeline::getNumTempoChanges (f.project) == 1);
    CHECK (timeline::buildTempoMap (f.project).tempoAt (0) == Catch::Approx (120.0));
    CHECK (f.project.getUndoManager().getUndoDescription() == "Put the timing back as played");
    CHECK_FALSE (last.canPutBack());
}

TEST_CASE ("Applying after the song changed looks at the notes again", "[ui][timing]")
{
    Fixture f;
    f.panel->moveBarStart (1);

    // The clip moves a bar later while the window is open: the beat is found again where the
    // notes are now, with bar 1 still a beat later.
    edit::setClipPosition (f.project, f.clipId, 4 * q, f.track);
    REQUIRE (f.panel->apply());
    CHECK (f.panel->getBeat().downbeat == 1);

    // The clip is gone (e.g. the recording was undone): nothing to do.
    Fixture g;
    edit::deleteClips (g.project, { g.clipId });
    CHECK_FALSE (g.panel->apply());
    CHECK (g.closed == 0);
}

TEST_CASE ("The timing window explains when it can't help", "[ui][timing]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    const auto theme = ui::Theme::dark();
    Project project;
    const auto track = edit::addTrack (project, TrackKind::midi).getId();
    const auto clipId = edit::addMidiClip (project, track, "Short", 0, 4 * q, { { 60, 100, 0, q }, { 62, 100, q, q } });

    ui::TimingPanel panel (theme, project, { clipId }, true);
    CHECK_FALSE (panel.getBeat().found);
    CHECK (panel.getSummary().startsWith ("There are too few notes"));
    CHECK_FALSE (panel.apply());

    edit::setPatternLooping (project, clipId, true);
    ui::TimingPanel patterns (theme, project, { clipId }, true);
    CHECK (patterns.getSummary().contains ("aren't repeating patterns"));
}

TEST_CASE ("Every control in the timing window and the quantize pop-up has a tooltip", "[ui][timing][tooltips]")
{
    Fixture f;
    checkTooltips (*f.panel);

    ui::QuantizePanel quantize (f.theme, {}, "\"Take 1\"", [] (const ui::QuantizeSettings&) {});
    checkTooltips (quantize);
}

TEST_CASE ("The quantize pop-up remembers its settings and quantizes", "[ui][timing]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    const auto theme = ui::Theme::dark();

    ui::QuantizeSettings settings;
    settings.grid = GridSize::eighthTriplet;
    settings.strength = 0.75;
    settings.swing = 0.25;
    settings.ends = true;

    const auto text = settings.toString();
    const auto back = ui::QuantizeSettings::fromString (text);
    CHECK (back.grid == GridSize::eighthTriplet);
    CHECK (back.strength == Catch::Approx (0.75));
    CHECK (back.swing == Catch::Approx (0.25));
    CHECK (back.ends);

    // Nonsense gives the defaults (1/16, full strength).
    CHECK (ui::QuantizeSettings::fromString ("what").grid == GridSize::sixteenth);
    CHECK (ui::QuantizeSettings::fromString ("off;1;0;0").grid == GridSize::sixteenth);

    std::optional<ui::QuantizeSettings> applied;
    ui::QuantizePanel panel (theme, settings, "3 clips", [&] (const ui::QuantizeSettings& s) { applied = s; });
    CHECK (panel.getSettings().grid == GridSize::eighthTriplet);
    panel.apply();
    REQUIRE (applied.has_value());
    CHECK (applied->strength == Catch::Approx (0.75));
    CHECK (applied->ends);
}
