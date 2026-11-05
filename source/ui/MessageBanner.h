// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <functional>

#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"

namespace aether::ui
{

/** A highlighted box that explains a problem in plain language, with a "Got it" button. */
class MessageBanner final : public juce::Component
{
public:
    explicit MessageBanner (const Theme& theme);

    void setMessage (const juce::String& text);
    const juce::String& getMessage() const noexcept   { return message; }

    /** Called when the user clicks "Got it". */
    std::function<void()> onDismiss;

    /** Height needed to show the whole message at the given width. */
    int getIdealHeight (int width) const;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::Rectangle<int> getTextArea() const;

    const Theme& theme;
    juce::String message;
    juce::TextButton dismissButton { "Got it" };
};

} // namespace aether::ui
