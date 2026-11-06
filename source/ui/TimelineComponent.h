// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <memory>

#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"
#include "TimelineView.h"
#include "TrackListComponent.h"
#include "engine/AudioEngine.h"
#include "model/Project.h"

namespace aether::ui
{

/** The timeline on the right of the project window. From top to bottom:

    - the ruler: a loop strip (drag to set the loop, double-click to switch it on/off) and the
      bar numbers (click or drag to move the playhead),
    - the time signature lane (click a signature to change it, right-click to add one),
    - the tempo lane (double-click to add a tempo change, drag points, right-click for options),
    - one lane per track, lined up with the track headers on the left (clips arrive in M3),
    - a scroll bar.

    Ctrl + mouse wheel zooms, Shift + mouse wheel scrolls sideways, and the mouse wheel on its
    own scrolls the tracks up and down. The playhead follows playback.
*/
class TimelineComponent final : public juce::Component,
                                private juce::ValueTree::Listener,
                                private juce::ScrollBar::Listener,
                                private juce::Timer
{
public:
    static constexpr int rulerHeight = 36;
    static constexpr int signatureHeight = 22;
    static constexpr int tempoHeight = 66;
    static constexpr int headerHeight = rulerHeight + signatureHeight + tempoHeight;
    static constexpr int scrollBarHeight = 14;

    TimelineComponent (const Theme& theme, Project& project, AudioEngine& engine,
                       juce::Viewport& trackViewport, TrackListComponent& trackList);
    ~TimelineComponent() override;

    TimelineView& getView() noexcept   { return view; }

    void zoomIn();
    void zoomOut();

    /** Call when the track list scrolls, so the lanes follow. */
    void tracksScrolled();

    void resized() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    class Ruler;
    class SignatureLane;
    class TempoLane;
    class TrackLanes;
    friend class Ruler;
    friend class SignatureLane;
    friend class TempoLane;
    friend class TrackLanes;

    void rebuildTempoMap();
    void updateScrollBar();
    void repaintAll();
    int playheadX() const;
    Tick snap (Tick tick, const juce::ModifierKeys& mods) const;
    void drawGrid (juce::Graphics&, juce::Rectangle<int> area, bool strong) const;

    void timerCallback() override;
    void scrollBarMoved (juce::ScrollBar*, double newRangeStart) override;

    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override  { rebuildTempoMap(); }
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override               { rebuildTempoMap(); }
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override        { rebuildTempoMap(); }
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override                { rebuildTempoMap(); }

    const Theme& theme;
    Project& project;
    AudioEngine& engine;
    juce::Viewport& trackViewport;
    TrackListComponent& trackList;
    juce::ValueTree state;

    TimelineView view;
    TempoMap tempoMap;   // a copy of the project's tempo, for drawing

    std::unique_ptr<Ruler> ruler;
    std::unique_ptr<SignatureLane> signatureLane;
    std::unique_ptr<TempoLane> tempoLane;
    std::unique_ptr<TrackLanes> trackLanes;
    juce::ScrollBar scrollBar { false };

    int lastPlayheadX = -1000;
    bool updatingScrollBar = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TimelineComponent)
};

} // namespace aether::ui
