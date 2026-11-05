// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "LevelMeter.h"

#include <juce_audio_basics/juce_audio_basics.h>

namespace aether::ui
{

namespace
{
    constexpr float fallRateDbPerSecond = 26.0f;
    constexpr float peakHoldTimeSeconds = 1.5f;
}

LevelMeter::LevelMeter (const Theme& themeToUse, juce::String name)
    : theme (themeToUse), channelName (std::move (name))
{
    setOpaque (false);
}

float LevelMeter::dbToProportion (float decibels) noexcept
{
    return juce::jlimit (0.0f, 1.0f, (decibels - minimumDb) / (maximumDb - minimumDb));
}

void LevelMeter::pushLevel (float peakGain, float elapsedSeconds)
{
    const auto newDb = juce::Decibels::gainToDecibels (peakGain, minimumDb);

    // Rise instantly, fall smoothly.
    displayedDb = juce::jmax (newDb, displayedDb - fallRateDbPerSecond * elapsedSeconds, minimumDb);

    if (newDb >= peakHoldDb)
    {
        peakHoldDb = newDb;
        peakHoldSecondsLeft = peakHoldTimeSeconds;
    }
    else
    {
        peakHoldSecondsLeft -= elapsedSeconds;

        if (peakHoldSecondsLeft <= 0.0f)
            peakHoldDb = juce::jmax (displayedDb, peakHoldDb - fallRateDbPerSecond * elapsedSeconds);
    }

    repaint();
}

void LevelMeter::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    auto labelArea = area.removeFromLeft (18.0f);
    auto readoutArea = area.removeFromRight (62.0f);
    auto bar = area.reduced (0.0f, 3.0f);

    g.setColour (theme.textDim);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText (channelName, labelArea, juce::Justification::centredLeft);

    g.setColour (theme.meterBackground);
    g.fillRoundedRectangle (bar, 3.0f);

    // Coloured fill: green up to -12 dB, yellow up to -3 dB, red above.
    const auto fillWidth = bar.getWidth() * dbToProportion (displayedDb);
    const auto yellowStart = bar.getX() + bar.getWidth() * dbToProportion (-12.0f);
    const auto redStart = bar.getX() + bar.getWidth() * dbToProportion (-3.0f);
    const auto fillEnd = bar.getX() + fillWidth;

    auto drawSegment = [&] (float from, float to, juce::Colour colour)
    {
        if (to > from)
        {
            g.setColour (colour);
            g.fillRect (juce::Rectangle<float> (from, bar.getY(), to - from, bar.getHeight()));
        }
    };

    drawSegment (bar.getX(), juce::jmin (fillEnd, yellowStart), theme.meterLow);
    drawSegment (yellowStart, juce::jmin (fillEnd, redStart), theme.meterMid);
    drawSegment (redStart, fillEnd, theme.meterHigh);

    // Peak-hold line.
    if (peakHoldDb > minimumDb)
    {
        const auto x = bar.getX() + bar.getWidth() * dbToProportion (peakHoldDb);
        g.setColour (theme.text.withAlpha (0.8f));
        g.fillRect (juce::Rectangle<float> (x - 1.0f, bar.getY(), 2.0f, bar.getHeight()));
    }

    // Numeric readout of the held peak.
    g.setColour (theme.textDim);
    const auto readout = peakHoldDb <= minimumDb ? juce::String::fromUTF8 ("\xe2\x88\x92\xe2\x88\x9e dB")   // "−∞ dB"
                                                 : juce::String (peakHoldDb, 1) + " dB";
    g.drawText (readout, readoutArea, juce::Justification::centredRight);
}

} // namespace aether::ui
