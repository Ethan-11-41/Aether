// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "ProjectView.h"

#include "AppCommands.h"

#include "io/AudioImport.h"

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
                     ButtonSetup { addAudioButton, commands::addAudioTrack, "+ Audio track" },
                     ButtonSetup { addMidiButton,  commands::addMidiTrack,  "+ MIDI track" } })
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

    trackList.onSelectionChanged = [this]
    {
        ctx.commands.commandStatusChanged();
        timeline->tracksScrolled();   // repaint the lanes: the selected one is highlighted
    };
}

void ProjectView::importFiles (const juce::Array<juce::File>& files, const juce::String& trackId, Tick position)
{
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
    fileChooser = std::make_unique<juce::FileChooser> ("Import audio files",
                                                       juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                       ctx.audioFiles.getWildcard());
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
        addAudioButton.setBounds (bar.removeFromLeft (118));
        bar.removeFromLeft (6);
        addMidiButton.setBounds (bar.removeFromLeft (112));
        bar.removeFromLeft (24);
        transportBar.setBounds (bar.removeFromLeft (std::min (bar.getWidth(), ui::TransportBar::idealWidth)));
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
