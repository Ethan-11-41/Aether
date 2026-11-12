// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "QuantizePanel.h"

#include <vector>

namespace aether::ui
{

namespace
{
    /** The grids offered: every snap size except "off". */
    std::vector<GridSize> choices()
    {
        std::vector<GridSize> result;
        for (const auto size : allGridSizes)
            if (size != GridSize::off)
                result.push_back (size);
        return result;
    }
}

juce::String QuantizeSettings::toString() const
{
    return juce::String (gridSizeId (grid)) + ";" + juce::String (strength, 3) + ";" + juce::String (swing, 3) + ";" + (ends ? "1" : "0");
}

QuantizeSettings QuantizeSettings::fromString (const juce::String& text)
{
    QuantizeSettings s;
    const auto parts = juce::StringArray::fromTokens (text, ";", {});

    if (parts.size() != 4)
        return s;

    const auto size = gridSizeFromId (parts[0].toRawUTF8());
    s.grid = size == GridSize::off ? GridSize::sixteenth : size;
    s.strength = juce::jlimit (0.0, 1.0, parts[1].getDoubleValue());
    s.swing = juce::jlimit (0.0, 1.0, parts[2].getDoubleValue());
    s.ends = parts[3] == "1";
    return s;
}

//==============================================================================
QuantizePanel::QuantizePanel (const Theme& themeToUse, const QuantizeSettings& initial, const juce::String& what,
                              std::function<void (const QuantizeSettings&)> onApplyToUse)
    : theme (themeToUse), onApply (std::move (onApplyToUse))
{
    heading.setText ("Quantize " + what, juce::dontSendNotification);
    heading.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    gridLabel.setText ("Grid", juce::dontSendNotification);
    strengthLabel.setText ("Strength", juce::dontSendNotification);
    swingLabel.setText ("Swing", juce::dontSendNotification);

    for (auto* label : { &heading, &gridLabel, &strengthLabel, &swingLabel })
    {
        label->setColour (juce::Label::textColourId, theme.text);
        addAndMakeVisible (*label);
    }

    const auto sizes = choices();
    for (size_t i = 0; i < sizes.size(); ++i)
    {
        gridBox.addItem (gridSizeName (sizes[i]), (int) i + 1);
        if (sizes[i] == initial.grid)
            gridBox.setSelectedId ((int) i + 1, juce::dontSendNotification);
    }

    if (gridBox.getSelectedId() == 0)
        gridBox.setSelectedId (4, juce::dontSendNotification);   // 1/16

    gridBox.setTooltip ("What notes move to: the nearest 1/16 note, 1/8 note, beat... of the song's grid.");
    addAndMakeVisible (gridBox);

    strength.setRange (0.0, 100.0, 1.0);
    strength.setValue (initial.strength * 100.0, juce::dontSendNotification);
    strength.setTooltip ("100%: exactly on the grid. Less: only part of the way, keeping some of the feel.");
    swing.setRange (0.0, 100.0, 1.0);
    swing.setValue (initial.swing * 100.0, juce::dontSendNotification);
    swing.setTooltip ("Pushes every second grid step later, for a shuffle feel (100%: like triplets).");

    for (auto* slider : { &strength, &swing })
    {
        slider->setTextValueSuffix (" %");
        slider->setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 22);
        addAndMakeVisible (*slider);
    }

    ends.setToggleState (initial.ends, juce::dontSendNotification);
    ends.setTooltip ("Also move where notes end onto the grid (otherwise notes keep their lengths).");
    addAndMakeVisible (ends);

    applyButton.setTooltip ("Quantize now (one Ctrl+Z puts the notes back).");
    applyButton.onClick = [this] { apply(); };
    applyButton.setColour (juce::TextButton::buttonColourId, theme.accent);
    applyButton.setColour (juce::TextButton::textColourOffId, theme.accentText);
    addAndMakeVisible (applyButton);

    setSize (360, 200);
}

QuantizeSettings QuantizePanel::getSettings() const
{
    QuantizeSettings s;
    const auto sizes = choices();
    const auto index = gridBox.getSelectedId() - 1;
    s.grid = juce::isPositiveAndBelow (index, (int) sizes.size()) ? sizes[(size_t) index] : GridSize::sixteenth;
    s.strength = strength.getValue() / 100.0;
    s.swing = swing.getValue() / 100.0;
    s.ends = ends.getToggleState();
    return s;
}

void QuantizePanel::apply()
{
    if (onApply)
        onApply (getSettings());

    if (auto* box = findParentComponentOfClass<juce::CallOutBox>())
        box->dismiss();
}

void QuantizePanel::paint (juce::Graphics& g)
{
    g.fillAll (theme.panel);
}

void QuantizePanel::resized()
{
    auto area = getLocalBounds().reduced (12, 8);
    heading.setBounds (area.removeFromTop (26));

    auto row = area.removeFromTop (30);
    gridLabel.setBounds (row.removeFromLeft (80));
    gridBox.setBounds (row.removeFromLeft (140).reduced (0, 3));

    row = area.removeFromTop (30);
    strengthLabel.setBounds (row.removeFromLeft (80));
    strength.setBounds (row.reduced (0, 3));

    row = area.removeFromTop (30);
    swingLabel.setBounds (row.removeFromLeft (80));
    swing.setBounds (row.reduced (0, 3));

    ends.setBounds (area.removeFromTop (28));
    applyButton.setBounds (area.removeFromBottom (32).removeFromRight (120));
}

} // namespace aether::ui
