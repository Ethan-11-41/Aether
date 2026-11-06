// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "TimelineComponent.h"

#include <array>
#include <cmath>
#include <limits>
#include <utility>

#include "io/AudioImport.h"
#include "model/ClipEdits.h"
#include "model/TimelineEdits.h"

namespace aether::ui
{

namespace
{
    constexpr std::array<std::pair<int, int>, 8> commonSignatures { { { 2, 4 }, { 3, 4 }, { 4, 4 }, { 5, 4 },
                                                                      { 6, 8 }, { 7, 8 }, { 9, 8 }, { 12, 8 } } };
    constexpr int loopStripHeight = 13;
    constexpr int clipHeaderHeight = 15;
    constexpr float edgeGrabWidth = 6.0f;
    constexpr float fadeHandleSize = 8.0f;
}

//==============================================================================
/** Loop strip (top) and bar numbers (bottom). */
class TimelineComponent::Ruler final : public juce::Component,
                                       public juce::TooltipClient
{
public:
    explicit Ruler (TimelineComponent& o) : owner (o) {}

    juce::String getTooltip() override
    {
        return getMouseXYRelative().y < loopStripHeight
                   ? "Loop: drag here to choose the part of the song to repeat, drag its edges to change it. "
                     "Double-click (or press L) to switch looping on or off."
                   : "Click or drag here to move the playhead.";
    }

    void paint (juce::Graphics& g) override
    {
        const auto& colours = owner.theme;
        const auto& tempo = owner.tempoMap;
        const auto& viewState = owner.view;
        g.fillAll (colours.panel);

        // Loop strip.
        auto strip = getLocalBounds().removeFromTop (loopStripHeight);
        g.setColour (colours.meterBackground);
        g.fillRect (strip);

        const auto loop = timeline::getLoop (owner.project);
        const auto x1 = (float) viewState.tickToX (loop.start);
        const auto x2 = (float) viewState.tickToX (loop.end);

        if (x2 > 0.0f && x1 < (float) getWidth())
        {
            const juce::Rectangle<float> range (x1, 1.0f, x2 - x1, (float) loopStripHeight - 2.0f);
            g.setColour (colours.accent.withAlpha (loop.enabled ? 0.85f : 0.28f));
            g.fillRoundedRectangle (range, 3.0f);
            g.setColour (colours.accent);
            g.drawRoundedRectangle (range, 3.0f, 1.0f);
        }

        // Bar numbers and lines.
        auto numbers = getLocalBounds().withTrimmedTop (loopStripHeight);
        owner.drawGrid (g, numbers, true);

        const auto pixelsPerBar = viewState.getPixelsPerQuarter() * (double) tempo.barLengthAt (viewState.getViewStart()) / (double) ticksPerQuarter;
        int labelEvery = 1;
        while (pixelsPerBar * labelEvery < 34.0)
            labelEvery *= 2;

        g.setFont (juce::FontOptions (12.0f));
        g.setColour (colours.textDim);

        for (int bar = std::max (0, tempo.ticksToBarBeat (viewState.getViewStart()).bar); bar < 100000; ++bar)
        {
            const auto x = (int) std::floor (viewState.tickToX (tempo.barToTicks (bar)));
            if (x > getWidth())
                break;

            if (bar % labelEvery == 0)
                g.drawText (juce::String (bar + 1), x + 4, numbers.getY() + 2, 60, numbers.getHeight() - 4,
                            juce::Justification::centredLeft, false);
        }

        g.setColour (colours.outline);
        g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        setMouseCursor (e.y < loopStripHeight ? cursorForLoop (e.x) : juce::MouseCursor::NormalCursor);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.y < loopStripHeight)
        {
            const auto loop = timeline::getLoop (owner.project);
            const auto x1 = owner.view.tickToX (loop.start), x2 = owner.view.tickToX (loop.end);
            const auto tick = owner.view.xToTick (e.x);

            if (std::abs (e.x - x1) < 6)       mode = Mode::resizeStart;
            else if (std::abs (e.x - x2) < 6)  mode = Mode::resizeEnd;
            else if (e.x > x1 && e.x < x2)     mode = Mode::move;
            else                               mode = Mode::create;

            dragStartLoop = loop;
            anchor = owner.snap (tick, e.mods);
            grabOffset = tick - loop.start;
            owner.project.getUndoManager().beginNewTransaction ("Set loop range");
        }
        else
        {
            mode = Mode::scrub;
            owner.engine.locate (owner.snap (owner.view.xToTick (e.x), e.mods));
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        auto& proj = owner.project;
        const auto tick = owner.view.xToTick (e.x);
        const auto snapped = owner.snap (tick, e.mods);
        const auto minLength = ticksPerQuarter / 4;

        switch (mode)
        {
            case Mode::scrub:
                owner.engine.locate (std::max<Tick> (0, snapped));
                break;

            case Mode::create:
                if (std::abs (snapped - anchor) >= minLength)
                {
                    edit::setLoopRange (proj, std::min (anchor, snapped), std::max (anchor, snapped), false);
                    edit::setLoopEnabled (proj, true, false);   // drawing a new loop switches looping on
                }
                break;

            case Mode::move:
            {
                const auto length = dragStartLoop.end - dragStartLoop.start;
                const auto start = std::max<Tick> (0, owner.snap (tick - grabOffset, e.mods));
                edit::setLoopRange (proj, start, start + length, false);
                break;
            }

            case Mode::resizeStart:
                edit::setLoopRange (proj, std::min (snapped, dragStartLoop.end - minLength), dragStartLoop.end, false);
                break;

            case Mode::resizeEnd:
                edit::setLoopRange (proj, dragStartLoop.start, std::max (snapped, dragStartLoop.start + minLength), false);
                break;

            case Mode::none:
                break;
        }
    }

    void mouseUp (const juce::MouseEvent&) override   { mode = Mode::none; }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (e.y < loopStripHeight)
            edit::setLoopEnabled (owner.project, ! timeline::getLoop (owner.project).enabled);
    }

private:
    enum class Mode { none, scrub, create, move, resizeStart, resizeEnd };

    juce::MouseCursor cursorForLoop (int x) const
    {
        const auto loop = timeline::getLoop (owner.project);
        const auto x1 = owner.view.tickToX (loop.start), x2 = owner.view.tickToX (loop.end);

        if (std::abs (x - x1) < 6 || std::abs (x - x2) < 6)
            return juce::MouseCursor::LeftRightResizeCursor;

        return (x > x1 && x < x2) ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor;
    }

    TimelineComponent& owner;
    Mode mode = Mode::none;
    LoopRange dragStartLoop;
    Tick anchor = 0, grabOffset = 0;
};

//==============================================================================
/** Time signature markers. */
class TimelineComponent::SignatureLane final : public juce::Component,
                                               public juce::SettableTooltipClient
{
public:
    explicit SignatureLane (TimelineComponent& o) : owner (o)
    {
        setTooltip ("Time signatures. Click one to change it, right-click to add a change at a bar.");
    }

    void paint (juce::Graphics& g) override
    {
        const auto& colours = owner.theme;
        g.fillAll (colours.panel.darker (0.1f));
        owner.drawGrid (g, getLocalBounds(), false);

        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));

        for (const auto& sig : owner.tempoMap.getTimeSignatures())
        {
            const auto x = (float) owner.view.tickToX (owner.tempoMap.barToTicks (sig.bar));
            if (x > (float) getWidth())
                break;

            const juce::Rectangle<float> pill (x + 2.0f, 3.0f, 38.0f, (float) getHeight() - 6.0f);
            g.setColour (colours.panelRaised);
            g.fillRoundedRectangle (pill, 4.0f);
            g.setColour (colours.text);
            g.drawText (juce::String (sig.numerator) + "/" + juce::String (sig.denominator), pill, juce::Justification::centred);
        }

        g.setColour (colours.outline);
        g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto tick = owner.view.xToTick (e.x);
        const auto bar = std::max (0, owner.tempoMap.ticksToBarBeat (tick).bar);

        // Clicked on a marker?
        const TimeSignature* clicked = nullptr;
        for (const auto& sig : owner.tempoMap.getTimeSignatures())
        {
            const auto x = owner.view.tickToX (owner.tempoMap.barToTicks (sig.bar));
            if (e.x >= x && e.x <= x + 42)
                clicked = &sig;
        }

        juce::PopupMenu menu;
        auto& proj = owner.project;

        if (clicked != nullptr)
        {
            const auto sigBar = clicked->bar;
            menu.addSectionHeader (sigBar == 0 ? "Time signature of the song" : "Time signature from bar " + juce::String (sigBar + 1));

            for (const auto& signature : commonSignatures)
            {
                const auto n = signature.first, d = signature.second;
                menu.addItem (juce::String (n) + "/" + juce::String (d), true, clicked->numerator == n && clicked->denominator == d,
                              [&proj, sigBar, n, d] { edit::setTimeSignature (proj, sigBar, n, d); });
            }

            if (sigBar > 0)
            {
                menu.addSeparator();
                menu.addItem ("Remove this change", [&proj, sigBar] { edit::deleteTimeSignature (proj, sigBar); });
            }
        }
        else if (e.mods.isPopupMenu())
        {
            menu.addSectionHeader ("Change the time signature from bar " + juce::String (bar + 1));

            for (const auto& signature : commonSignatures)
            {
                const auto n = signature.first, d = signature.second;
                menu.addItem (juce::String (n) + "/" + juce::String (d),
                              [&proj, bar, n, d] { edit::setTimeSignature (proj, bar, n, d); });
            }
        }

        if (menu.getNumItems() > 0)
            menu.showMenuAsync (juce::PopupMenu::Options().withMousePosition());
    }

private:
    TimelineComponent& owner;
};

//==============================================================================
/** The tempo graph: points you can add, drag and change. */
class TimelineComponent::TempoLane final : public juce::Component,
                                           public juce::SettableTooltipClient
{
public:
    explicit TempoLane (TimelineComponent& o) : owner (o)
    {
        setTooltip ("Tempo. Double-click to add a tempo change, drag a point to change it, "
                    "right-click a point for options (gradual change, delete).");
    }

    void paint (juce::Graphics& g) override
    {
        const auto& colours = owner.theme;
        g.fillAll (colours.panel.darker (0.2f));
        owner.drawGrid (g, getLocalBounds(), false);

        if (! dragging)
            range = computeRange();

        const auto& points = owner.tempoMap.getTempoPoints();

        // The tempo line.
        juce::Path line;
        for (size_t i = 0; i < points.size(); ++i)
        {
            const auto x = (float) owner.view.tickToX (points[i].tick);
            const auto y = bpmToY (points[i].bpm);

            if (i == 0) line.startNewSubPath (std::min (x, 0.0f), y);
            else        line.lineTo (x, line.getCurrentPosition().y);   // jumps are vertical

            line.lineTo (x, y);

            const auto endX = i + 1 < points.size() ? (float) owner.view.tickToX (points[i + 1].tick) : (float) getWidth() + 10.0f;
            const auto endY = (i + 1 < points.size() && points[i].curve == TempoCurve::ramp) ? bpmToY (points[i + 1].bpm) : y;
            line.lineTo (endX, endY);
        }

        g.setColour (colours.meterMid);
        g.strokePath (line, juce::PathStrokeType (2.0f));

        // The points, with their tempo.
        g.setFont (juce::FontOptions (11.0f));

        for (size_t i = 0; i < points.size(); ++i)
        {
            const auto x = (float) owner.view.tickToX (points[i].tick);
            if (x < -20.0f || x > (float) getWidth() + 20.0f)
                continue;

            const auto y = bpmToY (points[i].bpm);
            const auto active = (int) i == hoverIndex || (int) i == dragIndex;
            const auto radius = active ? 5.5f : 4.0f;

            g.setColour (colours.meterMid);
            g.fillEllipse (x - radius, y - radius, radius * 2.0f, radius * 2.0f);
            g.setColour (colours.text);
            g.drawText (juce::String (points[i].bpm, points[i].bpm == std::floor (points[i].bpm) ? 0 : 1)
                            + (points[i].curve == TempoCurve::ramp ? juce::String::fromUTF8 (" \xe2\x86\x97") : juce::String()),
                        (int) x + 6, (int) y - (y > (float) getHeight() * 0.5f ? 16 : -2), 70, 14, juce::Justification::left, false);
        }

        g.setColour (colours.outline);
        g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const auto index = pointAt (e.position);
        if (index != hoverIndex)
        {
            hoverIndex = index;
            setMouseCursor (index >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
            repaint();
        }
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        hoverIndex = -1;
        repaint();
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto index = pointAt (e.position);

        if (e.mods.isPopupMenu())
        {
            showMenu (index, e);
            return;
        }

        if (index >= 0)
        {
            dragIndex = index;
            dragging = true;
            range = computeRange();
            owner.project.getUndoManager().beginNewTransaction ("Change tempo");
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! dragging)
            return;

        auto& proj = owner.project;
        const auto step = e.mods.isShiftDown() ? 0.1 : 1.0;
        const auto bpm = std::round (yToBpm ((float) e.y) / step) * step;
        edit::setTempo (proj, dragIndex, bpm, false);

        if (dragIndex > 0)
            edit::moveTempoChange (proj, dragIndex, owner.snap (owner.view.xToTick (e.x), e.mods), false);
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        dragging = false;
        dragIndex = -1;
        repaint();
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const auto index = pointAt (e.position);

        if (index >= 0)
        {
            askForTempo (index);
            return;
        }

        const auto tick = std::max<Tick> (ticksPerQuarter, owner.snap (owner.view.xToTick (e.x), e.mods));
        edit::addTempoChange (owner.project, tick, std::round (yToBpm ((float) e.y)));
    }

private:
    struct Range { double low = 60.0, high = 180.0; };

    Range computeRange() const
    {
        double lo = 1.0e9, hi = 0.0;
        for (const auto& p : owner.tempoMap.getTempoPoints())
        {
            lo = std::min (lo, p.bpm);
            hi = std::max (hi, p.bpm);
        }

        // Room above and below, at least 40 BPM tall, in round numbers.
        lo = std::floor ((lo - 20.0) / 10.0) * 10.0;
        hi = std::ceil ((hi + 20.0) / 10.0) * 10.0;
        if (hi - lo < 40.0) { lo -= 10.0; hi += 10.0; }
        return { std::max (TempoMap::minBpm, lo), std::min (TempoMap::maxBpm, hi) };
    }

    float bpmToY (double bpm) const
    {
        const auto top = 8.0f, bottom = (float) getHeight() - 8.0f;
        const auto fraction = (float) ((bpm - range.low) / (range.high - range.low));
        return bottom - fraction * (bottom - top);
    }

    double yToBpm (float y) const
    {
        const auto top = 8.0f, bottom = (float) getHeight() - 8.0f;
        const auto fraction = (bottom - y) / (bottom - top);
        return juce::jlimit (TempoMap::minBpm, TempoMap::maxBpm, range.low + fraction * (range.high - range.low));
    }

    int pointAt (juce::Point<float> position) const
    {
        const auto& points = owner.tempoMap.getTempoPoints();

        for (int i = (int) points.size(); --i >= 0;)
        {
            const juce::Point<float> p ((float) owner.view.tickToX (points[(size_t) i].tick), bpmToY (points[(size_t) i].bpm));
            if (p.getDistanceFrom (position) <= 8.0f)
                return i;
        }

        return -1;
    }

    void showMenu (int index, const juce::MouseEvent& e)
    {
        auto& proj = owner.project;
        juce::PopupMenu menu;

        if (index >= 0)
        {
            const auto& points = owner.tempoMap.getTempoPoints();
            const auto& point = points[(size_t) index];
            const auto isLast = index == (int) points.size() - 1;

            menu.addSectionHeader ("Tempo " + juce::String (point.bpm, 1) + " BPM");
            menu.addItem ("Set tempo...", [this, index] { askForTempo (index); });
            menu.addSeparator();
            menu.addItem ("Change suddenly at the next point", ! isLast, point.curve == TempoCurve::jump,
                          [&proj, index] { edit::setTempoCurve (proj, index, TempoCurve::jump); });
            menu.addItem ("Change gradually to the next point", ! isLast, point.curve == TempoCurve::ramp,
                          [&proj, index] { edit::setTempoCurve (proj, index, TempoCurve::ramp); });

            if (index > 0)
            {
                menu.addSeparator();
                menu.addItem ("Delete this tempo change", [&proj, index] { edit::deleteTempoChange (proj, index); });
            }
        }
        else
        {
            const auto tick = std::max<Tick> (ticksPerQuarter, owner.snap (owner.view.xToTick (e.x), e.mods));
            const auto bpm = owner.tempoMap.tempoAt (tick);
            menu.addItem ("Add a tempo change here", [&proj, tick, bpm] { edit::addTempoChange (proj, tick, bpm); });
        }

        menu.showMenuAsync (juce::PopupMenu::Options().withMousePosition());
    }

    void askForTempo (int index)
    {
        const auto& points = owner.tempoMap.getTempoPoints();
        if (! juce::isPositiveAndBelow (index, (int) points.size()))
            return;

        auto* window = new juce::AlertWindow ("Set tempo", "Beats per minute (" + juce::String ((int) TempoMap::minBpm)
                                              + " to " + juce::String ((int) TempoMap::maxBpm) + "):",
                                              juce::MessageBoxIconType::NoIcon, this);
        window->addTextEditor ("bpm", juce::String (points[(size_t) index].bpm, 2).trimCharactersAtEnd ("0").trimCharactersAtEnd ("."));
        window->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
        window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

        juce::Component::SafePointer<TempoLane> safeThis (this);
        window->enterModalState (true, juce::ModalCallbackFunction::create ([safeThis, window, index] (int result)
        {
            if (safeThis != nullptr && result == 1)
            {
                const auto text = window->getTextEditorContents ("bpm").replaceCharacter (',', '.');
                if (text.containsAnyOf ("0123456789"))
                    edit::setTempo (safeThis->owner.project, index, text.getDoubleValue());
            }
        }), true);
    }

    TimelineComponent& owner;
    Range range;
    int hoverIndex = -1, dragIndex = -1;
    bool dragging = false;
};

//==============================================================================
/** One lane per track, lined up with the track headers, holding the clips. */
class TimelineComponent::TrackLanes final : public juce::Component,
                                            public juce::TooltipClient
{
public:
    explicit TrackLanes (TimelineComponent& o) : owner (o) {}

    juce::String getTooltip() override
    {
        const auto hit = findHit (getMouseXYRelative().toFloat());

        switch (hit.zone)
        {
            case Zone::trimStart:
            case Zone::trimEnd:  return "Drag to trim the clip (hold Alt for exact positions).";
            case Zone::fadeIn:   return "Drag to fade the clip in.";
            case Zone::fadeOut:  return "Drag to fade the clip out.";
            case Zone::body:     return "Drag to move the clip, also onto another track. Ctrl+click to select several. "
                                        "Right-click for more (split, loop, volume, fades).";
            case Zone::none:     break;
        }

        return "Drop audio files here to import them, or right-click to import. Click to move the playhead.";
    }

    void paint (juce::Graphics& g) override
    {
        const auto& colours = owner.theme;
        g.fillAll (colours.meterBackground);

        const auto rowHeight = TrackListComponent::rowHeight;
        const auto scrollY = owner.trackViewport.getViewPositionY();
        const auto& proj = owner.project;
        for (int i = 0; i < proj.getNumTracks(); ++i)
        {
            const auto y = i * rowHeight - scrollY;
            if (y > getHeight() || y + rowHeight < 0)
                continue;

            const auto track = proj.getTrack (i);
            juce::Rectangle<int> row (0, y, getWidth(), rowHeight);
            g.setColour (owner.trackList.isTrackSelected (track.getId()) ? colours.panelRaised.withAlpha (0.55f) : colours.panel.withAlpha (0.35f));
            g.fillRect (row);

            if (track.isBus())
            {
                // Buses have no clips: say so, faintly.
                g.setColour (colours.textDim.withAlpha (0.45f));
                g.setFont (juce::FontOptions (12.0f));
                g.drawText ("Bus: other tracks play into it (choose its tracks' Output in the mixer or by right-clicking them)",
                            row.reduced (12, 0), juce::Justification::centredLeft, true);
            }
        }

        owner.drawGrid (g, getLocalBounds(), false);

        g.setColour (colours.outline.withAlpha (0.6f));
        for (int i = 0; i <= proj.getNumTracks(); ++i)
            g.drawHorizontalLine (i * rowHeight - scrollY - 1, 0.0f, (float) getWidth());

        for (const auto& box : layoutClips())
            drawClip (g, box);

        // Where dropped files would go.
        if (owner.fileDragActive)
        {
            const auto x = (float) owner.view.tickToX (owner.dropTick);
            const auto y = (float) (std::min (owner.dropRow, proj.getNumTracks()) * rowHeight - scrollY);
            g.setColour (colours.accent.withAlpha (0.25f));
            g.fillRect (juce::Rectangle<float> (x, y, (float) getWidth() - x, (float) rowHeight));
            g.setColour (colours.accent);
            g.fillRect (juce::Rectangle<float> (x - 1.0f, y, 2.0f, (float) rowHeight));
        }
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const auto hit = findHit (e.position);

        switch (hit.zone)
        {
            case Zone::trimStart:
            case Zone::trimEnd:  setMouseCursor (juce::MouseCursor::LeftRightResizeCursor); break;
            case Zone::fadeIn:
            case Zone::fadeOut:  setMouseCursor (juce::MouseCursor::CrosshairCursor); break;
            case Zone::body:     setMouseCursor (juce::MouseCursor::DraggingHandCursor); break;
            case Zone::none:     setMouseCursor (juce::MouseCursor::NormalCursor); break;
        }

        // The fade handles only show on the clip under the mouse.
        setHoveredClip (hit.zone != Zone::none ? hit.clip.getId() : juce::String());
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        if (drag.zone == Zone::none)
            setHoveredClip ({});
    }

    void setHoveredClip (const juce::String& clipId)
    {
        if (hoveredClipId != clipId)
        {
            hoveredClipId = clipId;
            repaint();
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto hit = findHit (e.position);
        const auto row = rowAt (e.y);
        owner.trackList.grabKeyboardFocus();
        drag = {};

        if (juce::isPositiveAndBelow (row, owner.project.getNumTracks()))
            owner.trackList.focusTrack (owner.project.getTrack (row).getId());

        if (e.mods.isPopupMenu())
        {
            if (hit.zone != Zone::none)
            {
                if (! owner.selectedClips.contains (hit.clip.getId()))
                    owner.setSelection ({ hit.clip.getId() });
                showClipMenu (hit.clip);
            }
            else
            {
                showLaneMenu (row, owner.snap (owner.view.xToTick (e.x), e.mods));
            }
            return;
        }

        if (hit.zone == Zone::none)
        {
            owner.clearClipSelection();
            owner.engine.locate (std::max<Tick> (0, owner.snap (owner.view.xToTick (e.x), e.mods)));
            return;
        }

        const auto id = hit.clip.getId();

        if (e.mods.isCommandDown())
        {
            // Ctrl+click adds or removes a clip from the selection (no dragging).
            auto selection = owner.selectedClips;
            if (selection.contains (id)) selection.removeString (id);
            else                         selection.add (id);
            owner.setSelection (selection);
            return;
        }

        if (! owner.selectedClips.contains (id))
            owner.setSelection ({ id });

        // Remember where everything was, so the drag can be applied from there each time.
        drag.zone = hit.zone;
        drag.clipId = id;
        drag.downTick = owner.view.xToTick (e.x);
        drag.downRow = row;
        drag.anchorStart = hit.clip.getStart();

        for (const auto& selectedId : owner.selectedClips)
            if (auto clip = clips::findClip (owner.project, selectedId); clip.isValid())
                drag.originals.push_back ({ selectedId, clip.getStart(), owner.project.indexOfTrack (clip.getTrackState()[ids::id].toString()) });

        const char* names[] = { "", "Move clip", "Trim clip", "Trim clip", "Change fade", "Change fade" };
        owner.project.getUndoManager().beginNewTransaction (names[(int) hit.zone]);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (drag.zone == Zone::none)
            return;

        auto& proj = owner.project;
        const auto& tempo = owner.tempoMap;
        const auto tick = owner.view.xToTick (e.x);
        auto clip = clips::findClip (proj, drag.clipId);

        if (! clip.isValid())
            return;

        switch (drag.zone)
        {
            case Zone::body:
            {
                if (! drag.moved && e.getDistanceFromDragStart() < 4)
                    return;

                drag.moved = true;
                const auto newAnchor = std::max<Tick> (0, owner.snap (drag.anchorStart + (tick - drag.downTick), e.mods));
                const auto delta = newAnchor - drag.anchorStart;
                const auto rowDelta = rowAt (e.y) - drag.downRow;

                for (const auto& original : drag.originals)
                {
                    auto targetIndex = original.trackIndex + rowDelta;
                    if (! juce::isPositiveAndBelow (targetIndex, proj.getNumTracks())
                        || proj.getTrack (targetIndex).getKind() != TrackKind::audio)
                        targetIndex = original.trackIndex;

                    edit::setClipPosition (proj, original.clipId, std::max<Tick> (0, original.start + delta),
                                           proj.getTrack (targetIndex).getId(), false);
                }
                break;
            }

            case Zone::trimStart:
                edit::trimClipStart (proj, drag.clipId, owner.snap (tick, e.mods), false);
                break;

            case Zone::trimEnd:
                edit::trimClipEnd (proj, drag.clipId, owner.snap (tick, e.mods), false);
                break;

            case Zone::fadeIn:
            {
                const auto length = (tempo.ticksToSeconds (tick) - clip.getStartSeconds (tempo)) * clip.getSourceRate();
                edit::setClipFades (proj, drag.clipId, (juce::int64) std::max (0.0, length), clip.getFadeOut(), false);
                break;
            }

            case Zone::fadeOut:
            {
                const auto endSeconds = clip.getStartSeconds (tempo) + clip.getDurationSeconds();
                const auto length = (endSeconds - tempo.ticksToSeconds (tick)) * clip.getSourceRate();
                edit::setClipFades (proj, drag.clipId, clip.getFadeIn(), (juce::int64) std::max (0.0, length), false);
                break;
            }

            case Zone::none:
                break;
        }
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        drag = {};
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const auto hit = findHit (e.position);

        if (hit.zone == Zone::body)
            askForClipName (hit.clip.getId());
    }

private:
    enum class Zone { none, body, trimStart, trimEnd, fadeIn, fadeOut };

    struct ClipBox
    {
        Clip clip;
        juce::Rectangle<float> bounds;
        juce::Colour colour;
        bool selected = false;
    };

    struct Hit
    {
        Clip clip;
        Zone zone = Zone::none;
    };

    struct Original
    {
        juce::String clipId;
        Tick start = 0;
        int trackIndex = 0;
    };

    struct DragState
    {
        Zone zone = Zone::none;
        juce::String clipId;
        Tick downTick = 0, anchorStart = 0;
        int downRow = 0;
        bool moved = false;
        std::vector<Original> originals;
    };

    juce::String hoveredClipId;

    int rowAt (int y) const
    {
        const auto yInList = y + owner.trackViewport.getViewPositionY();
        return yInList < 0 ? -1 : yInList / TrackListComponent::rowHeight;
    }

    float xForClipPosition (const Clip& clip, double samplesIn) const
    {
        const auto seconds = clip.getStartSeconds (owner.tempoMap) + samplesIn / clip.getSourceRate();
        return (float) owner.view.tickToX (owner.tempoMap.secondsToTicks (seconds));
    }

    std::vector<ClipBox> layoutClips() const
    {
        std::vector<ClipBox> boxes;
        const auto& proj = owner.project;
        const auto rowHeight = TrackListComponent::rowHeight;
        const auto scrollY = owner.trackViewport.getViewPositionY();

        for (int i = 0; i < proj.getNumTracks(); ++i)
        {
            const auto y = i * rowHeight - scrollY;
            if (y > getHeight() || y + rowHeight < 0)
                continue;

            const auto track = proj.getTrack (i);

            for (const auto& clip : clips::clipsOnTrack (track.getState()))
            {
                const auto x1 = (float) owner.view.tickToX (clip.getStart());
                const auto x2 = (float) owner.view.tickToX (clip.getEnd (owner.tempoMap));

                if (x2 < -2.0f || x1 > (float) getWidth() + 2.0f)
                    continue;

                boxes.push_back ({ clip, juce::Rectangle<float> (x1, (float) y + 3.0f, std::max (3.0f, x2 - x1), (float) rowHeight - 7.0f),
                                   track.getColour(), owner.selectedClips.contains (clip.getId()) });
            }
        }

        return boxes;
    }

    Hit findHit (juce::Point<float> p) const
    {
        const auto boxes = layoutClips();

        // The last-drawn clip is on top.
        for (auto it = boxes.rbegin(); it != boxes.rend(); ++it)
        {
            const auto& r = it->bounds;
            if (! r.expanded (2.0f, 0.0f).contains (p))
                continue;

            const auto& clip = it->clip;
            const auto bodyTop = r.getY() + (float) clipHeaderHeight;
            const auto fadeInX = xForClipPosition (clip, (double) clip.getFadeIn());
            const auto fadeOutX = xForClipPosition (clip, (double) (clip.getDuration() - clip.getFadeOut()));
            const auto nearTop = p.y >= bodyTop && p.y <= bodyTop + fadeHandleSize + 4.0f;

            if (nearTop && std::abs (p.x - fadeInX) <= fadeHandleSize && r.getWidth() > 30.0f)
                return { clip, Zone::fadeIn };
            if (nearTop && std::abs (p.x - fadeOutX) <= fadeHandleSize && r.getWidth() > 30.0f)
                return { clip, Zone::fadeOut };
            if (p.x - r.getX() <= edgeGrabWidth)
                return { clip, Zone::trimStart };
            if (r.getRight() - p.x <= edgeGrabWidth)
                return { clip, Zone::trimEnd };

            return { clip, Zone::body };
        }

        return {};
    }

    void drawClip (juce::Graphics& g, const ClipBox& box) const
    {
        const auto& colours = owner.theme;
        const auto& clip = box.clip;
        auto r = box.bounds;

        g.setColour (box.colour.withAlpha (box.selected ? 0.5f : 0.3f));
        g.fillRoundedRectangle (r, 4.0f);

        // Title strip with the clip's name (and its volume if not 0 dB).
        auto header = r.removeFromTop ((float) clipHeaderHeight);
        g.setColour (box.colour.withAlpha (box.selected ? 0.95f : 0.7f));
        g.fillRoundedRectangle (header.withHeight (header.getHeight() + 4.0f), 4.0f);
        g.setColour (colours.accentText);
        g.setFont (juce::FontOptions (11.5f, juce::Font::bold));

        juce::String title = clip.getName();
        if (clip.isLooping())          title << juce::String::fromUTF8 ("  \xe2\x9f\xb3");   // ⟳
        if (clip.getGainDb() != 0.0)   title << "  " << (clip.getGainDb() > 0 ? "+" : "") << juce::String (clip.getGainDb(), 1) << " dB";
        g.drawText (title, header.reduced (5.0f, 0.0f), juce::Justification::centredLeft, true);

        drawWaveform (g, box, r);
        drawFades (g, clip, r);

        // Where a looping clip repeats.
        if (clip.isLooping())
        {
            g.setColour (colours.text.withAlpha (0.35f));
            for (auto pos = clip.getContentLength() - clip.getPhase(); pos < clip.getDuration(); pos += clip.getContentLength())
            {
                const auto x = xForClipPosition (clip, (double) pos);
                for (float y = r.getY(); y < r.getBottom(); y += 6.0f)
                    g.drawLine (x, y, x, std::min (y + 3.0f, r.getBottom()), 1.0f);
            }
        }

        g.setColour (box.selected ? colours.text : box.colour.darker (0.3f));
        g.drawRoundedRectangle (box.bounds, 4.0f, box.selected ? 1.8f : 1.0f);
    }

    void drawWaveform (juce::Graphics& g, const ClipBox& box, juce::Rectangle<float> area) const
    {
        const auto& clip = box.clip;
        const auto file = owner.resolve (clip.getFile());
        const auto peaks = owner.audioFiles.getPeaks (file);

        if (peaks == nullptr)
        {
            const auto loadState = owner.audioFiles.getState (file);
            g.setColour (owner.theme.textDim);
            g.setFont (juce::FontOptions (11.0f));
            g.drawText (loadState == AudioFileCache::State::failed ? "Missing or unreadable file" : "Loading...",
                        area.reduced (6.0f, 2.0f), juce::Justification::centredLeft, true);
            return;
        }

        const auto& tempo = owner.tempoMap;
        const auto rate = clip.getSourceRate();
        const auto startSeconds = clip.getStartSeconds (tempo);
        const auto gain = std::pow (10.0f, (float) clip.getGainDb() / 20.0f);
        const auto centre = area.getCentreY();
        const auto halfHeight = area.getHeight() * 0.5f - 1.0f;

        const auto left = std::max (0, (int) std::floor (area.getX()));
        const auto right = std::min (getWidth(), (int) std::ceil (area.getRight()));

        g.setColour (box.colour.brighter (0.5f).withAlpha (0.9f));

        for (int x = left; x < right; ++x)
        {
            const auto from = (tempo.ticksToSeconds (owner.view.xToTick (x)) - startSeconds) * rate;
            const auto to = (tempo.ticksToSeconds (owner.view.xToTick (x + 1)) - startSeconds) * rate;

            if (to <= 0.0 || from >= (double) clip.getDuration())
                continue;

            const auto pos = (juce::int64) std::max (0.0, from);
            const auto width = std::max<juce::int64> (1, (juce::int64) (to - std::max (0.0, from)));
            const auto source = clip.isLooping() ? clip.getOffset() + (clip.getPhase() + pos) % clip.getContentLength()
                                                 : clip.getOffset() + pos;

            float lo = 0.0f, hi = 0.0f;
            for (int ch = 0; ch < peaks->getNumChannels(); ++ch)
            {
                const auto [cLo, cHi] = peaks->range (ch, source, source + width);
                lo = std::min (lo, cLo);
                hi = std::max (hi, cHi);
            }

            const auto top = centre - std::min (1.0f, hi * gain) * halfHeight;
            const auto bottom = centre - std::max (-1.0f, lo * gain) * halfHeight;
            g.fillRect ((float) x, top, 1.0f, std::max (1.0f, bottom - top));
        }
    }

    void drawFades (juce::Graphics& g, const Clip& clip, juce::Rectangle<float> area) const
    {
        const auto& colours = owner.theme;

        auto drawFade = [&] (bool isFadeIn)
        {
            const auto length = isFadeIn ? clip.getFadeIn() : clip.getFadeOut();
            const auto curve = isFadeIn ? clip.getFadeInCurve() : clip.getFadeOutCurve();
            const auto from = isFadeIn ? 0.0 : (double) (clip.getDuration() - length);

            // The handle (a small dot where the fade ends or starts), only while the mouse is
            // over the clip or dragging its fade.
            if (clip.getId() == hoveredClipId || (drag.clipId == clip.getId() && (drag.zone == Zone::fadeIn || drag.zone == Zone::fadeOut)))
            {
                const auto handleX = xForClipPosition (clip, isFadeIn ? (double) length : from);
                const auto dot = juce::Rectangle<float> (7.0f, 7.0f)
                                     .withCentre ({ juce::jlimit (area.getX() + 5.0f, area.getRight() - 5.0f, handleX), area.getY() + 5.0f });
                g.setColour (juce::Colours::black.withAlpha (0.45f));
                g.fillEllipse (dot.expanded (1.5f));
                g.setColour (colours.text);
                g.fillEllipse (dot);
            }

            if (length <= 0)
                return;

            // Darken the part that's quieter, above the fade curve.
            juce::Path shade;
            constexpr int steps = 24;
            shade.startNewSubPath (xForClipPosition (clip, from), area.getY());

            for (int i = 0; i <= steps; ++i)
            {
                const auto t = (double) i / steps;
                const auto level = fadeShape (curve, isFadeIn ? t : 1.0 - t);
                shade.lineTo (xForClipPosition (clip, from + t * (double) length), area.getBottom() - level * area.getHeight());
            }

            shade.lineTo (xForClipPosition (clip, from + (double) length), area.getY());
            shade.closeSubPath();
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.fillPath (shade);
        };

        drawFade (true);
        drawFade (false);
    }

    void showClipMenu (const Clip& clip)
    {
        auto& proj = owner.project;
        const auto id = clip.getId();
        juce::Component::SafePointer<TimelineComponent> safeOwner (&owner);
        juce::PopupMenu menu;

        // What the file is, e.g. "Guitar.wav: 44.1 kHz, stereo, played at 48 kHz".
        {
            const auto rateText = [] (double rate)
            {
                return juce::String (rate / 1000.0, std::fmod (rate, 1000.0) == 0.0 ? 0 : 1) + " kHz";
            };

            juce::String info = juce::File::createFileWithoutCheckingPath (clip.getFile()).getFileName() + ": "
                              + rateText (clip.getSourceRate());

            if (const auto audio = owner.audioFiles.getAudio (owner.resolve (clip.getFile())))
            {
                info << (audio->samples.getNumChannels() > 1 ? ", stereo" : ", mono");

                if (audio->sampleRate != clip.getSourceRate())
                    info << " (converted to " << rateText (audio->sampleRate) << " for your sound card)";
            }

            menu.addSectionHeader (info);
        }

        menu.addItem ("Split at the playhead (S)", [safeOwner] { if (safeOwner) safeOwner->splitClipsAtPlayhead(); });
        menu.addItem ("Duplicate (Ctrl+D)", [safeOwner] { if (safeOwner) safeOwner->duplicateSelectedClips(); });
        menu.addItem ("Delete (Delete)", [safeOwner] { if (safeOwner) safeOwner->deleteSelectedClips(); });
        menu.addSeparator();
        menu.addItem ("Loop this clip", true, clip.isLooping(), [&proj, id, loop = ! clip.isLooping()] { edit::setClipLooping (proj, id, loop); });

        juce::PopupMenu volume;
        for (const auto& step : { std::pair { "Louder (+1.5 dB)", 1.5 }, std::pair { "Quieter (-1.5 dB)", -1.5 },
                                  std::pair { "Quieter (-6 dB)", -6.0 } })
            volume.addItem (step.first, [&proj, id, change = step.second, current = clip.getGainDb()] { edit::setClipGain (proj, id, current + change); });
        volume.addItem ("Back to 0 dB", clip.getGainDb() != 0.0, false, [&proj, id] { edit::setClipGain (proj, id, 0.0); });
        menu.addSubMenu ("Volume (now " + juce::String (clip.getGainDb(), 1) + " dB)", volume);

        for (const auto isFadeIn : { true, false })
        {
            juce::PopupMenu shapes;
            const auto current = isFadeIn ? clip.getFadeInCurve() : clip.getFadeOutCurve();

            for (const auto& shape : { std::pair { "Smooth", FadeCurve::smooth }, std::pair { "Straight line", FadeCurve::linear },
                                       std::pair { "Fast", FadeCurve::fast }, std::pair { "Slow", FadeCurve::slow } })
                shapes.addItem (shape.first, true, current == shape.second,
                                [&proj, id, isFadeIn, curve = shape.second] { edit::setClipFadeCurve (proj, id, isFadeIn, curve); });

            menu.addSubMenu (isFadeIn ? "Fade-in shape" : "Fade-out shape", shapes);
        }

        menu.addSeparator();
        menu.addItem ("Rename...", [this, id] { askForClipName (id); });
        menu.addItem ("Show the audio file in its folder", [file = owner.resolve (clip.getFile())] { file.revealToUser(); });

        menu.showMenuAsync (juce::PopupMenu::Options().withMousePosition());
    }

    void showLaneMenu (int row, Tick position)
    {
        const auto& proj = owner.project;
        const auto trackId = juce::isPositiveAndBelow (row, proj.getNumTracks()) ? proj.getTrack (row).getId() : juce::String();
        juce::Component::SafePointer<TimelineComponent> safeOwner (&owner);
        juce::PopupMenu menu;

        menu.addItem ("Import audio file here...", [safeOwner, trackId, position]
        {
            if (safeOwner != nullptr && safeOwner->onImportRequested)
                safeOwner->onImportRequested (trackId, position);
        });

        menu.showMenuAsync (juce::PopupMenu::Options().withMousePosition());
    }

    void askForClipName (const juce::String& clipId)
    {
        const auto clip = clips::findClip (owner.project, clipId);
        if (! clip.isValid())
            return;

        auto* window = new juce::AlertWindow ("Rename clip", {}, juce::MessageBoxIconType::NoIcon, this);
        window->addTextEditor ("name", clip.getName());
        window->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
        window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

        juce::Component::SafePointer<TrackLanes> safeThis (this);
        window->enterModalState (true, juce::ModalCallbackFunction::create ([safeThis, window, clipId] (int result)
        {
            if (safeThis != nullptr && result == 1)
                edit::renameClip (safeThis->owner.project, clipId, window->getTextEditorContents ("name"));
        }), true);
    }

    TimelineComponent& owner;
    DragState drag;
};

//==============================================================================
TimelineComponent::TimelineComponent (const Theme& themeToUse, Project& projectToShow, AudioEngine& engineToUse,
                                      AudioFileCache& cache, std::function<ProjectLocation()> projectLocation,
                                      juce::Viewport& tracksViewport, TrackListComponent& tracks)
    : theme (themeToUse), project (projectToShow), engine (engineToUse), audioFiles (cache),
      location (std::move (projectLocation)), trackViewport (tracksViewport), trackList (tracks),
      state (projectToShow.getState())
{
    ruler = std::make_unique<Ruler> (*this);
    signatureLane = std::make_unique<SignatureLane> (*this);
    tempoLane = std::make_unique<TempoLane> (*this);
    trackLanes = std::make_unique<TrackLanes> (*this);

    for (juce::Component* c : { (juce::Component*) ruler.get(), (juce::Component*) signatureLane.get(),
                                (juce::Component*) tempoLane.get(), (juce::Component*) trackLanes.get() })
        addAndMakeVisible (c);

    scrollBar.setAutoHide (false);
    scrollBar.addListener (this);
    addAndMakeVisible (scrollBar);

    view.onChange = [this]
    {
        updateScrollBar();
        repaintAll();
    };

    state.addListener (this);
    audioFiles.addChangeListener (this);
    rebuildTempoMap();
    startTimerHz (30);
}

TimelineComponent::~TimelineComponent()
{
    stopTimer();
    audioFiles.removeChangeListener (this);
    state.removeListener (this);
    scrollBar.removeListener (this);
}

juce::File TimelineComponent::resolve (const juce::String& relativePath) const
{
    return resolveClipFile (location(), relativePath);
}

void TimelineComponent::projectChanged()
{
    // Forget selected clips that no longer exist (deleted, or undone).
    juce::StringArray stillThere;
    for (const auto& id : selectedClips)
        if (clips::findClip (project, id).isValid())
            stillThere.add (id);

    if (stillThere != selectedClips)
        setSelection (stillThere);

    rebuildTempoMap();
}

void TimelineComponent::changeListenerCallback (juce::ChangeBroadcaster*)
{
    trackLanes->repaint();   // audio files finished loading: draw their waveforms
}

void TimelineComponent::rebuildTempoMap()
{
    tempoMap = timeline::buildTempoMap (project);
    updateScrollBar();
    repaintAll();
}

void TimelineComponent::repaintAll()
{
    ruler->repaint();
    signatureLane->repaint();
    tempoLane->repaint();
    trackLanes->repaint();
    repaint();
}

void TimelineComponent::tracksScrolled()
{
    trackLanes->repaint();
}

//==============================================================================
void TimelineComponent::setGrid (GridSize newGrid)
{
    if (newGrid == grid)
        return;

    if (grid != GridSize::off)
        lastGrid = grid;

    grid = newGrid;

    if (onGridChanged)
        onGridChanged();
}

void TimelineComponent::toggleSnap()
{
    setGrid (grid == GridSize::off ? (lastGrid == GridSize::off ? GridSize::beat : lastGrid) : GridSize::off);
}

Tick TimelineComponent::snap (Tick tick, const juce::ModifierKeys& mods) const
{
    // Hold Alt for exact positions.
    return mods.isAltDown() ? tick : snapToGrid (tempoMap, tick, grid);
}

//==============================================================================
void TimelineComponent::setSelection (const juce::StringArray& ids)
{
    if (ids == selectedClips)
        return;

    selectedClips = ids;
    trackLanes->repaint();

    if (onClipSelectionChanged)
        onClipSelectionChanged();
}

void TimelineComponent::selectClips (const juce::StringArray& clipIds)   { setSelection (clipIds); }
void TimelineComponent::clearClipSelection()                            { setSelection ({}); }

void TimelineComponent::selectAllClips()
{
    juce::StringArray all;
    for (const auto& clip : clips::allClips (project))
        all.add (clip.getId());
    setSelection (all);
}

bool TimelineComponent::splitClipsAtPlayhead()
{
    const auto position = engine.getTransportStatus().position;

    // The selected clips, or else every clip on the selected track.
    juce::Array<Clip> candidates;
    if (hasSelectedClips())
    {
        for (const auto& id : selectedClips)
            candidates.add (clips::findClip (project, id));
    }
    else if (auto track = project.findTrack (trackList.getSelectedTrackId()); track.isValid())
    {
        candidates = clips::clipsOnTrack (track.getState());
    }

    juce::StringArray rightParts;
    bool first = true;

    for (const auto& clip : candidates)
    {
        if (! clip.isValid() || position <= clip.getStart() || position >= clip.getEnd (tempoMap))
            continue;

        if (first)
            project.getUndoManager().beginNewTransaction ("Split clip");

        if (auto right = edit::splitClip (project, clip.getId(), position, false); right.isNotEmpty())
            rightParts.add (right);

        first = false;
    }

    if (! rightParts.isEmpty())
        setSelection (rightParts);

    return ! rightParts.isEmpty();
}

bool TimelineComponent::duplicateSelectedClips()
{
    const auto copies = edit::duplicateClips (project, selectedClips);

    if (! copies.isEmpty())
        setSelection (copies);

    return ! copies.isEmpty();
}

bool TimelineComponent::deleteSelectedClips()
{
    const auto deleted = edit::deleteClips (project, selectedClips);
    if (deleted)
        setSelection ({});
    return deleted;
}

void TimelineComponent::nudge (int direction, bool fine)
{
    const auto stepGrid = grid == GridSize::off ? GridSize::beat : grid;

    if (! hasSelectedClips())
    {
        // Nothing selected: move the playhead instead.
        const auto position = engine.getTransportStatus().position;
        engine.locate (fine ? tempoMap.secondsToTicks (tempoMap.ticksToSeconds (position) + direction * nudgeSeconds)
                            : moveByGridSteps (tempoMap, position, stepGrid, direction));
        return;
    }

    // Every selected clip moves by the same amount, worked out from the earliest one.
    Tick earliest = std::numeric_limits<Tick>::max();
    for (const auto& id : selectedClips)
        if (auto clip = clips::findClip (project, id); clip.isValid())
            earliest = std::min (earliest, clip.getStart());

    if (earliest == std::numeric_limits<Tick>::max())
        return;

    const auto target = fine ? tempoMap.secondsToTicks (tempoMap.ticksToSeconds (earliest) + direction * nudgeSeconds)
                             : moveByGridSteps (tempoMap, earliest, stepGrid, direction);
    const auto delta = std::max<Tick> (target, 0) - earliest;

    if (delta == 0)
        return;

    project.getUndoManager().beginNewTransaction ("Nudge clip");

    for (const auto& id : selectedClips)
        if (auto clip = clips::findClip (project, id); clip.isValid())
            edit::setClipPosition (project, id, clip.getStart() + delta, clip.getTrackState()[ids::id].toString(), false);
}

//==============================================================================
int TimelineComponent::trackRowAt (int yInTimeline) const
{
    const auto y = yInTimeline - headerHeight + trackViewport.getViewPositionY();
    return y < 0 ? -1 : y / TrackListComponent::rowHeight;
}

bool TimelineComponent::isInterestedInFileDrag (const juce::StringArray& files)
{
    const auto wildcard = audioFiles.getWildcard();

    for (const auto& path : files)
        if (wildcard.containsIgnoreCase ("*" + juce::File (path).getFileExtension() + ";")
            || wildcard.endsWithIgnoreCase ("*" + juce::File (path).getFileExtension()))
            return juce::File (path).getFileExtension().isNotEmpty();

    return false;
}

void TimelineComponent::fileDragEnter (const juce::StringArray& files, int x, int y)
{
    fileDragActive = true;
    fileDragMove (files, x, y);
}

void TimelineComponent::fileDragMove (const juce::StringArray&, int x, int y)
{
    dropRow = std::max (0, trackRowAt (y));
    dropTick = std::max<Tick> (0, snapToGrid (tempoMap, view.xToTick (x), grid));
    trackLanes->repaint();
}

void TimelineComponent::fileDragExit (const juce::StringArray&)
{
    fileDragActive = false;
    trackLanes->repaint();
}

void TimelineComponent::filesDropped (const juce::StringArray& paths, int x, int y)
{
    fileDragMove (paths, x, y);
    fileDragActive = false;
    trackLanes->repaint();

    juce::Array<juce::File> files;
    for (const auto& path : paths)
        files.add (juce::File (path));

    const auto trackId = juce::isPositiveAndBelow (dropRow, project.getNumTracks()) ? project.getTrack (dropRow).getId() : juce::String();

    if (onImportFiles)
        onImportFiles (files, trackId, dropTick);
}

//==============================================================================
void TimelineComponent::drawGrid (juce::Graphics& g, juce::Rectangle<int> area, bool strong) const
{
    const auto barColour = theme.outline.withAlpha (strong ? 0.9f : 0.55f);
    const auto beatColour = theme.outline.withAlpha (strong ? 0.45f : 0.22f);
    const auto top = (float) area.getY(), bottom = (float) area.getBottom();

    const auto pixelsPerBar = view.getPixelsPerQuarter() * (double) tempoMap.barLengthAt (view.getViewStart()) / (double) ticksPerQuarter;
    int barStep = 1;
    while (pixelsPerBar * barStep < 8.0)
        barStep *= 2;

    for (int bar = std::max (0, tempoMap.ticksToBarBeat (view.getViewStart()).bar); bar < 100000; ++bar)
    {
        const auto barTick = tempoMap.barToTicks (bar);
        const auto x = (float) view.tickToX (barTick);

        if (x > (float) area.getRight())
            break;

        if (bar % barStep == 0)
        {
            g.setColour (barColour);
            g.drawVerticalLine ((int) std::floor (x), top, bottom);
        }

        // Beat lines, if there's room for them.
        const auto beat = tempoMap.beatLengthAt (barTick);
        const auto pixelsPerBeat = view.getPixelsPerQuarter() * (double) beat / (double) ticksPerQuarter;

        if (pixelsPerBeat >= 12.0)
        {
            g.setColour (beatColour);
            const auto barLength = tempoMap.barLengthAt (barTick);

            for (Tick t = barTick + beat; t < barTick + barLength; t += beat)
                g.drawVerticalLine ((int) std::floor (view.tickToX (t)), top + (strong ? (bottom - top) * 0.55f : 0.0f), bottom);
        }
    }
}

void TimelineComponent::zoomIn()   { view.zoomBy (1.5, getWidth() * 0.5); }
void TimelineComponent::zoomOut()  { view.zoomBy (1.0 / 1.5, getWidth() * 0.5); }

void TimelineComponent::updateScrollBar()
{
    // Enough room for everything in the song, plus some empty bars to work in.
    Tick contentEnd = tempoMap.barToTicks (64);
    contentEnd = std::max (contentEnd, timeline::getLoop (project).end);
    for (const auto& p : tempoMap.getTempoPoints())
        contentEnd = std::max (contentEnd, p.tick);
    for (const auto& clip : clips::allClips (project))
        contentEnd = std::max (contentEnd, clip.getEnd (tempoMap));
    contentEnd += tempoMap.barToTicks (16);

    const auto visible = (double) view.ticksForWidth (getWidth());
    const auto start = (double) view.getViewStart();

    updatingScrollBar = true;
    scrollBar.setRangeLimits (0.0, std::max ((double) contentEnd, start + visible));
    scrollBar.setCurrentRange (start, visible, juce::dontSendNotification);
    scrollBar.setSingleStepSize (visible * 0.05);
    updatingScrollBar = false;
}

void TimelineComponent::scrollBarMoved (juce::ScrollBar*, double newRangeStart)
{
    if (! updatingScrollBar)
        view.setViewStart ((Tick) newRangeStart);
}

int TimelineComponent::playheadX() const
{
    return (int) std::floor (view.tickToX (engine.getTransportStatus().position));
}

void TimelineComponent::timerCallback()
{
    updatePlayhead();
}

void TimelineComponent::updatePlayhead()
{
    const auto status = engine.getTransportStatus();
    auto x = playheadX();

    // Follow the playhead while playing: when it reaches the right edge, turn the page. And when
    // it jumps while stopped (Go to start, Home, nudging) to somewhere off screen, scroll there.
    const auto jumped = ! status.playing && status.position != lastPlayheadPosition;
    lastPlayheadPosition = status.position;

    if ((status.playing || jumped) && (x > getWidth() - 30 || x < 0))
    {
        view.setViewStart (std::max<Tick> (0, status.position - view.ticksForWidth (getWidth() * 0.1)));
        x = playheadX();
    }

    if (x != lastPlayheadX)
    {
        const auto height = getHeight() - scrollBarHeight;
        repaint (lastPlayheadX - 6, 0, 13, height);
        repaint (x - 6, 0, 13, height);
        lastPlayheadX = x;
    }
}

void TimelineComponent::paint (juce::Graphics& g)
{
    g.fillAll (theme.meterBackground);
}

void TimelineComponent::paintOverChildren (juce::Graphics& g)
{
    const auto x = (float) playheadX();

    if (x < -2.0f || x > (float) getWidth() + 2.0f)
        return;

    const auto bottom = (float) (getHeight() - scrollBarHeight);
    g.setColour (theme.text.withAlpha (0.9f));
    g.drawLine (x + 0.5f, (float) loopStripHeight, x + 0.5f, bottom, 1.5f);

    // A small head on the ruler, so it's easy to see.
    juce::Path head;
    head.addTriangle (x - 6.0f, (float) loopStripHeight, x + 7.0f, (float) loopStripHeight, x + 0.5f, (float) loopStripHeight + 9.0f);
    g.fillPath (head);
}

void TimelineComponent::resized()
{
    auto area = getLocalBounds();
    scrollBar.setBounds (area.removeFromBottom (scrollBarHeight));
    ruler->setBounds (area.removeFromTop (rulerHeight));
    signatureLane->setBounds (area.removeFromTop (signatureHeight));
    tempoLane->setBounds (area.removeFromTop (tempoHeight));
    trackLanes->setBounds (area);
    updateScrollBar();
}

void TimelineComponent::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const auto local = e.getEventRelativeTo (this);

    if (e.mods.isCommandDown())
    {
        // Zoom around the mouse position.
        view.zoomBy (std::pow (2.0, (double) wheel.deltaY * 2.0), (double) local.x);
    }
    else if (e.mods.isShiftDown() || std::abs (wheel.deltaX) > std::abs (wheel.deltaY))
    {
        const auto delta = wheel.deltaX != 0.0f ? wheel.deltaX : wheel.deltaY;
        view.setViewStart (view.getViewStart() - (Tick) (delta * (float) view.ticksForWidth (getWidth() * 0.5)));
    }
    else
    {
        const auto newY = trackViewport.getViewPositionY() - (int) (wheel.deltaY * TrackListComponent::rowHeight * 4.0f);
        trackViewport.setViewPosition (0, std::max (0, newY));
    }
}

} // namespace aether::ui
