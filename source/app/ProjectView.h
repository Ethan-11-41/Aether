// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "model/Project.h"
#include "ui/Theme.h"
#include "ui/TrackListComponent.h"

namespace aether
{

/** The window contents while a project is open: a toolbar, the track list and (from
    milestones M2/M3) the timeline. */
class ProjectView final : public juce::Component
{
public:
    ProjectView (const ui::Theme& theme, Project& project, juce::ApplicationCommandManager& commands);

    ui::TrackListComponent& getTrackList() noexcept   { return trackList; }
    Project& getProject() noexcept                    { return project; }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    const ui::Theme& theme;
    Project& project;

    juce::TextButton undoButton, redoButton, addAudioButton, addMidiButton;
    juce::Viewport trackViewport;
    ui::TrackListComponent trackList;
    juce::Label timelineNote;
    juce::Rectangle<int> toolbarArea, timelineArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProjectView)
};

} // namespace aether
