// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <functional>
#include <vector>

#include "model/Automation.h"

namespace aether
{

/** Records automation (Milestone 14): while it's switched on and the song is playing, moving a
    fader, knob or switch writes its value into the setting's lane at the playhead (making and
    showing the lane if needed), replacing what was there while it moved. When you let go (no
    change for a moment), the lane carries on as it was.

    It listens to the project, so every control that changes an automatable setting records,
    wherever it is (track headers, the mixer, effect and instrument windows). The points join the
    control's own undo step, so one Ctrl+Z takes back a move and what it recorded. */
class AutomationRecorder final : private juce::ValueTree::Listener
{
public:
    AutomationRecorder (Project& project, std::function<bool()> isPlaying, std::function<Tick()> playhead);
    ~AutomationRecorder() override;

    void setEnabled (bool shouldRecord);
    bool isEnabled() const noexcept   { return enabled; }

    /** Called regularly (about 30 times a second) by the window. While a control is held (see
        controlHeld), keeps writing its value as the song moves on, even if it isn't moving, so it
        replaces what was there; otherwise ends the moves that haven't changed for `idleSeconds`.
        `now` is in seconds. */
    void update (double now, double idleSeconds = 0.4);

    /** True while a control is being held with the mouse (set by the window). */
    std::function<bool()> controlHeld = [] { return false; };

    /** How many settings are being moved right now (for the tests). */
    int getNumActiveMoves() const noexcept   { return (int) moves.size(); }

    /** The clock used to tell when a move has stopped (seconds; replaceable for the tests). */
    std::function<double()> clock;

private:
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;

    struct Move
    {
        juce::String trackId;
        automation::Target target;
        Tick lastTick = 0;
        double lastTime = 0.0;
    };

    Project& project;
    std::function<bool()> isPlaying;
    std::function<Tick()> playhead;
    juce::ValueTree state;
    std::vector<Move> moves;
    bool enabled = false, writing = false;
};

} // namespace aether
