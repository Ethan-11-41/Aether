// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <functional>

#include <juce_gui_basics/juce_gui_basics.h>

#include "io/RecentProjects.h"
#include "ui/Theme.h"

namespace aether
{

/** The first thing you see: create a new project, or open a recent one. */
class StartScreen final : public juce::Component,
                          private juce::ListBoxModel
{
public:
    StartScreen (const ui::Theme& theme, RecentProjects& recent, juce::File newProjectFolder);
    ~StartScreen() override;

    /** Reloads the list of recent projects. */
    void refresh();

    /** Puts the keyboard focus in the project-name box, ready to type. */
    void focusNameField();

    juce::File getNewProjectFolder() const   { return newProjectFolder; }

    std::function<void (const juce::File& parentFolder, const juce::String& name)> onCreate;
    std::function<void (const juce::File& projectFile)> onOpen;
    std::function<void()> onOpenOther;
    std::function<void()> onAudioSettings;
    std::function<void (const juce::File& folder)> onNewProjectFolderChanged;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
    void returnKeyPressed (int row) override;
    void selectedRowsChanged (int lastRowSelected) override;
    juce::String getTooltipForRow (int row) override;

    void create();
    void openSelected();
    void askToDelete (const juce::File& projectFile);
    void askToRename (const juce::File& projectFile);
    void chooseFolder();
    void updateLocationText();

    const ui::Theme& theme;
    RecentProjects& recent;
    juce::File newProjectFolder;
    juce::Array<juce::File> recentFiles;

    juce::Label title, tagline;
    std::unique_ptr<juce::Drawable> logo;   // the mark beside the title
    juce::Rectangle<int> logoArea;
    juce::Label newHeading, nameLabel, locationLabel, locationValue, templatesNote;
    juce::TextEditor nameEditor;
    juce::TextButton changeFolderButton { "Change..." }, createButton { "Create project" };

    juce::Label recentHeading, recentEmpty;
    juce::ListBox recentList { "Recent projects", this };
    juce::TextButton openButton { "Open" }, openOtherButton { "Open another project..." };
    juce::TextButton audioSettingsButton { "Audio settings..." };

    juce::Rectangle<int> newCard, recentCard;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::ScopedMessageBox messageBox;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StartScreen)
};

} // namespace aether
