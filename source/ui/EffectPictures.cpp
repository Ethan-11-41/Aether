// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "EffectPictures.h"

#include <cmath>

#include "dsp/ColourEffects.h"
#include "dsp/Dynamics.h"
#include "dsp/Eq.h"
#include "dsp/SpaceEffects.h"
#include "model/Mixer.h"
#include "model/TimelineEdits.h"

namespace aether::ui
{

namespace
{
    constexpr double pictureSampleRate = 48000.0;   // the pictures don't depend on the device's rate (above 40 kHz)
    constexpr float meterWidth = 40.0f;

    /** Frequencies from 20 Hz (left) to 20 kHz (right), spaced as you hear them. */
    float xForHz (juce::Rectangle<float> area, double hz)
    {
        return area.getX() + (float) (std::log (std::clamp (hz, 20.0, 20000.0) / 20.0) / std::log (1000.0)) * area.getWidth();
    }

    double hzForX (juce::Rectangle<float> area, float x)
    {
        return 20.0 * std::pow (1000.0, std::clamp ((x - area.getX()) / std::max (1.0f, area.getWidth()), 0.0f, 1.0f));
    }

    float yForDb (juce::Rectangle<float> area, double db, double top, double bottom)
    {
        return area.getY() + (float) ((top - std::clamp (db, bottom, top)) / (top - bottom)) * area.getHeight();
    }

    juce::String formatHz (double hz)
    {
        return hz >= 1000.0 ? juce::String (hz / 1000.0, hz >= 10000.0 ? 1 : 2) + " kHz" : juce::String (juce::roundToInt (hz)) + " Hz";
    }

    juce::String formatDb (double db)
    {
        return (db > 0.05 ? "+" : "") + juce::String (db, 1) + " dB";
    }

    /** The frequency grid (with labels) and dB lines shared by the EQ, filter and de-esser. */
    void drawFrequencyGrid (juce::Graphics& g, const Theme& t, juce::Rectangle<float> area, double top, double bottom, double dbStep)
    {
        g.setFont (juce::FontOptions (10.5f));

        for (const auto hz : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 })
        {
            const auto x = xForHz (area, hz);
            g.setColour (t.outline.withAlpha (hz == 100.0 || hz == 1000.0 || hz == 10000.0 ? 0.8f : 0.4f));
            g.drawVerticalLine ((int) x, area.getY(), area.getBottom());
            g.setColour (t.textDim.withAlpha (0.75f));
            g.drawText (hz >= 1000.0 ? juce::String ((int) (hz / 1000.0)) + "k" : juce::String ((int) hz),
                        juce::Rectangle<float> (x + 3.0f, area.getBottom() - 14.0f, 30.0f, 12.0f), juce::Justification::centredLeft);
        }

        for (auto db = std::ceil (bottom / dbStep) * dbStep; db <= top; db += dbStep)
        {
            const auto y = yForDb (area, db, top, bottom);
            g.setColour (t.outline.withAlpha (std::abs (db) < 0.01 ? 0.9f : 0.35f));
            g.drawHorizontalLine ((int) y, area.getX(), area.getRight());

            if (std::abs (db) > 0.01 && y > area.getY() + 8.0f && y < area.getBottom() - 16.0f)
            {
                g.setColour (t.textDim.withAlpha (0.6f));
                g.drawText ((db > 0 ? "+" : "") + juce::String ((int) db), juce::Rectangle<float> (area.getX() + 3.0f, y - 6.0f, 28.0f, 12.0f),
                            juce::Justification::centredLeft);
            }
        }
    }

    /** A curve across the frequency picture, filled down (or up) to the 0 dB line. */
    void drawFrequencyCurve (juce::Graphics& g, juce::Rectangle<float> area, double top, double bottom, juce::Colour colour,
                             const std::function<double (double)>& dbAt)
    {
        juce::Path curve, fill;
        const auto zero = yForDb (area, 0.0, top, bottom);

        for (int i = 0; i <= (int) area.getWidth(); i += 2)
        {
            const auto x = area.getX() + (float) i;
            const auto y = yForDb (area, dbAt (hzForX (area, x)), top, bottom);

            if (i == 0) { curve.startNewSubPath (x, y); fill.startNewSubPath (x, zero); }
            curve.lineTo (x, y);
            fill.lineTo (x, y);
        }

        fill.lineTo (area.getRight(), zero);
        fill.closeSubPath();
        g.setColour (colour.withAlpha (0.16f));
        g.fillPath (fill);
        g.setColour (colour);
        g.strokePath (curve, juce::PathStrokeType (2.0f));
    }

    /** A level-in / level-out square for the compressor and gate: input along the bottom, output
        up the side, both -60 to +6 dB. */
    void drawLevelCurve (juce::Graphics& g, const Theme& t, juce::Rectangle<float> area, juce::Colour colour,
                         const std::function<double (double)>& outputAt)
    {
        constexpr double low = -60.0, high = 6.0;
        auto xFor = [&] (double db) { return area.getX() + (float) ((db - low) / (high - low)) * area.getWidth(); };
        auto yFor = [&] (double db) { return yForDb (area, db, high, low); };

        g.setFont (juce::FontOptions (10.5f));
        for (const auto db : { -48.0, -36.0, -24.0, -12.0, 0.0 })
        {
            g.setColour (t.outline.withAlpha (db == 0.0 ? 0.8f : 0.35f));
            g.drawVerticalLine ((int) xFor (db), area.getY(), area.getBottom());
            g.drawHorizontalLine ((int) yFor (db), area.getX(), area.getRight());
            g.setColour (t.textDim.withAlpha (0.6f));
            g.drawText (juce::String ((int) db), juce::Rectangle<float> (xFor (db) + 2.0f, area.getBottom() - 13.0f, 30.0f, 12.0f),
                        juce::Justification::centredLeft);
        }

        // In = out, for comparison.
        g.setColour (t.textDim.withAlpha (0.35f));
        g.drawLine (xFor (low), yFor (low), xFor (high), yFor (high), 1.0f);

        juce::Path curve;
        for (int i = 0; i <= (int) area.getWidth(); ++i)
        {
            const auto in = low + (high - low) * i / std::max (1.0, (double) area.getWidth());
            const auto point = juce::Point<float> (xFor (in), yFor (outputAt (in)));
            if (i == 0) curve.startNewSubPath (point); else curve.lineTo (point);
        }

        g.setColour (colour);
        g.strokePath (curve, juce::PathStrokeType (2.0f));

        g.setColour (t.textDim);
        g.drawText ("level in", area.reduced (6.0f, 16.0f), juce::Justification::bottomRight);
        g.drawText ("level out", area.reduced (6.0f, 4.0f), juce::Justification::topLeft);
    }
}

//==============================================================================
EffectPicture::EffectPicture (const Theme& t, Project& p, juce::String id)
    : theme (t), project (p), insertId (std::move (id))
{
    type = dsp::findEffectType (insert()[ids::type].toString());
}

juce::ValueTree EffectPicture::insert() const
{
    return mixer::findInsert (project, insertId);
}

float EffectPicture::value (int parameterIndex) const
{
    if (type == nullptr || parameterIndex < 0 || parameterIndex >= (int) type->parameters.size())
        return 0.0f;

    return mixer::getInsertParameter (insert(), type->parameters[(size_t) parameterIndex].id);
}

dsp::EffectParameterValues EffectPicture::values() const
{
    dsp::EffectParameterValues v {};

    if (type != nullptr)
    {
        const auto node = insert();
        for (size_t i = 0; i < type->parameters.size() && i < v.size(); ++i)
            v[i] = mixer::getInsertParameter (node, type->parameters[i].id);
    }

    return v;
}

void EffectPicture::beginGesture (const juce::String& what)
{
    project.getUndoManager().beginNewTransaction (what);
}

void EffectPicture::setValue (int parameterIndex, float newValue)
{
    if (type != nullptr && parameterIndex >= 0 && parameterIndex < (int) type->parameters.size())
        edit::setInsertParameter (project, insertId, type->parameters[(size_t) parameterIndex].id, newValue, false);
}

void EffectPicture::tick()
{
    meterValue = meter != nullptr ? meter() : 0.0f;
    history[historyPosition] = meterValue;
    historyPosition = (historyPosition + 1) % history.size();
    animationSeconds += 1.0 / 30.0;
    repaint();
}

juce::Rectangle<float> EffectPicture::drawBackground (juce::Graphics& g, juce::Rectangle<float> area) const
{
    g.setColour (theme.meterBackground);
    g.fillRoundedRectangle (area, 6.0f);
    return area.reduced (8.0f, 8.0f);
}

void EffectPicture::drawReductionMeter (juce::Graphics& g, juce::Rectangle<float> area, float reductionDb, float range) const
{
    g.setColour (theme.panel);
    g.fillRoundedRectangle (area, 4.0f);

    const auto bar = area.reduced (8.0f, 18.0f);
    const auto amount = std::clamp (reductionDb / std::max (1.0f, range), 0.0f, 1.0f);
    g.setColour (theme.meterBackground);
    g.fillRect (bar);
    g.setColour (theme.warning);
    g.fillRect (bar.withHeight (bar.getHeight() * amount));

    g.setColour (theme.textDim);
    g.setFont (juce::FontOptions (10.0f));
    g.drawText ("0", area.withHeight (16.0f), juce::Justification::centred);
    g.drawText ("-" + juce::String ((int) range), area.withTrimmedTop (area.getHeight() - 16.0f), juce::Justification::centred);

    g.setColour (theme.text);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawFittedText (reductionDb >= 0.05f ? "-" + juce::String (reductionDb, 1) : juce::String ("0"),
                      bar.withHeight (16.0f).translated (0.0f, std::min (bar.getHeight() - 16.0f, bar.getHeight() * amount + 2.0f)).toNearestInt(),
                      juce::Justification::centred, 1);
}

void EffectPicture::drawHistory (juce::Graphics& g, juce::Rectangle<float> area, float range, bool reduction, const juce::String& caption) const
{
    g.setColour (theme.outline.withAlpha (0.35f));
    for (int i = 1; i < 4; ++i)
        g.drawHorizontalLine ((int) (area.getY() + area.getHeight() * (float) i / 4.0f), area.getX(), area.getRight());

    juce::Path line, fill;
    const auto count = history.size();

    for (size_t i = 0; i < count; ++i)
    {
        const auto reading = history[(historyPosition + i) % count];
        const auto amount = std::clamp (reading / std::max (0.001f, range), 0.0f, 1.0f);
        const auto x = area.getX() + area.getWidth() * (float) i / (float) (count - 1);
        const auto y = reduction ? area.getY() + area.getHeight() * amount : area.getBottom() - area.getHeight() * amount;
        const auto base = reduction ? area.getY() : area.getBottom();

        if (i == 0) { line.startNewSubPath (x, y); fill.startNewSubPath (x, base); }
        line.lineTo (x, y);
        fill.lineTo (x, y);

        if (i == count - 1) { fill.lineTo (x, base); fill.closeSubPath(); }
    }

    const auto colour = reduction ? theme.warning : theme.accent;
    g.setColour (colour.withAlpha (0.2f));
    g.fillPath (fill);
    g.setColour (colour);
    g.strokePath (line, juce::PathStrokeType (1.5f));

    g.setColour (theme.textDim);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText (caption, area.reduced (6.0f, 4.0f), reduction ? juce::Justification::bottomLeft : juce::Justification::topLeft);
    g.drawText ("the last 5 seconds", area.reduced (6.0f, 4.0f), reduction ? juce::Justification::bottomRight : juce::Justification::topRight);
}

//==============================================================================
namespace
{

/** The EQ: its curve, with a point for each band to drag (left/right: frequency, up/down: more
    or less; the scroll wheel makes a band wider or narrower; double-click: back to 0 dB). */
class EqPicture final : public EffectPicture
{
public:
    using EffectPicture::EffectPicture;

    void tick() override {}   // nothing live

    void paint (juce::Graphics& g) override
    {
        namespace e = dsp::eq;
        const auto area = drawBackground (g, getLocalBounds().toFloat());
        const auto v = values();
        const auto off = (bool) insert()[ids::bypass];

        drawFrequencyGrid (g, theme, area, top, bottom, 6.0);
        drawFrequencyCurve (g, area, top, bottom, off ? theme.textDim : theme.accent,
                            [&v] (double hz) { return e::responseDb (v, hz, pictureSampleRate); });

        for (int i = 0; i < (int) handles.size(); ++i)
        {
            const auto& h = handles[(size_t) i];
            const auto p = handlePosition (i, area);
            const auto active = h.on < 0 || value (h.on) >= 0.5f;
            const auto big = i == dragging || (dragging < 0 && i == hovering);
            const auto size = big ? 15.0f : 11.0f;
            const auto circle = juce::Rectangle<float> (size, size).withCentre (p);

            if (active)
            {
                g.setColour (h.colour);
                g.fillEllipse (circle);
            }
            g.setColour (active ? theme.meterBackground : h.colour.withAlpha (0.7f));
            g.drawEllipse (circle, 1.5f);

            g.setColour (active ? theme.meterBackground : h.colour);
            g.setFont (juce::FontOptions (9.0f, juce::Font::bold));
            if (big)
                g.drawText (h.shortName, circle, juce::Justification::centred);
        }

        // What the point being dragged (or under the mouse) is set to.
        if (const auto shown = dragging >= 0 ? dragging : hovering; shown >= 0)
        {
            const auto& h = handles[(size_t) shown];
            auto text = h.name + ": " + formatHz (value (h.freq));
            if (h.gain >= 0) text << ", " << formatDb (value (h.gain));
            if (h.q >= 0) text << ", width " << juce::String (value (h.q), 1);
            if (h.on >= 0 && value (h.on) < 0.5f) text << " (off: drag it to switch it on)";

            const auto p = handlePosition (shown, area);
            auto box = juce::Rectangle<float> (std::max (150.0f, (float) text.length() * 6.4f), 20.0f)
                           .withCentre ({ p.x, p.y < area.getCentreY() ? p.y + 24.0f : p.y - 24.0f });
            box = box.constrainedWithin (area);
            g.setColour (theme.panelRaised.withAlpha (0.95f));
            g.fillRoundedRectangle (box, 4.0f);
            g.setColour (theme.text);
            g.setFont (juce::FontOptions (12.0f));
            g.drawText (text, box, juce::Justification::centred);
        }

        if (off)
        {
            g.setColour (theme.warning);
            g.setFont (juce::FontOptions (13.0f));
            g.drawText ("Switched off: you hear the sound without it", area.reduced (34.0f, 6.0f), juce::Justification::topRight);
        }
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const auto found = handleAt (e.position);

        if (found != hovering)
        {
            hovering = found;
            repaint();
        }

        setMouseCursor (found >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
        setTooltip (found < 0 ? "The EQ's curve: low frequencies on the left, high on the right; above the middle line is louder, "
                                "below is quieter. Drag the coloured points."
                  : handles[(size_t) found].gain >= 0
                      ? "Drag left/right for the frequency, up/down to turn it up or down. Scroll to make it narrower or wider. "
                        "Double-click: back to 0 dB."
                      : "Drag left/right to choose where the cut starts. Double-click to switch it on or off.");
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        hovering = -1;
        repaint();
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        dragging = handleAt (e.position);

        if (dragging < 0)
            return;

        const auto& h = handles[(size_t) dragging];
        beginGesture ("Change EQ " + h.name.toLowerCase());

        if (h.on >= 0 && value (h.on) < 0.5f)
            setValue (h.on, 1.0f);   // dragging a cut that's off switches it on

        grabOffset = handlePosition (dragging, innerArea()) - e.position;
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragging < 0)
            return;

        const auto area = innerArea();
        const auto& h = handles[(size_t) dragging];
        const auto p = e.position + grabOffset;
        setValue (h.freq, (float) hzForX (area, p.x));

        if (h.gain >= 0)
        {
            const auto db = top - (double) (p.y - area.getY()) / std::max (1.0f, area.getHeight()) * (top - bottom);
            setValue (h.gain, (float) (std::abs (db) < 0.3 ? 0.0 : db));   // a little pull towards 0 dB
        }
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        dragging = -1;
        repaint();
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const auto found = handleAt (e.position);
        if (found < 0)
            return;

        const auto& h = handles[(size_t) found];

        if (h.gain >= 0)
        {
            beginGesture ("Reset EQ " + h.name.toLowerCase());
            setValue (h.gain, 0.0f);
        }
        else if (h.on >= 0)
        {
            beginGesture ((value (h.on) >= 0.5f ? "Switch off EQ " : "Switch on EQ ") + h.name.toLowerCase());
            setValue (h.on, value (h.on) >= 0.5f ? 0.0f : 1.0f);
        }
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
    {
        const auto found = handleAt (e.position);
        if (found < 0 || handles[(size_t) found].q < 0)
            return;

        // One undo step for a burst of scrolling.
        const auto now = juce::Time::getMillisecondCounterHiRes();
        if (now - lastWheel > 700.0 || wheelHandle != found)
            beginGesture ("Change EQ " + handles[(size_t) found].name.toLowerCase() + " width");

        lastWheel = now;
        wheelHandle = found;
        const auto q = handles[(size_t) found].q;
        setValue (q, value (q) * (float) std::pow (2.0, -(double) wheel.deltaY * 2.0));   // up: wider
    }

    int getNumHandles() const override   { return (int) handles.size(); }
    juce::Point<float> getHandlePosition (int handle) const override   { return handlePosition (handle, innerArea()); }

private:
    struct Handle
    {
        int freq, gain, q, on;
        juce::String name, shortName;
        juce::Colour colour;
    };

    static constexpr double top = 20.0, bottom = -20.0;

    const std::vector<Handle> handles {
        { dsp::eq::lowCut,        -1,                     -1,                  dsp::eq::lowCutOn,  "Low cut",   "LC", juce::Colour (0xff9aa0ad) },
        { dsp::eq::lowShelfFreq,  dsp::eq::lowShelfGain,  -1,                  -1,                 "Bass",      "B",  juce::Colour (0xffef7d57) },
        { dsp::eq::band1Freq,     dsp::eq::band1Gain,     dsp::eq::band1Q,     -1,                 "Low mids",  "1",  juce::Colour (0xffe6c84a) },
        { dsp::eq::band2Freq,     dsp::eq::band2Gain,     dsp::eq::band2Q,     -1,                 "Mids",      "2",  juce::Colour (0xff4cd07d) },
        { dsp::eq::band3Freq,     dsp::eq::band3Gain,     dsp::eq::band3Q,     -1,                 "High mids", "3",  juce::Colour (0xff4fc3c9) },
        { dsp::eq::highShelfFreq, dsp::eq::highShelfGain, -1,                  -1,                 "Treble",    "T",  juce::Colour (0xff7c8cff) },
        { dsp::eq::highCut,       -1,                     -1,                  dsp::eq::highCutOn, "High cut",  "HC", juce::Colour (0xff9aa0ad) },
    };

    juce::Rectangle<float> innerArea() const   { return getLocalBounds().toFloat().reduced (8.0f, 8.0f); }

    juce::Point<float> handlePosition (int index, juce::Rectangle<float> area) const
    {
        const auto& h = handles[(size_t) index];
        const auto hz = (double) value (h.freq);
        const auto db = h.gain >= 0 ? (double) value (h.gain) : -3.0;   // cuts: where they're 3 dB down
        return { xForHz (area, hz), yForDb (area, db, top, bottom) };
    }

    int handleAt (juce::Point<float> position) const
    {
        const auto area = innerArea();
        int best = -1;
        auto bestDistance = 14.0f;

        for (int i = 0; i < (int) handles.size(); ++i)
            if (const auto d = handlePosition (i, area).getDistanceFrom (position); d < bestDistance)
            {
                best = i;
                bestDistance = d;
            }

        return best;
    }

    int dragging = -1, hovering = -1, wheelHandle = -1;
    juce::Point<float> grabOffset;
    double lastWheel = 0.0;
};

//==============================================================================
/** The compressor: level in against level out (the bend is where it starts turning down), and
    how much it's turning down right now. */
class CompressorPicture final : public EffectPicture
{
public:
    CompressorPicture (const Theme& t, Project& p, juce::String id) : EffectPicture (t, p, std::move (id))
    {
        setTooltip ("Left: how loud it comes out for how loud it goes in. Above the threshold (where the line bends), "
                    "the loud parts are turned down. Right: how much it's turning down as the song plays.");
    }

    void paint (juce::Graphics& g) override
    {
        namespace c = dsp::compressor;
        auto area = getLocalBounds().toFloat();
        const auto meterArea = area.removeFromRight (meterWidth);
        area.removeFromRight (6.0f);
        auto curveArea = drawBackground (g, area.removeFromLeft (std::min (area.getHeight(), area.getWidth() * 0.45f)));
        area.removeFromLeft (6.0f);
        const auto historyArea = drawBackground (g, area);

        const auto v = values();
        drawLevelCurve (g, theme, curveArea, theme.accent, [&v] (double in) { return c::outputDb (v, in); });

        // The threshold.
        const auto x = curveArea.getX() + (float) ((v[c::threshold] + 60.0) / 66.0) * curveArea.getWidth();
        g.setColour (theme.warning.withAlpha (0.7f));
        const float dashes[] { 4.0f, 3.0f };
        g.drawDashedLine (juce::Line<float> (x, curveArea.getY(), x, curveArea.getBottom()), dashes, 2, 1.0f);
        g.setFont (juce::FontOptions (11.0f));
        g.drawText ("threshold " + juce::String (juce::roundToInt (v[c::threshold])) + " dB, " + juce::String (v[c::ratio], 1) + ":1",
                    juce::Rectangle<float> (curveArea.getX() + 4.0f, curveArea.getY() + 16.0f, curveArea.getWidth() - 8.0f, 14.0f),
                    juce::Justification::centredLeft);

        drawHistory (g, historyArea, 24.0f, true, "Turning down");
        drawReductionMeter (g, meterArea, meterValue, 24.0f);
    }
};

/** The limiter: how much it's turning down, against the ceiling. */
class LimiterPicture final : public EffectPicture
{
public:
    LimiterPicture (const Theme& t, Project& p, juce::String id) : EffectPicture (t, p, std::move (id))
    {
        setTooltip ("How much the limiter is turning down as the song plays. A little (1 to 3 dB) on the loudest "
                    "moments is normal; if it's turning down all the time, lower the gain.");
    }

    void paint (juce::Graphics& g) override
    {
        namespace l = dsp::limiter;
        auto area = getLocalBounds().toFloat();
        const auto meterArea = area.removeFromRight (meterWidth);
        area.removeFromRight (6.0f);
        const auto historyArea = drawBackground (g, area);

        drawHistory (g, historyArea, 12.0f, true, "Turning down");
        drawReductionMeter (g, meterArea, meterValue, 12.0f);

        g.setColour (theme.text);
        g.setFont (juce::FontOptions (14.0f));
        g.drawText ("Louder by " + formatDb (value (l::gain)) + ", and nothing goes above " + formatDb (value (l::ceiling)),
                    historyArea.reduced (6.0f, 22.0f), juce::Justification::topLeft);
    }
};

/** The gate: level in against level out (below the threshold it's turned down by the range), and
    whether it's open now. */
class GatePicture final : public EffectPicture
{
public:
    GatePicture (const Theme& t, Project& p, juce::String id) : EffectPicture (t, p, std::move (id))
    {
        setTooltip ("Left: below the threshold, the sound is turned down by the range (to silence at -80 dB). "
                    "Right: when the gate is open (the sound comes through).");
    }

    void paint (juce::Graphics& g) override
    {
        namespace gt = dsp::gate;
        auto area = getLocalBounds().toFloat();
        const auto lampArea = area.removeFromRight (meterWidth);
        area.removeFromRight (6.0f);
        auto curveArea = drawBackground (g, area.removeFromLeft (std::min (area.getHeight(), area.getWidth() * 0.45f)));
        area.removeFromLeft (6.0f);
        const auto historyArea = drawBackground (g, area);

        const auto v = values();
        drawLevelCurve (g, theme, curveArea, theme.accent, [&v] (double in) { return gt::outputDb (v, in); });
        drawHistory (g, historyArea, 1.0f, false, "Open");

        g.setColour (theme.panel);
        g.fillRoundedRectangle (lampArea, 4.0f);
        const auto open = meterValue > 0.5f;
        const auto lamp = juce::Rectangle<float> (22.0f, 22.0f).withCentre (lampArea.getCentre().translated (0.0f, -10.0f));
        g.setColour (open ? theme.meterLow : theme.meterBackground);
        g.fillEllipse (lamp);
        g.setColour (theme.textDim);
        g.setFont (juce::FontOptions (10.5f));
        g.drawText (open ? "open" : "closed", lamp.translated (-10.0f, 24.0f).withWidth (42.0f), juce::Justification::centred);
    }
};

/** The de-esser: where it listens for "s" sounds (above the frequency), and how much it's
    turning them down right now. */
class DeesserPicture final : public EffectPicture
{
public:
    DeesserPicture (const Theme& t, Project& p, juce::String id) : EffectPicture (t, p, std::move (id))
    {
        setTooltip ("The shaded part is where \"s\" and \"t\" sounds are. When they're louder than the threshold, "
                    "that part is turned down (the line dips, up to the amount).");
    }

    void paint (juce::Graphics& g) override
    {
        namespace d = dsp::deesser;
        auto area = getLocalBounds().toFloat();
        const auto meterArea = area.removeFromRight (meterWidth);
        area.removeFromRight (6.0f);
        const auto inner = drawBackground (g, area);
        constexpr double top = 6.0, bottom = -24.0;

        drawFrequencyGrid (g, theme, inner, top, bottom, 6.0);

        const auto frequency = (double) value (d::frequency);
        const auto from = xForHz (inner, frequency);
        g.setColour (theme.warning.withAlpha (0.08f));
        g.fillRect (juce::Rectangle<float> (from, inner.getY(), inner.getRight() - from, inner.getHeight()));

        // The most it can turn down (dashed), and what it's doing now.
        const auto most = dsp::BiquadCoefficients::highShelf (frequency * 0.8, -value (d::amount), pictureSampleRate);
        juce::Path dashed;
        for (int i = 0; i <= (int) inner.getWidth(); i += 3)
        {
            const auto x = inner.getX() + (float) i;
            const auto y = yForDb (inner, most.magnitudeDb (hzForX (inner, x), pictureSampleRate), top, bottom);
            if (i == 0) dashed.startNewSubPath (x, y); else dashed.lineTo (x, y);
        }
        juce::Path dashedStroke;
        const float dashes[] { 4.0f, 4.0f };
        juce::PathStrokeType (1.0f).createDashedStroke (dashedStroke, dashed, dashes, 2);
        g.setColour (theme.warning.withAlpha (0.5f));
        g.fillPath (dashedStroke);

        const auto now = dsp::BiquadCoefficients::highShelf (frequency * 0.8, -meterValue, pictureSampleRate);
        drawFrequencyCurve (g, inner, top, bottom, theme.accent, [&now] (double hz) { return now.magnitudeDb (hz, pictureSampleRate); });

        drawReductionMeter (g, meterArea, meterValue, std::max (6.0f, value (d::amount)));
    }
};

//==============================================================================
/** Saturation: what each sample becomes (the bend is the warmth), and a wave before and after. */
class SaturationPicture final : public EffectPicture
{
public:
    SaturationPicture (const Theme& t, Project& p, juce::String id) : EffectPicture (t, p, std::move (id))
    {
        setTooltip ("Left: the more the line bends, the more the loud parts are rounded off (warmth, then grit). "
                    "Right: a wave going in (grey) and coming out (coloured).");
    }

    void tick() override {}

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();
        auto curveArea = drawBackground (g, area.removeFromLeft (std::min (area.getHeight(), area.getWidth() * 0.4f)));
        area.removeFromLeft (6.0f);
        const auto waveArea = drawBackground (g, area);
        const auto v = values();

        g.setColour (theme.outline.withAlpha (0.6f));
        g.drawHorizontalLine ((int) curveArea.getCentreY(), curveArea.getX(), curveArea.getRight());
        g.drawVerticalLine ((int) curveArea.getCentreX(), curveArea.getY(), curveArea.getBottom());
        g.setColour (theme.textDim.withAlpha (0.35f));
        g.drawLine (curveArea.getX(), curveArea.getBottom(), curveArea.getRight(), curveArea.getY(), 1.0f);

        juce::Path curve;
        for (int i = 0; i <= (int) curveArea.getWidth(); ++i)
        {
            const auto in = -1.0f + 2.0f * (float) i / std::max (1.0f, curveArea.getWidth());
            const auto out = std::clamp (dsp::saturation::shape (v, in), -1.1f, 1.1f);
            const auto point = juce::Point<float> (curveArea.getX() + (float) i, curveArea.getCentreY() - out * curveArea.getHeight() * 0.45f);
            if (i == 0) curve.startNewSubPath (point); else curve.lineTo (point);
        }
        g.setColour (theme.accent);
        g.strokePath (curve, juce::PathStrokeType (2.0f));

        // Two cycles of a loud wave, before and after.
        juce::Path before, after;
        for (int i = 0; i <= (int) waveArea.getWidth(); ++i)
        {
            const auto phase = juce::MathConstants<float>::twoPi * 2.0f * (float) i / std::max (1.0f, waveArea.getWidth());
            const auto in = 0.9f * std::sin (phase);
            const auto x = waveArea.getX() + (float) i;
            const auto yIn = waveArea.getCentreY() - in * waveArea.getHeight() * 0.42f;
            const auto yOut = waveArea.getCentreY() - std::clamp (dsp::saturation::shape (v, in), -1.1f, 1.1f) * waveArea.getHeight() * 0.42f;
            if (i == 0) { before.startNewSubPath (x, yIn); after.startNewSubPath (x, yOut); }
            else        { before.lineTo (x, yIn); after.lineTo (x, yOut); }
        }
        g.setColour (theme.textDim.withAlpha (0.5f));
        g.strokePath (before, juce::PathStrokeType (1.5f));
        g.setColour (theme.accent);
        g.strokePath (after, juce::PathStrokeType (2.0f));
    }
};

/** The filter: which frequencies it lets through, sweeping by itself when the sweep is up. */
class FilterPicture final : public EffectPicture
{
public:
    FilterPicture (const Theme& t, Project& p, juce::String id) : EffectPicture (t, p, std::move (id))
    {
        setTooltip ("Which frequencies come through: low on the left, high on the right. With the sweep turned up, "
                    "the cutoff moves by itself (the shaded part is how far).");
    }

    void paint (juce::Graphics& g) override
    {
        namespace f = dsp::filter;
        const auto inner = drawBackground (g, getLocalBounds().toFloat());
        constexpr double top = 24.0, bottom = -36.0;
        const auto v = values();
        drawFrequencyGrid (g, theme, inner, top, bottom, 12.0);

        const auto cutoff = (double) v[f::cutoff];
        const auto sweepOctaves = v[f::lfoDepth] / 100.0 * 3.0;
        auto now = cutoff;

        if (sweepOctaves > 0.0)
        {
            const auto low = xForHz (inner, cutoff * std::pow (2.0, -sweepOctaves));
            const auto high = xForHz (inner, cutoff * std::pow (2.0, sweepOctaves));
            g.setColour (theme.accent.withAlpha (0.07f));
            g.fillRect (juce::Rectangle<float> (low, inner.getY(), high - low, inner.getHeight()));

            const auto phase = std::fmod (animationSeconds * std::clamp ((double) v[f::lfoRate], 0.01, 20.0), 1.0);
            now = cutoff * std::pow (2.0, sweepOctaves * std::sin (juce::MathConstants<double>::twoPi * phase));
        }

        drawFrequencyCurve (g, inner, top, bottom, theme.accent,
                            [&v, now] (double hz) { return f::responseDb (v, now, hz, pictureSampleRate); });

        g.setColour (theme.accent);
        const auto x = xForHz (inner, now);
        const auto y = yForDb (inner, f::responseDb (v, now, now, pictureSampleRate), top, bottom);
        g.fillEllipse (x - 5.0f, y - 5.0f, 10.0f, 10.0f);
    }

    void tick() override
    {
        // Only moves when it's sweeping.
        if (value (dsp::filter::lfoDepth) > 0.0f)
            EffectPicture::tick();
    }
};

/** The chorus: the copy's delay, wavering over time (left and right a little apart). */
class ChorusPicture final : public EffectPicture
{
public:
    ChorusPicture (const Theme& t, Project& p, juce::String id) : EffectPicture (t, p, std::move (id))
    {
        setTooltip ("A copy of the sound is mixed in a few milliseconds late, and how late wavers up and down "
                    "(rate: how fast, depth: how far). Left and right waver apart, which makes it wider.");
    }

    void paint (juce::Graphics& g) override
    {
        namespace c = dsp::chorus;
        const auto inner = drawBackground (g, getLocalBounds().toFloat());
        const auto v = values();
        const auto base = (double) v[c::delay];
        const auto swing = v[c::depth] / 100.0 * base * 0.5;
        const auto rate = std::clamp ((double) v[c::rate], 0.01, 20.0);
        const auto spread = v[c::width] / 100.0;
        constexpr double seconds = 4.0, maxMs = 45.0;

        auto yFor = [&] (double ms) { return inner.getBottom() - (float) (ms / maxMs) * inner.getHeight(); };

        g.setFont (juce::FontOptions (10.5f));
        for (const auto ms : { 10.0, 20.0, 30.0, 40.0 })
        {
            g.setColour (theme.outline.withAlpha (0.4f));
            g.drawHorizontalLine ((int) yFor (ms), inner.getX(), inner.getRight());
            g.setColour (theme.textDim.withAlpha (0.6f));
            g.drawText (juce::String ((int) ms) + " ms", juce::Rectangle<float> (inner.getX() + 3.0f, yFor (ms) - 13.0f, 50.0f, 12.0f),
                        juce::Justification::centredLeft);
        }

        for (const auto side : { 0, 1 })
        {
            juce::Path line;
            for (int i = 0; i <= (int) inner.getWidth(); i += 2)
            {
                const auto time = animationSeconds + seconds * i / std::max (1.0, (double) inner.getWidth());
                const auto phase = rate * time + (side == 1 ? 0.25 * spread : 0.0);
                const auto ms = base + swing * std::sin (juce::MathConstants<double>::twoPi * phase);
                const auto point = juce::Point<float> (inner.getX() + (float) i, yFor (ms));
                if (i == 0) line.startNewSubPath (point); else line.lineTo (point);
            }

            g.setColour (side == 0 ? theme.accent : theme.meterMid);
            g.strokePath (line, juce::PathStrokeType (2.0f));
        }

        g.setColour (theme.text);
        g.setFont (juce::FontOptions (13.0f));
        g.drawText ("The copy is " + juce::String (base - swing, 1) + " to " + juce::String (base + swing, 1) + " ms late, wavering "
                        + juce::String (rate, rate < 1.0 ? 2 : 1) + " times a second",
                    inner.reduced (6.0f, 4.0f), juce::Justification::topRight);
        g.setColour (theme.accent);
        g.setFont (juce::FontOptions (11.0f));
        g.drawText ("left", inner.reduced (6.0f, 22.0f), juce::Justification::topRight);
        g.setColour (theme.meterMid);
        g.drawText ("right", inner.reduced (6.0f, 22.0f).withTrimmedRight (34.0f), juce::Justification::topRight);
    }
};

/** The delay: the sound, then its echoes in time with the tempo, each quieter (ping-pong: left,
    right, left...). */
class DelayPicture final : public EffectPicture
{
public:
    DelayPicture (const Theme& t, Project& p, juce::String id) : EffectPicture (t, p, std::move (id))
    {
        setTooltip ("The sound (white), then its echoes, each quieter by the feedback. With ping-pong on, "
                    "they bounce between left (top) and right (bottom).");
    }

    void tick() override {}

    void paint (juce::Graphics& g) override
    {
        namespace d = dsp::echo;
        const auto inner = drawBackground (g, getLocalBounds().toFloat()).withTrimmedBottom (14.0f);
        const auto v = values();
        const auto choice = std::clamp ((int) std::lround (v[d::time]), 0, (int) dsp::delay::timeChoicesInQuarters.size() - 1);
        const auto bpm = timeline::buildTempoMap (project).tempoAt (0);
        const auto echoSeconds = dsp::delay::timeChoicesInQuarters[(size_t) choice] * 60.0 / std::max (1.0, bpm);
        const auto shown = std::clamp (echoSeconds * 9.0, 1.0, 8.0);
        const auto feedback = v[d::feedback] / 100.0;
        const auto pingPong = v[d::pingPong] >= 0.5f;
        const auto middle = inner.getCentreY();
        auto xFor = [&] (double s) { return inner.getX() + 10.0f + (float) (s / shown) * (inner.getWidth() - 20.0f); };

        // Beats along the bottom.
        g.setFont (juce::FontOptions (10.5f));
        const auto beat = 60.0 / std::max (1.0, bpm);
        for (int b = 0; b * beat <= shown; ++b)
        {
            g.setColour (theme.outline.withAlpha (b % 4 == 0 ? 0.8f : 0.4f));
            g.drawVerticalLine ((int) xFor (b * beat), inner.getY(), inner.getBottom());
            g.setColour (theme.textDim.withAlpha (0.6f));
            g.drawText ("beat " + juce::String (b + 1), juce::Rectangle<float> (xFor (b * beat) + 2.0f, inner.getBottom(), 50.0f, 14.0f),
                        juce::Justification::centredLeft);
        }

        g.setColour (theme.outline.withAlpha (0.6f));
        g.drawHorizontalLine ((int) middle, inner.getX(), inner.getRight());

        auto drawHit = [&] (double seconds, double level, int side, juce::Colour colour)   // side: 0 both, 1 left, 2 right
        {
            // Heights by loudness (as you hear them): 0 dB is full height, -48 dB nothing.
            const auto x = xFor (seconds);
            const auto h = (float) std::clamp (1.0 + 20.0 * std::log10 (std::max (level, 1.0e-6)) / 48.0, 0.0, 1.0) * inner.getHeight() * 0.46f;
            g.setColour (colour);
            if (side != 2) g.fillRoundedRectangle (x - 3.0f, middle - h, 6.0f, h, 2.0f);
            if (side != 1) g.fillRoundedRectangle (x - 3.0f, middle, 6.0f, h, 2.0f);
        };

        drawHit (0.0, 1.0, 0, theme.text);

        auto level = 0.5 + 0.5 * v[d::mix] / 100.0;
        for (int n = 1; n < 64 && level > 0.004 && n * echoSeconds <= shown; ++n)
        {
            drawHit (n * echoSeconds, level, pingPong ? (n % 2 == 1 ? 1 : 2) : 0, theme.accent.withAlpha ((float) std::clamp (0.4 + level, 0.4, 1.0)));
            level *= feedback;
        }

        g.setColour (theme.text);
        g.setFont (juce::FontOptions (13.0f));
        g.drawText ("Every " + type->parameters[(size_t) d::time].choices[choice] + " note: " + juce::String (juce::roundToInt (echoSeconds * 1000.0))
                        + " ms at " + juce::String (bpm, 0) + " BPM",
                    inner.reduced (6.0f, 4.0f), juce::Justification::topRight);
    }
};

/** The reverb: the sound, a short gap (pre-delay), then the room's tail dying away. */
class ReverbPicture final : public EffectPicture
{
public:
    ReverbPicture (const Theme& t, Project& p, juce::String id) : EffectPicture (t, p, std::move (id))
    {
        setTooltip ("The sound (white), then the room answering after the pre-delay, dying away over the time shown. "
                    "Bigger rooms take longer; damping makes the tail darker.");
    }

    void tick() override {}

    void paint (juce::Graphics& g) override
    {
        namespace r = dsp::reverb;
        const auto inner = drawBackground (g, getLocalBounds().toFloat()).withTrimmedBottom (14.0f);
        const auto v = values();
        const auto decay = r::decaySeconds (v);
        const auto predelay = v[r::predelay] / 1000.0;
        const auto shown = std::max (1.0, std::ceil (decay + predelay + 0.3));
        const auto middle = inner.getCentreY();
        const auto height = inner.getHeight() * 0.46f;
        auto xFor = [&] (double s) { return inner.getX() + 10.0f + (float) (s / shown) * (inner.getWidth() - 20.0f); };

        g.setFont (juce::FontOptions (10.5f));
        for (int s = 0; s <= (int) shown; ++s)
        {
            g.setColour (theme.outline.withAlpha (0.5f));
            g.drawVerticalLine ((int) xFor (s), inner.getY(), inner.getBottom());
            g.setColour (theme.textDim.withAlpha (0.6f));
            g.drawText (juce::String (s) + " s", juce::Rectangle<float> (xFor (s) + 2.0f, inner.getBottom(), 40.0f, 14.0f),
                        juce::Justification::centredLeft);
        }

        // The tail: many small reflections under an envelope falling 60 dB over the decay time,
        // drawn by loudness (as you hear it), in a fixed pattern so it doesn't flicker. Damping
        // darkens its colour.
        const auto level = 0.35f + 0.65f * v[r::mix] / 100.0f;
        const auto colour = theme.accent.interpolatedWith (theme.meterMid, v[r::damping] / 200.0f);
        juce::Random random (1234);

        for (float x = xFor (predelay); x < inner.getRight() - 10.0f; x += 2.0f)
        {
            const auto seconds = (double) (x - inner.getX() - 10.0f) / (inner.getWidth() - 20.0f) * shown - predelay;
            const auto envelope = (float) std::max (0.0, 1.0 - seconds / decay) * std::min (1.0f, (float) seconds * 60.0f + 0.2f);
            const auto h = envelope * level * height * (0.45f + 0.55f * random.nextFloat());

            if (h < 0.5f)
                break;

            g.setColour (colour.withAlpha (0.75f));
            g.fillRect (x, middle - h, 1.4f, h * 2.0f);
        }

        g.setColour (theme.text);
        g.fillRoundedRectangle (xFor (0.0) - 3.0f, middle - height, 6.0f, height * 2.0f, 2.0f);

        g.setFont (juce::FontOptions (13.0f));
        g.drawText ("Dies away in " + juce::String (decay, 1) + " s" + (predelay >= 0.0005 ? ", starting " + juce::String (juce::roundToInt (predelay * 1000.0)) + " ms after the sound" : juce::String()),
                    inner.reduced (6.0f, 4.0f), juce::Justification::topRight);
    }
};

/** Effects without a picture of their own: what they do, in words. */
class PlainPicture final : public EffectPicture
{
public:
    using EffectPicture::EffectPicture;

    void tick() override {}

    void paint (juce::Graphics& g) override
    {
        const auto inner = drawBackground (g, getLocalBounds().toFloat());
        g.setColour (theme.textDim);
        g.setFont (juce::FontOptions (14.0f));
        g.drawFittedText (type != nullptr ? type->description : juce::String ("This effect isn't known to this version of Aether."),
                          inner.reduced (12.0f).toNearestInt(), juce::Justification::centred, 3);
    }
};

} // namespace

std::unique_ptr<EffectPicture> createEffectPicture (const Theme& theme, Project& project, const juce::String& insertId)
{
    const auto typeId = mixer::findInsert (project, insertId)[ids::type].toString();

    if (typeId == dsp::eq::typeId)          return std::make_unique<EqPicture> (theme, project, insertId);
    if (typeId == dsp::compressor::typeId)  return std::make_unique<CompressorPicture> (theme, project, insertId);
    if (typeId == dsp::limiter::typeId)     return std::make_unique<LimiterPicture> (theme, project, insertId);
    if (typeId == dsp::gate::typeId)        return std::make_unique<GatePicture> (theme, project, insertId);
    if (typeId == dsp::deesser::typeId)     return std::make_unique<DeesserPicture> (theme, project, insertId);
    if (typeId == dsp::saturation::typeId)  return std::make_unique<SaturationPicture> (theme, project, insertId);
    if (typeId == dsp::filter::typeId)      return std::make_unique<FilterPicture> (theme, project, insertId);
    if (typeId == dsp::chorus::typeId)      return std::make_unique<ChorusPicture> (theme, project, insertId);
    if (typeId == dsp::echo::typeId)        return std::make_unique<DelayPicture> (theme, project, insertId);
    if (typeId == dsp::reverb::typeId)      return std::make_unique<ReverbPicture> (theme, project, insertId);

    return std::make_unique<PlainPicture> (theme, project, insertId);
}

} // namespace aether::ui
