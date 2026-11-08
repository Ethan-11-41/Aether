// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <memory>
#include <vector>

#include "MixerControls.h"
#include "dsp/EffectTypes.h"

namespace aether::ui
{

/** A knob for one effect setting, with its name and value written underneath. */
class ParameterKnob final : public DragControl
{
public:
    ParameterKnob (const Theme& theme, const dsp::EffectParameter& parameter);

    /** Written under the knob instead of the setting's name (e.g. "Gain" in a group called "Mids"). */
    juce::String label;

    void paint (juce::Graphics&) override;

private:
    double valueToProportion (double value) const override;
    double proportionToValue (double proportion) const override;
    double dragDistance (juce::Point<float> movement) const override   { return (movement.x - movement.y) / 160.0; }

    const dsp::EffectParameter& parameter;
};

/** The settings of one effect (an insert) or instrument, shown in a small pop-up next to the
    mixer slot or the track. Instruments also get a preset menu.
    Every change is undoable; it follows undo/redo while open, and closes itself if the effect
    is removed. */
class EffectEditor final : public juce::Component,
                           private juce::ValueTree::Listener
{
public:
    EffectEditor (const Theme& theme, Project& project, juce::String insertId);
    ~EffectEditor() override;

    /** Shows the editor in a pop-up pointing at `area` (in screen coordinates). */
    static void show (const Theme& theme, Project& project, const juce::String& insertId, juce::Rectangle<int> screenArea);

    int getNumKnobs() const noexcept   { return (int) knobs.size(); }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override   { refresh(); }
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override       { closeIfGone(); }
    void refresh();
    void closeIfGone();

    const Theme& theme;
    Project& project;
    juce::String insertId;
    const dsp::EffectType* type = nullptr;
    juce::ValueTree projectState;

    ToggleBox onOff;
    juce::ComboBox presetBox;   // instruments: their presets
    std::vector<std::unique_ptr<ParameterKnob>> knobs;
    std::vector<std::unique_ptr<juce::ComboBox>> choices;
    std::vector<const dsp::EffectParameter*> choiceParameters;
};

} // namespace aether::ui
