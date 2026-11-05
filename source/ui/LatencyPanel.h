// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <array>

#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"
#include "devices/LatencyInfo.h"

namespace aether::ui
{

/** Shows the latency figures of the current audio device as a small table. */
class LatencyPanel final : public juce::Component,
                           public juce::TooltipClient
{
public:
    explicit LatencyPanel (const Theme& theme);

    void setLatency (const LatencyInfo& info);

    void paint (juce::Graphics&) override;
    juce::String getTooltip() override;

    static constexpr int rowHeight = 24;
    static constexpr int numRows = 4;

private:
    struct Row
    {
        juce::String name, value, tooltip;
    };

    const Theme& theme;
    LatencyInfo latency;
    std::array<Row, numRows> rows;
};

} // namespace aether::ui
