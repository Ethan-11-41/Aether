// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <functional>

#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"
#include "time/Grid.h"

namespace aether::ui
{

/** How to quantize (remembered between uses by the app). */
struct QuantizeSettings
{
    GridSize grid = GridSize::sixteenth;
    double strength = 1.0;   // 0..1
    double swing = 0.0;      // 0..1
    bool ends = false;       // note ends too

    /** "sixteenth;1;0;0": for saving in the settings, and back (bad text gives the defaults). */
    juce::String toString() const;
    static QuantizeSettings fromString (const juce::String& text);
};

/** The pop-up for quantizing whole MIDI clips from the timeline (Milestone 15): the grid,
    strength, swing and note ends, and a button that does it. */
class QuantizePanel final : public juce::Component
{
public:
    /** `what` says what will be quantized, e.g. "3 clips" or "the clips on Piano". */
    QuantizePanel (const Theme& theme, const QuantizeSettings& initial, const juce::String& what,
                   std::function<void (const QuantizeSettings&)> onApply);

    QuantizeSettings getSettings() const;

    /** Quantizes (calls onApply) and closes the pop-up it's in. */
    void apply();

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    const Theme& theme;
    std::function<void (const QuantizeSettings&)> onApply;
    juce::Label heading, gridLabel, strengthLabel, swingLabel;
    juce::ComboBox gridBox;
    juce::Slider strength { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight },
                 swing { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::ToggleButton ends { "Quantize note ends too" };
    juce::TextButton applyButton { "Quantize" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (QuantizePanel)
};

} // namespace aether::ui
