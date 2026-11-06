// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "IconButton.h"

namespace aether::ui
{

IconButton::IconButton (const Theme& themeToUse, Icon iconToShow, const juce::String& name)
    : juce::Button (name), theme (themeToUse), icon (iconToShow)
{
    setWantsKeyboardFocus (false);
}

void IconButton::setAlternateIcon (bool useAlternate)
{
    if (alternate != useAlternate)
    {
        alternate = useAlternate;
        repaint();
    }
}

juce::Path IconButton::makeIcon (Icon icon, juce::Rectangle<float> r, bool alt)
{
    juce::Path p;
    const auto x = r.getX(), y = r.getY(), w = r.getWidth(), h = r.getHeight();

    switch (icon)
    {
        case Icon::play:
            if (alt)
            {
                // Pause: two bars.
                p.addRoundedRectangle (x + w * 0.18f, y + h * 0.1f, w * 0.22f, h * 0.8f, 1.5f);
                p.addRoundedRectangle (x + w * 0.60f, y + h * 0.1f, w * 0.22f, h * 0.8f, 1.5f);
            }
            else
            {
                p.addTriangle (x + w * 0.2f, y + h * 0.08f, x + w * 0.2f, y + h * 0.92f, x + w * 0.92f, y + h * 0.5f);
                p = p.createPathWithRoundedCorners (2.0f);
            }
            break;

        case Icon::stop:
            p.addRoundedRectangle (x + w * 0.15f, y + h * 0.15f, w * 0.7f, h * 0.7f, 2.0f);
            break;

        case Icon::toStart:
            p.addRoundedRectangle (x + w * 0.1f, y + h * 0.12f, w * 0.14f, h * 0.76f, 1.0f);
            p.addTriangle (x + w * 0.9f, y + h * 0.12f, x + w * 0.9f, y + h * 0.88f, x + w * 0.28f, y + h * 0.5f);
            break;

        case Icon::loop:
        {
            // Two curved arrows chasing each other.
            juce::Path arc;
            const auto c = r.getCentre();
            const auto radius = std::min (w, h) * 0.36f;
            arc.addCentredArc (c.x, c.y, radius, radius, 0.0f, 0.35f, 2.75f, true);
            arc.addCentredArc (c.x, c.y, radius, radius, 0.0f, 3.5f, 5.9f, true);
            juce::PathStrokeType (std::max (1.5f, w * 0.11f)).createStrokedPath (p, arc);

            const auto arrow = w * 0.2f;
            const auto a1 = c.getPointOnCircumference (radius, 2.75f);
            const auto a2 = c.getPointOnCircumference (radius, 5.9f);
            p.addTriangle (a1.x - arrow * 0.6f, a1.y - arrow * 0.3f, a1.x + arrow * 0.5f, a1.y - arrow * 0.7f, a1.x + arrow * 0.2f, a1.y + arrow * 0.6f);
            p.addTriangle (a2.x + arrow * 0.6f, a2.y + arrow * 0.3f, a2.x - arrow * 0.5f, a2.y + arrow * 0.7f, a2.x - arrow * 0.2f, a2.y - arrow * 0.6f);
            break;
        }

        case Icon::metronome:
        {
            // A metronome: a tapering body with its pendulum.
            juce::Path body;
            body.startNewSubPath (x + w * 0.32f, y + h * 0.08f);
            body.lineTo (x + w * 0.68f, y + h * 0.08f);
            body.lineTo (x + w * 0.88f, y + h * 0.92f);
            body.lineTo (x + w * 0.12f, y + h * 0.92f);
            body.closeSubPath();
            juce::PathStrokeType (std::max (1.3f, w * 0.09f)).createStrokedPath (p, body);

            juce::Path arm, armStroke;
            arm.startNewSubPath (x + w * 0.5f, y + h * 0.78f);
            arm.lineTo (x + w * 0.74f, y + h * 0.22f);
            juce::PathStrokeType (std::max (1.3f, w * 0.09f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded)
                .createStrokedPath (armStroke, arm);
            p.addPath (armStroke);
            break;
        }

        case Icon::record:
            p.addEllipse (r.reduced (w * 0.12f));
            break;
    }

    return p;
}

void IconButton::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu() && onRightClick)
    {
        onRightClick();
        return;
    }

    juce::Button::mouseDown (e);
}

void IconButton::paintButton (juce::Graphics& g, bool isMouseOver, bool isButtonDown)
{
    auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    const auto on = getToggleState();

    auto background = on ? theme.accent : theme.panelRaised;
    if (! isEnabled())     background = background.withMultipliedAlpha (0.5f);
    else if (isButtonDown) background = background.darker (0.15f);
    else if (isMouseOver)  background = background.brighter (0.08f);

    g.setColour (background);
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (on ? background : theme.outline);
    g.drawRoundedRectangle (bounds, 6.0f, 1.0f);

    const auto iconSize = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto iconArea = juce::Rectangle<float> (iconSize, iconSize).withCentre (bounds.getCentre());
    auto colour = on ? theme.accentText : theme.text;

    if (icon == Icon::record && ! on)
        colour = theme.meterHigh;

    g.setColour (isEnabled() ? colour : colour.withMultipliedAlpha (0.4f));
    g.fillPath (makeIcon (icon, iconArea, alternate));
}

} // namespace aether::ui
