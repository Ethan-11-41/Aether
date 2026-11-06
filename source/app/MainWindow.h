// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <functional>

#include <juce_gui_basics/juce_gui_basics.h>

#include "AppContext.h"
#include "ProjectView.h"
#include "StartScreen.h"
#include "ui/DelayedTooltipWindow.h"

namespace aether
{

/** Everything inside Aether's main window: the menu bar, the start screen or the open project,
    and a status bar. It also handles the commands (menus and keyboard shortcuts) and the
    "file" conversations with the user: save prompts, open dialogs, crash recovery. */
class MainContent final : public juce::Component,
                          public juce::ApplicationCommandTarget,
                          public juce::FileDragAndDropTarget,
                          public juce::MenuBarModel,
                          private juce::Timer
{
public:
    explicit MainContent (AppContext& context);
    ~MainContent() override;

    /** Opens a project file, asking about unsaved changes and offering recovery if needed. */
    void openProjectFile (const juce::File& file);

    /** Called once after start-up: offers to recover work if Aether didn't close properly. */
    void checkForRecoveryAtStartup (juce::Time previousLaunchTime);

    /** Asks about unsaved changes, then quits. */
    void requestQuit();

    /** The window title, e.g. "My Song* - Aether". */
    juce::String getWindowTitle() const;
    std::function<void()> onTitleChanged;

    // juce::ApplicationCommandTarget
    juce::ApplicationCommandTarget* getNextCommandTarget() override   { return nullptr; }
    void getAllCommands (juce::Array<juce::CommandID>&) override;
    void getCommandInfo (juce::CommandID, juce::ApplicationCommandInfo&) override;
    bool perform (const InvocationInfo&) override;

    // juce::MenuBarModel
    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex (int menuIndex, const juce::String& menuName) override;
    void menuItemSelected (int, int) override {}

    // Files dropped anywhere that doesn't take them itself: a project file opens; audio and MIDI
    // files become new tracks (or, on the start screen, a new project).
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray& files, int x, int y) override;
    void fileDragExit (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    /** What dropping these files would do, e.g. "Open My Song", or empty if nothing. */
    juce::String describeDrop (const juce::StringArray& files) const;
    void createProjectWithFiles (const juce::Array<juce::File>& files);
    juce::String dropMessage;   // shown while files are dragged over the window

    void showStartScreen (bool focusName = false);
    void showProjectView();

    /** Runs `then` once any unsaved changes have been saved or deliberately discarded.
        Does nothing if the user cancels. */
    void afterUnsavedChangesAreHandled (std::function<void()> then);

    void createProject (const juce::File& parentFolder, const juce::String& name);
    void openProjectFileAfterChecks (const juce::File& file);
    void offerRecovery (const juce::File& file, const juce::String& intro);
    void chooseAndOpenProject();
    void save (std::function<void (bool saved)> then = {});
    void saveAs();
    void closeProject();
    void showAudioSettings();
    void showAbout();
    void showExportDialog();
    void showShortcuts();
    void closeExportWindow();
    void testCrashRecovery();
    void showError (const juce::String& title, const juce::String& message);

    void addTrack (TrackKind kind);
    void deleteSelectedTrack();
    void moveSelectedTrack (int delta);
    juce::String selectedTrackId() const;

    void updateStatus();

    AppContext& ctx;
    std::unique_ptr<StartScreen> startScreen;
    std::unique_ptr<ProjectView> projectView;
    juce::MenuBarComponent menuBar;
    juce::Label statusLabel;

    /** Shows each control's tooltip (its name and what it does) when the mouse rests on it. */
    ui::DelayedTooltipWindow tooltipWindow { this };

    juce::ScopedMessageBox messageBox;
    std::unique_ptr<juce::FileChooser> fileChooser;
    juce::Component::SafePointer<juce::DialogWindow> audioSettingsWindow;
    std::unique_ptr<juce::DocumentWindow> exportWindow;   // not modal: you can keep working while it exports
    std::unique_ptr<juce::DocumentWindow> shortcutsWindow;

    // Last seen state, so the timer only updates things that changed.
    juce::String lastTitle;
    bool lastCanUndo = false, lastCanRedo = false, lastHasProject = false, lastRunning = false, lastLooping = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainContent)
};

//==============================================================================
class MainWindow final : public juce::DocumentWindow
{
public:
    explicit MainWindow (AppContext& context);
    ~MainWindow() override;

    MainContent& getMainContent()   { return *content; }

    void closeButtonPressed() override;

private:
    AppContext& ctx;
    MainContent* content = nullptr;   // owned by the window (setContentOwned)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
};

} // namespace aether
