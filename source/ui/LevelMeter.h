// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"

namespace aether::ui
{

/** A horizontal level meter (like the bouncing lights on a mixing desk).

    The owner calls pushLevel() regularly (about 30 times a second) with the peak level measured
    since the last call. The meter falls back smoothly and briefly holds the highest peak.
*/
class LevelMeter final : public juce::Component,
                         public juce::SettableTooltipClient
{
public:
    static constexpr float minimumDb = -60.0f;
    static constexpr float maximumDb = 0.0f;

    LevelMeter (const Theme& theme, juce::String channelName);

    /** `peakGain` is a linear level (1.0 = 0 dBFS). `elapsedSeconds` since the previous call. */
    void pushLevel (float peakGain, float elapsedSeconds);

    /** Current displayed level in decibels (for tests and readouts). */
    float getDisplayedDb() const noexcept   { return displayedDb; }

    /** Converts a level in dB to a position along the meter, 0..1. */
    static float dbToProportion (float decibels) noexcept;

    void paint (juce::Graphics&) override;

private:
    const Theme& theme;
    juce::String channelName;
    float displayedDb = minimumDb;
    float peakHoldDb = minimumDb;
    float peakHoldSecondsLeft = 0.0f;
};

} // namespace aether::ui
