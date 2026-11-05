// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "AppSettings.h"
#include "devices/AudioDeviceService.h"
#include "engine/AudioEngine.h"
#include "io/ProjectSession.h"
#include "io/RecentProjects.h"
#include "ui/Theme.h"

namespace aether
{

/** The long-lived parts of the running application, handed to the windows that need them.
    Everything here is owned by AetherApplication (Main.cpp) and outlives every window. */
struct AppContext
{
    const ui::Theme& theme;
    AppSettings& settings;
    AudioEngine& engine;
    AudioDeviceService& devices;
    RecentProjects& recentProjects;
    ProjectSession& session;
    juce::ApplicationCommandManager& commands;
};

} // namespace aether
