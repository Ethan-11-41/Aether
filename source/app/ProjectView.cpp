// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "ProjectView.h"

#include "AppCommands.h"

#include <map>

#include "io/AudioImport.h"
#include "model/MidiClips.h"
#include "dsp/DrumKit.h"
#include "dsp/Sampler.h"
#include "io/SamplerImport.h"
#include "engine/SamplerLibrary.h"
#include "io/FileTypes.h"
#include "io/MidiImport.h"
#include "model/DrumPatterns.h"
#include "model/TimelineEdits.h"
#include "ui/DelayedTooltipWindow.h"

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
                     ButtonSetup { addDrumsButton, commands::addDrumTrack,  "+ Drums" },
                     ButtonSetup { addBusButton,   commands::addBus,        "+ Bus" },
                     ButtonSetup { mixerButton,    commands::toggleMixer,   "Mixer" } })
    {
        b.button.setCommandToTrigger (&ctx.commands, b.command, true);
        b.button.setButtonText (b.text);
        b.button.setWantsKeyboardFocus (false);   // keep the arrow keys working in the track list
        addAndMakeVisible (b.button);
    }

    // "+ MIDI" offers the Synth, drums, and the recorded instruments.
    addMidiButton.setCommandToTrigger (nullptr, 0, false);
    addMidiButton.setTooltip ("Add a MIDI track: the Synth (Ctrl+Shift+T), drums, or a recorded instrument (piano, strings, "
                              "woodwinds, brass, mallets...).");
    addMidiButton.onClick = [this] { showAddMidiMenu(); };

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
    timeline->onEditNotes = [this] (const juce::String& clipId) { editNotes (clipId); };
    timeline->onOpenPianoRoll = [this] (const juce::String& clipId) { showPianoRoll (clipId); };

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

    trackList.onOpenInstrument = [this] (const juce::String& trackId)
    {
        if (drumpattern::isDrumTrack (project, trackId))
            showDrumKit (trackId);
        else if (midi::instrumentOf (project, trackId)[ids::type].toString() == dsp::sampler::typeId)
            showSampler (trackId);
        else
            showSynth (trackId);
    };

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

        // On a drum track, the computer keys play the pads (C2 upwards: A is the kick, S the snare,
        // T Y U the hi-hats); the octave you had comes back on other tracks.
        const auto drums = drumpattern::isDrumTrack (project, trackList.getSelectedTrackId());

        if (drums && octaveBeforeDrums < 0)
        {
            octaveBeforeDrums = keyboard->getOctave();
            keyboard->setOctave (2);
        }
        else if (! drums && octaveBeforeDrums >= 0)
        {
            keyboard->setOctave (octaveBeforeDrums);
            octaveBeforeDrums = -1;
        }
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

void ProjectView::startPatternPlay()
{
    if (drumEditor == nullptr)
        return;

    const auto range = drumEditor->getPatternRange();
    if (range.isEmpty())
        return;

    const auto status = ctx.engine.getTransportStatus();

    if (! patternPlaying)
    {
        // Afterwards, the playhead goes back to where it was.
        positionBeforePattern = status.position;

        if (status.playing)
            ctx.engine.stop();
    }

    const PartLoop loop { drumEditor->getTrackId(), range.getStart(), range.getEnd(), ! drumEditor->playsWithSong() };
    const auto changed = ! (timelineSync.getPartLoop() == loop);
    timelineSync.setPartLoop (loop);

    if (! patternPlaying || changed)
    {
        // From the start of the pattern (or, if only "With song" changed, carrying on).
        if (! patternPlaying || status.position < range.getStart() || status.position >= range.getEnd())
            ctx.engine.locate (range.getStart());

        ctx.engine.play();
    }

    if (! patternPlaying)
    {
        patternPlaying = true;
        patternSeenPlaying = false;
        patternStartTime = juce::Time::getMillisecondCounterHiRes();
    }

    drumEditor->setPatternPlaying (true);
}

void ProjectView::stopPatternPlay()
{
    if (! patternPlaying)
        return;

    patternPlaying = false;
    ctx.engine.stop();
    timelineSync.setPartLoop ({});
    ctx.engine.locate (positionBeforePattern);

    if (drumEditor != nullptr)
        drumEditor->setPatternPlaying (false);
}

void ProjectView::followPatternPlay()
{
    if (! patternPlaying)
        return;

    // Stopped from elsewhere (Space in the main window, the transport's Stop): back to normal.
    const auto status = ctx.engine.getTransportStatus();
    const auto running = status.playing || status.countingIn;
    patternSeenPlaying = patternSeenPlaying || running;

    if (drumEditor == nullptr
        || (! running && (patternSeenPlaying || juce::Time::getMillisecondCounterHiRes() - patternStartTime > 1500.0)))
    {
        stopPatternPlay();
        return;
    }

    // The pattern changed length, or another one is shown: follow it.
    const auto range = drumEditor->getPatternRange();

    if (range.isEmpty())
        stopPatternPlay();
    else
        timelineSync.setPartLoop ({ drumEditor->getTrackId(), range.getStart(), range.getEnd(), ! drumEditor->playsWithSong() });
}

void ProjectView::timerCallback()
{
    followPatternPlay();

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

namespace
{
    /** The piano roll's (and the Synth's) window: it doesn't block the main window, and Aether's
        shortcuts (Space, Ctrl+Z...) work in it too. */
    class PianoRollWindow final : public juce::DocumentWindow
    {
    public:
        PianoRollWindow (juce::Colour background, std::function<void()> whenClosed)
            : juce::DocumentWindow ("Piano roll", background, juce::DocumentWindow::allButtons),
              onClosed (std::move (whenClosed))
        {
        }

        void closeButtonPressed() override   { onClosed(); }

    private:
        std::function<void()> onClosed;
        ui::DelayedTooltipWindow tooltips { this };
    };
}

void ProjectView::showSynth (const juce::String& trackId)
{
    if (synthWindow == nullptr)
    {
        juce::Component::SafePointer<ProjectView> safeThis (this);
        auto window = std::make_unique<PianoRollWindow> (ctx.theme.background, [safeThis]
        {
            juce::MessageManager::callAsync ([safeThis]
            {
                if (safeThis != nullptr)
                {
                    safeThis->synthEditor = nullptr;
                    safeThis->synthWindow = nullptr;
                }
            });
        });

        auto editor = std::make_unique<ui::SynthEditor> (ctx.theme, project, trackId);
        synthEditor = editor.get();
        synthEditor->playNote = [safeThis] (int pitch, int velocity, bool on)
        {
            if (safeThis != nullptr && safeThis->synthEditor != nullptr)
                safeThis->ctx.engine.sendPreviewNote (safeThis->synthEditor->getTrackId(), pitch, velocity, on);
        };
        synthEditor->onTitleChanged = [safeThis]
        {
            if (safeThis != nullptr && safeThis->synthWindow != nullptr && safeThis->synthEditor != nullptr)
                safeThis->synthWindow->setName (safeThis->synthEditor->getTitle());
        };

        window->setUsingNativeTitleBar (true);
        window->setResizable (true, false);
        window->setResizeLimits (760, 520, 4000, 3000);
        window->setContentOwned (editor.release(), true);
        window->addKeyListener (ctx.commands.getKeyMappings());
        window->centreAroundComponent (this, window->getWidth(), window->getHeight());
        window->setVisible (true);
        synthWindow = std::move (window);
    }

    synthEditor->setTrack (trackId);
    synthWindow->setName (synthEditor->getTitle());

    // Playing the keyboard plays this track, so you hear the changes.
    trackList.focusTrack (trackId);
    synthWindow->toFront (true);
    synthEditor->focusKeyboard();
}

void ProjectView::showPianoRoll (const juce::String& clipId)
{
    if (pianoRollWindow == nullptr)
    {
        juce::Component::SafePointer<ProjectView> safeThis (this);
        auto window = std::make_unique<PianoRollWindow> (ctx.theme.background, [safeThis]
        {
            // Closed a moment later, not from inside the window's own callback.
            juce::MessageManager::callAsync ([safeThis]
            {
                if (safeThis != nullptr)
                {
                    safeThis->pianoRoll = nullptr;
                    safeThis->pianoRollWindow = nullptr;
                }
            });
        });

        auto roll = std::make_unique<ui::PianoRoll> (ctx.theme, project, ctx.engine);
        pianoRoll = roll.get();
        pianoRoll->onClipChanged = [safeThis]
        {
            if (safeThis == nullptr || safeThis->pianoRoll == nullptr || safeThis->pianoRollWindow == nullptr)
                return;

            const auto clip = clips::findClip (safeThis->project, safeThis->pianoRoll->getClipId());
            safeThis->pianoRollWindow->setName (clip.isValid() ? "Piano roll - " + clip.getName() : juce::String ("Piano roll"));
        };

        window->setUsingNativeTitleBar (true);
        window->setResizable (true, false);
        window->setResizeLimits (720, 420, 8000, 6000);
        window->setContentOwned (roll.release(), true);
        window->addKeyListener (ctx.commands.getKeyMappings());
        window->centreAroundComponent (this, window->getWidth(), window->getHeight());
        window->setVisible (true);
        pianoRollWindow = std::move (window);
    }

    pianoRoll->setClip (clipId);

    // The keyboard and the on-screen keyboard play the same instrument as the notes being edited.
    if (const auto clip = clips::findClip (project, clipId); clip.isValid())
        trackList.focusTrack (clip.getTrackState()[ids::id].toString());

    pianoRollWindow->toFront (true);
    pianoRoll->grabKeyboardFocus();
}

void ProjectView::showSampler (const juce::String& trackId)
{
    if (samplerWindow == nullptr)
    {
        juce::Component::SafePointer<ProjectView> safeThis (this);
        auto window = std::make_unique<PianoRollWindow> (ctx.theme.background, [safeThis]
        {
            juce::MessageManager::callAsync ([safeThis]
            {
                if (safeThis != nullptr)
                {
                    safeThis->samplerEditor = nullptr;
                    safeThis->samplerWindow = nullptr;
                }
            });
        });

        auto editor = std::make_unique<ui::SamplerEditor> (ctx.theme, project, trackId);
        samplerEditor = editor.get();
        samplerEditor->playNote = [safeThis] (int pitch, int velocity, bool on)
        {
            if (safeThis != nullptr && safeThis->samplerEditor != nullptr)
                safeThis->ctx.engine.sendPreviewNote (safeThis->samplerEditor->getTrackId(), pitch, velocity, on);
        };
        samplerEditor->onTitleChanged = [safeThis]
        {
            if (safeThis != nullptr && safeThis->samplerWindow != nullptr && safeThis->samplerEditor != nullptr)
                safeThis->samplerWindow->setName (safeThis->samplerEditor->getTitle());
        };
        samplerEditor->projectFolder = [safeThis]
        {
            return safeThis != nullptr ? safeThis->ctx.session.getLocation().getFolder() : juce::File();
        };
        samplerEditor->loadingProgress = [safeThis]
        {
            if (safeThis == nullptr || safeThis->samplerEditor == nullptr)
                return std::pair<int, int> { 0, 0 };

            return samplerlibrary::loadingProgress (safeThis->project, safeThis->samplerEditor->getTrackId(),
                                                    safeThis->ctx.session.getLocation().getFolder(), safeThis->ctx.audioFiles);
        };
        samplerEditor->onImportFiles = [safeThis] (const juce::Array<juce::File>& files)
        {
            if (safeThis != nullptr && safeThis->samplerEditor != nullptr)
                safeThis->importSamplerFiles (safeThis->samplerEditor->getTrackId(), files);
        };
        samplerEditor->onChooseFiles = [safeThis]
        {
            if (safeThis != nullptr && safeThis->samplerEditor != nullptr)
                safeThis->chooseSamplerFiles (safeThis->samplerEditor->getTrackId());
        };

        window->setUsingNativeTitleBar (true);
        window->setResizable (true, false);
        window->setResizeLimits (960, 520, 4000, 3000);
        window->setContentOwned (editor.release(), true);
        window->addKeyListener (ctx.commands.getKeyMappings());
        window->centreAroundComponent (this, window->getWidth(), window->getHeight());
        window->setVisible (true);
        samplerWindow = std::move (window);
    }

    samplerEditor->setTrack (trackId);
    samplerWindow->setName (samplerEditor->getTitle());
    trackList.focusTrack (trackId);
    samplerWindow->toFront (true);
    samplerEditor->focusKeyboard();
}

void ProjectView::importSamplerFiles (const juce::String& trackId, const juce::Array<juce::File>& files)
{
    const auto location = ctx.session.getLocation();
    const auto result = importSamplerSound (location, ctx.audioFiles.getFormats(), files);

    if (result.problem.isNotEmpty())
    {
        showProblem ("Sound not used", result.problem);
        return;
    }

    // No track yet ("+ MIDI" > Your own sound): a new one.
    if (trackId.isEmpty() || ! project.findTrack (trackId).isValid())
    {
        addSamplerTrack (result.sound, result.name, result.rootKey);
        return;
    }

    edit::setSamplerSound (project, trackId, result.sound, result.name, result.rootKey,
                           samplerlibrary::suggestedSettings (result.sound, location.getFolder()));
}

void ProjectView::chooseSamplerFiles (const juce::String& trackId)
{
    fileChooser = std::make_unique<juce::FileChooser> ("Choose your sound: one recording, several (named by their notes), a folder, or an SFZ instrument",
                                                       juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                       ctx.audioFiles.getWildcard() + ";*.sfz");
    juce::Component::SafePointer<ProjectView> safeThis (this);

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::canSelectDirectories | juce::FileBrowserComponent::canSelectMultipleItems,
                              [safeThis, trackId] (const juce::FileChooser& chooser)
    {
        if (safeThis != nullptr && ! chooser.getResults().isEmpty())
            safeThis->importSamplerFiles (trackId, chooser.getResults());
    });
}

void ProjectView::addSamplerTrack (const samplersound::Reference& sound, const juce::String& name, int rootKey, bool slide)
{
    const auto index = project.indexOfTrack (trackList.getSelectedTrackId());
    auto settings = samplerlibrary::suggestedSettings (sound, ctx.session.getLocation().getFolder());
    if (slide)
        settings["voiceMode"] = (float) dsp::sampler::slide;
    const auto track = edit::addSamplerTrack (project, sound, name, rootKey, settings, index >= 0 ? index + 1 : -1);

    trackList.selectTrack (track.getId());
    showSampler (track.getId());
}

void ProjectView::showAddMidiMenu()
{
    juce::PopupMenu menu;
    juce::Component::SafePointer<ProjectView> safeThis (this);

    menu.addCommandItem (&ctx.commands, commands::addMidiTrack, "Synth (electronic sounds)");
    menu.addCommandItem (&ctx.commands, commands::addDrumTrack, "Drums");
    menu.addSectionHeader ("Recorded instruments");

    const auto entries = samplerlibrary::list();
    juce::StringArray categories;
    for (const auto& e : entries)
        categories.addIfNotAlreadyThere (e.category);

    for (const auto& category : categories)
    {
        juce::PopupMenu sub;
        for (const auto& e : entries)
            if (e.category == category)
                sub.addItem (e.name, [safeThis, e] { if (safeThis) safeThis->addSamplerTrack (e.reference, e.name, -1); });
        menu.addSubMenu (category, sub);
    }

    if (entries.empty())
        menu.addItem ("(not installed: reinstall Aether to get them)", false, false, nullptr);

    menu.addSeparator();
    menu.addItem ("Your own sound (audio files or an SFZ)...", [safeThis] { if (safeThis) safeThis->chooseSamplerFiles ({}); });
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&addMidiButton));
}

void ProjectView::movePadToOwnTrack (int pad)
{
    // The pad's sound in a sampler, at its own pitch on its own key, sliding between notes.
    const auto drumTrack = drumEditor->getTrackId();
    const auto reference = midi::instrumentOf (project, drumTrack)[dsp::drumkit::soundProperty (pad)].toString();
    const auto name = dsp::drumkit::describeSound (reference);
    samplersound::Reference sound;
    int key = -1;

    if (dsp::drumkit::isBuiltIn (reference))
    {
        sound = samplersound::drum (dsp::drumkit::builtInId (reference));
        key = findDrumRootKey (sound.path);
    }
    else if (reference.isNotEmpty())
    {
        sound = samplersound::recording (reference);
        key = findRootKey (ctx.audioFiles.getFormats(), ctx.session.getLocation().getFolder().getChildFile (reference));
    }
    else
    {
        return;
    }

    key = key >= 0 ? key : 36;
    auto settings = samplerlibrary::suggestedSettings (sound, ctx.session.getLocation().getFolder());
    settings["voiceMode"] = (float) dsp::sampler::slide;

    const auto drumClip = drumEditor->getClipId();
    const auto moved = edit::movePadToOwnTrack (project, drumTrack, pad, sound, name, key, settings, drumClip);

    if (! moved.track.isValid())
        return;

    trackList.selectTrack (moved.track.getId());

    // Straight to its notes, in the piano roll, around its own key.
    const auto it = moved.clipFor.find (drumClip);
    const auto clipId = it != moved.clipFor.end() ? it->second : (moved.clipFor.empty() ? juce::String() : moved.clipFor.begin()->second);

    if (clipId.isEmpty())
    {
        showSampler (moved.track.getId());
        return;
    }

    showPianoRoll (clipId);
    if (pianoRoll != nullptr)
        pianoRoll->scrollToPitch (key + 6);
}

void ProjectView::editNotes (const juce::String& clipId)
{
    if (drumpattern::isDrumClip (project, clips::findClip (project, clipId)))
        showDrumEditor (clipId);
    else
        showPianoRoll (clipId);
}

void ProjectView::openDrumWindow()
{
    if (drumWindow != nullptr)
        return;

    juce::Component::SafePointer<ProjectView> safeThis (this);
    auto window = std::make_unique<PianoRollWindow> (ctx.theme.background, [safeThis]
    {
        juce::MessageManager::callAsync ([safeThis]
        {
            if (safeThis != nullptr)
            {
                safeThis->stopPatternPlay();
                safeThis->drumEditor = nullptr;
                safeThis->drumWindow = nullptr;
            }
        });
    });

    auto editor = std::make_unique<ui::DrumEditor> (ctx.theme, project, ctx.engine);
    drumEditor = editor.get();
    drumEditor->onPlayPattern = [safeThis] (bool play)
    {
        if (safeThis == nullptr)
            return;

        if (play)
            safeThis->startPatternPlay();
        else
            safeThis->stopPatternPlay();
    };
    drumEditor->onTitleChanged = [safeThis]
    {
        if (safeThis != nullptr && safeThis->drumWindow != nullptr && safeThis->drumEditor != nullptr)
            safeThis->drumWindow->setName (safeThis->drumEditor->getTitle());
    };
    drumEditor->onOpenPianoRoll = [safeThis] (const juce::String& clipId)
    {
        if (safeThis != nullptr)
            safeThis->showPianoRoll (clipId);
    };
    drumEditor->onImportSound = [safeThis] (int pad, const juce::File& file)
    {
        if (safeThis == nullptr || safeThis->drumEditor == nullptr)
            return;

        const auto problem = importPadSound (safeThis->project, safeThis->ctx.session.getLocation(), safeThis->ctx.audioFiles.getFormats(),
                                             file, safeThis->drumEditor->getTrackId(), pad);
        if (problem.isNotEmpty())
            safeThis->showProblem ("Sound not used", problem);
        else
            safeThis->drumEditor->hearPad (pad);
    };
    drumEditor->onPlayPadAsNotes = [safeThis] (int pad)
    {
        if (safeThis != nullptr && safeThis->drumEditor != nullptr)
            safeThis->movePadToOwnTrack (pad);
    };
    drumEditor->onChooseSoundFile = [safeThis] (int pad)
    {
        if (safeThis != nullptr)
            safeThis->chooseDrumSoundFile (pad);
    };

    window->setUsingNativeTitleBar (true);
    window->setResizable (true, false);
    window->setResizeLimits (900, 560, 6000, 3000);
    window->setContentOwned (editor.release(), true);
    window->addKeyListener (ctx.commands.getKeyMappings());
    window->centreAroundComponent (this, window->getWidth(), window->getHeight());
    window->setVisible (true);
    drumWindow = std::move (window);
}

void ProjectView::chooseDrumSoundFile (int pad)
{
    fileChooser = std::make_unique<juce::FileChooser> ("Choose a sound for pad " + juce::String (pad + 1),
                                                       juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                       ctx.audioFiles.getWildcard());
    juce::Component::SafePointer<ProjectView> safeThis (this);

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [safeThis, pad] (const juce::FileChooser& chooser)
    {
        if (safeThis != nullptr && safeThis->drumEditor != nullptr && chooser.getResult() != juce::File())
            safeThis->drumEditor->onImportSound (pad, chooser.getResult());
    });
}

void ProjectView::showDrumEditor (const juce::String& clipId)
{
    openDrumWindow();
    drumEditor->setClip (clipId);
    drumWindow->setName (drumEditor->getTitle());

    if (const auto clip = clips::findClip (project, clipId); clip.isValid())
        trackList.focusTrack (clip.getTrackState()[ids::id].toString());

    drumWindow->toFront (true);
    drumEditor->grabKeyboardFocus();
}

void ProjectView::showDrumKit (const juce::String& trackId)
{
    openDrumWindow();
    drumEditor->setTrack (trackId);
    drumWindow->setName (drumEditor->getTitle());
    trackList.focusTrack (trackId);
    drumWindow->toFront (true);
    drumEditor->grabKeyboardFocus();
}

ProjectView::~ProjectView()
{
    stopTimer();
    stopPatternPlay();
    samplerEditor = nullptr;
    samplerWindow = nullptr;
    drumEditor = nullptr;
    drumWindow = nullptr;
    pianoRoll = nullptr;
    pianoRollWindow = nullptr;
    synthEditor = nullptr;
    synthWindow = nullptr;
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
        undoButton.setBounds (bar.removeFromLeft (56));
        bar.removeFromLeft (4);
        redoButton.setBounds (bar.removeFromLeft (56));
        bar.removeFromLeft (12);
        addAudioButton.setBounds (bar.removeFromLeft (70));
        bar.removeFromLeft (4);
        addMidiButton.setBounds (bar.removeFromLeft (64));
        bar.removeFromLeft (4);
        addDrumsButton.setBounds (bar.removeFromLeft (72));
        bar.removeFromLeft (4);
        addBusButton.setBounds (bar.removeFromLeft (56));
        bar.removeFromLeft (14);
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
