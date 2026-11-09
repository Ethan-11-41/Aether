// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "AutomationRecorder.h"

namespace aether
{

AutomationRecorder::AutomationRecorder (Project& p, std::function<bool()> playing, std::function<Tick()> position)
    : clock ([] { return juce::Time::getMillisecondCounterHiRes() / 1000.0; }),
      project (p), isPlaying (std::move (playing)), playhead (std::move (position)), state (p.getState())
{
    state.addListener (this);
}

AutomationRecorder::~AutomationRecorder()
{
    state.removeListener (this);
}

void AutomationRecorder::setEnabled (bool shouldRecord)
{
    enabled = shouldRecord;
    moves.clear();
}

void AutomationRecorder::update (double now, double idleSeconds)
{
    if (! isPlaying() || ! enabled)
    {
        moves.clear();
        return;
    }

    if (controlHeld())
    {
        // Held still: the value stays, and replaces what was there as the song moves on.
        const auto tick = std::max<Tick> (0, playhead());
        const juce::ScopedValueSetter<bool> guard (writing, true);

        for (auto& m : moves)
        {
            if (tick <= m.lastTick)
                continue;

            edit::writeAutomation (project, m.trackId, m.target, m.lastTick, tick,
                                   automation::staticValue (project, m.trackId, m.target));
            m.lastTick = tick;
            m.lastTime = now;
        }

        return;
    }

    moves.erase (std::remove_if (moves.begin(), moves.end(), [&] (const Move& m) { return now - m.lastTime > idleSeconds; }),
                 moves.end());
}

void AutomationRecorder::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    if (! enabled || writing || ! isPlaying())
        return;

    const auto [trackId, target] = automation::targetOfProperty (project, tree, property);

    if (! target.isValid())
        return;

    const auto tick = std::max<Tick> (0, playhead());
    const auto value = automation::staticValue (project, trackId, target);
    const auto now = clock();

    auto it = std::find_if (moves.begin(), moves.end(), [&] (const Move& m) { return m.trackId == trackId && m.target == target; });

    // A new move (or the loop jumped back): start writing here, keeping what's before.
    if (it == moves.end() || tick < it->lastTick)
    {
        if (it != moves.end())
            moves.erase (it);

        moves.push_back ({ trackId, target, tick, now });
        it = moves.end() - 1;
    }

    const juce::ScopedValueSetter<bool> guard (writing, true);   // (writing changes the project too)
    edit::writeAutomation (project, trackId, target, it->lastTick, tick, value);
    it->lastTick = tick;
    it->lastTime = now;
}

} // namespace aether
