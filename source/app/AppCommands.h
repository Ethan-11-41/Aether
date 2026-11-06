// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace aether
{

/** Every command in Aether's menus and keyboard shortcuts.

    Commands are registered with a juce::ApplicationCommandManager, which gives menus, keyboard
    shortcuts and (later) a shortcut editor from one list. The numbers are stored with custom
    shortcuts, so never reuse or renumber them.
*/
namespace commands
{
    enum : juce::CommandID
    {
        // File
        newProject          = 0x1001,
        openProject         = 0x1002,
        saveProject         = 0x1003,
        saveProjectAs       = 0x1004,
        closeProject        = 0x1005,
        showProjectFolder   = 0x1006,
        quit                = 0x1007,

        // Edit
        undo                = 0x2001,
        redo                = 0x2002,

        // Track
        addAudioTrack       = 0x3001,
        addMidiTrack        = 0x3002,
        renameTrack         = 0x3003,
        deleteTrack         = 0x3004,
        moveTrackUp         = 0x3005,
        moveTrackDown       = 0x3006,

        // Settings & help
        audioSettings       = 0x4001,
        about               = 0x5001,
        testCrashRecovery   = 0x5002,
        openLogFolder       = 0x5003,
    };

    namespace categories
    {
        inline const juce::String file     = "File";
        inline const juce::String edit     = "Edit";
        inline const juce::String track    = "Track";
        inline const juce::String settings = "Settings";
        inline const juce::String help     = "Help";
    }
}

} // namespace aether
