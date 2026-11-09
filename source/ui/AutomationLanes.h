// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <functional>

#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"
#include "TrackRows.h"
#include "model/Automation.h"

namespace aether::ui
{

/** The automation lanes in the timeline (Milestone 14), drawn over the track area on the rows
    under the tracks (TrackRows); the rest of the area is left to the clips.

    In a lane: click to add a point (on the grid; hold Alt for anywhere), drag a point to move it,
    double-click a point to remove it, Ctrl+drag a line to bend it, right-click for more. The line
    is the setting's value as the song plays; with no points it shows the value it has now. */
class AutomationLaneArea final : public juce::Component,
                                 public juce::SettableTooltipClient,
                                 private juce::ValueTree::Listener
{
public:
    /** What the lanes need from the timeline. */
    struct Host
    {
        std::function<double (Tick)> tickToX;
        std::function<Tick (double)> xToTick;
        std::function<Tick (Tick, juce::ModifierKeys)> snap;
        std::function<int()> scrollY;                                 // the track list's scroll position
        std::function<void (const juce::String& trackId)> focusTrack;
        std::function<juce::String (Tick)> describeTime;              // "bar 3, beat 2"
    };

    AutomationLaneArea (const Theme& theme, Project& project, Host host);
    ~AutomationLaneArea() override;

    /** Only the lanes take the mouse; everywhere else, the clips do. */
    bool hitTest (int x, int y) override;

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    // For the tests.
    juce::Rectangle<int> laneArea (const juce::String& laneId) const;
    juce::Point<float> pointPosition (const juce::String& laneId, int index) const;
    juce::Point<float> positionFor (const juce::String& laneId, Tick tick, float value) const;

private:
    struct LaneInfo
    {
        juce::String laneId, trackId;
        juce::ValueTree lane;
        automation::Target target;
        automation::Range range;
        juce::Rectangle<int> area;   // the whole row, in this component
    };

    struct Hit
    {
        int point = -1;      // a point under the mouse
        int segment = -1;    // or the line after this point
    };

    std::optional<LaneInfo> laneAt (juce::Point<int> p) const;
    std::optional<LaneInfo> laneById (const juce::String& laneId) const;
    juce::Rectangle<float> inner (const LaneInfo&) const;
    float yFor (const LaneInfo&, float value) const;
    float valueForY (const LaneInfo&, float y) const;
    Hit hitAt (const LaneInfo&, juce::Point<float> p) const;
    void drawLane (juce::Graphics&, const LaneInfo&) const;
    void showMenu (const LaneInfo&, Hit hit, Tick tick);

    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override   { repaint(); }
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override               { repaint(); }
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override        { repaint(); }

    const Theme& theme;
    Project& project;
    Host host;
    juce::ValueTree state;

    struct Drag
    {
        enum class Kind { none, point, bend } kind = Kind::none;
        juce::String laneId;
        int index = -1;
        float startBend = 0.0f;
        float downY = 0.0f;
    } drag;

    juce::String hoverLane;
    int hoverPoint = -1;
    juce::Point<float> hoverPosition;
};

} // namespace aether::ui
