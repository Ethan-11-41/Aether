// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "MessageBanner.h"

namespace aether::ui
{

namespace
{
    constexpr int padding = 12;
    constexpr int iconSize = 22;
    constexpr int buttonHeight = 26;
    constexpr float fontSize = 13.5f;

    juce::TextLayout layoutText (const juce::String& text, juce::Colour colour, int width)
    {
        juce::AttributedString attributed;
        attributed.append (text, juce::FontOptions (fontSize), colour);
        attributed.setWordWrap (juce::AttributedString::byWord);

        juce::TextLayout layout;
        layout.createLayout (attributed, (float) juce::jmax (1, width));
        return layout;
    }
}

MessageBanner::MessageBanner (const Theme& themeToUse)
    : theme (themeToUse)
{
    dismissButton.setTooltip ("Hide this message.");
    dismissButton.onClick = [this]
    {
        if (onDismiss)
            onDismiss();
    };
    addAndMakeVisible (dismissButton);
}

void MessageBanner::setMessage (const juce::String& text)
{
    message = text;
    repaint();
}

juce::Rectangle<int> MessageBanner::getTextArea() const
{
    return getLocalBounds().reduced (padding)
                           .withTrimmedLeft (iconSize + padding)
                           .withTrimmedBottom (buttonHeight + padding / 2);
}

int MessageBanner::getIdealHeight (int width) const
{
    const auto textWidth = width - 2 * padding - iconSize - padding;
    const auto textHeight = (int) std::ceil (layoutText (message, theme.text, textWidth).getHeight());
    return padding + textHeight + padding / 2 + buttonHeight + padding;
}

void MessageBanner::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (theme.warning.withAlpha (0.12f));
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (theme.warning.withAlpha (0.7f));
    g.drawRoundedRectangle (bounds, 6.0f, 1.0f);

    // Warning icon: a filled circle with "!".
    juce::Rectangle<float> icon ((float) padding, (float) padding, (float) iconSize, (float) iconSize);
    g.setColour (theme.warning);
    g.fillEllipse (icon);
    g.setColour (theme.accentText);
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.drawText ("!", icon, juce::Justification::centred);

    layoutText (message, theme.text, getTextArea().getWidth()).draw (g, getTextArea().toFloat());
}

void MessageBanner::resized()
{
    auto area = getLocalBounds().reduced (padding);
    dismissButton.setBounds (area.removeFromBottom (buttonHeight).removeFromRight (90));
}

} // namespace aether::ui
