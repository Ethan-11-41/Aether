// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "LatencyPanel.h"

namespace aether::ui
{

LatencyPanel::LatencyPanel (const Theme& themeToUse)
    : theme (themeToUse)
{
    rows[0] = { "Buffer size", {},
                "How much audio is processed at once. Smaller = less delay, but more risk of "
                "crackles if your computer can't keep up. Change it under 'Audio buffer size'." };
    rows[1] = { "Output latency", {},
                "Delay between Aether producing a sound and it reaching your speakers or headphones, "
                "as reported by the audio driver." };
    rows[2] = { "Input latency", {},
                "Delay between sound reaching your microphone and Aether receiving it, "
                "as reported by the audio driver." };
    rows[3] = { "Round trip (driver)", {},
                "Input + output latency as the driver reports it: the delay you'd hear when listening to "
                "yourself through Aether. Drivers are often wrong about this, so it can be measured (below)." };
    rows[4] = { "Recordings corrected by", {},
                "How much earlier Aether puts what you record, so it lines up with what you heard while "
                "playing. Uses the measured round trip if you've measured it for this setting, otherwise "
                "the driver's figure. Click \"Measure latency\" to measure it." };

    setLatency ({});
}

void LatencyPanel::setLatency (const LatencyInfo& info)
{
    latency = info;

    if (! info.isValid())
    {
        for (auto& row : rows)
            row.value = juce::String::fromUTF8 ("\xe2\x80\x94");   // "—"
    }
    else
    {
        rows[0].value = LatencyInfo::formatSamplesAndMs (info.bufferSizeSamples, info.sampleRate);
        rows[1].value = info.hasOutput ? LatencyInfo::formatSamplesAndMs (info.outputLatencySamples, info.sampleRate)
                                       : juce::String ("No output selected");
        rows[2].value = info.hasInput ? LatencyInfo::formatSamplesAndMs (info.inputLatencySamples, info.sampleRate)
                                      : juce::String ("No input selected");
        rows[3].value = (info.hasInput && info.hasOutput)
                            ? juce::String (LatencyInfo::formatSamplesAndMs (info.roundTripSamples(), info.sampleRate))
                            : juce::String ("Needs an input and an output");
        rows[4].value = juce::String (LatencyInfo::formatSamplesAndMs (info.recordingCorrectionSamples(), info.sampleRate))
                      + (info.measuredRoundTripSamples >= 0 ? " (measured)" : " (not measured)");
    }

    repaint();
}

void LatencyPanel::paint (juce::Graphics& g)
{
    g.setFont (juce::FontOptions (14.0f));

    for (int i = 0; i < numRows; ++i)
    {
        juce::Rectangle<int> rowArea (0, i * rowHeight, getWidth(), rowHeight);
        const auto isRoundTrip = (i == numRows - 1);   // the figure that matters most

        g.setColour (theme.textDim);
        g.drawText (rows[(size_t) i].name, rowArea, juce::Justification::centredLeft);
        g.setColour (isRoundTrip ? theme.accent : theme.text);
        g.drawText (rows[(size_t) i].value, rowArea, juce::Justification::centredRight);
    }
}

juce::String LatencyPanel::getTooltip()
{
    const auto row = getMouseXYRelative().y / rowHeight;
    return juce::isPositiveAndBelow (row, numRows) ? rows[(size_t) row].tooltip : juce::String();
}

} // namespace aether::ui
