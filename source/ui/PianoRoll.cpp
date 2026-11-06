// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "PianoRoll.h"

#include <algorithm>
#include <cmath>

#include "model/ClipEdits.h"
#include "model/Scales.h"
#include "model/TimelineEdits.h"
#include "model/Track.h"

namespace aether::ui
{

namespace
{
    constexpr int defaultVelocity = 100;
    constexpr int minRowHeight = 8, maxRowHeight = 30;

    /** Notes copied in the piano roll (start relative to the earliest), for pasting. */
    std::vector<NoteData>& noteClipboard()
    {
        static std::vector<NoteData> notes;
        return notes;
    }

    bool isBlackKey (int pitch)   { return juce::MidiMessage::isMidiNoteBlack (pitch); }

    bool isIn (const NoteNodes& nodes, const juce::ValueTree& n)
    {
        return std::find (nodes.begin(), nodes.end(), n) != nodes.end();
    }

    struct LengthChoice { const char* name; Tick length; };
    constexpr LengthChoice lengthChoices[] {
        { "Last used", 0 },
        { "1 bar (4/4)", 4 * ticksPerQuarter },
        { "1/2", 2 * ticksPerQuarter },
        { "1/4", ticksPerQuarter },
        { "1/8", ticksPerQuarter / 2 },
        { "1/16", ticksPerQuarter / 4 },
        { "1/32", ticksPerQuarter / 8 },
    };

    juce::String describeLength (Tick length)
    {
        for (const auto& c : lengthChoices)
            if (c.length == length)
                return c.name;

        return juce::String ((double) length / (double) ticksPerQuarter, 2) + " beats";
    }
}

//==============================================================================
/** Bar numbers above the notes; click or drag to move the playhead. */
class PianoRoll::Ruler final : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit Ruler (PianoRoll& o) : owner (o)
    {
        setTooltip ("Click to move the playhead here.");
    }

    void paint (juce::Graphics& g) override
    {
        const auto& t = owner.theme;
        g.fillAll (t.panel);

        const auto clip = owner.getClip();
        if (clip.isValid())
        {
            const auto x1 = (float) owner.view.tickToX (clip.getStart());
            const auto x2 = (float) owner.view.tickToX (clip.getStart() + clip.getLengthTicks());
            g.setColour (t.accent.withAlpha (0.25f));
            g.fillRect (juce::Rectangle<float> (x1, (float) getHeight() - 5.0f, x2 - x1, 5.0f));
        }

        g.setFont (juce::FontOptions (12.0f));

        for (int bar = std::max (0, owner.tempo.ticksToBarBeat (owner.view.getViewStart()).bar); bar < 100000; ++bar)
        {
            const auto x = (int) owner.view.tickToX (owner.tempo.barToTicks (bar));
            if (x > getWidth())
                break;

            g.setColour (t.outline);
            g.drawVerticalLine (x, 0.0f, (float) getHeight());
            g.setColour (t.textDim);
            g.drawText (juce::String (bar + 1), x + 4, 0, 40, getHeight() - 4, juce::Justification::centredLeft);
        }

        const auto playX = (float) owner.view.tickToX (owner.engine.getTransportStatus().position);
        juce::Path head;
        head.addTriangle (playX - 5.0f, 0.0f, playX + 5.0f, 0.0f, playX, 8.0f);
        g.setColour (t.text);
        g.fillPath (head);
        g.drawLine (playX, 0.0f, playX, (float) getHeight());
        g.setColour (t.outline);
        g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());
    }

    void mouseDown (const juce::MouseEvent& e) override   { locate (e); }
    void mouseDrag (const juce::MouseEvent& e) override   { locate (e); }

private:
    void locate (const juce::MouseEvent& e)
    {
        owner.engine.locate (std::max<Tick> (0, owner.snapNearest (owner.view.xToTick (e.x), e.mods)));
        owner.repaint();
    }

    PianoRoll& owner;
};

//==============================================================================
/** The keyboard down the left side: click a key to hear it. */
class PianoRoll::Keys final : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit Keys (PianoRoll& o) : owner (o)
    {
        setTooltip ("Click a key to hear it.");
    }

    void paint (juce::Graphics& g) override
    {
        const auto& t = owner.theme;
        g.fillAll (t.background);
        const auto h = owner.rowHeight;

        for (int pitch = 0; pitch < 128; ++pitch)
        {
            const auto y = owner.pitchToY (pitch);
            if (y + h < 0 || y > getHeight())
                continue;

            const juce::Rectangle<int> row (0, y, getWidth(), h);
            const auto black = isBlackKey (pitch);
            auto colour = black ? juce::Colour (0xff23262d) : juce::Colour (0xffd9dce3);

            if (owner.scaleActive() && ! owner.inScale (pitch))
                colour = colour.interpolatedWith (t.background, 0.55f);

            if (pitch == pressed)
                colour = t.accent;

            g.setColour (colour);
            g.fillRect (black ? row.withTrimmedRight (getWidth() / 3) : row);

            if (black)
            {
                g.setColour (juce::Colour (0xffd9dce3).interpolatedWith (t.background, owner.scaleActive() && ! owner.inScale (pitch) ? 0.55f : 0.0f));
                g.fillRect (row.withTrimmedLeft (getWidth() * 2 / 3));
            }

            g.setColour (t.outline.withAlpha (0.6f));
            g.drawHorizontalLine (y + h - 1, 0.0f, (float) getWidth());

            if (pitch % 12 == 0 && h >= 10)
            {
                g.setColour (juce::Colour (0xff3a3f4a));
                g.setFont (juce::FontOptions ((float) std::min (12, h - 2)));
                g.drawText (midi::noteName (pitch), row.reduced (4, 0), juce::Justification::centredRight);
            }
        }

        g.setColour (t.outline);
        g.drawVerticalLine (getWidth() - 1, 0.0f, (float) getHeight());
    }

    void mouseDown (const juce::MouseEvent& e) override   { press (owner.yToPitch (e.y)); }
    void mouseDrag (const juce::MouseEvent& e) override   { press (owner.yToPitch (e.y)); }

    void mouseUp (const juce::MouseEvent&) override
    {
        owner.releasePreview();
        pressed = -1;
        repaint();
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override;

private:
    void press (int pitch)
    {
        if (! juce::isPositiveAndBelow (pitch, 128) || pitch == pressed)
            return;

        pressed = pitch;
        owner.play (pitch, defaultVelocity, true, true);
        repaint();
    }

    PianoRoll& owner;
    int pressed = -1;
};

//==============================================================================
/** The notes. */
class PianoRoll::Grid final : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit Grid (PianoRoll& o) : owner (o)
    {
        setTooltip ("Click to add a note. Drag a note to move it, its right edge to change its length. "
                    "Right-click a note to delete it. Ctrl+click or Ctrl+drag to select several.");
    }

    void paint (juce::Graphics& g) override
    {
        const auto& t = owner.theme;
        const auto h = owner.rowHeight;
        const auto key = owner.songKey();

        // Rows: white and black keys, and (with a scale) the notes outside it darker.
        for (int pitch = 0; pitch < 128; ++pitch)
        {
            const auto y = owner.pitchToY (pitch);
            if (y + h < 0 || y > getHeight())
                continue;

            auto colour = isBlackKey (pitch) ? t.background : t.panel;

            if (owner.scaleActive())
            {
                if (! owner.inScale (pitch))
                    colour = t.background.darker (0.35f);
                else if ((pitch - key) % 12 == 0)
                    colour = colour.interpolatedWith (t.accent, 0.08f);
            }

            g.setColour (colour);
            g.fillRect (0, y, getWidth(), h);
            g.setColour (t.outline.withAlpha (pitch % 12 == 0 ? 0.9f : 0.3f));
            g.drawHorizontalLine (y + h - 1, 0.0f, (float) getWidth());
        }

        drawTimeLines (g);

        // The clip: everything outside it is dimmed.
        const auto clip = owner.getClip();
        if (clip.isValid())
        {
            const auto x1 = (float) owner.view.tickToX (clip.getStart());
            const auto x2 = (float) owner.view.tickToX (clip.getStart() + clip.getLengthTicks());
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            if (x1 > 0.0f)                   g.fillRect (0.0f, 0.0f, x1, (float) getHeight());
            if (x2 < (float) getWidth())     g.fillRect (x2, 0.0f, (float) getWidth() - x2, (float) getHeight());
            g.setColour (t.accent.withAlpha (0.6f));
            g.drawVerticalLine ((int) x2, 0.0f, (float) getHeight());

            drawNotes (g, clip);
        }

        if (mode == Mode::box)
        {
            g.setColour (t.accent.withAlpha (0.15f));
            g.fillRect (box);
            g.setColour (t.accent);
            g.drawRect (box, 1.0f);
        }

        const auto playX = (float) owner.view.tickToX (owner.engine.getTransportStatus().position);
        g.setColour (t.text.withAlpha (0.8f));
        g.drawLine (playX, 0.0f, playX, (float) getHeight());
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const auto hit = hitAt (e.position);
        setMouseCursor (! hit.note.isValid() ? juce::MouseCursor::NormalCursor
                        : hit.edge           ? juce::MouseCursor::LeftRightResizeCursor
                                             : juce::MouseCursor::DraggingHandCursor);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        owner.grabKeyboardFocus();
        const auto clip = owner.getClip();
        mode = Mode::none;
        changed = false;

        if (! clip.isValid())
            return;

        const auto hit = hitAt (e.position);
        const auto tick = owner.view.xToTick (e.x);

        if (e.mods.isPopupMenu())
        {
            // Right-click deletes the note under the mouse.
            if (hit.note.isValid())
                edit::deleteNotes (owner.project, { hit.note });

            return;
        }

        if (! hit.note.isValid())
        {
            if (e.mods.isCommandDown())
            {
                mode = Mode::box;
                boxBase = owner.selection;
                box = { e.position, e.position };
                return;
            }

            // Add a note where the mouse is.
            const auto pitch = owner.yToPitch (e.y);
            if (! juce::isPositiveAndBelow (pitch, 128))
                return;

            const auto start = owner.snapDown (tick, e.mods);
            const NoteData note { pitch, defaultVelocity, std::max<Tick> (0, owner.toContent (start)), owner.getNewNoteLength() };
            grabbed = edit::addNote (owner.project, clip.getId(), note, true);
            owner.setSelection ({ grabbed });
            grabStart = owner.toTimeline (note.start);
            grabEnd = grabStart + note.length;
            downTick = tick;
            mode = Mode::draw;
            owner.play (pitch, note.velocity, true);
            return;
        }

        // On a note.
        if (e.mods.isCommandDown())
        {
            auto newSelection = owner.selection;
            if (isIn (newSelection, hit.note))
                newSelection.erase (std::find (newSelection.begin(), newSelection.end(), hit.note));
            else
                newSelection.push_back (hit.note);

            owner.setSelection (newSelection);

            if (! isIn (owner.selection, hit.note))
                return;
        }
        else if (! isIn (owner.selection, hit.note))
        {
            owner.setSelection ({ hit.note });
        }

        grabbed = hit.note;
        const auto n = notes::read (grabbed);
        grabStart = owner.toTimeline (n.start);
        grabEnd = grabStart + n.length;
        downTick = tick;
        mode = hit.edge ? Mode::resize : Mode::move;
        owner.lastLength = n.length;   // new notes are as long as the last one touched
        owner.play (n.pitch, n.velocity, true);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! grabbed.isValid() && mode != Mode::box)
            return;

        auto& proj = owner.project;
        const auto tick = owner.view.xToTick (e.x);

        switch (mode)
        {
            case Mode::draw:
            case Mode::resize:
            {
                const auto n = notes::read (grabbed);
                const auto start = owner.toTimeline (n.start);
                const auto minimum = e.mods.isAltDown() ? ticksPerQuarter / 32 : owner.stepAt (start);
                const auto newEnd = std::max (start + minimum, owner.snapNearest (grabEnd + (tick - downTick), e.mods));
                const auto delta = newEnd - (start + n.length);

                if (delta != 0)
                {
                    if (mode == Mode::resize && ! changed)
                        proj.getUndoManager().beginNewTransaction (owner.selection.size() == 1 ? "Change note length"
                                                                                                  : "Change note lengths");
                    edit::resizeNotes (proj, mode == Mode::draw ? NoteNodes { grabbed } : owner.selection, delta, minimum, false);
                    changed = true;
                }
                break;
            }

            case Mode::move:
            {
                if (! changed && e.getDistanceFromDragStart() < 3)
                    return;

                const auto n = notes::read (grabbed);
                const auto newStart = owner.snapNearest (grabStart + (tick - downTick), e.mods);
                const auto deltaTicks = newStart - owner.toTimeline (n.start);
                const auto deltaPitch = juce::jlimit (0, 127, owner.yToPitch (e.y)) - n.pitch;

                if (deltaTicks != 0 || deltaPitch != 0)
                {
                    if (! changed)
                        proj.getUndoManager().beginNewTransaction (owner.selection.size() == 1 ? "Move note" : "Move notes");

                    edit::moveNotes (proj, owner.selection, deltaTicks, deltaPitch, false);
                    changed = true;

                    if (deltaPitch != 0)
                        owner.play (notes::read (grabbed).pitch, n.velocity, true);
                }
                break;
            }

            case Mode::box:
            {
                box = juce::Rectangle<float> (e.mouseDownPosition, e.position);
                auto newSelection = boxBase;

                for (const auto& n : notes::nodesOf (owner.getClip()))
                    if (noteBounds (n).intersects (box) && ! isIn (newSelection, n))
                        newSelection.push_back (n);

                owner.setSelection (newSelection);
                repaint();
                break;
            }

            case Mode::none:
                break;
        }
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if ((mode == Mode::draw || mode == Mode::resize) && grabbed.isValid() && grabbed.getParent().isValid())
            owner.lastLength = notes::read (grabbed).length;

        owner.releasePreview();
        mode = Mode::none;
        grabbed = {};
        box = {};
        owner.updateToolbar();
        repaint();
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
    {
        if (e.mods.isCommandDown())
        {
            owner.view.zoomBy (std::pow (2.0, (double) wheel.deltaY * 2.0), (double) e.x);
        }
        else if (e.mods.isAltDown())
        {
            const auto middle = owner.yToPitch (e.y);
            owner.rowHeight = juce::jlimit (minRowHeight, maxRowHeight, owner.rowHeight + (wheel.deltaY > 0 ? 2 : -2));
            owner.scrollToPitch (middle);
        }
        else if (e.mods.isShiftDown() || std::abs (wheel.deltaX) > std::abs (wheel.deltaY))
        {
            const auto delta = wheel.deltaX != 0.0f ? wheel.deltaX : wheel.deltaY;
            owner.view.setViewStart (owner.view.getViewStart() - (Tick) (delta * (float) owner.view.ticksForWidth (getWidth() * 0.5)));
        }
        else
        {
            owner.scrollY = juce::jlimit (0, std::max (0, 128 * owner.rowHeight - getHeight()),
                                          owner.scrollY - (int) (wheel.deltaY * (float) owner.rowHeight * 8.0f));
            owner.updateScrollBars();
            owner.repaint();
        }
    }

    juce::Rectangle<float> noteBounds (const juce::ValueTree& note) const
    {
        const auto n = notes::read (note);
        const auto x1 = owner.view.tickToX (owner.toTimeline (n.start));
        const auto x2 = owner.view.tickToX (owner.toTimeline (n.start + n.length));
        return { (float) x1, (float) owner.pitchToY (n.pitch), (float) std::max (3.0, x2 - x1), (float) owner.rowHeight - 1.0f };
    }

private:
    enum class Mode { none, draw, move, resize, box };

    struct Hit
    {
        juce::ValueTree note;
        bool edge = false;
    };

    Hit hitAt (juce::Point<float> p) const
    {
        const auto all = notes::nodesOf (owner.getClip());

        // The last drawn is on top, so search from the end.
        for (auto it = all.rbegin(); it != all.rend(); ++it)
        {
            const auto r = noteBounds (*it);

            if (r.contains (p))
                return { *it, p.x >= r.getRight() - std::min (6.0f, r.getWidth() / 3.0f) };
        }

        return {};
    }

    void drawTimeLines (juce::Graphics& g) const
    {
        const auto& t = owner.theme;
        const auto& map = owner.tempo;
        const auto height = (float) getHeight();

        for (int bar = std::max (0, map.ticksToBarBeat (owner.view.getViewStart()).bar); bar < 100000; ++bar)
        {
            const auto barTick = map.barToTicks (bar);
            if (owner.view.tickToX (barTick) > getWidth())
                break;

            const auto barEnd = barTick + map.barLengthAt (barTick);
            const auto step = owner.stepAt (barTick);
            const auto beat = map.beatLengthAt (barTick);
            const auto stepPixels = owner.view.getPixelsPerQuarter() * (double) step / (double) ticksPerQuarter;

            for (auto tick = barTick; tick < barEnd; tick += (stepPixels >= 6.0 ? step : beat))
            {
                const auto x = (int) owner.view.tickToX (tick);
                const auto alpha = tick == barTick ? 0.95f : (tick - barTick) % beat == 0 ? 0.5f : 0.2f;
                g.setColour (t.outline.withAlpha (alpha));
                g.drawVerticalLine (x, 0.0f, height);
            }
        }
    }

    void drawNotes (juce::Graphics& g, const Clip& clip) const
    {
        const auto& t = owner.theme;
        const auto clipStart = clip.getContentStart(), clipEnd = clip.getContentStart() + clip.getLengthTicks();

        for (const auto& node : notes::nodesOf (clip))
        {
            const auto n = notes::read (node);
            auto r = noteBounds (node);

            if (r.getRight() < 0.0f || r.getX() > (float) getWidth() || r.getBottom() < 0.0f || r.getY() > (float) getHeight())
                continue;

            const auto selected = isIn (owner.selection, node);
            const auto outside = n.start + n.length <= clipStart || n.start >= clipEnd;   // trimmed away: not heard
            auto colour = owner.noteColour (n.velocity, selected);

            if (outside)
                colour = colour.withAlpha (0.35f);

            g.setColour (colour);
            g.fillRoundedRectangle (r.reduced (0.0f, 0.5f), 2.5f);

            if (selected)
            {
                g.setColour (t.text);
                g.drawRoundedRectangle (r.reduced (0.5f, 1.0f), 2.5f, 1.5f);
            }

            if (r.getWidth() >= 30.0f && owner.rowHeight >= 11)
            {
                g.setColour (juce::Colours::black.withAlpha (0.75f));
                g.setFont (juce::FontOptions ((float) std::min (11, owner.rowHeight - 2)));
                g.drawText (midi::noteName (n.pitch), r.reduced (4.0f, 0.0f), juce::Justification::centredLeft, false);
            }
        }
    }

    PianoRoll& owner;
    Mode mode = Mode::none;
    juce::ValueTree grabbed;
    Tick grabStart = 0, grabEnd = 0, downTick = 0;
    bool changed = false;
    juce::Rectangle<float> box;
    NoteNodes boxBase;
};

void PianoRoll::Keys::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    // Scrolling over the keys scrolls the notes.
    owner.noteGrid->mouseWheelMove (e.getEventRelativeTo (owner.noteGrid.get()), w);
}

//==============================================================================
/** Note velocities: drag a stem up or down. */
class PianoRoll::VelocityLane final : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit VelocityLane (PianoRoll& o) : owner (o)
    {
        setTooltip ("Velocity: how hard each note is played. Drag a note's stem up or down (with several notes "
                    "selected, they all change together). Drag across stems to set several.");
    }

    void paint (juce::Graphics& g) override
    {
        const auto& t = owner.theme;
        g.fillAll (t.background);

        g.setColour (t.outline.withAlpha (0.4f));
        for (int i = 1; i <= 4; ++i)
            g.drawHorizontalLine (yFor (i * 127 / 4), 0.0f, (float) getWidth());

        g.setColour (t.outline);
        g.drawHorizontalLine (0, 0.0f, (float) getWidth());

        const auto clip = owner.getClip();
        if (! clip.isValid())
            return;

        for (const auto& node : notes::nodesOf (clip))
        {
            const auto n = notes::read (node);
            const auto x = (float) owner.view.tickToX (owner.toTimeline (n.start));

            if (x < -6.0f || x > (float) getWidth() + 6.0f)
                continue;

            const auto y = (float) yFor (n.velocity);
            g.setColour (owner.noteColour (n.velocity, isIn (owner.selection, node)));
            g.drawLine (x, (float) getHeight(), x, y, 2.0f);
            g.fillEllipse (x - 3.5f, y - 3.5f, 7.0f, 7.0f);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        owner.project.getUndoManager().beginNewTransaction ("Change velocity");
        grabbed = stemAt ((float) e.x);
        group = grabbed.isValid() && isIn (owner.selection, grabbed) && owner.selection.size() > 1;
        apply (e);
    }

    void mouseDrag (const juce::MouseEvent& e) override   { apply (e); }
    void mouseUp (const juce::MouseEvent&) override       { grabbed = {}; }

    int velocityAt (int y) const
    {
        return juce::jlimit (1, 127, (int) std::lround (1.0 + (1.0 - (double) (y - top) / (double) (getHeight() - top)) * 126.0));
    }

private:
    static constexpr int top = 6;

    int yFor (int velocity) const
    {
        return top + (int) std::lround ((1.0 - (velocity - 1) / 126.0) * (getHeight() - top));
    }

    juce::ValueTree stemAt (float x) const
    {
        juce::ValueTree best;
        auto bestDistance = 6.0f;

        for (const auto& node : notes::nodesOf (owner.getClip()))
        {
            const auto distance = std::abs ((float) owner.view.tickToX (owner.toTimeline (notes::read (node).start)) - x);

            // Prefer selected notes where stems overlap (e.g. a chord).
            if (distance < bestDistance || (distance <= bestDistance + 0.01f && isIn (owner.selection, node)))
            {
                best = node;
                bestDistance = distance;
            }
        }

        return best;
    }

    void apply (const juce::MouseEvent& e)
    {
        const auto velocity = velocityAt (e.y);
        auto& proj = owner.project;

        if (grabbed.isValid())
        {
            if (group)
                edit::changeNoteVelocity (proj, owner.selection, velocity - notes::read (grabbed).velocity, false);
            else
                edit::setNoteVelocity (proj, { grabbed }, velocity, false);
            return;
        }

        // Not on a stem: set every stem the mouse passes over.
        if (auto stem = stemAt ((float) e.x); stem.isValid())
            edit::setNoteVelocity (proj, { stem }, velocity, false);
    }

    PianoRoll& owner;
    juce::ValueTree grabbed;
    bool group = false;
};

//==============================================================================
namespace
{
    /** A small panel in a pop-up bubble: sliders and an Apply button. */
    class SettingsPanel final : public juce::Component
    {
    public:
        struct Row { juce::String name, tooltip; double min, max, step, value; juce::String suffix; };

        SettingsPanel (const Theme& theme, std::vector<Row> rowsToShow, const juce::String& toggleText, bool toggleValue,
                       const juce::String& applyText, std::function<void (std::vector<double>, bool)> onApplyToUse)
            : rows (std::move (rowsToShow)), onApply (std::move (onApplyToUse))
        {
            for (const auto& r : rows)
            {
                auto label = std::make_unique<juce::Label> (juce::String(), r.name);
                label->setColour (juce::Label::textColourId, theme.text);
                addAndMakeVisible (*label);
                labels.push_back (std::move (label));

                auto slider = std::make_unique<juce::Slider> (juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
                slider->setRange (r.min, r.max, r.step);
                slider->setValue (r.value, juce::dontSendNotification);
                slider->setTextValueSuffix (r.suffix);
                slider->setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 22);
                slider->setTooltip (r.tooltip);
                addAndMakeVisible (*slider);
                sliders.push_back (std::move (slider));
            }

            toggle.setButtonText (toggleText);
            toggle.setToggleState (toggleValue, juce::dontSendNotification);
            toggle.setVisible (toggleText.isNotEmpty());
            addChildComponent (toggle);

            apply.setButtonText (applyText);
            apply.onClick = [this]
            {
                std::vector<double> values;
                for (auto& s : sliders)
                    values.push_back (s->getValue());

                onApply (values, toggle.getToggleState());

                if (auto* box = findParentComponentOfClass<juce::CallOutBox>())
                    box->dismiss();
            };
            addAndMakeVisible (apply);

            setSize (340, 16 + (int) rows.size() * 34 + (toggleText.isNotEmpty() ? 30 : 0) + 44);
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced (10, 8);
            for (size_t i = 0; i < rows.size(); ++i)
            {
                auto row = area.removeFromTop (34);
                labels[i]->setBounds (row.removeFromLeft (90));
                sliders[i]->setBounds (row.reduced (0, 4));
            }

            if (toggle.isVisible())
                toggle.setBounds (area.removeFromTop (30));

            apply.setBounds (area.removeFromBottom (32).removeFromRight (120));
        }

    private:
        std::vector<Row> rows;
        std::vector<std::unique_ptr<juce::Label>> labels;
        std::vector<std::unique_ptr<juce::Slider>> sliders;
        juce::ToggleButton toggle;
        juce::TextButton apply;
        std::function<void (std::vector<double>, bool)> onApply;
    };
}

//==============================================================================
PianoRoll::PianoRoll (const Theme& themeToUse, Project& projectToEdit, AudioEngine& audioEngine)
    : theme (themeToUse), project (projectToEdit), engine (audioEngine)
{
    ruler = std::make_unique<Ruler> (*this);
    keys = std::make_unique<Keys> (*this);
    noteGrid = std::make_unique<Grid> (*this);
    velocityLane = std::make_unique<VelocityLane> (*this);

    for (auto* c : std::initializer_list<juce::Component*> { ruler.get(), keys.get(), noteGrid.get(), velocityLane.get(), &hScroll, &vScroll })
        addAndMakeVisible (*c);

    hScroll.addListener (this);
    vScroll.addListener (this);
    hScroll.setAutoHide (false);
    vScroll.setAutoHide (false);

    previewNote = [this] (int pitch, int velocity, bool on)
    {
        if (const auto track = getTrackId(); track.isNotEmpty())
            engine.sendPreviewNote (track, pitch, velocity, on);
    };

    view.onChange = [this]
    {
        updateScrollBars();
        ruler->repaint();
        noteGrid->repaint();
        velocityLane->repaint();
    };

    // Toolbar ------------------------------------------------------------------------------------
    titleLabel.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, theme.text);
    addAndMakeVisible (titleLabel);

    for (size_t i = 0; i < allGridSizes.size(); ++i)
        gridBox.addItem (juce::String ("Snap: ") + gridSizeName (allGridSizes[i]), (int) i + 1);
    gridBox.onChange = [this]
    {
        const auto index = gridBox.getSelectedId() - 1;
        if (juce::isPositiveAndBelow (index, (int) allGridSizes.size()))
            setGrid (allGridSizes[(size_t) index]);
    };
    gridBox.setTooltip ("Where notes snap to when you add, move or resize them. Hold Alt while dragging to place freely.");
    addAndMakeVisible (gridBox);

    lengthBox.onChange = [this]
    {
        const auto index = lengthBox.getSelectedId() - 1;
        if (juce::isPositiveAndBelow (index, (int) std::size (lengthChoices)))
            setFixedNoteLength (lengthChoices[index].length);
    };
    lengthBox.setTooltip ("How long new notes are. \"Last used\": as long as the last note you drew, resized or clicked.");
    addAndMakeVisible (lengthBox);

    for (int k = 0; k < 12; ++k)
        keyBox.addItem (juce::String ("Key: ") + scales::keyName (k), k + 1);
    keyBox.onChange = [this] { edit::setScale (project, keyBox.getSelectedId() - 1, project.getState()[ids::scale].toString()); };
    keyBox.setTooltip ("The song's key: its note is tinted, and with a scale chosen, notes outside it are shaded.");
    addAndMakeVisible (keyBox);

    for (int i = 0; i < (int) scales::allScales.size(); ++i)
        scaleBox.addItem (scales::allScales[(size_t) i].name, i + 1);
    scaleBox.onChange = [this]
    {
        const auto index = scaleBox.getSelectedId() - 1;
        if (juce::isPositiveAndBelow (index, (int) scales::allScales.size()))
            edit::setScale (project, songKey(), scales::allScales[(size_t) index].id);
    };
    scaleBox.setTooltip ("Shades the notes that aren't in this scale, so it's easy to stay in key. "
                         "It only shows them: you can still use any note.");
    addAndMakeVisible (scaleBox);

    quantizeButton.setTooltip ("Moves notes onto the snap grid (Q): the selected notes, or all of them. "
                               "Choose how strongly, and add swing.");
    quantizeButton.onClick = [this] { showQuantizePanel(); };
    addAndMakeVisible (quantizeButton);

    humanizeButton.setTooltip ("Makes notes a little less exact, like a person playing: small random changes to "
                               "timing and velocity.");
    humanizeButton.onClick = [this] { showHumanizePanel(); };
    addAndMakeVisible (humanizeButton);

    previewButton.setClickingTogglesState (true);
    previewButton.setToggleState (true, juce::dontSendNotification);
    previewButton.setTooltip ("Hear notes as you add, move and change them.");
    previewButton.onClick = [this] { setPreviewEnabled (previewButton.getToggleState()); };
    addAndMakeVisible (previewButton);

    setWantsKeyboardFocus (true);
    project.getState().addListener (this);
    updateTempo();
    updateToolbar();
    setSize (1000, 640);
    scrollToPitch (60);
    startTimerHz (30);
}

PianoRoll::~PianoRoll()
{
    releasePreview();
    project.getState().removeListener (this);
}

juce::Component& PianoRoll::getNoteGrid() noexcept       { return *noteGrid; }
juce::Component& PianoRoll::getVelocityLane() noexcept   { return *velocityLane; }
juce::Component& PianoRoll::getKeys() noexcept           { return *keys; }

//==============================================================================
void PianoRoll::setClip (const juce::String& clipId)
{
    releasePreview();
    const auto clip = clips::findClip (project, clipId);
    clipState = clip.isValid() && clip.isMidi() ? clip.getState() : juce::ValueTree();
    selection.clear();
    updateToolbar();
    zoomToClip();

    // Show where the notes are (or middle C).
    if (const auto all = notes::nodesOf (getClip()); ! all.empty())
    {
        int low = 127, high = 0;
        for (const auto& n : all)
        {
            low = std::min (low, notes::read (n).pitch);
            high = std::max (high, notes::read (n).pitch);
        }
        scrollToPitch ((low + high) / 2);
    }
    else
    {
        scrollToPitch (60);
    }

    repaint();

    if (onClipChanged)
        onClipChanged();
}

juce::String PianoRoll::getClipId() const   { return clipState[ids::id].toString(); }

Clip PianoRoll::getClip() const
{
    return Clip (clipState.isValid() && clipState.getRoot() == project.getState() ? clipState : juce::ValueTree());
}

juce::String PianoRoll::getTrackId() const
{
    const auto clip = getClip();
    return clip.isValid() ? clip.getTrackState()[ids::id].toString() : juce::String();
}

void PianoRoll::zoomToClip()
{
    const auto clip = getClip();
    const auto width = std::max (200, noteGrid->getWidth());

    if (! clip.isValid())
        return;

    // The whole clip with a little room after it.
    const auto length = (double) clip.getLengthTicks() / (double) ticksPerQuarter + 1.0;
    view.setPixelsPerQuarter (juce::jlimit (12.0, 240.0, (double) width / length), 0.0);
    view.setViewStart (std::max<Tick> (0, clip.getStart() - ticksPerQuarter / 2));
}

void PianoRoll::scrollToPitch (int pitch)
{
    scrollY = juce::jlimit (0, std::max (0, 128 * rowHeight - noteGrid->getHeight()),
                            (127 - pitch) * rowHeight - noteGrid->getHeight() / 2);
    updateScrollBars();
    repaint();
}

//==============================================================================
Tick PianoRoll::toTimeline (Tick contentTick) const
{
    const auto clip = getClip();
    return clip.isValid() ? clip.getStart() - clip.getContentStart() + contentTick : contentTick;
}

Tick PianoRoll::toContent (Tick timelineTick) const
{
    const auto clip = getClip();
    return clip.isValid() ? timelineTick - clip.getStart() + clip.getContentStart() : timelineTick;
}

int PianoRoll::pitchToY (int pitch) const   { return (127 - pitch) * rowHeight - scrollY; }

int PianoRoll::yToPitch (int y) const
{
    return 127 - (int) std::floor ((double) (y + scrollY) / (double) rowHeight);
}

juce::Point<int> PianoRoll::positionOf (Tick contentTick, int pitch) const
{
    return { (int) std::lround (view.tickToX (toTimeline (contentTick))), pitchToY (pitch) };
}

Tick PianoRoll::stepAt (Tick timelineTick) const
{
    const auto step = gridStepAt (tempo, timelineTick, grid);
    return step > 0 ? step : ticksPerQuarter / 4;
}

Tick PianoRoll::snapDown (Tick timelineTick, const juce::ModifierKeys& mods) const
{
    if (mods.isAltDown() || grid == GridSize::off)
        return timelineTick;

    const auto barStart = tempo.barToTicks (tempo.ticksToBarBeat (timelineTick).bar);
    const auto step = stepAt (timelineTick);
    return barStart + (timelineTick - barStart) / step * step;
}

Tick PianoRoll::snapNearest (Tick timelineTick, const juce::ModifierKeys& mods) const
{
    return mods.isAltDown() ? timelineTick : snapToGrid (tempo, timelineTick, grid);
}

int PianoRoll::songKey() const      { return juce::jlimit (0, 11, (int) project.getState()[ids::songKey]); }
bool PianoRoll::scaleActive() const { return scales::highlights (scales::find (project.getState()[ids::scale].toString())); }

bool PianoRoll::inScale (int pitch) const
{
    return scales::contains (scales::find (project.getState()[ids::scale].toString()), songKey(), pitch);
}

juce::Colour PianoRoll::noteColour (int velocity, bool selected) const
{
    const auto clip = getClip();
    auto base = clip.isValid() ? Track (clip.getTrackState()).getColour() : theme.accent;
    base = base.withMultipliedBrightness (0.55f + 0.45f * (float) velocity / 127.0f);
    return selected ? base.brighter (0.45f) : base;
}

//==============================================================================
void PianoRoll::setGrid (GridSize size)
{
    grid = size;
    updateToolbar();
    repaint();
}

void PianoRoll::setPreviewEnabled (bool shouldPreview)
{
    previewEnabled = shouldPreview;

    if (! previewEnabled)
        releasePreview();

    updateToolbar();
}

void PianoRoll::setFixedNoteLength (Tick length)
{
    fixedLength = std::max<Tick> (0, length);
    updateToolbar();
}

Tick PianoRoll::getNewNoteLength() const
{
    return fixedLength > 0 ? fixedLength : std::max<Tick> (1, lastLength);
}

void PianoRoll::play (int pitch, int velocity, bool on, bool always)
{
    if (! previewNote || (! previewEnabled && ! always))
        return;

    if (sounding >= 0)
        previewNote (sounding, 0, false);

    sounding = on ? pitch : -1;

    if (on)
        previewNote (pitch, velocity, true);
}

void PianoRoll::releasePreview()
{
    if (sounding >= 0 && previewNote)
        previewNote (sounding, 0, false);

    sounding = -1;
}

//==============================================================================
void PianoRoll::setSelection (NoteNodes notes)
{
    selection = std::move (notes);
    noteGrid->repaint();
    velocityLane->repaint();
}

void PianoRoll::tidySelection()
{
    // Notes that were deleted (or undone) are no longer selected.
    const auto before = selection.size();
    selection.erase (std::remove_if (selection.begin(), selection.end(),
                                     [this] (const juce::ValueTree& n) { return n.getParent() != clipState; }),
                     selection.end());

    if (selection.size() != before)
        velocityLane->repaint();
}

void PianoRoll::selectAll()   { setSelection (notes::nodesOf (getClip())); }

void PianoRoll::deleteSelected()
{
    tidySelection();
    edit::deleteNotes (project, selection);
    setSelection ({});
}

void PianoRoll::duplicateSelected()
{
    tidySelection();
    if (! selection.empty())
        setSelection (edit::duplicateNotes (project, selection, stepAt (toTimeline (0))));
}

void PianoRoll::transposeSelected (int semitones)
{
    tidySelection();
    if (selection.empty())
        return;

    edit::moveNotes (project, selection, 0, semitones);
    play (notes::read (selection.front()).pitch, notes::read (selection.front()).velocity, true);
    juce::Timer::callAfterDelay (250, [safe = juce::Component::SafePointer<PianoRoll> (this)]
    {
        if (safe != nullptr)
            safe->releasePreview();
    });
}

void PianoRoll::nudgeSelected (int gridSteps, bool fine)
{
    tidySelection();
    if (selection.empty())
        return;

    const auto step = fine ? ticksPerQuarter / 16 : stepAt (toTimeline (notes::read (selection.front()).start));
    edit::moveNotes (project, selection, gridSteps * step, 0);
}

void PianoRoll::quantize()
{
    tidySelection();
    const auto targets = selection.empty() ? notes::nodesOf (getClip()) : selection;
    const auto step = stepAt (toTimeline (0));

    // Quantizing is done in the clip's content positions, which line up with the bars when the
    // clip starts on a grid line (as clips normally do).
    edit::quantizeNotes (project, targets, step, quantizeSettings.strength, quantizeSettings.swing, quantizeSettings.ends);
}

void PianoRoll::humanize()
{
    tidySelection();
    const auto targets = selection.empty() ? notes::nodesOf (getClip()) : selection;
    const auto clip = getClip();

    if (! clip.isValid())
        return;

    const auto startSeconds = tempo.ticksToSeconds (clip.getStart());
    const auto timing = tempo.secondsToTicks (startSeconds + humanizeSettings.timingMs / 1000.0) - clip.getStart();
    juce::Random random;
    edit::humanizeNotes (project, targets, timing, humanizeSettings.velocity, random);
}

void PianoRoll::copySelected()
{
    tidySelection();
    if (selection.empty())
        return;

    auto& clipboard = noteClipboard();
    clipboard.clear();
    Tick first = std::numeric_limits<Tick>::max();

    for (const auto& n : selection)
        first = std::min (first, notes::read (n).start);

    for (const auto& n : selection)
    {
        auto data = notes::read (n);
        data.start -= first;
        clipboard.push_back (data);
    }
}

void PianoRoll::cutSelected()
{
    copySelected();
    deleteSelected();
}

void PianoRoll::paste()
{
    const auto clip = getClip();
    auto copies = noteClipboard();

    if (! clip.isValid() || copies.empty())
        return;

    const auto at = std::max<Tick> (0, toContent (engine.getTransportStatus().position));
    for (auto& n : copies)
        n.start += at;

    setSelection (edit::addNotes (project, clip.getId(), copies));
}

//==============================================================================
bool PianoRoll::keyPressed (const juce::KeyPress& key)
{
    const auto mods = key.getModifiers();
    const auto code = key.getKeyCode();
    const auto ctrl = mods.isCommandDown();

    if (! getClip().isValid())
        return false;

    if (code == juce::KeyPress::deleteKey || code == juce::KeyPress::backspaceKey)   { deleteSelected(); return true; }
    if (code == juce::KeyPress::escapeKey)                                            { setSelection ({}); return true; }
    if (code == juce::KeyPress::upKey && ! ctrl)      { transposeSelected (mods.isShiftDown() ? 12 : 1); return true; }
    if (code == juce::KeyPress::downKey && ! ctrl)    { transposeSelected (mods.isShiftDown() ? -12 : -1); return true; }
    if (code == juce::KeyPress::leftKey && ! ctrl)    { nudgeSelected (-1, mods.isShiftDown()); return true; }
    if (code == juce::KeyPress::rightKey && ! ctrl)   { nudgeSelected (1, mods.isShiftDown()); return true; }

    const auto character = juce::CharacterFunctions::toLowerCase (key.getTextCharacter());

    if (ctrl && ! mods.isShiftDown() && ! mods.isAltDown())
    {
        if (code == 'A' || character == 'a') { selectAll(); return true; }
        if (code == 'C' || character == 'c') { copySelected(); return true; }
        if (code == 'X' || character == 'x') { cutSelected(); return true; }
        if (code == 'V' || character == 'v') { paste(); return true; }
        if (code == 'D' || character == 'd') { duplicateSelected(); return true; }
    }

    if (! mods.isAnyModifierKeyDown() && (code == 'Q' || character == 'q'))   { quantize(); return true; }

    return false;   // Space, Ctrl+Z... go on to Aether's shortcuts
}

//==============================================================================
void PianoRoll::showQuantizePanel()
{
    juce::Component::SafePointer<PianoRoll> safe (this);
    auto panel = std::make_unique<SettingsPanel> (
        theme,
        std::vector<SettingsPanel::Row> {
            { "Strength", "100%: exactly on the grid. Less: only part of the way, keeping some of the feel.",
              0.0, 100.0, 1.0, quantizeSettings.strength * 100.0, " %" },
            { "Swing", "Pushes every second grid step later, for a shuffle feel (100%: like triplets).",
              0.0, 100.0, 1.0, quantizeSettings.swing * 100.0, " %" } },
        "Quantize note ends too", quantizeSettings.ends,
        "Quantize",
        [safe] (std::vector<double> values, bool ends)
        {
            if (safe == nullptr)
                return;

            safe->quantizeSettings = { values[0] / 100.0, values[1] / 100.0, ends };
            safe->quantize();
        });

    juce::CallOutBox::launchAsynchronously (std::move (panel), quantizeButton.getScreenBounds(), nullptr);
}

void PianoRoll::showHumanizePanel()
{
    juce::Component::SafePointer<PianoRoll> safe (this);
    auto panel = std::make_unique<SettingsPanel> (
        theme,
        std::vector<SettingsPanel::Row> {
            { "Timing", "How far notes can move earlier or later, at most.", 0.0, 40.0, 1.0, humanizeSettings.timingMs, " ms" },
            { "Velocity", "How much velocities can change, at most.", 0.0, 40.0, 1.0, (double) humanizeSettings.velocity, "" } },
        juce::String(), false,
        "Humanize",
        [safe] (std::vector<double> values, bool)
        {
            if (safe == nullptr)
                return;

            safe->humanizeSettings = { values[0], (int) values[1] };
            safe->humanize();
        });

    juce::CallOutBox::launchAsynchronously (std::move (panel), humanizeButton.getScreenBounds(), nullptr);
}

void PianoRoll::updateToolbar()
{
    const auto clip = getClip();
    titleLabel.setText (clip.isValid() ? clip.getName() + "  ·  " + Track (clip.getTrackState()).getName()
                                       : juce::String ("No clip (double-click a MIDI clip to edit it)"),
                        juce::dontSendNotification);

    for (size_t i = 0; i < allGridSizes.size(); ++i)
        if (allGridSizes[i] == grid)
            gridBox.setSelectedId ((int) i + 1, juce::dontSendNotification);

    lengthBox.clear (juce::dontSendNotification);
    for (int i = 0; i < (int) std::size (lengthChoices); ++i)
        lengthBox.addItem (i == 0 ? "Length: last used (" + describeLength (lastLength) + ")"
                                  : "Length: " + juce::String (lengthChoices[i].name), i + 1);

    for (int i = 0; i < (int) std::size (lengthChoices); ++i)
        if (lengthChoices[i].length == fixedLength)
            lengthBox.setSelectedId (i + 1, juce::dontSendNotification);

    keyBox.setSelectedId (songKey() + 1, juce::dontSendNotification);

    const auto scaleId = scales::find (project.getState()[ids::scale].toString()).id;
    for (int i = 0; i < (int) scales::allScales.size(); ++i)
        if (juce::String (scales::allScales[(size_t) i].id) == scaleId)
            scaleBox.setSelectedId (i + 1, juce::dontSendNotification);

    previewButton.setToggleState (previewEnabled, juce::dontSendNotification);
    previewButton.setButtonText (previewEnabled ? "Sound: on" : "Sound: off");

    for (auto* c : std::initializer_list<juce::Component*> { &gridBox, &lengthBox, &keyBox, &scaleBox, &quantizeButton, &humanizeButton })
        c->setEnabled (clip.isValid());
}

void PianoRoll::updateTempo()
{
    tempo = timeline::buildTempoMap (project);
}

void PianoRoll::updateScrollBars()
{
    const auto clip = getClip();
    Tick end = tempo.barToTicks (8);
    if (clip.isValid())
        end = std::max (end, clip.getStart() + clip.getLengthTicks() + tempo.barToTicks (4));

    const auto visible = (double) view.ticksForWidth (noteGrid->getWidth());
    hScroll.setRangeLimits (0.0, std::max ((double) end, (double) view.getViewStart() + visible), juce::dontSendNotification);
    hScroll.setCurrentRange ((double) view.getViewStart(), visible, juce::dontSendNotification);

    vScroll.setRangeLimits (0.0, (double) (128 * rowHeight), juce::dontSendNotification);
    vScroll.setCurrentRange ((double) scrollY, (double) noteGrid->getHeight(), juce::dontSendNotification);
}

void PianoRoll::scrollBarMoved (juce::ScrollBar* bar, double newRangeStart)
{
    if (bar == &hScroll)
    {
        view.setViewStart ((Tick) newRangeStart);
    }
    else
    {
        scrollY = (int) newRangeStart;
        keys->repaint();
        noteGrid->repaint();
    }
}

void PianoRoll::modelChanged()
{
    // The clip was deleted (or its adding undone): nothing to edit any more.
    if (clipState.isValid() && clipState.getRoot() != project.getState())
    {
        clipState = {};
        selection.clear();
        updateToolbar();

        if (onClipChanged)
            onClipChanged();
    }

    tidySelection();
    updateTempo();

    if (! repaintPending)
    {
        repaintPending = true;
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<PianoRoll> (this)]
        {
            if (safe == nullptr)
                return;

            safe->repaintPending = false;
            safe->updateToolbar();
            safe->updateScrollBars();
            safe->repaint();
        });
    }
}

void PianoRoll::timerCallback()
{
    const auto status = engine.getTransportStatus();

    if (status.position == lastPlayhead)
        return;

    lastPlayhead = status.position;

    // Follow the playhead while playing.
    if (status.playing)
    {
        const auto x = view.tickToX (status.position);
        if (x > noteGrid->getWidth() - 20 || x < 0)
            view.setViewStart (std::max<Tick> (0, status.position - view.ticksForWidth (40)));
    }

    ruler->repaint();
    noteGrid->repaint();
}

//==============================================================================
void PianoRoll::paint (juce::Graphics& g)
{
    g.fillAll (theme.background);

    auto toolbar = getLocalBounds().removeFromTop (toolbarHeight);
    g.setColour (theme.panel);
    g.fillRect (toolbar);
    g.setColour (theme.outline);
    g.drawHorizontalLine (toolbarHeight - 1, 0.0f, (float) getWidth());

    // The corner above the keys, and the label beside the velocity lane.
    g.setColour (theme.panel);
    g.fillRect (0, toolbarHeight, keysWidth, rulerHeight);
    g.fillRect (velocityLane->getX() - keysWidth, velocityLane->getY(), keysWidth, velocityLane->getHeight());
    g.setColour (theme.textDim);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("Velocity", juce::Rectangle<int> (0, velocityLane->getY(), keysWidth, 24), juce::Justification::centred);

    if (! getClip().isValid())
    {
        g.setColour (theme.textDim);
        g.setFont (juce::FontOptions (15.0f));
        g.drawText ("Double-click a MIDI clip on the timeline to edit its notes here.", noteGrid->getBounds(),
                    juce::Justification::centred);
    }
}

void PianoRoll::resized()
{
    auto area = getLocalBounds();
    auto toolbar = area.removeFromTop (toolbarHeight).reduced (8, 7);

    previewButton.setBounds (toolbar.removeFromRight (100));
    toolbar.removeFromRight (6);
    humanizeButton.setBounds (toolbar.removeFromRight (96));
    toolbar.removeFromRight (6);
    quantizeButton.setBounds (toolbar.removeFromRight (90));
    toolbar.removeFromRight (12);
    scaleBox.setBounds (toolbar.removeFromRight (150));
    toolbar.removeFromRight (6);
    keyBox.setBounds (toolbar.removeFromRight (88));
    toolbar.removeFromRight (12);
    lengthBox.setBounds (toolbar.removeFromRight (200));
    toolbar.removeFromRight (6);
    gridBox.setBounds (toolbar.removeFromRight (130));
    toolbar.removeFromRight (8);
    titleLabel.setBounds (toolbar);

    area.removeFromBottom (0);
    auto bottomBar = area.removeFromBottom (scrollBarSize);
    hScroll.setBounds (bottomBar.withTrimmedLeft (keysWidth).withTrimmedRight (scrollBarSize));

    auto velocityRow = area.removeFromBottom (velocityHeight);
    velocityLane->setBounds (velocityRow.withTrimmedLeft (keysWidth).withTrimmedRight (scrollBarSize));

    auto rulerRow = area.removeFromTop (rulerHeight);
    ruler->setBounds (rulerRow.withTrimmedLeft (keysWidth).withTrimmedRight (scrollBarSize));

    vScroll.setBounds (area.removeFromRight (scrollBarSize));
    keys->setBounds (area.removeFromLeft (keysWidth));
    noteGrid->setBounds (area);

    scrollY = juce::jlimit (0, std::max (0, 128 * rowHeight - noteGrid->getHeight()), scrollY);
    updateScrollBars();
}

} // namespace aether::ui
