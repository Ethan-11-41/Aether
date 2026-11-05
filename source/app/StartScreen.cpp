// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "StartScreen.h"

#include "ui/AetherLookAndFeel.h"

namespace aether
{

namespace
{
    void styleLabel (juce::Label& label, const ui::Theme& theme, float size, bool bold, bool dim)
    {
        label.setBorderSize ({});
        label.setFont (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
        label.setColour (juce::Label::textColourId, dim ? theme.textDim : theme.text);
        label.setJustificationType (juce::Justification::topLeft);
    }

    juce::String describeTime (juce::Time t)
    {
        const auto now = juce::Time::getCurrentTime();
        const auto days = (int) ((now - t).inDays());

        if (t.formatted ("%Y-%m-%d") == now.formatted ("%Y-%m-%d"))
            return "Today, " + t.formatted ("%H:%M");

        if (days < 7)
            return t.formatted ("%A, %H:%M");

        return t.formatted ("%d %b %Y");
    }
}

StartScreen::StartScreen (const ui::Theme& themeToUse, RecentProjects& recentProjects, juce::File folder)
    : theme (themeToUse), recent (recentProjects), newProjectFolder (std::move (folder))
{
    title.setText ("Aether", juce::dontSendNotification);
    styleLabel (title, theme, 34.0f, true, false);
    addAndMakeVisible (title);

    tagline.setText ("Record, arrange and mix your music.", juce::dontSendNotification);
    styleLabel (tagline, theme, 15.0f, false, true);
    addAndMakeVisible (tagline);

    // New project -------------------------------------------------------------------------------
    newHeading.setText ("New project", juce::dontSendNotification);
    styleLabel (newHeading, theme, 18.0f, true, false);
    addAndMakeVisible (newHeading);

    nameLabel.setText ("Name", juce::dontSendNotification);
    styleLabel (nameLabel, theme, 13.0f, false, true);
    addAndMakeVisible (nameLabel);

    nameEditor.setText ("My Song", false);
    nameEditor.setFont (juce::FontOptions (16.0f));
    nameEditor.setTooltip ("What to call your project. Aether makes a folder with this name for it.");
    nameEditor.setColour (juce::TextEditor::backgroundColourId, theme.panelRaised);
    nameEditor.setColour (juce::TextEditor::outlineColourId, theme.outline);
    nameEditor.setColour (juce::TextEditor::focusedOutlineColourId, theme.accent);
    nameEditor.setColour (juce::TextEditor::textColourId, theme.text);
    nameEditor.setIndents (8, 6);
    nameEditor.onReturnKey = [this] { create(); };
    addAndMakeVisible (nameEditor);

    locationLabel.setText ("Saved in", juce::dontSendNotification);
    styleLabel (locationLabel, theme, 13.0f, false, true);
    addAndMakeVisible (locationLabel);

    styleLabel (locationValue, theme, 13.0f, false, false);
    locationValue.setMinimumHorizontalScale (1.0f);
    addAndMakeVisible (locationValue);
    updateLocationText();

    changeFolderButton.setTooltip ("Choose a different folder for new projects.");
    changeFolderButton.onClick = [this] { chooseFolder(); };
    addAndMakeVisible (changeFolderButton);

    createButton.setColour (juce::TextButton::buttonColourId, theme.accent);
    createButton.setColour (juce::TextButton::textColourOffId, theme.accentText);
    createButton.setTooltip ("Create the project and open it (Enter).");
    createButton.onClick = [this] { create(); };
    addAndMakeVisible (createButton);

    templatesNote.setText ("Project templates (beat making, band recording, podcast) are coming in a later version.",
                           juce::dontSendNotification);
    styleLabel (templatesNote, theme, 12.5f, false, true);
    addAndMakeVisible (templatesNote);

    // Recent projects ---------------------------------------------------------------------------
    recentHeading.setText ("Recent projects", juce::dontSendNotification);
    styleLabel (recentHeading, theme, 18.0f, true, false);
    addAndMakeVisible (recentHeading);

    recentEmpty.setText ("Projects you open will be listed here.", juce::dontSendNotification);
    styleLabel (recentEmpty, theme, 13.0f, false, true);
    addChildComponent (recentEmpty);

    recentList.setRowHeight (52);
    recentList.setColour (juce::ListBox::backgroundColourId, theme.panel);
    recentList.setColour (juce::ListBox::outlineColourId, juce::Colours::transparentBlack);
    recentList.setTooltip ("Double-click a project to open it. Right-click for more options.");
    addAndMakeVisible (recentList);

    openButton.setTooltip ("Open the selected project.");
    openButton.onClick = [this] { openSelected(); };
    addAndMakeVisible (openButton);

    openOtherButton.setTooltip ("Find a project file (.aether) somewhere else on your computer (Ctrl+O).");
    openOtherButton.onClick = [this] { if (onOpenOther) onOpenOther(); };
    addAndMakeVisible (openOtherButton);

    audioSettingsButton.setTooltip ("Choose your sound device and check that sound works.");
    audioSettingsButton.onClick = [this] { if (onAudioSettings) onAudioSettings(); };
    addAndMakeVisible (audioSettingsButton);

    refresh();
}

StartScreen::~StartScreen()
{
    recentList.setModel (nullptr);
}

void StartScreen::refresh()
{
    recentFiles = recent.getFiles();
    recentList.updateContent();
    recentList.repaint();

    const auto hasRecent = ! recentFiles.isEmpty();
    recentEmpty.setVisible (! hasRecent);
    recentList.setVisible (hasRecent);

    if (hasRecent && recentList.getSelectedRow() < 0)
        recentList.selectRow (0);

    openButton.setEnabled (hasRecent && recentList.getSelectedRow() >= 0);
}

void StartScreen::focusNameField()
{
    nameEditor.grabKeyboardFocus();
    nameEditor.selectAll();
}

void StartScreen::updateLocationText()
{
    locationValue.setText (newProjectFolder.getFullPathName(), juce::dontSendNotification);
    locationValue.setTooltip (newProjectFolder.getFullPathName());
}

void StartScreen::create()
{
    const auto name = nameEditor.getText().trim();

    if (onCreate)
        onCreate (newProjectFolder, name.isEmpty() ? juce::String ("My Song") : name);
}

void StartScreen::openSelected()
{
    const auto row = recentList.getSelectedRow();

    if (juce::isPositiveAndBelow (row, recentFiles.size()) && onOpen)
        onOpen (recentFiles[row]);
}

void StartScreen::chooseFolder()
{
    chooser = std::make_unique<juce::FileChooser> ("Choose where new projects are saved", newProjectFolder);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                          [this] (const juce::FileChooser& fc)
    {
        const auto folder = fc.getResult();

        if (folder != juce::File())
        {
            newProjectFolder = folder;
            updateLocationText();

            if (onNewProjectFolderChanged)
                onNewProjectFolderChanged (folder);
        }
    });
}

//==============================================================================
int StartScreen::getNumRows()
{
    return recentFiles.size();
}

void StartScreen::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected)
{
    if (! juce::isPositiveAndBelow (row, recentFiles.size()))
        return;

    const auto& file = recentFiles.getReference (row);
    auto area = juce::Rectangle<int> (0, 0, width, height).reduced (4, 2);

    if (selected)
    {
        g.setColour (theme.accent.withAlpha (0.18f));
        g.fillRoundedRectangle (area.toFloat(), 6.0f);
    }

    area.reduce (10, 6);
    auto top = area.removeFromTop (area.getHeight() / 2);

    g.setColour (theme.text);
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    const auto when = describeTime (file.getLastModificationTime());
    g.drawText (file.getFileNameWithoutExtension(), top.withTrimmedRight (140), juce::Justification::centredLeft, true);

    g.setColour (theme.textDim);
    g.setFont (juce::FontOptions (12.5f));
    g.drawText (when, top, juce::Justification::centredRight, false);
    g.drawText (file.getParentDirectory().getParentDirectory().getFullPathName(), area, juce::Justification::centredLeft, true);
}

void StartScreen::listBoxItemClicked (int row, const juce::MouseEvent& e)
{
    if (! e.mods.isPopupMenu() || ! juce::isPositiveAndBelow (row, recentFiles.size()))
        return;

    const auto file = recentFiles[row];
    juce::PopupMenu menu;
    menu.addItem ("Open", [this, file] { if (onOpen) onOpen (file); });
    menu.addItem ("Show in folder", [file] { file.revealToUser(); });
    menu.addSeparator();
    menu.addItem ("Remove from this list", [this, file] { recent.remove (file); refresh(); });
    menu.showMenuAsync (juce::PopupMenu::Options().withMousePosition());
}

void StartScreen::listBoxItemDoubleClicked (int row, const juce::MouseEvent&)
{
    if (juce::isPositiveAndBelow (row, recentFiles.size()) && onOpen)
        onOpen (recentFiles[row]);
}

void StartScreen::returnKeyPressed (int row)
{
    if (juce::isPositiveAndBelow (row, recentFiles.size()) && onOpen)
        onOpen (recentFiles[row]);
}

void StartScreen::selectedRowsChanged (int lastRowSelected)
{
    openButton.setEnabled (lastRowSelected >= 0);
}

juce::String StartScreen::getTooltipForRow (int row)
{
    return juce::isPositiveAndBelow (row, recentFiles.size()) ? recentFiles[row].getFullPathName() : juce::String();
}

//==============================================================================
void StartScreen::paint (juce::Graphics& g)
{
    g.fillAll (theme.background);
    ui::AetherLookAndFeel::drawCard (g, newCard.toFloat(), theme);
    ui::AetherLookAndFeel::drawCard (g, recentCard.toFloat(), theme);
}

void StartScreen::resized()
{
    constexpr int margin = 24;
    constexpr int padding = 20;

    // Keep the content a comfortable width on very wide windows.
    auto area = getLocalBounds().reduced (margin);
    if (area.getWidth() > 1100)
        area = area.withSizeKeepingCentre (1100, area.getHeight());

    auto header = area.removeFromTop (84);
    title.setBounds (header.removeFromTop (46));
    tagline.setBounds (header.removeFromTop (24));

    auto footer = area.removeFromBottom (36);
    audioSettingsButton.setBounds (footer.removeFromLeft (160).reduced (0, 2));
    area.removeFromBottom (16);

    newCard = area.removeFromLeft (juce::jmin (420, area.getWidth() * 42 / 100));
    area.removeFromLeft (margin);
    recentCard = area;

    {
        auto inner = newCard.reduced (padding);
        newHeading.setBounds (inner.removeFromTop (28));
        inner.removeFromTop (12);
        nameLabel.setBounds (inner.removeFromTop (20));
        nameEditor.setBounds (inner.removeFromTop (36));
        inner.removeFromTop (16);
        locationLabel.setBounds (inner.removeFromTop (20));
        auto locationRow = inner.removeFromTop (30);
        changeFolderButton.setBounds (locationRow.removeFromRight (96).reduced (0, 2));
        locationRow.removeFromRight (8);
        locationValue.setBounds (locationRow.withTrimmedTop (6));
        inner.removeFromTop (20);
        createButton.setBounds (inner.removeFromTop (40));
        inner.removeFromTop (20);
        templatesNote.setBounds (inner.removeFromTop (40));
    }

    {
        auto inner = recentCard.reduced (padding);
        recentHeading.setBounds (inner.removeFromTop (28));
        inner.removeFromTop (12);
        auto buttons = inner.removeFromBottom (34);
        openButton.setBounds (buttons.removeFromLeft (100));
        buttons.removeFromLeft (8);
        openOtherButton.setBounds (buttons.removeFromLeft (200));
        inner.removeFromBottom (12);
        recentList.setBounds (inner);
        recentEmpty.setBounds (inner.removeFromTop (24));
    }
}

} // namespace aether
