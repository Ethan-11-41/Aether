// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "MixerControls.h"

#include "AutomationFollow.h"

#include <cmath>

#include "model/Mixer.h"

namespace aether::ui
{

//==============================================================================
void DragControl::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
        return;

    startProportion = valueToProportion (currentValue());
    dragging = true;

    if (onGestureStart)
        onGestureStart();
}

void DragControl::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging)
        return;

    const auto fine = e.mods.isShiftDown() ? 0.1 : 1.0;
    const auto movement = (e.position - e.mouseDownPosition) * (float) fine;
    const auto proportion = juce::jlimit (0.0, 1.0, startProportion + dragDistance (movement));

    if (onValueChange)
        onValueChange (proportionToValue (proportion));

    repaint();
}

void DragControl::mouseUp (const juce::MouseEvent&)
{
    if (! dragging)
        return;

    dragging = false;

    if (onGestureEnd)
        onGestureEnd();
}

void DragControl::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
        return;

    if (onGestureStart) onGestureStart();
    if (onValueChange)  onValueChange (resetValue);
    if (onGestureEnd)   onGestureEnd();
    repaint();
}

//==============================================================================
VolumeFader::VolumeFader (const Theme& t, bool isVertical)
    : DragControl (t), vertical (isVertical)
{
    resetValue = 0.0;
    setTooltip ("Volume. Drag to change (hold Shift for fine changes). Double-click for 0 dB.");
}

double VolumeFader::valueToProportion (double value) const   { return mixer::volumeToFaderPosition (value); }
double VolumeFader::proportionToValue (double proportion) const   { return mixer::faderPositionToVolume (proportion); }

juce::Rectangle<float> VolumeFader::getTrack() const
{
    const auto bounds = getLocalBounds().toFloat();
    return vertical ? bounds.reduced (0.0f, 8.0f) : bounds.reduced (6.0f, 0.0f);
}

double VolumeFader::dragDistance (juce::Point<float> movement) const
{
    const auto track = getTrack();
    return vertical ? -movement.y / std::max (1.0f, track.getHeight())
                    : movement.x / std::max (1.0f, track.getWidth());
}

void VolumeFader::paint (juce::Graphics& g)
{
    const auto track = getTrack();
    const auto proportion = (float) valueToProportion (currentValue());

    if (vertical)
    {
        const auto x = track.getCentreX();

        // Scale marks.
        g.setFont (juce::FontOptions (9.0f));
        for (const auto db : { 6.0, 0.0, -6.0, -12.0, -24.0, -48.0 })
        {
            const auto y = track.getBottom() - (float) valueToProportion (db) * track.getHeight();
            g.setColour (theme.outline.brighter (db == 0.0 ? 0.6f : 0.1f));
            g.drawHorizontalLine ((int) y, track.getX(), track.getX() + 5.0f);
            g.drawHorizontalLine ((int) y, track.getRight() - 5.0f, track.getRight());
        }

        g.setColour (theme.meterBackground);
        g.fillRoundedRectangle (juce::Rectangle<float> (x - 2.0f, track.getY(), 4.0f, track.getHeight()), 2.0f);

        const auto thumbY = track.getBottom() - proportion * track.getHeight();
        g.setColour (theme.accent.withAlpha (0.5f));
        g.fillRect (juce::Rectangle<float> (x - 2.0f, thumbY, 4.0f, track.getBottom() - thumbY));

        auto thumb = juce::Rectangle<float> (std::min (26.0f, track.getWidth()), 14.0f).withCentre ({ x, thumbY });
        g.setColour (theme.panelRaised.brighter (0.25f));
        g.fillRoundedRectangle (thumb, 3.0f);
        g.setColour (theme.text);
        g.drawHorizontalLine ((int) thumbY, thumb.getX() + 3.0f, thumb.getRight() - 3.0f);
    }
    else
    {
        const auto y = track.getCentreY();
        g.setColour (theme.meterBackground);
        g.fillRoundedRectangle (juce::Rectangle<float> (track.getX(), y - 2.0f, track.getWidth(), 4.0f), 2.0f);

        // A small mark at 0 dB.
        const auto zero = (float) valueToProportion (0.0);
        g.setColour (theme.outline.brighter (0.5f));
        g.drawVerticalLine ((int) (track.getX() + zero * track.getWidth()), y - 5.0f, y + 5.0f);

        const auto thumbX = track.getX() + proportion * track.getWidth();
        g.setColour (theme.accent.withAlpha (0.5f));
        g.fillRect (juce::Rectangle<float> (track.getX(), y - 2.0f, thumbX - track.getX(), 4.0f));

        auto thumb = juce::Rectangle<float> (8.0f, std::min (14.0f, (float) getHeight())).withCentre ({ thumbX, y });
        g.setColour (theme.panelRaised.brighter (0.35f));
        g.fillRoundedRectangle (thumb, 2.0f);
    }
}

//==============================================================================
PanKnob::PanKnob (const Theme& t) : DragControl (t)
{
    resetValue = 0.0;
    setTooltip ("Pan: where the sound sits between the left and right speakers. Drag to change; double-click to centre.");
}

double PanKnob::dragDistance (juce::Point<float> movement) const
{
    return (movement.x - movement.y) / 120.0;
}

void PanKnob::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto size = std::min (bounds.getWidth(), bounds.getHeight()) - 2.0f;
    const auto circle = juce::Rectangle<float> (size, size).withCentre (bounds.getCentre());
    const auto value = juce::jlimit (-1.0, 1.0, currentValue());

    constexpr auto range = juce::MathConstants<float>::pi * 0.75f;   // ±135 degrees
    const auto angle = (float) value * range;

    g.setColour (theme.panelRaised);
    g.fillEllipse (circle);

    juce::Path track;
    track.addCentredArc (circle.getCentreX(), circle.getCentreY(), size * 0.5f - 1.5f, size * 0.5f - 1.5f, 0.0f, -range, range, true);
    g.setColour (theme.meterBackground);
    g.strokePath (track, juce::PathStrokeType (2.5f));

    if (std::abs (value) > 0.005)
    {
        juce::Path amount;
        amount.addCentredArc (circle.getCentreX(), circle.getCentreY(), size * 0.5f - 1.5f, size * 0.5f - 1.5f, 0.0f,
                              std::min (0.0f, angle), std::max (0.0f, angle), true);
        g.setColour (theme.accent);
        g.strokePath (amount, juce::PathStrokeType (2.5f));
    }

    const auto pointer = circle.getCentre() + juce::Point<float> (std::sin (angle), -std::cos (angle)) * (size * 0.5f - 4.0f);
    g.setColour (theme.text);
    g.drawLine ({ circle.getCentre(), pointer }, 1.6f);
}

//==============================================================================
ToggleBox::ToggleBox (const Theme& t, juce::String label, juce::Colour colourWhenOn)
    : theme (t), text (std::move (label)), onColour (colourWhenOn)
{
}

void ToggleBox::paint (juce::Graphics& g)
{
    const auto on = isOn && isOn();
    const auto area = getLocalBounds().toFloat().reduced (0.5f);

    g.setColour (on ? onColour : theme.panelRaised);
    g.fillRoundedRectangle (area, 3.0f);

    if (! on)
    {
        g.setColour (theme.outline);
        g.drawRoundedRectangle (area, 3.0f, 1.0f);
    }

    g.setColour (on ? theme.accentText : theme.textDim);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText (text, getLocalBounds(), juce::Justification::centred);
}

void ToggleBox::mouseDown (const juce::MouseEvent& e)
{
    if (! e.mods.isPopupMenu() && onClick)
        onClick();

    repaint();
}

//==============================================================================
StereoMeter::StereoMeter (const Theme& t, bool isVertical) : theme (t), vertical (isVertical)
{
    setInterceptsMouseClicks (false, false);
}

float StereoMeter::dbToProportion (float decibels) noexcept
{
    return juce::jlimit (0.0f, 1.0f, (decibels - minimumDb) / (maximumDb - minimumDb));
}

void StereoMeter::pushLevels (float left, float right, float elapsedSeconds)
{
    const std::array<float, 2> levels { left, right };
    constexpr float fallDbPerSecond = 24.0f;
    bool changed = false;

    for (size_t side = 0; side < 2; ++side)
    {
        const auto db = juce::jmax (minimumDb, juce::Decibels::gainToDecibels (levels[side], minimumDb));
        const auto fallen = juce::jmax (minimumDb, displayed[side] - fallDbPerSecond * elapsedSeconds);
        const auto next = juce::jmax (db, fallen);

        if (db >= hold[side])
        {
            hold[side] = db;
            holdTime[side] = 1.2f;
        }
        else if ((holdTime[side] -= elapsedSeconds) <= 0.0f)
        {
            hold[side] = juce::jmax (minimumDb, hold[side] - fallDbPerSecond * elapsedSeconds);
        }

        changed = changed || next != displayed[side] || hold[side] > minimumDb;
        displayed[side] = next;
    }

    if (changed)
        repaint();
}

void StereoMeter::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    const auto gap = 1.0f;
    const auto zero = dbToProportion (0.0f);
    const auto amber = dbToProportion (-12.0f);

    for (size_t side = 0; side < 2; ++side)
    {
        auto bar = vertical ? (side == 0 ? area.withWidth ((area.getWidth() - gap) * 0.5f)
                                         : area.withTrimmedLeft ((area.getWidth() + gap) * 0.5f))
                            : (side == 0 ? area.withHeight ((area.getHeight() - gap) * 0.5f)
                                         : area.withTrimmedTop ((area.getHeight() + gap) * 0.5f));

        g.setColour (theme.meterBackground);
        g.fillRect (bar);

        const auto level = dbToProportion (displayed[side]);

        // Green up to -12 dB, yellow up to 0 dB, red above (clipping).
        auto drawSegment = [&] (float from, float to, juce::Colour colour)
        {
            if (level <= from)
                return;

            const auto end = std::min (level, to);
            g.setColour (colour);

            if (vertical)
                g.fillRect (bar.getX(), bar.getBottom() - end * bar.getHeight(), bar.getWidth(), (end - from) * bar.getHeight());
            else
                g.fillRect (bar.getX() + from * bar.getWidth(), bar.getY(), (end - from) * bar.getWidth(), bar.getHeight());
        };

        drawSegment (0.0f, amber, theme.meterLow);
        drawSegment (amber, zero, theme.meterMid);
        drawSegment (zero, 1.0f, theme.meterHigh);

        if (hold[side] > minimumDb + 0.5f)
        {
            const auto p = dbToProportion (hold[side]);
            g.setColour (hold[side] > 0.0f ? theme.meterHigh : theme.text.withAlpha (0.7f));

            if (vertical)
                g.fillRect (bar.getX(), bar.getBottom() - p * bar.getHeight(), bar.getWidth(), 1.5f);
            else
                g.fillRect (bar.getX() + p * bar.getWidth() - 1.0f, bar.getY(), 1.5f, bar.getHeight());
        }
    }
}

//==============================================================================
namespace
{
    template <typename Control>
    void connect (Control& control, Project& project, std::function<juce::String()> channelId,
                  std::function<juce::StringArray()> group, edit::ChannelGesture::Parameter parameter)
    {
        auto gesture = std::make_shared<std::unique_ptr<edit::ChannelGesture>>();

        control.getValue = [&project, channelId, parameter]
        {
            // An automated volume or pan shows the lane's value as the song plays.
            const auto id = channelId();
            return parameter == edit::ChannelGesture::Parameter::volume
                     ? (double) automationfollow::shown (project, id, automation::Target::volume(), (float) mixer::getVolumeDb (project, id))
                     : (double) automationfollow::shown (project, id, automation::Target::pan(), (float) mixer::getPan (project, id));
        };

        control.onGestureStart = [&project, channelId, group, parameter, gesture]
        {
            *gesture = std::make_unique<edit::ChannelGesture> (project, parameter, channelId(),
                                                               group ? group() : juce::StringArray());
        };

        control.onValueChange = [gesture] (double value)
        {
            if (*gesture != nullptr)
                (*gesture)->setPrimaryValue (value);
        };

        control.onGestureEnd = [gesture] { gesture->reset(); };
    }
}

void connectVolume (VolumeFader& fader, Project& project, std::function<juce::String()> channelId,
                    std::function<juce::StringArray()> group)
{
    connect (fader, project, std::move (channelId), std::move (group), edit::ChannelGesture::Parameter::volume);
}

void connectPan (PanKnob& knob, Project& project, std::function<juce::String()> channelId,
                 std::function<juce::StringArray()> group)
{
    connect (knob, project, std::move (channelId), std::move (group), edit::ChannelGesture::Parameter::pan);
}

} // namespace aether::ui
