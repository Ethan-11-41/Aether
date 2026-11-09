// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <functional>

#include "model/Automation.h"

/** Controls that follow automation (Milestone 14): a fader or knob whose setting is automated
    shows the lane's value where the playhead is, so you see it move as the song plays. The
    project window says where the playhead is. */
namespace aether::ui::automationfollow
{

/** Where the playhead is (set by the project window; none: controls show their own values). */
void setPlayhead (std::function<Tick()> playhead);

/** The value a control shows: the lane's at the playhead if it's automated, otherwise `own`. */
float shown (const Project& project, const juce::String& trackId, const automation::Target& target, float own);

} // namespace aether::ui::automationfollow
