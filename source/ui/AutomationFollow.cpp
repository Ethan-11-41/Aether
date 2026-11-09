// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "AutomationFollow.h"

namespace aether::ui::automationfollow
{

namespace
{
    std::function<Tick()>& playheadFunction()
    {
        static std::function<Tick()> playhead;
        return playhead;
    }
}

void setPlayhead (std::function<Tick()> playhead)
{
    playheadFunction() = std::move (playhead);
}

float shown (const Project& project, const juce::String& trackId, const automation::Target& target, float own)
{
    const auto& playhead = playheadFunction();

    if (playhead == nullptr || ! automation::isAutomated (project, trackId, target))
        return own;

    return automation::currentValue (project, trackId, target, playhead());
}

} // namespace aether::ui::automationfollow
