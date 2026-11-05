// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "ProjectView.h"

#include "AppCommands.h"

namespace aether
{

ProjectView::ProjectView (const ui::Theme& themeToUse, Project& projectToShow, juce::ApplicationCommandManager& commands)
    : theme (themeToUse), project (projectToShow), trackList (themeToUse, projectToShow)
{
    // Toolbar buttons trigger the same commands as the menus, so they share shortcuts,
    // tooltips (which show the shortcut) and enabled/disabled state.
    struct ButtonSetup { juce::TextButton& button; juce::CommandID command; const char* text; };

    for (auto& b : { ButtonSetup { undoButton,     commands::undo,          "Undo" },
                     ButtonSetup { redoButton,     commands::redo,          "Redo" },
                     ButtonSetup { addAudioButton, commands::addAudioTrack, "+ Audio track" },
                     ButtonSetup { addMidiButton,  commands::addMidiTrack,  "+ MIDI track" } })
    {
        b.button.setCommandToTrigger (&commands, b.command, true);
        b.button.setButtonText (b.text);
        b.button.setWantsKeyboardFocus (false);   // keep the arrow keys working in the track list
        addAndMakeVisible (b.button);
    }

    trackViewport.setViewedComponent (&trackList, false);
    trackViewport.setScrollBarsShown (true, false);
    addAndMakeVisible (trackViewport);

    timelineNote.setText ("The timeline (bars and beats, clips, playback and recording) arrives in milestones M2 and M3.",
                          juce::dontSendNotification);
    timelineNote.setColour (juce::Label::textColourId, theme.textDim);
    timelineNote.setJustificationType (juce::Justification::centred);
    timelineNote.setFont (juce::FontOptions (14.0f));
    addAndMakeVisible (timelineNote);
}

void ProjectView::paint (juce::Graphics& g)
{
    g.fillAll (theme.background);

    g.setColour (theme.panel);
    g.fillRect (toolbarArea);
    g.setColour (theme.outline);
    g.drawHorizontalLine (toolbarArea.getBottom() - 1, 0.0f, (float) getWidth());

    // An empty timeline with faint vertical lines, so the layout makes sense already.
    g.setColour (theme.meterBackground);
    g.fillRect (timelineArea);
    g.setColour (theme.outline.withAlpha (0.4f));

    for (int x = timelineArea.getX() + 80; x < timelineArea.getRight(); x += 80)
        g.drawVerticalLine (x, (float) timelineArea.getY(), (float) timelineArea.getBottom());

    g.setColour (theme.outline);
    g.drawVerticalLine (timelineArea.getX(), (float) timelineArea.getY(), (float) timelineArea.getBottom());
}

void ProjectView::resized()
{
    auto area = getLocalBounds();

    toolbarArea = area.removeFromTop (48);
    {
        auto bar = toolbarArea.reduced (12, 8);
        undoButton.setBounds (bar.removeFromLeft (70));
        bar.removeFromLeft (6);
        redoButton.setBounds (bar.removeFromLeft (70));
        bar.removeFromLeft (24);
        addAudioButton.setBounds (bar.removeFromLeft (130));
        bar.removeFromLeft (6);
        addMidiButton.setBounds (bar.removeFromLeft (130));
    }

    auto trackArea = area.removeFromLeft (300);
    trackViewport.setBounds (trackArea);
    trackList.setSize (trackArea.getWidth() - trackViewport.getScrollBarThickness(), trackList.getIdealHeight());

    timelineArea = area;
    timelineNote.setBounds (timelineArea.reduced (40));
}

} // namespace aether
