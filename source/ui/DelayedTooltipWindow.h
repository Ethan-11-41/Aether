// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace aether::ui
{

/** The waiting rule on its own: a tooltip is shown only once the mouse has stayed on the same
    control (and the same tip) for `delayMs`. */
class TooltipDelay
{
public:
    explicit TooltipDelay (juce::uint32 delayMs) : delay (delayMs) {}

    /** `tip` if the mouse has rested on `control` long enough by `now` (ms), otherwise empty. */
    juce::String filter (const void* control, const juce::String& tip, juce::uint32 now)
    {
        // A different control (or a different part of one): wait again.
        if (control != lastControl || tip != lastTip)
        {
            lastControl = control;
            lastTip = tip;
            restingSince = now;
        }

        return now - restingSince >= delay ? tip : juce::String();
    }

private:
    const void* lastControl = nullptr;   // only compared, never used
    juce::String lastTip;
    juce::uint32 restingSince = 0, delay;
};

/** Shows tooltips only after the mouse has rested on a control for a moment, every time.

    (JUCE's own tooltip window shows the next tooltip instantly once one is visible, so moving
    the mouse across a row of buttons flashes every explanation.) */
class DelayedTooltipWindow final : public juce::TooltipWindow
{
public:
    static constexpr int defaultDelayMs = 700;

    explicit DelayedTooltipWindow (juce::Component* parent = nullptr, int delayMs = defaultDelayMs)
        : juce::TooltipWindow (parent, 50), rule ((juce::uint32) delayMs)
    {
    }

    juce::String getTipFor (juce::Component& component) override
    {
        return rule.filter (&component, juce::TooltipWindow::getTipFor (component), juce::Time::getMillisecondCounter());
    }

private:
    TooltipDelay rule;
};

} // namespace aether::ui
