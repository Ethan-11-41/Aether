// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "AutomationLanes.h"

#include <cmath>

namespace aether::ui
{

namespace
{
    constexpr float margin = 6.0f;        // above the top and below the bottom of a lane's range
    constexpr float pointSize = 8.0f;
    constexpr float grabDistance = 9.0f;
}

AutomationLaneArea::AutomationLaneArea (const Theme& t, Project& p, Host h)
    : theme (t), project (p), host (std::move (h)), state (p.getState())
{
    setTooltip ("Click to add a point, drag points to move them (Alt: off the grid), double-click a point to remove it, "
                "Ctrl+drag a line to bend it. Right-click for more.");
    state.addListener (this);
}

AutomationLaneArea::~AutomationLaneArea()
{
    state.removeListener (this);
}

//==============================================================================
std::optional<AutomationLaneArea::LaneInfo> AutomationLaneArea::laneById (const juce::String& laneId) const
{
    const TrackRows rows (project);
    const auto* row = rows.laneRow (laneId);

    if (row == nullptr)
        return std::nullopt;

    LaneInfo info;
    info.laneId = laneId;
    info.trackId = row->trackId;
    info.lane = automation::findLane (project, laneId);
    info.target = automation::Target::fromString (info.lane[ids::setting].toString());
    const auto range = automation::rangeOf (project, info.trackId, info.target);

    if (! range)
        return std::nullopt;

    info.range = *range;
    info.area = { 0, row->top - (host.scrollY != nullptr ? host.scrollY() : 0), getWidth(), row->height };
    return info;
}

std::optional<AutomationLaneArea::LaneInfo> AutomationLaneArea::laneAt (juce::Point<int> p) const
{
    const TrackRows rows (project);
    const auto* row = rows.rowAt (p.y + (host.scrollY != nullptr ? host.scrollY() : 0));

    if (row == nullptr || ! row->isLane())
        return std::nullopt;

    return laneById (row->laneId);
}

bool AutomationLaneArea::hitTest (int x, int y)
{
    juce::ignoreUnused (x);
    return laneAt ({ x, y }).has_value();
}

juce::Rectangle<int> AutomationLaneArea::laneArea (const juce::String& laneId) const
{
    const auto info = laneById (laneId);
    return info ? info->area : juce::Rectangle<int>();
}

juce::Rectangle<float> AutomationLaneArea::inner (const LaneInfo& info) const
{
    return info.area.toFloat().reduced (0.0f, margin).withTrimmedBottom (1.0f);
}

float AutomationLaneArea::yFor (const LaneInfo& info, float value) const
{
    const auto area = inner (info);
    return area.getBottom() - info.range.toPosition (value) * area.getHeight();
}

float AutomationLaneArea::valueForY (const LaneInfo& info, float y) const
{
    const auto area = inner (info);
    return info.range.fromPosition ((area.getBottom() - y) / std::max (1.0f, area.getHeight()));
}

juce::Point<float> AutomationLaneArea::positionFor (const juce::String& laneId, Tick tick, float value) const
{
    const auto info = laneById (laneId);
    return info ? juce::Point<float> ((float) host.tickToX (tick), yFor (*info, value)) : juce::Point<float>();
}

juce::Point<float> AutomationLaneArea::pointPosition (const juce::String& laneId, int index) const
{
    const auto points = automation::pointsOf (automation::findLane (project, laneId));

    if (! juce::isPositiveAndBelow (index, (int) points.size()))
        return {};

    return positionFor (laneId, points[(size_t) index].tick, points[(size_t) index].value);
}

AutomationLaneArea::Hit AutomationLaneArea::hitAt (const LaneInfo& info, juce::Point<float> p) const
{
    Hit hit;
    const auto points = automation::pointsOf (info.lane);
    auto best = grabDistance;

    for (size_t i = 0; i < points.size(); ++i)
    {
        const auto at = juce::Point<float> ((float) host.tickToX (points[i].tick), yFor (info, points[i].value));
        if (const auto d = at.getDistanceFrom (p); d < best)
        {
            best = d;
            hit.point = (int) i;
        }
    }

    if (hit.point >= 0)
        return hit;

    // The line after which point?
    for (size_t i = 0; i + 1 < points.size(); ++i)
        if (p.x > (float) host.tickToX (points[i].tick) && p.x < (float) host.tickToX (points[i + 1].tick))
            hit.segment = (int) i;

    return hit;
}

//==============================================================================
void AutomationLaneArea::drawLane (juce::Graphics& g, const LaneInfo& info) const
{
    const auto area = inner (info);
    const auto track = project.findTrack (info.trackId);
    const auto colour = track.getColour().brighter (0.3f);
    const auto points = automation::pointsOf (info.lane);

    // Guides: the top and bottom, and a middle mark (0 dB for volume, the centre for pan).
    g.setColour (theme.outline.withAlpha (0.35f));
    g.drawHorizontalLine ((int) area.getY(), 0.0f, (float) getWidth());
    g.drawHorizontalLine ((int) area.getBottom(), 0.0f, (float) getWidth());

    const auto middle = info.target.kind == automation::Target::Kind::volume || info.target.kind == automation::Target::Kind::send
                          ? std::optional<float> (0.0f)
                      : info.target.kind == automation::Target::Kind::pan ? std::optional<float> (0.0f)
                                                                           : std::nullopt;
    if (middle)
    {
        const float dashes[] { 3.0f, 4.0f };
        g.setColour (theme.outline.withAlpha (0.6f));
        g.drawDashedLine (juce::Line<float> (0.0f, yFor (info, *middle), (float) getWidth(), yFor (info, *middle)), dashes, 2);
    }

    if (points.empty())
    {
        // No points: the setting's value now, as a faint dashed line.
        const auto y = yFor (info, automation::staticValue (project, info.trackId, info.target));
        const float dashes[] { 6.0f, 4.0f };
        g.setColour (colour.withAlpha (0.5f));
        g.drawDashedLine (juce::Line<float> (0.0f, y, (float) getWidth(), y), dashes, 2, 1.5f);
        g.setColour (theme.textDim);
        g.setFont (juce::FontOptions (12.0f));
        g.drawText ("Click to add points. The dashed line is " + automation::nameOf (project, info.trackId, info.target).toLowerCase()
                        + " now: " + automation::formatValue (project, info.trackId, info.target,
                                                              automation::staticValue (project, info.trackId, info.target)),
                    info.area.reduced (10, 2), juce::Justification::centredLeft, true);
        return;
    }

    // The line: flat before the first point and after the last, along (or stepping between) the points.
    juce::Path line, fill;
    const auto firstX = (float) host.tickToX (points.front().tick);
    const auto lastX = (float) host.tickToX (points.back().tick);
    const auto holds = info.range.holds();

    line.startNewSubPath (std::min (0.0f, firstX), yFor (info, points.front().value));

    for (size_t i = 0; i < points.size(); ++i)
    {
        const auto& p = points[i];
        const auto x = (float) host.tickToX (p.tick);
        const auto y = yFor (info, p.value);

        if (holds && i > 0)
            line.lineTo (x, yFor (info, points[i - 1].value));   // steps: hold, then jump

        if (! holds && i > 0 && std::abs (points[i - 1].bend) > 1.0e-3f)
        {
            // A bent line, drawn in small pieces.
            const auto& a = points[i - 1];
            const auto ax = (float) host.tickToX (a.tick);
            const auto from = info.range.toPosition (a.value), to = info.range.toPosition (p.value);
            const auto steps = std::max (2, (int) ((x - ax) / 3.0f));

            for (int s = 1; s < steps; ++s)
            {
                const auto along = (float) s / (float) steps;
                const auto position = automation::bendPosition (from, to, along, a.bend);
                line.lineTo (ax + along * (x - ax), inner (info).getBottom() - position * inner (info).getHeight());
            }
        }

        line.lineTo (x, y);
    }

    line.lineTo (std::max ((float) getWidth(), lastX), yFor (info, points.back().value));

    fill = line;
    fill.lineTo (std::max ((float) getWidth(), lastX), area.getBottom());
    fill.lineTo (std::min (0.0f, firstX), area.getBottom());
    fill.closeSubPath();

    g.setColour (colour.withAlpha (0.12f));
    g.fillPath (fill);
    g.setColour (colour);
    g.strokePath (line, juce::PathStrokeType (2.0f));

    for (size_t i = 0; i < points.size(); ++i)
    {
        const auto x = (float) host.tickToX (points[i].tick);
        if (x < -pointSize || x > (float) getWidth() + pointSize)
            continue;

        const auto big = info.laneId == hoverLane && (int) i == hoverPoint;
        const auto size = big ? pointSize + 4.0f : pointSize;
        const auto dot = juce::Rectangle<float> (size, size).withCentre ({ x, yFor (info, points[i].value) });
        g.setColour (theme.meterBackground);
        g.fillEllipse (dot.expanded (1.5f));
        g.setColour (big ? theme.text : colour);
        g.fillEllipse (dot);
    }
}

void AutomationLaneArea::paint (juce::Graphics& g)
{
    const TrackRows rows (project);
    const auto scroll = host.scrollY != nullptr ? host.scrollY() : 0;

    for (const auto& row : rows.getRows())
    {
        if (! row.isLane() || row.bottom() - scroll < 0 || row.top - scroll > getHeight())
            continue;

        if (const auto info = laneById (row.laneId))
        {
            juce::Graphics::ScopedSaveState save (g);
            g.reduceClipRegion (info->area);
            drawLane (g, *info);
        }
    }

    // What's under the mouse (or being dragged): its value and time.
    if (hoverLane.isNotEmpty())
    {
        if (const auto info = laneById (hoverLane))
        {
            const auto points = automation::pointsOf (info->lane);
            juce::String text;

            if (juce::isPositiveAndBelow (hoverPoint, (int) points.size()))
            {
                const auto& p = points[(size_t) hoverPoint];
                text = automation::formatValue (project, info->trackId, info->target, p.value)
                     + (host.describeTime != nullptr ? "   at " + host.describeTime (p.tick) : juce::String());
            }
            else if (drag.kind == Drag::Kind::none)
            {
                text = automation::formatValue (project, info->trackId, info->target, valueForY (*info, hoverPosition.y));
            }

            if (text.isNotEmpty())
            {
                auto box = juce::Rectangle<float> ((float) text.length() * 6.6f + 14.0f, 18.0f)
                               .withPosition (hoverPosition.x + 12.0f, (float) info->area.getY() + 2.0f);
                if (box.getRight() > (float) getWidth())
                    box.setX (hoverPosition.x - box.getWidth() - 12.0f);

                g.setColour (theme.panelRaised.withAlpha (0.95f));
                g.fillRoundedRectangle (box, 3.0f);
                g.setColour (theme.text);
                g.setFont (juce::FontOptions (11.5f));
                g.drawText (text, box, juce::Justification::centred);
            }
        }
    }
}

//==============================================================================
void AutomationLaneArea::mouseMove (const juce::MouseEvent& e)
{
    const auto info = laneAt (e.getPosition());
    const auto lane = info ? info->laneId : juce::String();
    const auto point = info ? hitAt (*info, e.position).point : -1;
    hoverPosition = e.position;

    if (lane != hoverLane || point != hoverPoint || lane.isNotEmpty())
    {
        hoverLane = lane;
        hoverPoint = point;
        repaint();
    }

    const auto segment = info ? hitAt (*info, e.position).segment : -1;
    setMouseCursor (point >= 0 ? juce::MouseCursor::DraggingHandCursor
                  : e.mods.isCommandDown() && segment >= 0 ? juce::MouseCursor::UpDownResizeCursor
                                                          : juce::MouseCursor::CrosshairCursor);
}

void AutomationLaneArea::mouseExit (const juce::MouseEvent&)
{
    if (drag.kind == Drag::Kind::none)
    {
        hoverLane = {};
        hoverPoint = -1;
        repaint();
    }
}

void AutomationLaneArea::mouseDown (const juce::MouseEvent& e)
{
    const auto info = laneAt (e.getPosition());
    drag = {};

    if (! info)
        return;

    if (host.focusTrack != nullptr)
        host.focusTrack (info->trackId);

    const auto hit = hitAt (*info, e.position);
    const auto tick = std::max<Tick> (0, host.snap (host.xToTick (e.position.x), e.mods));

    if (e.mods.isPopupMenu())
    {
        showMenu (*info, hit, tick);
        return;
    }

    if (e.mods.isCommandDown() && hit.point < 0 && hit.segment >= 0 && ! info->range.holds())
    {
        // Bend the line.
        drag.kind = Drag::Kind::bend;
        drag.laneId = info->laneId;
        drag.index = hit.segment;
        drag.startBend = automation::pointsOf (info->lane)[(size_t) hit.segment].bend;
        drag.downY = e.position.y;
        project.getUndoManager().beginNewTransaction ("Bend automation line");
        return;
    }

    drag.kind = Drag::Kind::point;
    drag.laneId = info->laneId;

    if (hit.point >= 0)
    {
        drag.index = hit.point;
        project.getUndoManager().beginNewTransaction ("Move automation point");
    }
    else
    {
        // A new point, which can be dragged straight away (all one undo step).
        drag.index = edit::addAutomationPoint (project, info->laneId, tick, valueForY (*info, e.position.y));
    }

    hoverLane = info->laneId;
    hoverPoint = drag.index;
    repaint();
}

void AutomationLaneArea::mouseDrag (const juce::MouseEvent& e)
{
    const auto info = laneById (drag.laneId);

    if (drag.kind == Drag::Kind::none || ! info)
        return;

    hoverPosition = e.position;

    if (drag.kind == Drag::Kind::bend)
    {
        // Dragging towards the start's side flattens the beginning, and so on: the line follows the mouse.
        const auto points = automation::pointsOf (info->lane);
        if (! juce::isPositiveAndBelow (drag.index + 1, (int) points.size()))
            return;

        const auto rising = points[(size_t) drag.index + 1].value >= points[(size_t) drag.index].value;
        const auto amount = (e.position.y - drag.downY) / 60.0f * (rising ? 1.0f : -1.0f);
        edit::setAutomationBend (project, drag.laneId, drag.index, drag.startBend + amount, false);
        return;
    }

    const auto tick = std::max<Tick> (0, host.snap (host.xToTick (e.position.x), e.mods));
    const auto y = juce::jlimit (inner (*info).getY(), inner (*info).getBottom(), e.position.y);
    edit::moveAutomationPoint (project, drag.laneId, drag.index, tick, valueForY (*info, y), false);
}

void AutomationLaneArea::mouseUp (const juce::MouseEvent&)
{
    drag = {};
    repaint();
}

void AutomationLaneArea::mouseDoubleClick (const juce::MouseEvent& e)
{
    const auto info = laneAt (e.getPosition());
    if (! info)
        return;

    if (const auto hit = hitAt (*info, e.position); hit.point >= 0)
    {
        edit::removeAutomationPoints (project, info->laneId, { hit.point });
        hoverPoint = -1;
    }
}

void AutomationLaneArea::showMenu (const LaneInfo& info, Hit hit, Tick tick)
{
    auto& proj = project;
    const auto laneId = info.laneId;
    const auto points = automation::pointsOf (info.lane);
    juce::PopupMenu menu;

    if (hit.point >= 0)
    {
        const auto index = hit.point;
        menu.addItem ("Remove this point", [&proj, laneId, index] { edit::removeAutomationPoints (proj, laneId, { index }); });
    }
    else
    {
        const auto value = automation::currentValue (proj, info.trackId, info.target, tick);
        menu.addItem ("Add a point here", [&proj, laneId, tick, value] { edit::addAutomationPoint (proj, laneId, tick, value); });
    }

    if (hit.segment >= 0 && std::abs (points[(size_t) hit.segment].bend) > 1.0e-3f)
    {
        const auto index = hit.segment;
        menu.addItem ("Straighten this line", [&proj, laneId, index] { edit::setAutomationBend (proj, laneId, index, 0.0f); });
    }

    menu.addSeparator();
    menu.addItem ("Clear all points", ! points.empty(), false, [&proj, laneId] { edit::clearAutomationLane (proj, laneId); });
    menu.addItem ("Hide this lane", [&proj, laneId] { edit::hideAutomationLane (proj, laneId); });
    menu.addItem ("Remove the lane and its points", [&proj, laneId] { edit::removeAutomationLane (proj, laneId); });
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition());
}

} // namespace aether::ui
