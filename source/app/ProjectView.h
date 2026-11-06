// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <functional>

#include <juce_gui_basics/juce_gui_basics.h>

#include "AppContext.h"
#include "engine/TimelineSync.h"
#include "model/Project.h"
#include "ui/TimelineComponent.h"
#include "ui/TrackListComponent.h"
#include "ui/TransportBar.h"

namespace aether
{

/** The window contents while a project is open: the toolbar and transport at the top, the
    track headers on the left and the timeline on the right. */
class ProjectView final : public juce::Component
{
public:
    ProjectView (AppContext& context, Project& project);
    ~ProjectView() override;

    ui::TrackListComponent& getTrackList() noexcept   { return trackList; }
    ui::TimelineComponent& getTimeline() noexcept     { return *timeline; }
    Project& getProject() noexcept                    { return project; }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    /** A scrolling area that says when it has been scrolled, so the timeline lanes can follow. */
    struct TrackViewport final : public juce::Viewport
    {
        std::function<void()> onScroll;
        void visibleAreaChanged (const juce::Rectangle<int>&) override   { if (onScroll) onScroll(); }
    };

    AppContext& ctx;
    Project& project;
    TimelineSync timelineSync;

    juce::TextButton undoButton, redoButton, addAudioButton, addMidiButton;
    ui::TransportBar transportBar;
    TrackViewport trackViewport;
    ui::TrackListComponent trackList;
    std::unique_ptr<ui::TimelineComponent> timeline;
    juce::Rectangle<int> toolbarArea, laneHeaderArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProjectView)
};

} // namespace aether
