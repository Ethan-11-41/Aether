// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
//
// The Help > Keyboard shortcuts list.

#include <catch2/catch_test_macros.hpp>

#include "ui/ShortcutList.h"

using aether::ui::ShortcutList;
using juce::KeyPress;

namespace
{
    /** A few commands, like the real ones in MainContent. */
    struct Commands final : public juce::ApplicationCommandTarget
    {
        juce::ApplicationCommandTarget* getNextCommandTarget() override   { return nullptr; }

        void getAllCommands (juce::Array<juce::CommandID>& list) override   { list.addArray ({ 1, 2, 3, 4 }); }

        void getCommandInfo (juce::CommandID id, juce::ApplicationCommandInfo& info) override
        {
            constexpr auto ctrl = juce::ModifierKeys::commandModifier;
            constexpr auto shift = juce::ModifierKeys::shiftModifier;

            switch (id)
            {
                case 1: info.setInfo ("Save", "", "File", 0);   info.addDefaultKeypress ('s', ctrl); break;
                case 2: info.setInfo ("Save as...", "", "File", 0); info.addDefaultKeypress ('s', ctrl | shift); break;
                case 3: info.setInfo ("Play / stop", "", "Transport", 0); info.addDefaultKeypress (KeyPress::spaceKey, 0); break;
                case 4: info.setInfo ("Show project folder", "", "File", 0); break;   // no shortcut
                default: break;
            }
        }

        bool perform (const InvocationInfo&) override   { return true; }
    };
}

TEST_CASE ("Keys are described the way Windows users know them", "[ui][shortcuts]")
{
    constexpr auto ctrl = juce::ModifierKeys::commandModifier;
    constexpr auto shift = juce::ModifierKeys::shiftModifier;

    CHECK (ShortcutList::describeKey (KeyPress ('s', ctrl, 0)) == "Ctrl+S");
    CHECK (ShortcutList::describeKey (KeyPress ('z', ctrl | shift, 0)) == "Ctrl+Shift+Z");
    CHECK (ShortcutList::describeKey (KeyPress (KeyPress::spaceKey)) == "Space");
    CHECK (ShortcutList::describeKey (KeyPress (KeyPress::F2Key)) == "F2");
    CHECK (ShortcutList::describeKey (KeyPress (KeyPress::leftKey, shift, 0)) == "Shift+Left arrow");
    CHECK (ShortcutList::describeKey (KeyPress ('=', ctrl, 0)) == "Ctrl+=");
    CHECK (ShortcutList::describeKey (KeyPress (KeyPress::escapeKey)) == "Esc");
}

TEST_CASE ("The shortcut list comes from the commands, and can be searched", "[ui][shortcuts]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    juce::ApplicationCommandManager manager;
    Commands target;
    manager.registerAllCommandsForTarget (&target);

    const auto entries = ShortcutList::commandShortcuts (manager);
    REQUIRE (entries.size() == 3);   // "Show project folder" has no shortcut
    CHECK (entries[0].category == "File");
    CHECK (entries[0].action == "Save");
    CHECK (entries[0].keys == "Ctrl+S");
    CHECK (entries[1].action == "Save as");   // without the "..."
    CHECK (entries[2].keys == "Space");

    // A shortcut the user changed shows up as it is now.
    manager.getKeyMappings()->addKeyPress (3, KeyPress ('p', 0, 0));
    CHECK (ShortcutList::commandShortcuts (manager)[2].keys == "Space  or  P");

    const auto theme = aether::ui::Theme::dark();
    ShortcutList list (theme, manager);
    const auto total = list.getNumShownEntries();
    CHECK (total == 3 + (int) ShortcutList::otherShortcuts().size());

    list.setFilter ("save");
    CHECK (list.getNumShownEntries() == 2);

    list.setFilter ("ctrl+s");
    CHECK (list.getNumShownEntries() == 2);   // Ctrl+S and Ctrl+Shift+S

    list.setFilter ("wheel");
    CHECK (list.getNumShownEntries() >= 2);
    CHECK (list.getShownEntry (0).category == "Mouse and other keys");

    list.setFilter ("nothing like this");
    CHECK (list.getNumShownEntries() == 0);

    list.setFilter ({});
    CHECK (list.getNumShownEntries() == total);

    list.setActionName (3, "Play or stop");
    CHECK (list.getShownEntry (2).action == "Play or stop");
}
