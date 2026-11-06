// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "ProjectView.h"

#include "AppCommands.h"

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
      timelineSync (projectToShow, context.engine),
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

    timeline = std::make_unique<ui::TimelineComponent> (ctx.theme, project, ctx.engine, trackViewport, trackList);
    addAndMakeVisible (*timeline);

    trackViewport.onScroll = [this] { timeline->tracksScrolled(); };

    trackList.onSelectionChanged = [this]
    {
        ctx.commands.commandStatusChanged();
        timeline->tracksScrolled();   // repaint the lanes: the selected one is highlighted
    };
}

ProjectView::~ProjectView()
{
    trackList.onSelectionChanged = nullptr;
    trackViewport.onScroll = nullptr;
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
    g.drawText ("Loop  /  Bars", labels.removeFromTop (ui::TimelineComponent::rulerHeight), juce::Justification::centredLeft);
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
    left.removeFromBottom (ui::TimelineComponent::scrollBarHeight);   // line up with the timeline's lanes
    trackViewport.setBounds (left);
    trackList.setSize (left.getWidth() - trackViewport.getScrollBarThickness(), trackList.getIdealHeight());

    timeline->setBounds (area);
}

} // namespace aether
