// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "AetherLookAndFeel.h"

namespace aether::ui
{

AetherLookAndFeel::AetherLookAndFeel (const Theme& themeToUse)
    : theme (themeToUse)
{
    setColourScheme ({ theme.background,      // windowBackground
                       theme.panel,           // widgetBackground
                       theme.panel,           // menuBackground
                       theme.outline,         // outline
                       theme.text,            // defaultText
                       theme.panelRaised,     // defaultFill
                       theme.accentText,      // highlightedText
                       theme.accent,          // highlightedFill
                       theme.text });         // menuText

    setColour (juce::ResizableWindow::backgroundColourId, theme.background);
    setColour (juce::DocumentWindow::textColourId, theme.text);

    setColour (juce::TextButton::buttonColourId, theme.panelRaised);
    setColour (juce::TextButton::buttonOnColourId, theme.accent);
    setColour (juce::TextButton::textColourOffId, theme.text);
    setColour (juce::TextButton::textColourOnId, theme.accentText);

    setColour (juce::ComboBox::backgroundColourId, theme.panelRaised);
    setColour (juce::ComboBox::outlineColourId, theme.outline);
    setColour (juce::ComboBox::textColourId, theme.text);
    setColour (juce::ComboBox::arrowColourId, theme.textDim);

    setColour (juce::PopupMenu::backgroundColourId, theme.panel);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, theme.accent);
    setColour (juce::PopupMenu::highlightedTextColourId, theme.accentText);

    // Text boxes: light text, a visible (accent-coloured) typing cursor, and selected text that
    // stays readable. JUCE's defaults draw selected text almost black and the cursor dark grey.
    setColour (juce::TextEditor::backgroundColourId, theme.panelRaised);
    setColour (juce::TextEditor::textColourId, theme.text);
    setColour (juce::TextEditor::highlightColourId, theme.accent.withAlpha (0.35f));
    setColour (juce::TextEditor::highlightedTextColourId, theme.text);
    setColour (juce::TextEditor::outlineColourId, theme.outline);
    setColour (juce::TextEditor::focusedOutlineColourId, theme.accent);
    setColour (juce::CaretComponent::caretColourId, theme.accent);

    setColour (juce::Label::textColourId, theme.text);
    setColour (juce::ListBox::backgroundColourId, theme.panelRaised);
    setColour (juce::ListBox::outlineColourId, theme.outline);
    setColour (juce::ScrollBar::thumbColourId, theme.outline);

    setColour (juce::TooltipWindow::backgroundColourId, theme.panelRaised.brighter (0.1f));
    setColour (juce::TooltipWindow::textColourId, theme.text);
    setColour (juce::TooltipWindow::outlineColourId, theme.outline);

    setColour (juce::AlertWindow::backgroundColourId, theme.panel);
    setColour (juce::AlertWindow::textColourId, theme.text);
    setColour (juce::AlertWindow::outlineColourId, theme.outline);

    setColour (juce::ToggleButton::textColourId, theme.text);
    setColour (juce::ToggleButton::tickColourId, theme.accent);
    setColour (juce::ToggleButton::tickDisabledColourId, theme.textDim);
}

void AetherLookAndFeel::drawCard (juce::Graphics& g, juce::Rectangle<float> area, const Theme& t)
{
    g.setColour (t.panel);
    g.fillRoundedRectangle (area, 8.0f);
    g.setColour (t.outline);
    g.drawRoundedRectangle (area.reduced (0.5f), 8.0f, 1.0f);
}

void AetherLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                              const juce::Colour& backgroundColour,
                                              bool isMouseOverButton, bool isButtonDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    auto colour = backgroundColour;

    if (! button.isEnabled())
        colour = colour.withMultipliedAlpha (0.5f);
    else if (isButtonDown)
        colour = colour.darker (0.15f);
    else if (isMouseOverButton)
        colour = colour.brighter (0.08f);

    g.setColour (colour);
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (button.getToggleState() ? colour : theme.outline);
    g.drawRoundedRectangle (bounds, 6.0f, 1.0f);
}

void AetherLookAndFeel::drawAlertBox (juce::Graphics& g, juce::AlertWindow& alert,
                                      const juce::Rectangle<int>&, juce::TextLayout& textLayout)
{
    constexpr int iconColumnWidth = 80;   // must match the space JUCE reserves when laying out the text
    auto bounds = alert.getLocalBounds().toFloat();

    g.setColour (theme.panel);
    g.fillRoundedRectangle (bounds, 8.0f);
    g.setColour (theme.outline);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 8.0f, 1.0f);

    auto textLeft = 24;

    if (alert.getAlertType() != juce::MessageBoxIconType::NoIcon)
    {
        const auto isWarning = alert.getAlertType() == juce::MessageBoxIconType::WarningIcon;
        const auto symbol = isWarning ? "!" : (alert.getAlertType() == juce::MessageBoxIconType::InfoIcon ? "i" : "?");
        juce::Rectangle<float> icon (24.0f, 28.0f, 36.0f, 36.0f);

        g.setColour (isWarning ? theme.warning : theme.accent);
        g.fillEllipse (icon);
        g.setColour (theme.accentText);
        g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
        g.drawText (symbol, icon, juce::Justification::centred);

        textLeft = iconColumnWidth;
    }

    g.setColour (theme.text);
    const juce::Rectangle<float> textBounds ((float) textLeft, 30.0f,
                                             bounds.getWidth() - (float) textLeft - 16.0f,
                                             bounds.getHeight() - (float) getAlertWindowButtonHeight() - 20.0f);
    textLayout.draw (g, textBounds);
}

juce::Rectangle<int> AetherLookAndFeel::getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos,
                                                         juce::Rectangle<int> parentArea)
{
    const auto size = LookAndFeel_V4::getTooltipBounds (tipText, screenPos, parentArea);
    const auto w = size.getWidth(), h = size.getHeight();
    constexpr int gap = 6;

    // `screenPos` is the mouse position, in the tooltip's own coordinates (the window it's in).
    // The control under the mouse is in screen coordinates; this is the difference.
    const auto mouseSource = juce::Desktop::getInstance().getMainMouseSource();
    const auto offset = screenPos - mouseSource.getScreenPosition().roundToInt();

    // Below a button-sized control; for big areas (the timeline...), just below the pointer.
    juce::Rectangle<int> avoid (screenPos.x, screenPos.y - 2, 1, 22);

    if (auto* control = mouseSource.getComponentUnderMouse(); control != nullptr && control->getHeight() <= 80)
        avoid = control->getScreenBounds() + offset;

    auto x = screenPos.x - 12;
    auto y = avoid.getBottom() + gap;

    if (y + h > parentArea.getBottom())
        y = avoid.getY() - gap - h;   // no room below: above it instead

    x = juce::jlimit (parentArea.getX(), std::max (parentArea.getX(), parentArea.getRight() - w), x);
    y = juce::jlimit (parentArea.getY(), std::max (parentArea.getY(), parentArea.getBottom() - h), y);
    return { x, y, w, h };
}

} // namespace aether::ui
