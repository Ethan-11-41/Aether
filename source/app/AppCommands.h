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
        exportAudio         = 0x1008,
        renameProject       = 0x1009,

        // Edit
        undo                = 0x2001,
        redo                = 0x2002,
        selectAll           = 0x2003,
        copyClips           = 0x2004,
        cutClips            = 0x2005,
        pasteClips          = 0x2006,
        deselect            = 0x2007,

        // Track
        addAudioTrack       = 0x3001,
        addMidiTrack        = 0x3002,
        renameTrack         = 0x3003,
        deleteTrack         = 0x3004,
        moveTrackUp         = 0x3005,
        moveTrackDown       = 0x3006,
        addBus              = 0x3007,
        addDrumTrack        = 0x3008,

        // Clips
        importAudio         = 0x3101,
        splitClips          = 0x3102,
        duplicateClips      = 0x3103,
        nudgeLeft           = 0x3104,
        nudgeRight          = 0x3105,
        nudgeLeftFine       = 0x3106,
        nudgeRightFine      = 0x3107,
        quantizeClips       = 0x3108,
        fixTiming           = 0x3109,

        // Transport and view
        playStop            = 0x6001,
        stop                = 0x6002,
        goToStart           = 0x6003,
        toggleLoop          = 0x6004,
        toggleMetronome     = 0x6005,
        zoomIn              = 0x6006,
        zoomOut             = 0x6007,
        toggleSnap          = 0x6008,
        toggleMixer         = 0x6009,
        record              = 0x600A,
        togglePunch         = 0x600B,
        toggleKeyboard      = 0x600C,
        writeAutomation     = 0x600D,

        // Settings & help
        audioSettings       = 0x4001,
        about               = 0x5001,
        testCrashRecovery   = 0x5002,
        openLogFolder       = 0x5003,
        keyboardShortcuts   = 0x5004,
    };

    namespace categories
    {
        inline const juce::String file     = "File";
        inline const juce::String edit     = "Edit";
        inline const juce::String track    = "Track";
        inline const juce::String transport = "Transport";
        inline const juce::String view     = "View";
        inline const juce::String settings = "Settings";
        inline const juce::String help     = "Help";
    }
}

} // namespace aether
