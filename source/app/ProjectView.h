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

    /** Imports audio files onto a track (or new tracks) at a position, and selects the new clips. */
    void importFiles (const juce::Array<juce::File>& files, const juce::String& trackId, Tick position);

    /** Asks which audio files to import, then imports them at `position` on `trackId`. */
    void chooseFilesToImport (const juce::String& trackId, Tick position);

    static constexpr const char* gridKey = "gridSize";

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
    juce::ComboBox gridBox;
    juce::Rectangle<int> toolbarArea, laneHeaderArea;
    std::unique_ptr<juce::FileChooser> fileChooser;
    juce::ScopedMessageBox messageBox;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProjectView)
};

} // namespace aether
