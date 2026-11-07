// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "ShortcutList.h"

namespace aether::ui
{

namespace
{
    constexpr int rowHeight = 26;
    constexpr int headingHeight = 34;
    const juce::String otherCategory = "Mouse and other keys";
    const juce::String pianoRollCategory = "Piano roll";
}

//==============================================================================
/** The list itself (inside the scrolling viewport). */
class ShortcutList::Rows final : public juce::Component
{
public:
    explicit Rows (ShortcutList& ownerToUse) : owner (ownerToUse) {}

    int getIdealHeight() const
    {
        int height = 8;
        juce::String category;

        for (const auto& entry : owner.shown)
        {
            if (entry.category != category)
            {
                category = entry.category;
                height += headingHeight;
            }

            height += rowHeight;
        }

        return height + 8;
    }

    void paint (juce::Graphics& g) override
    {
        const auto& colours = owner.theme;
        const auto keysWidth = juce::jlimit (150, 260, getWidth() * 38 / 100);
        int y = 8;
        juce::String category;
        bool shade = false;

        if (owner.shown.empty())
        {
            g.setColour (colours.textDim);
            g.setFont (juce::FontOptions (14.0f));
            g.drawText ("No shortcuts match.", getLocalBounds().reduced (16, 12), juce::Justification::topLeft);
            return;
        }

        for (const auto& entry : owner.shown)
        {
            if (entry.category != category)
            {
                category = entry.category;
                g.setColour (colours.text);
                g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
                g.drawText (category, 12, y + 10, getWidth() - 24, headingHeight - 10, juce::Justification::centredLeft);
                y += headingHeight;
                shade = false;
            }

            juce::Rectangle<int> row (0, y, getWidth(), rowHeight);

            if (shade)
            {
                g.setColour (colours.panelRaised.withAlpha (0.45f));
                g.fillRect (row);
            }

            shade = ! shade;

            g.setColour (colours.text);
            g.setFont (juce::FontOptions (14.0f));
            g.drawFittedText (entry.action, row.withTrimmedLeft (12).withTrimmedRight (keysWidth + 12),
                              juce::Justification::centredLeft, 1, 0.9f);

            g.setColour (colours.accent);
            g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
            g.drawFittedText (entry.keys, row.removeFromRight (keysWidth + 12).withTrimmedRight (12),
                              juce::Justification::centredLeft, 1, 0.9f);

            y += rowHeight;
        }
    }

private:
    ShortcutList& owner;
};

//==============================================================================
ShortcutList::ShortcutList (const Theme& themeToUse, juce::ApplicationCommandManager& commands)
    : theme (themeToUse)
{
    all = commandShortcuts (commands);
    const auto others = otherShortcuts();
    all.insert (all.end(), others.begin(), others.end());
    shown = all;

    title.setText ("Keyboard shortcuts", juce::dontSendNotification);
    title.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    title.setColour (juce::Label::textColourId, theme.text);
    title.setBorderSize ({});
    addAndMakeVisible (title);

    intro.setText ("The same shortcuts are shown next to each command in the menus.", juce::dontSendNotification);
    intro.setFont (juce::FontOptions (13.0f));
    intro.setColour (juce::Label::textColourId, theme.textDim);
    intro.setBorderSize ({});
    addAndMakeVisible (intro);

    searchBox.setTextToShowWhenEmpty ("Search, e.g. \"loop\" or \"Ctrl\"", theme.textDim);
    searchBox.setTooltip ("Type part of a command's name, or a key, to find its shortcut.");
    searchBox.onTextChange = [this] { setFilter (searchBox.getText()); };
    searchBox.onEscapeKey = [this] { searchBox.clear(); setFilter ({}); };
    addAndMakeVisible (searchBox);

    rows = std::make_unique<Rows> (*this);
    viewport.setViewedComponent (rows.get(), false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);

    setSize (620, 640);
}

ShortcutList::~ShortcutList() = default;

std::vector<ShortcutList::Entry> ShortcutList::commandShortcuts (juce::ApplicationCommandManager& commands)
{
    std::vector<Entry> entries;
    auto* mappings = commands.getKeyMappings();

    for (const auto& category : commands.getCommandCategories())
    {
        for (const auto id : commands.getCommandsInCategory (category))
        {
            const auto* info = commands.getCommandForID (id);
            if (info == nullptr || (info->flags & juce::ApplicationCommandInfo::hiddenFromKeyEditor) != 0)
                continue;

            const auto keys = mappings != nullptr ? mappings->getKeyPressesAssignedToCommand (id) : info->defaultKeypresses;
            if (keys.isEmpty())
                continue;

            juce::StringArray keyNames;
            for (const auto& key : keys)
                keyNames.addIfNotAlreadyThere (describeKey (key));

            auto name = info->shortName.trimCharactersAtEnd (".");
            entries.push_back ({ category, name, keyNames.joinIntoString ("  or  "), id });
        }
    }

    return entries;
}

std::vector<ShortcutList::Entry> ShortcutList::otherShortcuts()
{
    return {
        { otherCategory, "Select several tracks or clips", "Ctrl+click" },
        { otherCategory, "Select a range of tracks", "Shift+click" },
        { otherCategory, "Select clips with a box", "Drag on an empty part of the timeline" },
        { otherCategory, "Move or trim a clip without snapping to the grid", "Hold Alt while dragging" },
        { otherCategory, "Rename a track or clip", "Double-click it" },
        { otherCategory, "Reset a fader, knob or pan to its default", "Double-click it" },
        { otherCategory, "Fine adjustment of a fader, knob or tempo", "Hold Shift while dragging" },
        { otherCategory, "Turn the loop on or off", "Double-click the loop strip" },
        { otherCategory, "Scroll up and down", "Mouse wheel" },
        { otherCategory, "Scroll left and right", "Shift+mouse wheel" },
        { otherCategory, "Zoom in and out around the mouse", "Ctrl+mouse wheel" },
        { otherCategory, "Select the track above or below", "Up / Down arrow" },
        { otherCategory, "Add the track above or below to the selection", "Shift+Up / Down arrow" },
        { otherCategory, "More options for a track, clip or control", "Right-click" },
        { otherCategory, "On-screen keyboard: play notes (after clicking it)", "A W S E D F T G Y H U J K..." },
        { otherCategory, "On-screen keyboard: octave down / up", "Z / X" },
        { otherCategory, "Import audio, MIDI or open a project", "Drag files onto the window" },
        { otherCategory, "Open a MIDI clip in the piano roll", "Double-click it" },
        { otherCategory, "Make a new MIDI clip and open it", "Double-click an empty part of a MIDI track" },

        { pianoRollCategory, "Add a note", "Click an empty place (drag to make it longer)" },
        { pianoRollCategory, "Delete a note (or all the selected notes)", "Right-click it" },
        { pianoRollCategory, "Delete several notes", "Right-drag across them" },
        { pianoRollCategory, "Move a note / change its length", "Drag it / drag its right edge" },
        { pianoRollCategory, "Select several notes", "Ctrl+click, or Ctrl+drag a box" },
        { pianoRollCategory, "Move selected notes up / down a semitone", "Up / Down arrow" },
        { pianoRollCategory, "Move selected notes up / down an octave", "Shift+Up / Down arrow" },
        { pianoRollCategory, "Move selected notes by a grid step", "Left / Right arrow" },
        { pianoRollCategory, "Quantize (the selected notes, or all)", "Q" },
        { pianoRollCategory, "Duplicate / copy / cut / paste notes", "Ctrl+D / Ctrl+C / Ctrl+X / Ctrl+V" },
        { pianoRollCategory, "Select all / none", "Ctrl+A / Esc" },
        { pianoRollCategory, "Delete the selected notes", "Delete" },
        { pianoRollCategory, "Zoom in time / note height", "Ctrl+mouse wheel / Alt+mouse wheel" },
        { pianoRollCategory, "Change a chord's velocity / one note of it", "Drag its stem / Alt+drag the note's dot" },
    };
}

juce::String ShortcutList::describeKey (const juce::KeyPress& key)
{
    juce::String text;
    const auto mods = key.getModifiers();

    if (mods.isCtrlDown() || mods.isCommandDown())  text << "Ctrl+";
    if (mods.isAltDown())                            text << "Alt+";
    if (mods.isShiftDown())                          text << "Shift+";

    const auto code = key.getKeyCode();
    using KP = juce::KeyPress;

    if (code == KP::spaceKey)              text << "Space";
    else if (code == KP::deleteKey)        text << "Delete";
    else if (code == KP::backspaceKey)     text << "Backspace";
    else if (code == KP::escapeKey)        text << "Esc";
    else if (code == KP::returnKey)        text << "Enter";
    else if (code == KP::tabKey)           text << "Tab";
    else if (code == KP::homeKey)          text << "Home";
    else if (code == KP::endKey)           text << "End";
    else if (code == KP::pageUpKey)        text << "Page Up";
    else if (code == KP::pageDownKey)      text << "Page Down";
    else if (code == KP::leftKey)          text << "Left arrow";
    else if (code == KP::rightKey)         text << "Right arrow";
    else if (code == KP::upKey)            text << "Up arrow";
    else if (code == KP::downKey)          text << "Down arrow";
    else if (code >= KP::F1Key && code <= KP::F12Key)
        text << "F" << (code - KP::F1Key + 1);
    else
        text << juce::String::charToString ((juce::juce_wchar) code).toUpperCase();

    return text;
}

void ShortcutList::setActionName (juce::CommandID command, const juce::String& name)
{
    for (auto& entry : all)
        if (entry.command == command)
            entry.action = name;

    setFilter (searchBox.getText());
}

void ShortcutList::setFilter (const juce::String& text)
{
    const auto filter = text.trim();
    shown.clear();

    for (const auto& entry : all)
        if (filter.isEmpty() || entry.action.containsIgnoreCase (filter) || entry.keys.containsIgnoreCase (filter)
            || entry.category.containsIgnoreCase (filter))
            shown.push_back (entry);

    updateRows();
}

void ShortcutList::updateRows()
{
    rows->setSize (viewport.getMaximumVisibleWidth(), std::max (rows->getIdealHeight(), viewport.getHeight()));
    rows->repaint();
}

void ShortcutList::paint (juce::Graphics& g)
{
    g.fillAll (theme.background);
    g.setColour (theme.outline);
    g.drawHorizontalLine (viewport.getY() - 1, 0.0f, (float) getWidth());
}

void ShortcutList::resized()
{
    auto area = getLocalBounds().reduced (16, 0);
    area.removeFromTop (14);
    title.setBounds (area.removeFromTop (28));
    intro.setBounds (area.removeFromTop (20));
    area.removeFromTop (10);
    searchBox.setBounds (area.removeFromTop (32));
    area.removeFromTop (10);
    viewport.setBounds (getLocalBounds().withTop (area.getY()));
    updateRows();
}

} // namespace aether::ui
