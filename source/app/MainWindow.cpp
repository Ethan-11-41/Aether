// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "MainWindow.h"

#include "AppCommands.h"
#include "AudioSettingsComponent.h"
#include "CrashReporter.h"
#include "io/AppFolders.h"
#include "model/EditCommands.h"
#include "model/Recording.h"
#include "model/TimelineEdits.h"

namespace aether
{

namespace
{
    constexpr const char* newProjectFolderKey = "newProjectFolder";
    constexpr const char* windowStateKey = "mainWindowState";
    constexpr const char* metronomeKey = "metronomeEnabled";

    juce::String describeWhen (juce::Time t)
    {
        const auto now = juce::Time::getCurrentTime();
        return t.formatted ("%Y-%m-%d") == now.formatted ("%Y-%m-%d") ? "today at " + t.formatted ("%H:%M")
                                                                       : t.formatted ("%d %b %Y at %H:%M");
    }
}

//==============================================================================
MainContent::MainContent (AppContext& context)
    : ctx (context), menuBar (this)
{
    addAndMakeVisible (menuBar);

    statusLabel.setFont (juce::FontOptions (13.0f));
    statusLabel.setColour (juce::Label::textColourId, ctx.theme.textDim);
    statusLabel.setBorderSize ({});
    addAndMakeVisible (statusLabel);

    showStartScreen();
    setSize (1280, 780);
    startTimerHz (4);
}

MainContent::~MainContent()
{
    stopTimer();

    if (audioSettingsWindow != nullptr)
        delete audioSettingsWindow.getComponent();

    projectView = nullptr;
    startScreen = nullptr;
}

juce::String MainContent::getWindowTitle() const
{
    if (auto* project = ctx.session.getProject())
        return project->getName() + (project->hasUnsavedChanges() ? "*" : "") + " - Aether";

    return "Aether";
}

//==============================================================================
void MainContent::showStartScreen (bool focusName)
{
    projectView = nullptr;

    const auto savedFolder = ctx.settings.get().getValue (newProjectFolderKey);
    const auto folder = juce::File::isAbsolutePath (savedFolder) ? juce::File (savedFolder)
                                                                 : projectfiles::defaultProjectsFolder();

    startScreen = std::make_unique<StartScreen> (ctx.theme, ctx.recentProjects, folder);
    juce::Component::SafePointer<MainContent> safeThis (this);

    startScreen->onCreate = [safeThis] (const juce::File& parent, const juce::String& name)
    {
        if (safeThis != nullptr) safeThis->createProject (parent, name);
    };
    startScreen->onOpen = [safeThis] (const juce::File& file)
    {
        if (safeThis != nullptr) safeThis->openProjectFile (file);
    };
    startScreen->onOpenOther = [safeThis]
    {
        if (safeThis != nullptr) safeThis->chooseAndOpenProject();
    };
    startScreen->onAudioSettings = [safeThis]
    {
        if (safeThis != nullptr) safeThis->showAudioSettings();
    };
    startScreen->onNewProjectFolderChanged = [safeThis] (const juce::File& f)
    {
        if (safeThis != nullptr) safeThis->ctx.settings.get().setValue (newProjectFolderKey, f.getFullPathName());
    };

    addAndMakeVisible (*startScreen);
    resized();

    if (focusName)
        startScreen->focusNameField();

    ctx.commands.commandStatusChanged();
}

void MainContent::showProjectView()
{
    auto* project = ctx.session.getProject();

    if (project == nullptr)
    {
        showStartScreen();
        return;
    }

    startScreen = nullptr;
    projectView = std::make_unique<ProjectView> (ctx, *project);
    addAndMakeVisible (*projectView);
    resized();
    projectView->getTrackList().grabKeyboardFocus();
    ctx.commands.commandStatusChanged();
    updateStatus();
}

//==============================================================================
void MainContent::afterUnsavedChangesAreHandled (std::function<void()> then)
{
    auto* project = ctx.session.getProject();

    if (project == nullptr || ! project->hasUnsavedChanges())
    {
        then();
        return;
    }

    juce::Component::SafePointer<MainContent> safeThis (this);

    messageBox = juce::AlertWindow::showScopedAsync (
        juce::MessageBoxOptions()
            .withIconType (juce::MessageBoxIconType::QuestionIcon)
            .withTitle ("Save changes?")
            .withMessage ("Do you want to save the changes to \"" + project->getName() + "\"?")
            .withButton ("Save", 1)
            .withButton ("Don't save", 2)
            .withButton ("Cancel", 0)
            .withAssociatedComponent (this),
        [safeThis, then] (int result)
        {
            if (safeThis == nullptr)
                return;

            if (result == 1)
            {
                safeThis->save ([then] (bool saved) { if (saved) then(); });
            }
            else if (result == 2)
            {
                // The user chose to throw the changes away, so the autosave must go too,
                // otherwise Aether would offer to "recover" them next time.
                if (auto* autosaver = safeThis->ctx.session.getAutosaver())
                    autosaver->discard();

                then();
            }
        });
}

void MainContent::createProject (const juce::File& parentFolder, const juce::String& name)
{
    ctx.settings.get().setValue (newProjectFolderKey, parentFolder.getFullPathName());

    projectView = nullptr;
    const auto result = ctx.session.createNew (parentFolder, name);

    if (result.failed())
    {
        showError ("Couldn't create the project", result.getErrorMessage());
        return;
    }

    showProjectView();
}

void MainContent::openProjectFile (const juce::File& file)
{
    juce::Component::SafePointer<MainContent> safeThis (this);

    afterUnsavedChangesAreHandled ([safeThis, file]
    {
        if (safeThis != nullptr)
            safeThis->openProjectFileAfterChecks (file);
    });
}

void MainContent::openProjectFileAfterChecks (const juce::File& file)
{
    if (! file.existsAsFile())
    {
        ctx.recentProjects.remove (file);

        if (startScreen != nullptr)
            startScreen->refresh();

        showError ("Project not found", "The project file " + file.getFullPathName()
                                        + " doesn't exist any more. It may have been moved or deleted.");
        return;
    }

    if (ctx.session.hasProject() && ctx.session.getLocation().getMainFile() == file)
        return;   // already open

    const ProjectLocation location (file);

    if (projectfiles::hasRecoverableAutosave (location))
    {
        offerRecovery (file, "\"" + location.getName() + "\" has unsaved changes from "
                             + describeWhen (location.getAutosaveFile().getLastModificationTime())
                             + ", probably because Aether closed unexpectedly.");
        return;
    }

    projectView = nullptr;   // the view refers to the project that is about to be replaced
    const auto result = ctx.session.open (file);

    if (ctx.session.hasProject())
        showProjectView();
    else
        showStartScreen();

    if (result.failed())
        showError ("Couldn't open the project", result.getErrorMessage());
}

void MainContent::offerRecovery (const juce::File& file, const juce::String& intro)
{
    juce::Component::SafePointer<MainContent> safeThis (this);

    messageBox = juce::AlertWindow::showScopedAsync (
        juce::MessageBoxOptions()
            .withIconType (juce::MessageBoxIconType::WarningIcon)
            .withTitle ("Recover unsaved work?")
            .withMessage (intro + "\n\nRecover these changes? You can then save them, or close without saving.")
            .withButton ("Recover", 1)
            .withButton ("Open last saved version", 2)
            .withButton ("Cancel", 0)
            .withAssociatedComponent (this),
        [safeThis, file] (int choice)
        {
            if (safeThis == nullptr || choice == 0)
                return;

            auto& self = *safeThis;
            self.projectView = nullptr;
            const auto result = self.ctx.session.open (file, choice == 1);

            if (result.wasOk() && choice == 2)
                if (auto* autosaver = self.ctx.session.getAutosaver())
                    autosaver->discard();   // the user chose not to keep the unsaved changes

            if (self.ctx.session.hasProject())
                self.showProjectView();
            else
                self.showStartScreen();

            if (result.failed())
                self.showError ("Couldn't open the project", result.getErrorMessage());
        });
}

void MainContent::checkForRecoveryAtStartup (juce::Time previousLaunchTime)
{
    const auto crashReport = crashreporter::findReportNewerThan (previousLaunchTime);
    const auto reportNote = crashReport.has_value()
                                ? "\n\nA crash report was saved, which can help find the cause:\n" + crashReport->getFullPathName()
                                : juce::String();

    if (const auto leftOpen = ctx.session.getProjectLeftOpenLastTime(); leftOpen.has_value() && leftOpen->existsAsFile())
    {
        const ProjectLocation location (*leftOpen);

        if (projectfiles::hasRecoverableAutosave (location))
        {
            offerRecovery (*leftOpen, "Aether didn't close properly last time, while \"" + location.getName()
                                      + "\" was open. A copy of your unsaved work from "
                                      + describeWhen (location.getAutosaveFile().getLastModificationTime())
                                      + " was kept." + reportNote);
            return;
        }
    }

    if (crashReport.has_value())
        messageBox = juce::AlertWindow::showScopedAsync (
            juce::MessageBoxOptions()
                .withIconType (juce::MessageBoxIconType::InfoIcon)
                .withTitle ("Aether closed unexpectedly")
                .withMessage ("Aether closed unexpectedly last time. Your saved work is safe: nothing was waiting to be saved."
                              + reportNote)
                .withButton ("OK", 0)
                .withAssociatedComponent (this),
            nullptr);
}

void MainContent::chooseAndOpenProject()
{
    const auto start = ctx.session.hasProject() ? ctx.session.getLocation().getFolder().getParentDirectory()
                                                : projectfiles::defaultProjectsFolder();

    fileChooser = std::make_unique<juce::FileChooser> ("Open a project", start, "*" + juce::String (projectformat::fileExtension));
    juce::Component::SafePointer<MainContent> safeThis (this);

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [safeThis] (const juce::FileChooser& chooser)
    {
        const auto file = chooser.getResult();

        if (safeThis != nullptr && file != juce::File())
            safeThis->openProjectFile (file);
    });
}

void MainContent::save (std::function<void (bool)> then)
{
    const auto result = ctx.session.save();

    if (result.failed())
        showError ("Couldn't save the project", result.getErrorMessage()
                   + "\n\nYour work is still open in Aether. Try \"Save as...\" to save it somewhere else.");

    updateStatus();

    if (then)
        then (result.wasOk());
}

void MainContent::saveAs()
{
    if (! ctx.session.hasProject())
        return;

    const auto current = ctx.session.getLocation();
    const auto suggestion = current.getFolder().getParentDirectory()
                                   .getChildFile (current.getName() + " copy" + projectformat::fileExtension);

    fileChooser = std::make_unique<juce::FileChooser> ("Save a copy of the project (Aether makes a folder with this name)",
                                                       suggestion, "*" + juce::String (projectformat::fileExtension));
    juce::Component::SafePointer<MainContent> safeThis (this);

    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                              [safeThis] (const juce::FileChooser& chooser)
    {
        const auto chosen = chooser.getResult();

        if (safeThis == nullptr || chosen == juce::File())
            return;

        const auto result = safeThis->ctx.session.saveAs (chosen.getParentDirectory(), chosen.getFileNameWithoutExtension());

        if (result.failed())
            safeThis->showError ("Couldn't save the project", result.getErrorMessage());

        safeThis->updateStatus();
    });
}

void MainContent::closeProject()
{
    juce::Component::SafePointer<MainContent> safeThis (this);

    afterUnsavedChangesAreHandled ([safeThis]
    {
        if (safeThis == nullptr)
            return;

        safeThis->projectView = nullptr;
        safeThis->ctx.session.close();
        safeThis->showStartScreen();
    });
}

void MainContent::requestQuit()
{
    afterUnsavedChangesAreHandled ([] { juce::JUCEApplication::getInstance()->quit(); });
}

void MainContent::showAudioSettings()
{
    if (audioSettingsWindow != nullptr)
    {
        audioSettingsWindow->toFront (true);
        return;
    }

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned (new AudioSettingsComponent (ctx.devices, ctx.engine, ctx.theme));
    options.dialogTitle = "Audio settings";
    options.dialogBackgroundColour = ctx.theme.background;
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = true;
    options.componentToCentreAround = this;
    audioSettingsWindow = options.launchAsync();
}

void MainContent::showAbout()
{
    messageBox = juce::AlertWindow::showScopedAsync (
        juce::MessageBoxOptions()
            .withIconType (juce::MessageBoxIconType::InfoIcon)
            .withTitle ("About Aether")
            .withMessage (juce::String ("Aether ") + AETHER_VERSION_STRING + "\n"
                          "A free, open-source digital audio workstation.\n\n"
                          "Aether is free software under the GNU Affero General Public License v3: you may use, "
                          "share and change it. The source code is at https://github.com/Ethan-11-41/Aether\n\n"
                          "Built with JUCE 9 and other open-source libraries; see CREDITS.txt in Aether's "
                          "install folder for the full list and their licences.")
            .withButton ("OK", 0)
            .withButton ("Open the source code page", 1)
            .withAssociatedComponent (this),
        [] (int result)
        {
            if (result == 1)
                juce::URL ("https://github.com/Ethan-11-41/Aether").launchInDefaultBrowser();
        });
}

void MainContent::testCrashRecovery()
{
    juce::Component::SafePointer<MainContent> safeThis (this);

    messageBox = juce::AlertWindow::showScopedAsync (
        juce::MessageBoxOptions()
            .withIconType (juce::MessageBoxIconType::WarningIcon)
            .withTitle ("Test crash recovery")
            .withMessage ("Aether will now crash on purpose, so you can check that your unsaved work is "
                          "offered back when you start it again.\n\nAether autosaves first, so your latest "
                          "changes are included. Continue?")
            .withButton ("Crash now", 1)
            .withButton ("Cancel", 0)
            .withAssociatedComponent (this),
        [safeThis] (int result)
        {
            if (safeThis == nullptr || result != 1)
                return;

            if (auto* autosaver = safeThis->ctx.session.getAutosaver())
            {
                autosaver->autosaveNow();
                autosaver->waitUntilIdle();
            }

            safeThis->ctx.settings.save();
            crashreporter::crashDeliberately();
        });
}

void MainContent::showError (const juce::String& title, const juce::String& message)
{
    juce::Logger::writeToLog ("Error shown: " + title + ": " + message.replaceCharacter ('\n', ' '));

    messageBox = juce::AlertWindow::showScopedAsync (
        juce::MessageBoxOptions()
            .withIconType (juce::MessageBoxIconType::WarningIcon)
            .withTitle (title)
            .withMessage (message)
            .withButton ("OK", 0)
            .withAssociatedComponent (this),
        nullptr);
}

//==============================================================================
juce::String MainContent::selectedTrackId() const
{
    return projectView != nullptr ? projectView->getTrackList().getSelectedTrackId() : juce::String();
}

void MainContent::addTrack (TrackKind kind)
{
    auto* project = ctx.session.getProject();

    if (project == nullptr || projectView == nullptr)
        return;

    // New tracks go below the selected one (or at the end if nothing is selected).
    const auto selectedIndex = project->indexOfTrack (selectedTrackId());
    auto track = edit::addTrack (*project, kind, selectedIndex >= 0 ? selectedIndex + 1 : -1);
    projectView->getTrackList().selectTrack (track.getId());
    projectView->getTrackList().grabKeyboardFocus();
}

void MainContent::deleteSelectedTrack()
{
    auto* project = ctx.session.getProject();
    const auto trackId = selectedTrackId();

    if (project == nullptr || trackId.isEmpty() || projectView == nullptr)
        return;

    // All the selected tracks go, in one undo step.
    const auto index = project->indexOfTrack (projectView->getTrackList().getSelectedTrackIds()[0]);
    edit::deleteTracks (*project, projectView->getTrackList().getSelectedTrackIds());

    // Select the neighbour, so pressing Delete repeatedly works as expected.
    if (project->getNumTracks() > 0 && projectView != nullptr)
        projectView->getTrackList().selectTrack (project->getTrack (juce::jmin (index, project->getNumTracks() - 1)).getId());
}

void MainContent::moveSelectedTrack (int delta)
{
    auto* project = ctx.session.getProject();
    const auto trackId = selectedTrackId();

    if (project != nullptr && trackId.isNotEmpty())
        edit::moveTrack (*project, trackId, project->indexOfTrack (trackId) + delta);
}

//==============================================================================
void MainContent::getAllCommands (juce::Array<juce::CommandID>& list)
{
    list.addArray ({ commands::newProject, commands::openProject, commands::saveProject, commands::saveProjectAs,
                     commands::closeProject, commands::showProjectFolder, commands::quit,
                     commands::undo, commands::redo, commands::selectAll,
                     commands::importAudio, commands::splitClips, commands::duplicateClips,
                     commands::nudgeLeft, commands::nudgeRight, commands::nudgeLeftFine, commands::nudgeRightFine,
                     commands::toggleSnap, commands::toggleMixer, commands::addBus, commands::record, commands::togglePunch,
                     commands::addAudioTrack, commands::addMidiTrack, commands::renameTrack, commands::deleteTrack,
                     commands::moveTrackUp, commands::moveTrackDown,
                     commands::playStop, commands::stop, commands::goToStart, commands::toggleLoop,
                     commands::toggleMetronome, commands::zoomIn, commands::zoomOut,
                     commands::audioSettings, commands::about, commands::testCrashRecovery, commands::openLogFolder });
}

void MainContent::getCommandInfo (juce::CommandID id, juce::ApplicationCommandInfo& info)
{
    using juce::KeyPress;
    constexpr auto ctrl = juce::ModifierKeys::commandModifier;
    constexpr auto shift = juce::ModifierKeys::shiftModifier;

    auto* project = ctx.session.getProject();
    const auto hasProject = project != nullptr && projectView != nullptr;
    const auto hasTrack = hasProject && selectedTrackId().isNotEmpty();
    const auto trackIndex = hasProject ? project->indexOfTrack (selectedTrackId()) : -1;

    switch (id)
    {
        case commands::newProject:
            info.setInfo ("New project...", "Close this project and start a new one", commands::categories::file, 0);
            info.addDefaultKeypress ('n', ctrl);
            break;

        case commands::openProject:
            info.setInfo ("Open project...", "Open a project file (.aether)", commands::categories::file, 0);
            info.addDefaultKeypress ('o', ctrl);
            break;

        case commands::saveProject:
            info.setInfo ("Save", "Save the project", commands::categories::file, 0);
            info.addDefaultKeypress ('s', ctrl);
            info.setActive (hasProject);
            break;

        case commands::saveProjectAs:
            info.setInfo ("Save as...", "Save a copy of the project under a new name and keep working on the copy",
                          commands::categories::file, 0);
            info.addDefaultKeypress ('s', ctrl | shift);
            info.setActive (hasProject);
            break;

        case commands::closeProject:
            info.setInfo ("Close project", "Close the project and go back to the start screen", commands::categories::file, 0);
            info.addDefaultKeypress ('w', ctrl);
            info.setActive (hasProject);
            break;

        case commands::showProjectFolder:
            info.setInfo ("Show project folder", "Open the project's folder in File Explorer", commands::categories::file, 0);
            info.setActive (hasProject);
            break;

        case commands::quit:
            info.setInfo ("Exit", "Close Aether", commands::categories::file, 0);
            break;

        case commands::undo:
        {
            const auto canUndo = hasProject && project->getUndoManager().canUndo();
            info.setInfo (canUndo ? "Undo " + project->getUndoManager().getUndoDescription() : juce::String ("Undo"),
                          "Undo the last change", commands::categories::edit, 0);
            info.addDefaultKeypress ('z', ctrl);
            info.setActive (canUndo);
            break;
        }

        case commands::redo:
        {
            const auto canRedo = hasProject && project->getUndoManager().canRedo();
            info.setInfo (canRedo ? "Redo " + project->getUndoManager().getRedoDescription() : juce::String ("Redo"),
                          "Redo the change you just undid", commands::categories::edit, 0);
            info.addDefaultKeypress ('y', ctrl);
            info.addDefaultKeypress ('z', ctrl | shift);
            info.setActive (canRedo);
            break;
        }

        case commands::addAudioTrack:
            info.setInfo ("Add audio track", "Add a track for recorded or imported sound", commands::categories::track, 0);
            info.addDefaultKeypress ('t', ctrl);
            info.setActive (hasProject);
            break;

        case commands::addMidiTrack:
            info.setInfo ("Add MIDI track", "Add a track for notes that play an instrument", commands::categories::track, 0);
            info.addDefaultKeypress ('t', ctrl | shift);
            info.setActive (hasProject);
            break;

        case commands::addBus:
            info.setInfo ("Add bus", "Add a bus: a channel other tracks can play into, to share effects (like a "
                          "reverb) or set the volume of several tracks at once", commands::categories::track, 0);
            info.addDefaultKeypress ('b', ctrl | shift);
            info.setActive (hasProject);
            break;

        case commands::renameTrack:
            info.setInfo ("Rename track", "Rename the selected track", commands::categories::track, 0);
            info.addDefaultKeypress (KeyPress::F2Key, 0);
            info.setActive (hasTrack);
            break;

        case commands::deleteTrack:
        {
            const auto clipsSelected = hasProject && projectView->getTimeline().hasSelectedClips();
            const auto tracks = hasProject ? projectView->getTrackList().getSelectedTrackIds().size() : 0;
            info.setInfo (clipsSelected ? "Delete selected clips" : tracks > 1 ? "Delete " + juce::String (tracks) + " tracks" : "Delete track",
                          "Delete the selected clips, or the selected tracks if no clips are selected (you can undo this)",
                          commands::categories::track, 0);
            info.addDefaultKeypress (KeyPress::deleteKey, 0);
            info.addDefaultKeypress (KeyPress::backspaceKey, 0);
            info.setActive (clipsSelected || hasTrack);
            break;
        }

        case commands::selectAll:
            info.setInfo ("Select all clips", "Select every clip in the project", commands::categories::edit, 0);
            info.addDefaultKeypress ('a', ctrl);
            info.setActive (hasProject);
            break;

        case commands::importAudio:
            info.setInfo ("Import audio file...", "Add audio files (WAV, AIFF, FLAC, MP3, Ogg) at the playhead, "
                          "on the selected track. You can also drag files onto the timeline.", commands::categories::track, 0);
            info.addDefaultKeypress ('i', ctrl);
            info.setActive (hasProject);
            break;

        case commands::splitClips:
            info.setInfo ("Split at playhead", "Cut the selected clips (or the clips on the selected track) in two at the playhead",
                          commands::categories::edit, 0);
            info.addDefaultKeypress ('s', 0);
            info.setActive (hasProject);
            break;

        case commands::duplicateClips:
            info.setInfo ("Duplicate", "Copy the selected clips and put the copies right after them", commands::categories::edit, 0);
            info.addDefaultKeypress ('d', ctrl);
            info.setActive (hasProject && projectView->getTimeline().hasSelectedClips());
            break;

        case commands::nudgeLeft:
        case commands::nudgeRight:
        case commands::nudgeLeftFine:
        case commands::nudgeRightFine:
        {
            const auto left = id == commands::nudgeLeft || id == commands::nudgeLeftFine;
            const auto fine = id == commands::nudgeLeftFine || id == commands::nudgeRightFine;
            info.setInfo (juce::String ("Nudge ") + (left ? "left" : "right") + (fine ? " by 10 ms" : " by one grid step"),
                          "Move the selected clips (or the playhead, if no clips are selected)", commands::categories::edit, 0);
            info.addDefaultKeypress (left ? KeyPress::leftKey : KeyPress::rightKey, fine ? shift : 0);
            info.setActive (hasProject);
            break;
        }

        case commands::toggleSnap:
            info.setInfo ("Snap to grid", "Switch snapping to the grid off or on", commands::categories::view, 0);
            info.addDefaultKeypress ('g', 0);
            info.setActive (hasProject);
            info.setTicked (hasProject && projectView->getTimeline().getGrid() != GridSize::off);
            break;

        case commands::moveTrackUp:
            info.setInfo ("Move track up", "Move the selected track up the list", commands::categories::track, 0);
            info.addDefaultKeypress (KeyPress::upKey, ctrl);
            info.setActive (hasTrack && trackIndex > 0);
            break;

        case commands::moveTrackDown:
            info.setInfo ("Move track down", "Move the selected track down the list", commands::categories::track, 0);
            info.addDefaultKeypress (KeyPress::downKey, ctrl);
            info.setActive (hasTrack && trackIndex < project->getNumTracks() - 1);
            break;

        case commands::playStop:
        {
            const auto status = ctx.engine.getTransportStatus();
            const auto running = status.playing || status.countingIn;
            info.setInfo (running ? "Pause" : "Play", "Start or stop playback", commands::categories::transport, 0);
            info.addDefaultKeypress (KeyPress::spaceKey, 0);
            info.setActive (hasProject);
            break;
        }

        case commands::stop:
            info.setInfo ("Stop", "Stop playback. Press again to go back to the start.", commands::categories::transport, 0);
            info.setActive (hasProject);
            break;

        case commands::record:
            info.setInfo ("Record", "Record the armed tracks (the R button on a track arms it). Press again, or Space, "
                          "to stop.", commands::categories::transport, 0);
            info.addDefaultKeypress ('r', 0);
            info.setActive (hasProject);
            info.setTicked (hasProject && projectView->isRecording());
            break;

        case commands::togglePunch:
            info.setInfo ("Punch in/out", "Only record inside the loop region (set it in the strip at the top of the ruler)",
                          commands::categories::transport, 0);
            info.addDefaultKeypress ('p', 0);
            info.setActive (hasProject);
            info.setTicked (hasProject && (bool) project->getState()[ids::punch]);
            break;

        case commands::goToStart:
            info.setInfo ("Go to start", "Move the playhead to the start of the song", commands::categories::transport, 0);
            info.addDefaultKeypress (KeyPress::homeKey, 0);
            info.setActive (hasProject);
            break;

        case commands::toggleLoop:
            info.setInfo ("Loop", "Repeat the loop region (drag in the top strip of the ruler to set it)",
                          commands::categories::transport, 0);
            info.addDefaultKeypress ('l', 0);
            info.setActive (hasProject);
            info.setTicked (hasProject && timeline::getLoop (*project).enabled);
            break;

        case commands::toggleMetronome:
            info.setInfo ("Metronome", "Click on every beat while playing. Right-click the button for the volume.",
                          commands::categories::transport, 0);
            info.addDefaultKeypress ('k', 0);
            info.setTicked (ctx.engine.isMetronomeEnabled());
            break;

        case commands::zoomIn:
            info.setInfo ("Zoom in", "Show less of the song in more detail (or Ctrl + mouse wheel)", commands::categories::view, 0);
            info.addDefaultKeypress ('=', ctrl);
            info.addDefaultKeypress ('+', ctrl);
            info.setActive (hasProject);
            break;

        case commands::zoomOut:
            info.setInfo ("Zoom out", "Show more of the song", commands::categories::view, 0);
            info.addDefaultKeypress ('-', ctrl);
            info.setActive (hasProject);
            break;

        case commands::toggleMixer:
            info.setInfo ("Mixer", "Show or hide the mixer (volume, pan, effects and routing for every track)",
                          commands::categories::view, 0);
            info.addDefaultKeypress (KeyPress::F9Key, 0);
            info.setActive (hasProject);
            info.setTicked (hasProject && projectView->isMixerVisible());
            break;

        case commands::audioSettings:
            info.setInfo ("Audio settings...", "Choose your sound device and buffer size", commands::categories::settings, 0);
            info.addDefaultKeypress (',', ctrl);
            break;

        case commands::about:
            info.setInfo ("About Aether", "Version, licence and source code", commands::categories::help, 0);
            break;

        case commands::testCrashRecovery:
            info.setInfo ("Test crash recovery...", "Crash Aether on purpose to check that unsaved work is recovered",
                          commands::categories::help, 0);
            break;

        case commands::openLogFolder:
            info.setInfo ("Open log folder", "Show Aether's log file and crash reports (useful when reporting a problem)",
                          commands::categories::help, 0);
            break;

        default:
            break;
    }
}

bool MainContent::perform (const InvocationInfo& invocation)
{
    juce::Component::SafePointer<MainContent> safeThis (this);
    auto* project = ctx.session.getProject();

    switch (invocation.commandID)
    {
        case commands::newProject:
            afterUnsavedChangesAreHandled ([safeThis]
            {
                if (safeThis == nullptr) return;
                safeThis->projectView = nullptr;
                safeThis->ctx.session.close();
                safeThis->showStartScreen (true);
            });
            return true;

        case commands::openProject:        chooseAndOpenProject(); return true;
        case commands::saveProject:        save(); return true;
        case commands::saveProjectAs:      saveAs(); return true;
        case commands::closeProject:       closeProject(); return true;
        case commands::quit:               requestQuit(); return true;

        case commands::showProjectFolder:
            if (ctx.session.hasProject())
                ctx.session.getLocation().getMainFile().revealToUser();
            return true;

        case commands::undo:
            if (project != nullptr) project->getUndoManager().undo();
            return true;

        case commands::redo:
            if (project != nullptr) project->getUndoManager().redo();
            return true;

        case commands::addAudioTrack:      addTrack (TrackKind::audio); return true;
        case commands::addMidiTrack:       addTrack (TrackKind::midi); return true;
        case commands::deleteTrack:
            if (projectView != nullptr && projectView->getTimeline().hasSelectedClips())
                projectView->getTimeline().deleteSelectedClips();
            else
                deleteSelectedTrack();
            return true;

        case commands::selectAll:
            if (projectView != nullptr) projectView->getTimeline().selectAllClips();
            return true;

        case commands::importAudio:
            if (projectView != nullptr)
                projectView->chooseFilesToImport (selectedTrackId(), ctx.engine.getTransportStatus().position);
            return true;

        case commands::splitClips:
            if (projectView != nullptr) projectView->getTimeline().splitClipsAtPlayhead();
            return true;

        case commands::duplicateClips:
            if (projectView != nullptr) projectView->getTimeline().duplicateSelectedClips();
            return true;

        case commands::nudgeLeft:       if (projectView != nullptr) projectView->getTimeline().nudge (-1, false); return true;
        case commands::nudgeRight:      if (projectView != nullptr) projectView->getTimeline().nudge (+1, false); return true;
        case commands::nudgeLeftFine:   if (projectView != nullptr) projectView->getTimeline().nudge (-1, true);  return true;
        case commands::nudgeRightFine:  if (projectView != nullptr) projectView->getTimeline().nudge (+1, true);  return true;

        case commands::toggleSnap:
            if (projectView != nullptr) projectView->getTimeline().toggleSnap();
            return true;
        case commands::moveTrackUp:        moveSelectedTrack (-1); return true;
        case commands::moveTrackDown:      moveSelectedTrack (+1); return true;

        case commands::renameTrack:
            if (projectView != nullptr) projectView->getTrackList().startRenamingSelected();
            return true;

        case commands::playStop:
        {
            const auto status = ctx.engine.getTransportStatus();
            if (projectView != nullptr && projectView->isRecording()) projectView->stopRecording();
            else if (status.playing || status.countingIn)            ctx.engine.stop();
            else                                                      ctx.engine.play();
            ctx.commands.commandStatusChanged();
            return true;
        }

        case commands::stop:
        {
            const auto status = ctx.engine.getTransportStatus();
            if (projectView != nullptr && projectView->isRecording()) projectView->stopRecording();
            else if (status.playing || status.countingIn)            ctx.engine.stop();
            else                                                      ctx.engine.locate (0);
            ctx.commands.commandStatusChanged();
            return true;
        }

        case commands::record:
            if (projectView != nullptr)
                projectView->toggleRecording();
            ctx.commands.commandStatusChanged();
            return true;

        case commands::togglePunch:
            if (project != nullptr)
                edit::setPunch (*project, ! (bool) project->getState()[ids::punch]);
            ctx.commands.commandStatusChanged();
            return true;

        case commands::goToStart:
            ctx.engine.locate (0);
            return true;

        case commands::toggleLoop:
            if (project != nullptr)
                edit::setLoopEnabled (*project, ! timeline::getLoop (*project).enabled);
            ctx.commands.commandStatusChanged();
            return true;

        case commands::toggleMetronome:
            ctx.engine.setMetronomeEnabled (! ctx.engine.isMetronomeEnabled());
            ctx.settings.get().setValue (metronomeKey, ctx.engine.isMetronomeEnabled());
            ctx.commands.commandStatusChanged();
            return true;

        case commands::zoomIn:
            if (projectView != nullptr) projectView->getTimeline().zoomIn();
            return true;

        case commands::zoomOut:
            if (projectView != nullptr) projectView->getTimeline().zoomOut();
            return true;

        case commands::toggleMixer:
            if (projectView != nullptr) projectView->setMixerVisible (! projectView->isMixerVisible());
            return true;

        case commands::addBus:             addTrack (TrackKind::bus); return true;

        case commands::audioSettings:      showAudioSettings(); return true;
        case commands::about:              showAbout(); return true;
        case commands::testCrashRecovery:  testCrashRecovery(); return true;

        case commands::openLogFolder:
        {
            const auto logFile = appfolders::logs().getChildFile ("Aether.log");
            if (logFile.existsAsFile())
                logFile.revealToUser();
            else
                appfolders::localData().revealToUser();
            return true;
        }

        default:
            return false;
    }
}

//==============================================================================
juce::StringArray MainContent::getMenuBarNames()
{
    return { "File", "Edit", "Track", "Transport", "View", "Settings", "Help" };
}

juce::PopupMenu MainContent::getMenuForIndex (int menuIndex, const juce::String&)
{
    juce::PopupMenu menu;
    auto* cm = &ctx.commands;

    if (menuIndex == 0)
    {
        menu.addCommandItem (cm, commands::newProject);
        menu.addCommandItem (cm, commands::openProject);

        juce::PopupMenu recentMenu;
        juce::Component::SafePointer<MainContent> safeThis (this);

        for (const auto& file : ctx.recentProjects.getFiles())
            recentMenu.addItem (file.getFileNameWithoutExtension(), [safeThis, file]
            {
                if (safeThis != nullptr) safeThis->openProjectFile (file);
            });

        menu.addSubMenu ("Open recent", recentMenu, recentMenu.getNumItems() > 0);
        menu.addSeparator();
        menu.addCommandItem (cm, commands::saveProject);
        menu.addCommandItem (cm, commands::saveProjectAs);
        menu.addCommandItem (cm, commands::closeProject);
        menu.addSeparator();
        menu.addCommandItem (cm, commands::showProjectFolder);
        menu.addSeparator();
        menu.addCommandItem (cm, commands::quit);
    }
    else if (menuIndex == 1)
    {
        menu.addCommandItem (cm, commands::undo);
        menu.addCommandItem (cm, commands::redo);
        menu.addSeparator();
        menu.addCommandItem (cm, commands::splitClips);
        menu.addCommandItem (cm, commands::duplicateClips);
        menu.addCommandItem (cm, commands::deleteTrack);
        menu.addCommandItem (cm, commands::selectAll);
        menu.addSeparator();
        menu.addCommandItem (cm, commands::nudgeLeft);
        menu.addCommandItem (cm, commands::nudgeRight);
        menu.addCommandItem (cm, commands::nudgeLeftFine);
        menu.addCommandItem (cm, commands::nudgeRightFine);
    }
    else if (menuIndex == 2)
    {
        menu.addCommandItem (cm, commands::addAudioTrack);
        menu.addCommandItem (cm, commands::addMidiTrack);
        menu.addCommandItem (cm, commands::addBus);
        menu.addCommandItem (cm, commands::importAudio);
        menu.addSeparator();
        menu.addCommandItem (cm, commands::renameTrack);
        menu.addCommandItem (cm, commands::moveTrackUp);
        menu.addCommandItem (cm, commands::moveTrackDown);
        menu.addSeparator();
        menu.addCommandItem (cm, commands::deleteTrack);
    }
    else if (menuIndex == 3)
    {
        menu.addCommandItem (cm, commands::playStop);
        menu.addCommandItem (cm, commands::stop);
        menu.addCommandItem (cm, commands::goToStart);
        menu.addCommandItem (cm, commands::record);
        menu.addSeparator();
        menu.addCommandItem (cm, commands::toggleLoop);
        menu.addCommandItem (cm, commands::togglePunch);
        menu.addCommandItem (cm, commands::toggleMetronome);
    }
    else if (menuIndex == 4)
    {
        menu.addCommandItem (cm, commands::toggleMixer);
        menu.addSeparator();
        menu.addCommandItem (cm, commands::zoomIn);
        menu.addCommandItem (cm, commands::zoomOut);
        menu.addCommandItem (cm, commands::toggleSnap);
    }
    else if (menuIndex == 5)
    {
        menu.addCommandItem (cm, commands::audioSettings);
    }
    else if (menuIndex == 6)
    {
        menu.addCommandItem (cm, commands::about);
        menu.addSeparator();
        menu.addCommandItem (cm, commands::openLogFolder);
        menu.addCommandItem (cm, commands::testCrashRecovery);
    }

    return menu;
}

//==============================================================================
void MainContent::updateStatus()
{
    juce::StringArray parts;

    if (auto* project = ctx.session.getProject())
    {
        parts.add (project->hasUnsavedChanges() ? "Unsaved changes" : "Saved");

        if (auto* autosaver = ctx.session.getAutosaver(); autosaver != nullptr && autosaver->getLastAutosaveTime().toMilliseconds() > 0)
            parts.add ("autosaved at " + autosaver->getLastAutosaveTime().formatted ("%H:%M"));
    }

    const auto device = ctx.devices.describeCurrentDevice();
    parts.add (device.isNotEmpty() ? device : juce::String ("No audio device (Settings > Audio settings)"));

    statusLabel.setText (parts.joinIntoString (juce::String::fromUTF8 ("  \xc2\xb7  ")), juce::dontSendNotification);
}

void MainContent::timerCallback()
{
    // Free timelines (and their audio) the audio thread has finished with, even with no project open.
    ctx.engine.collectGarbage();

    // Keep the title, status bar and toolbar buttons up to date.
    const auto title = getWindowTitle();

    if (title != lastTitle)
    {
        lastTitle = title;

        if (onTitleChanged)
            onTitleChanged();
    }

    auto* project = ctx.session.getProject();
    const auto canUndo = project != nullptr && project->getUndoManager().canUndo();
    const auto canRedo = project != nullptr && project->getUndoManager().canRedo();
    const auto hasProject = project != nullptr;

    const auto status = ctx.engine.getTransportStatus();
    const auto running = status.playing || status.countingIn;
    const auto looping = project != nullptr && timeline::getLoop (*project).enabled;

    if (canUndo != lastCanUndo || canRedo != lastCanRedo || hasProject != lastHasProject
        || running != lastRunning || looping != lastLooping)
    {
        lastCanUndo = canUndo;
        lastCanRedo = canRedo;
        lastHasProject = hasProject;
        lastRunning = running;
        lastLooping = looping;
        ctx.commands.commandStatusChanged();
    }

    updateStatus();
}

void MainContent::paint (juce::Graphics& g)
{
    g.fillAll (ctx.theme.background);

    auto statusArea = getLocalBounds().removeFromBottom (28);
    g.setColour (ctx.theme.panel);
    g.fillRect (statusArea);
    g.setColour (ctx.theme.outline);
    g.drawHorizontalLine (statusArea.getY(), 0.0f, (float) getWidth());
}

void MainContent::resized()
{
    auto area = getLocalBounds();
    menuBar.setBounds (area.removeFromTop (juce::LookAndFeel::getDefaultLookAndFeel().getDefaultMenuBarHeight()));
    statusLabel.setBounds (area.removeFromBottom (28).reduced (16, 0));

    if (startScreen != nullptr)  startScreen->setBounds (area);
    if (projectView != nullptr)  projectView->setBounds (area);
}

//==============================================================================
MainWindow::MainWindow (AppContext& context)
    : DocumentWindow ("Aether", context.theme.background, DocumentWindow::allButtons),
      ctx (context)
{
    setUsingNativeTitleBar (true);

    content = new MainContent (ctx);
    setContentOwned (content, true);

    ctx.commands.registerAllCommandsForTarget (content);
    ctx.commands.setFirstCommandTarget (content);
    addKeyListener (ctx.commands.getKeyMappings());

    content->onTitleChanged = [this] { setName (content->getWindowTitle()); };

    setResizable (true, true);
    setResizeLimits (1000, 640, 10000, 10000);

    const auto savedState = ctx.settings.get().getValue (windowStateKey);

    if (savedState.isEmpty() || ! restoreWindowStateFromString (savedState))
        centreWithSize (getWidth(), getHeight());

    setVisible (true);
}

MainWindow::~MainWindow()
{
    ctx.settings.get().setValue (windowStateKey, getWindowStateAsString());
    removeKeyListener (ctx.commands.getKeyMappings());
    ctx.commands.setFirstCommandTarget (nullptr);
    clearContentComponent();
}

void MainWindow::closeButtonPressed()
{
    juce::JUCEApplication::getInstance()->systemRequestedQuit();
}

} // namespace aether
