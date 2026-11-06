// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "ProjectView.h"

#include "AppCommands.h"

#include <map>

#include "io/AudioImport.h"
#include "io/FileTypes.h"
#include "io/MidiImport.h"
#include "model/TimelineEdits.h"

namespace aether
{

namespace
{
    constexpr int toolbarHeight = 54;
    constexpr int trackColumnWidth = 300;
}

ProjectView::ProjectView (AppContext& context, Project& projectToShow)
    : ctx (context),
      project (projectToShow),
      timelineSync (projectToShow, context.engine, context.audioFiles, [&session = context.session] { return session.getLocation(); }),
      transportBar (context.theme, projectToShow, context.engine, context.commands, context.settings.get()),
      trackList (context.theme, projectToShow)
{
    // Toolbar buttons trigger the same commands as the menus, so they share shortcuts,
    // tooltips (which show the shortcut) and enabled/disabled state.
    struct ButtonSetup { juce::TextButton& button; juce::CommandID command; const char* text; };

    for (auto& b : { ButtonSetup { undoButton,     commands::undo,          "Undo" },
                     ButtonSetup { redoButton,     commands::redo,          "Redo" },
                     ButtonSetup { addAudioButton, commands::addAudioTrack, "+ Audio" },
                     ButtonSetup { addMidiButton,  commands::addMidiTrack,  "+ MIDI" },
                     ButtonSetup { addBusButton,   commands::addBus,        "+ Bus" },
                     ButtonSetup { mixerButton,    commands::toggleMixer,   "Mixer" } })
    {
        b.button.setCommandToTrigger (&ctx.commands, b.command, true);
        b.button.setButtonText (b.text);
        b.button.setWantsKeyboardFocus (false);   // keep the arrow keys working in the track list
        addAndMakeVisible (b.button);
    }

    addAndMakeVisible (transportBar);

    trackViewport.setViewedComponent (&trackList, false);
    trackViewport.setScrollBarsShown (true, false);
    addAndMakeVisible (trackViewport);

    timeline = std::make_unique<ui::TimelineComponent> (ctx.theme, project, ctx.engine, ctx.audioFiles,
                                                        [&session = ctx.session] { return session.getLocation(); },
                                                        trackViewport, trackList);
    addAndMakeVisible (*timeline);

    timeline->onImportFiles = [this] (const juce::Array<juce::File>& files, const juce::String& trackId, Tick position)
    {
        importFiles (files, trackId, position);
    };
    timeline->onImportRequested = [this] (const juce::String& trackId, Tick position) { chooseFilesToImport (trackId, position); };
    timeline->onClipSelectionChanged = [this] { ctx.commands.commandStatusChanged(); };

    // The snap grid, remembered between sessions.
    for (size_t i = 0; i < allGridSizes.size(); ++i)
        gridBox.addItem (juce::String ("Snap: ") + gridSizeName (allGridSizes[i]), (int) i + 1);

    gridBox.setTooltip ("What clips, the loop and the playhead snap to when you drag them. "
                        "Press G to switch snapping off and on. Hold Alt while dragging for exact positions.");
    gridBox.setWantsKeyboardFocus (false);
    gridBox.onChange = [this]
    {
        const auto index = gridBox.getSelectedId() - 1;
        if (juce::isPositiveAndBelow (index, (int) allGridSizes.size()))
            timeline->setGrid (allGridSizes[(size_t) index]);
    };
    timeline->onGridChanged = [this]
    {
        const auto grid = timeline->getGrid();
        for (size_t i = 0; i < allGridSizes.size(); ++i)
            if (allGridSizes[i] == grid)
                gridBox.setSelectedId ((int) i + 1, juce::dontSendNotification);
        ctx.settings.get().setValue (gridKey, gridSizeId (grid));
        ctx.commands.commandStatusChanged();
    };
    timeline->setGrid (gridSizeFromId (ctx.settings.get().getValue (gridKey, "beat").toRawUTF8()));
    timeline->onGridChanged();
    addAndMakeVisible (gridBox);

    trackViewport.onScroll = [this] { timeline->tracksScrolled(); };

    // Recording. Recordings are corrected by the measured latency (or the driver's figure).
    recorder = std::make_unique<Recorder> (project, ctx.engine, timelineSync,
                                           [&session = ctx.session] { return session.getLocation(); },
                                           [&devices = ctx.devices] { return devices.getLatencyInfo().recordingCorrectionSamples(); },
                                           [&devices = ctx.devices] { return devices.getNumInputChannels(); },
                                           [&devices = ctx.devices]
                                           {
                                               // MIDI is late only by what you hear: the output side of the round trip.
                                               const auto info = devices.getLatencyInfo();
                                               return info.measuredRoundTripSamples >= 0
                                                          ? std::max (0, info.measuredRoundTripSamples - info.inputLatencySamples)
                                                          : info.outputLatencySamples;
                                           });
    recorder->onFinished = [this] (const juce::StringArray& clipIds)
    {
        if (! clipIds.isEmpty())
            timeline->selectClips (clipIds);

        ctx.commands.commandStatusChanged();
    };
    recorder->onProblem = [this] (const juce::String& problem) { showProblem ("Recording", problem); };

    timeline->liveRecording = [this]
    {
        ui::TimelineComponent::LiveRecording live;
        const auto status = recorder->getStatus();
        const auto rate = ctx.engine.getCurrentSampleRate();

        if (recorder->isActive() && status.recording && rate > 0.0)
        {
            live.trackIds = recorder->getRecordingTracks();
            live.start = timeline::buildTempoMap (project).secondsToTicks ((double) status.startSample / rate);
        }

        return live;
    };

    trackList.inputChannelCount = [&devices = ctx.devices] { return devices.getNumInputChannels(); };

    // Audio or MIDI files dropped on a track header go onto that track, at the playhead.
    trackList.canImport = [this] (const juce::File& file) { return filetypes::isImportable (file, ctx.audioFiles.getWildcard()); };
    trackList.onImportFiles = [this] (const juce::Array<juce::File>& files, const juce::String& trackId)
    {
        importFiles (files, trackId, ctx.engine.getTransportStatus().position);
    };

    // The mixer, below the timeline.
    mixer = std::make_unique<ui::MixerComponent> (ctx.theme, project, trackList);
    addChildComponent (*mixer);
    divider.theme = &ctx.theme;
    divider.onDragStart = [this] { heightAtDragStart = mixerHeight; };
    divider.onDrag = [this] (int distance)
    {
        mixerHeight = juce::jlimit (ui::MixerComponent::minimumHeight, std::max (ui::MixerComponent::minimumHeight, getHeight() - 300),
                                    heightAtDragStart + distance);
        ctx.settings.get().setValue (mixerHeightKey, mixerHeight);
        resized();
    };
    addChildComponent (divider);
    mixerHeight = ctx.settings.get().getIntValue (mixerHeightKey, 340);
    setMixerVisible (ctx.settings.get().getBoolValue (mixerVisibleKey, false));

    // The on-screen keyboard, at the very bottom.
    keyboard = std::make_unique<ui::KeyboardPanel> (ctx.theme, [&engine = ctx.engine] (const juce::MidiMessage& m) { engine.sendLiveMidi (m); });
    addChildComponent (*keyboard);
    setKeyboardVisible (ctx.settings.get().getBoolValue (keyboardVisibleKey, false));

    midiLight.theme = &ctx.theme;
    addAndMakeVisible (midiLight);

    trackList.onSelectionChanged = [this]
    {
        ctx.commands.commandStatusChanged();
        timeline->tracksScrolled();   // repaint the lanes: the selected ones are highlighted
        mixer->selectionChanged();
        timelineSync.setFocusedTrack (trackList.getSelectedTrackId());   // a MIDI keyboard plays the selected track
    };
    timelineSync.setFocusedTrack (trackList.getSelectedTrackId());

    startTimerHz (30);   // the level meters
}

void ProjectView::setKeyboardVisible (bool shouldBeVisible)
{
    keyboardVisible = shouldBeVisible;
    keyboard->setVisible (keyboardVisible);
    ctx.settings.get().setValue (keyboardVisibleKey, keyboardVisible);
    resized();
    ctx.commands.commandStatusChanged();
}

void ProjectView::toggleRecording()
{
    if (recorder->isActive())
    {
        recorder->stop();
        return;
    }

    const auto problem = recorder->start();

    if (problem.isNotEmpty())
        showProblem ("Can't record yet", problem);
}

void ProjectView::showProblem (const juce::String& title, const juce::String& message)
{
    messageBox = juce::AlertWindow::showScopedAsync (juce::MessageBoxOptions()
                                                         .withIconType (juce::MessageBoxIconType::InfoIcon)
                                                         .withTitle (title)
                                                         .withMessage (message)
                                                         .withButton ("OK", 0)
                                                         .withAssociatedComponent (this),
                                                     nullptr);
}

void ProjectView::setMixerVisible (bool shouldBeVisible)
{
    mixerVisible = shouldBeVisible;
    mixer->setVisible (mixerVisible);
    divider.setVisible (mixerVisible);
    mixerButton.setToggleState (mixerVisible, juce::dontSendNotification);
    ctx.settings.get().setValue (mixerVisibleKey, mixerVisible);
    resized();
    ctx.commands.commandStatusChanged();
}

void ProjectView::timerCallback()
{
    const auto now = juce::Time::getMillisecondCounterHiRes();
    const auto elapsed = lastMeterTime > 0.0 ? (float) ((now - lastMeterTime) / 1000.0) : 0.0f;
    lastMeterTime = now;

    // Each channel's peak is taken once per frame and shared by the track headers and the mixer.
    std::map<juce::String, std::pair<float, float>> peaks;
    auto peaksOf = [this, &peaks] (const juce::String& channelId)
    {
        auto it = peaks.find (channelId);
        if (it == peaks.end())
            it = peaks.emplace (channelId, timelineSync.takeChannelPeaks (channelId)).first;
        return it->second;
    };

    trackList.updateMeters (peaksOf, elapsed);

    // The MIDI light: on for a moment after each message.
    if (const auto activity = ctx.engine.getMidiActivity(); activity != lastMidiActivity)
    {
        lastMidiActivity = activity;
        midiLitUntil = now + 150.0;
    }

    if (const auto lit = now < midiLitUntil; lit != midiLight.lit)
    {
        midiLight.lit = lit;
        midiLight.repaint();
    }

    if (++tooltipRefresh % 60 == 0)
    {
        const auto inputs = ctx.devices.getEnabledMidiInputNames();
        midiLight.setTooltip (inputs.isEmpty() ? "No MIDI keyboard found. Connect one (e.g. by USB), or use the on-screen keyboard (View menu)."
                                               : "MIDI keyboards: " + inputs.joinIntoString (", ") + ". This lights up when they send notes.");
    }

    if (mixerVisible)
        mixer->updateMeters (peaksOf, elapsed);
}

void ProjectView::importFiles (const juce::Array<juce::File>& allFiles, const juce::String& trackId, Tick position)
{
    // MIDI files become MIDI tracks; everything else is imported as audio.
    juce::Array<juce::File> files;
    juce::StringArray midiClips, midiProblems;

    for (const auto& file : allFiles)
    {
        if (! isMidiFile (file))
        {
            files.add (file);
            continue;
        }

        const auto index = project.indexOfTrack (trackList.getSelectedTrackId());
        const auto midiResult = importMidiFile (project, file, position, index >= 0 ? index + 1 : -1, trackId);
        midiClips.addArray (midiResult.clipIds);

        if (midiResult.problem.isNotEmpty())
            midiProblems.add (midiResult.problem);
        else if (! midiResult.trackIds.isEmpty())
            trackList.selectTrack (midiResult.trackIds[0]);

        // The project already had music in it, so its tempo wasn't changed: ask.
        const auto map = timeline::buildTempoMap (project);
        const auto current = map.tempoAt (position);

        if (midiResult.problem.isEmpty() && ! midiResult.tempoTaken && midiResult.fileTempo > 0.0
            && std::abs (midiResult.fileTempo - current) > 0.05)
        {
            const auto fileTempo = midiResult.fileTempo;
            const auto point = map.tempoPointIndexAt (position);
            juce::Component::SafePointer<ProjectView> safeThis (this);

            messageBox = juce::AlertWindow::showScopedAsync (
                juce::MessageBoxOptions()
                    .withIconType (juce::MessageBoxIconType::QuestionIcon)
                    .withTitle ("Use the MIDI file's tempo?")
                    .withMessage (file.getFileName() + " was written at " + juce::String (fileTempo, 1) + " BPM; this song is at "
                                  + juce::String (current, 1) + " BPM.\n\nThe notes keep their place in the bars either way: "
                                  "the tempo only changes how fast everything plays (recorded audio stays as it is).")
                    .withButton ("Use " + juce::String (fileTempo, 1) + " BPM", 1)
                    .withButton ("Keep " + juce::String (current, 1) + " BPM", 0)
                    .withAssociatedComponent (this),
                [safeThis, fileTempo, point] (int result)
                {
                    if (safeThis != nullptr && result == 1)
                        edit::setTempo (safeThis->project, point, fileTempo);
                });
        }
    }

    if (! midiClips.isEmpty())
        timeline->selectClips (midiClips);

    if (! midiProblems.isEmpty())
        showProblem ("MIDI file not imported", midiProblems.joinIntoString ("\n"));

    if (files.isEmpty())
        return;

    const auto result = importAudioFiles (project, ctx.session.getLocation(), ctx.audioFiles.getFormats(), files, trackId, position);

    if (! result.clipIds.isEmpty())
    {
        timeline->selectClips (result.clipIds);
        trackList.selectTrack (result.trackIds[0]);
    }

    if (! result.problems.isEmpty())
    {
        juce::Logger::writeToLog ("Import problems: " + result.problems.joinIntoString ("; "));
        messageBox = juce::AlertWindow::showScopedAsync (
            juce::MessageBoxOptions()
                .withIconType (juce::MessageBoxIconType::WarningIcon)
                .withTitle (result.clipIds.isEmpty() ? "Nothing was imported" : "Some files weren't imported")
                .withMessage (result.problems.joinIntoString ("\n")
                              + "\n\nAether can import WAV, AIFF, FLAC, MP3 and Ogg Vorbis files.")
                .withButton ("OK", 0)
                .withAssociatedComponent (this),
            nullptr);
    }
}

void ProjectView::chooseFilesToImport (const juce::String& trackId, Tick position)
{
    fileChooser = std::make_unique<juce::FileChooser> ("Import audio or MIDI files",
                                                       juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                       ctx.audioFiles.getWildcard() + ";*.mid;*.midi");
    juce::Component::SafePointer<ProjectView> safeThis (this);

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::canSelectMultipleItems,
                              [safeThis, trackId, position] (const juce::FileChooser& chooser)
    {
        if (safeThis != nullptr && ! chooser.getResults().isEmpty())
            safeThis->importFiles (chooser.getResults(), trackId, position);
    });
}

ProjectView::~ProjectView()
{
    stopTimer();
    recorder = nullptr;   // a recording still running is stopped (its files are kept)
    timeline->liveRecording = nullptr;
    trackList.onSelectionChanged = nullptr;
    trackViewport.onScroll = nullptr;

    // Free the decoded audio of this project. (Anything the audio thread is still playing stays
    // alive until the engine has finished with it.)
    ctx.audioFiles.clear();
}

void ProjectView::paint (juce::Graphics& g)
{
    const auto& theme = ctx.theme;
    g.fillAll (theme.background);

    g.setColour (theme.panel);
    g.fillRect (toolbarArea);
    g.setColour (theme.outline);
    g.drawHorizontalLine (toolbarArea.getBottom() - 1, 0.0f, (float) getWidth());

    // Names for the timeline's top lanes, lined up with them.
    g.setColour (theme.panel);
    g.fillRect (laneHeaderArea);

    auto labels = laneHeaderArea.reduced (14, 0);
    g.setFont (juce::FontOptions (12.5f));
    g.setColour (theme.textDim);
    g.drawText ("Bars", labels.removeFromTop (ui::TimelineComponent::rulerHeight), juce::Justification::centredLeft);
    g.drawText ("Time signature", labels.removeFromTop (ui::TimelineComponent::signatureHeight), juce::Justification::centredLeft);
    g.drawText ("Tempo", labels.removeFromTop (ui::TimelineComponent::tempoHeight), juce::Justification::centredLeft);

    g.setColour (theme.outline);
    g.drawHorizontalLine (laneHeaderArea.getBottom() - 1, (float) laneHeaderArea.getX(), (float) laneHeaderArea.getRight());
    g.drawVerticalLine (laneHeaderArea.getRight() - 1, (float) laneHeaderArea.getY(), (float) getHeight());
}

void ProjectView::resized()
{
    auto area = getLocalBounds();

    toolbarArea = area.removeFromTop (toolbarHeight);
    {
        auto bar = toolbarArea.reduced (12, 8);
        undoButton.setBounds (bar.removeFromLeft (64));
        bar.removeFromLeft (6);
        redoButton.setBounds (bar.removeFromLeft (64));
        bar.removeFromLeft (18);
        addAudioButton.setBounds (bar.removeFromLeft (78));
        bar.removeFromLeft (6);
        addMidiButton.setBounds (bar.removeFromLeft (72));
        bar.removeFromLeft (6);
        addBusButton.setBounds (bar.removeFromLeft (62));
        bar.removeFromLeft (24);
        mixerButton.setBounds (bar.removeFromRight (76));
        bar.removeFromRight (8);
        midiLight.setBounds (bar.removeFromRight (44).withSizeKeepingCentre (44, 26));
        bar.removeFromRight (8);
        transportBar.setBounds (bar.removeFromLeft (std::min (bar.getWidth(), ui::TransportBar::idealWidth)));
    }

    if (keyboardVisible)
        keyboard->setBounds (area.removeFromBottom (ui::KeyboardPanel::idealHeight));

    if (mixerVisible)
    {
        const auto height = juce::jlimit (ui::MixerComponent::minimumHeight,
                                          std::max (ui::MixerComponent::minimumHeight, area.getHeight() - 220), mixerHeight);
        mixer->setBounds (area.removeFromBottom (height));
        divider.setBounds (area.removeFromBottom (6));
    }

    auto left = area.removeFromLeft (trackColumnWidth);
    laneHeaderArea = left.removeFromTop (ui::TimelineComponent::headerHeight);
    gridBox.setBounds (laneHeaderArea.withHeight (ui::TimelineComponent::rulerHeight).reduced (8, 5).withTrimmedLeft (60));
    left.removeFromBottom (ui::TimelineComponent::scrollBarHeight);   // line up with the timeline's lanes
    trackViewport.setBounds (left);
    trackList.setSize (left.getWidth() - trackViewport.getScrollBarThickness(), trackList.getIdealHeight());

    timeline->setBounds (area);
}

} // namespace aether
