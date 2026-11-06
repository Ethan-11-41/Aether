// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <functional>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"
#include "model/Project.h"

namespace aether::ui
{

/** A drag control (fader or knob) that reports a gesture: start, values while dragging, end.
    The owner turns a gesture into one undoable edit (see connectVolume / connectPan). */
class DragControl : public juce::Component,
                    public juce::SettableTooltipClient
{
public:
    /** The value shown (read from the project whenever the control is painted). */
    std::function<double()> getValue;

    std::function<void()> onGestureStart, onGestureEnd;
    std::function<void (double)> onValueChange;

    /** The value a double-click sets. */
    double resetValue = 0.0;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

protected:
    explicit DragControl (const Theme& themeToUse) : theme (themeToUse) {}

    /** The control's travel: value <-> 0..1. */
    virtual double valueToProportion (double value) const = 0;
    virtual double proportionToValue (double proportion) const = 0;

    /** How far (0..1) a mouse movement takes the control. */
    virtual double dragDistance (juce::Point<float> movement) const = 0;

    double currentValue() const   { return getValue ? getValue() : resetValue; }

    const Theme& theme;

private:
    double startProportion = 0.0;
    bool dragging = false;
};

/** A volume fader: vertical in the mixer, horizontal on track headers. 0 dB sits about
    four-fifths of the way along. Double-click for 0 dB; hold Shift for fine moves. */
class VolumeFader final : public DragControl
{
public:
    VolumeFader (const Theme& theme, bool vertical);

    void paint (juce::Graphics&) override;

private:
    double valueToProportion (double value) const override;
    double proportionToValue (double proportion) const override;
    double dragDistance (juce::Point<float> movement) const override;
    juce::Rectangle<float> getTrack() const;

    bool vertical;
};

/** A pan knob (-1 left .. +1 right). Drag up/down or left/right; double-click to centre. */
class PanKnob final : public DragControl
{
public:
    explicit PanKnob (const Theme& theme);

    void paint (juce::Graphics&) override;

private:
    double valueToProportion (double value) const override   { return (value + 1.0) * 0.5; }
    double proportionToValue (double proportion) const override   { return proportion * 2.0 - 1.0; }
    double dragDistance (juce::Point<float> movement) const override;
};

/** A small lit-when-on button, like the M (mute) and S (solo) buttons on a mixer. */
class ToggleBox final : public juce::Component,
                        public juce::SettableTooltipClient
{
public:
    ToggleBox (const Theme& theme, juce::String label, juce::Colour colourWhenOn);

    std::function<bool()> isOn;
    std::function<void()> onClick;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    const Theme& theme;
    juce::String text;
    juce::Colour onColour;
};

/** A two-channel level meter (left and right), vertical or horizontal, from -60 to +6 dB.
    Turns red above 0 dB (clipping). */
class StereoMeter final : public juce::Component
{
public:
    static constexpr float minimumDb = -60.0f;
    static constexpr float maximumDb = 6.0f;

    StereoMeter (const Theme& theme, bool vertical);

    /** Peak levels (linear) since the last call, and the time since then. */
    void pushLevels (float left, float right, float elapsedSeconds);
    float getDisplayedDb (int side) const noexcept   { return displayed[(size_t) (side & 1)]; }

    static float dbToProportion (float decibels) noexcept;

    void paint (juce::Graphics&) override;

private:
    const Theme& theme;
    bool vertical;
    std::array<float, 2> displayed { minimumDb, minimumDb }, hold { minimumDb, minimumDb }, holdTime { 0.0f, 0.0f };
};

//==============================================================================
/** Makes a fader edit a channel's volume. While a drag is on, the channels in `group()` (other
    selected tracks) move by the same amount; the whole drag is one undo step.
    `channelId` is a track id or mixer::masterId. */
void connectVolume (VolumeFader& fader, Project& project, std::function<juce::String()> channelId,
                    std::function<juce::StringArray()> group = {});

/** The same for a pan knob. */
void connectPan (PanKnob& knob, Project& project, std::function<juce::String()> channelId,
                 std::function<juce::StringArray()> group = {});

} // namespace aether::ui
