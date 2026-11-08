// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <array>
#include <functional>
#include <memory>

#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"
#include "dsp/EffectTypes.h"
#include "model/Project.h"

namespace aether::ui
{

/** The picture at the top of an effect's window: what the effect does, drawn from the same
    maths the sound uses (so the picture is what you hear). Some are live (the compressor shows
    how much it's turning down as the song plays) and the EQ's points can be dragged.

    Every change made from a picture is one undo step per drag. */
class EffectPicture : public juce::Component,
                      public juce::SettableTooltipClient
{
public:
    EffectPicture (const Theme& theme, Project& project, juce::String insertId);

    /** What the effect is doing now, from the engine (see dsp::Effect::getMeter). */
    std::function<float()> meter;

    /** Called about 30 times a second while the window is open: live pictures move. */
    virtual void tick();

    /** The latest meter reading (for the tests). */
    float getMeterValue() const noexcept   { return meterValue; }

    /** Points that can be dragged (the EQ's bands), and where each is now (for the tests). */
    virtual int getNumHandles() const   { return 0; }
    virtual juce::Point<float> getHandlePosition (int) const   { return {}; }

protected:
    juce::ValueTree insert() const;
    float value (int parameterIndex) const;
    dsp::EffectParameterValues values() const;

    /** Starts one undo step for a drag (then setValue as the mouse moves). */
    void beginGesture (const juce::String& what);
    void setValue (int parameterIndex, float newValue);

    /** The picture's dark rounded background; returns the area inside it. */
    juce::Rectangle<float> drawBackground (juce::Graphics&, juce::Rectangle<float> area) const;

    /** A vertical meter of how much is being turned down (0 at the top, down to `range` dB). */
    void drawReductionMeter (juce::Graphics&, juce::Rectangle<float> area, float reductionDb, float range) const;

    /** The meter's last few seconds, as a line moving left. */
    void drawHistory (juce::Graphics&, juce::Rectangle<float> area, float range, bool reduction, const juce::String& caption) const;

    const Theme& theme;
    Project& project;
    juce::String insertId;
    const dsp::EffectType* type = nullptr;

    float meterValue = 0.0f;
    std::array<float, 150> history {};   // five seconds of meter readings
    size_t historyPosition = 0;
    double animationSeconds = 0.0;       // for pictures that move by themselves (the filter's sweep)
};

/** The picture for an insert's effect type (a plain one for types without their own). */
std::unique_ptr<EffectPicture> createEffectPicture (const Theme& theme, Project& project, const juce::String& insertId);

} // namespace aether::ui
