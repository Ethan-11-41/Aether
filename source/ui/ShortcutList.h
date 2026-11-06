// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"

namespace aether::ui
{

/** The Help > Keyboard shortcuts window: every shortcut, read from the command manager (so it is
    always up to date), plus the mouse actions and keys that aren't menu commands. A search box
    narrows the list down. */
class ShortcutList final : public juce::Component
{
public:
    struct Entry
    {
        juce::String category, action, keys;
        juce::CommandID command = 0;   // 0 for mouse actions
    };

    ShortcutList (const Theme& theme, juce::ApplicationCommandManager& commands);
    ~ShortcutList() override;

    /** The menu commands that have a keyboard shortcut, grouped by menu. */
    static std::vector<Entry> commandShortcuts (juce::ApplicationCommandManager& commands);

    /** Mouse actions and keys that aren't menu commands. */
    static std::vector<Entry> otherShortcuts();

    /** "Ctrl+Shift+S", "Space", "Left arrow"... */
    static juce::String describeKey (const juce::KeyPress& key);

    /** Replaces a command's name in the list (for commands whose menu name depends on what's
        selected, e.g. "Delete track" / "Delete selected clips"). */
    void setActionName (juce::CommandID command, const juce::String& name);

    /** Shows only entries containing `text` (in the action or the keys). */
    void setFilter (const juce::String& text);
    int getNumShownEntries() const noexcept   { return (int) shown.size(); }
    const Entry& getShownEntry (int index) const   { return shown[(size_t) index]; }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class Rows;

    void updateRows();

    const Theme& theme;
    std::vector<Entry> all, shown;
    juce::Label title, intro;
    juce::TextEditor searchBox;
    juce::Viewport viewport;
    std::unique_ptr<Rows> rows;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ShortcutList)
};

} // namespace aether::ui
