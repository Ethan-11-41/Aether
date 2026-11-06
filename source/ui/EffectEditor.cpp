// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "EffectEditor.h"

#include <cmath>

#include "model/MidiClips.h"
#include "model/Mixer.h"

namespace aether::ui
{

namespace
{
    constexpr int knobWidth = 72, knobHeight = 86, titleHeight = 40, margin = 12, knobsPerRow = 6, presetHeight = 34;
}

//==============================================================================
ParameterKnob::ParameterKnob (const Theme& t, const dsp::EffectParameter& parameterToShow)
    : DragControl (t), parameter (parameterToShow)
{
    resetValue = parameter.defaultValue;
    setTooltip (parameter.name + ". Drag up/down to change (hold Shift for fine changes); double-click for the default.");
}

double ParameterKnob::valueToProportion (double value) const
{
    if (parameter.logarithmic)
        return std::log (value / parameter.minimum) / std::log ((double) parameter.maximum / parameter.minimum);

    return (value - parameter.minimum) / ((double) parameter.maximum - parameter.minimum);
}

double ParameterKnob::proportionToValue (double proportion) const
{
    if (parameter.logarithmic)
        return parameter.minimum * std::pow ((double) parameter.maximum / parameter.minimum, proportion);

    return parameter.minimum + proportion * ((double) parameter.maximum - parameter.minimum);
}

void ParameterKnob::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    const auto knobArea = area.removeFromTop (48.0f);
    const auto size = 40.0f;
    const auto circle = juce::Rectangle<float> (size, size).withCentre (knobArea.getCentre());
    const auto proportion = (float) juce::jlimit (0.0, 1.0, valueToProportion (currentValue()));

    constexpr auto range = juce::MathConstants<float>::pi * 0.75f;
    const auto angle = -range + proportion * 2.0f * range;

    g.setColour (theme.panelRaised);
    g.fillEllipse (circle);

    juce::Path track, amount;
    track.addCentredArc (circle.getCentreX(), circle.getCentreY(), size * 0.5f - 2.0f, size * 0.5f - 2.0f, 0.0f, -range, range, true);
    amount.addCentredArc (circle.getCentreX(), circle.getCentreY(), size * 0.5f - 2.0f, size * 0.5f - 2.0f, 0.0f, -range, angle, true);
    g.setColour (theme.meterBackground);
    g.strokePath (track, juce::PathStrokeType (3.0f));
    g.setColour (theme.accent);
    g.strokePath (amount, juce::PathStrokeType (3.0f));

    const auto pointer = circle.getCentre() + juce::Point<float> (std::sin (angle), -std::cos (angle)) * (size * 0.5f - 7.0f);
    g.setColour (theme.text);
    g.drawLine ({ circle.getCentre(), pointer }, 2.0f);

    g.setFont (juce::FontOptions (12.0f));
    g.setColour (theme.textDim);
    g.drawText (parameter.name, area.removeFromTop (18.0f), juce::Justification::centred);
    g.setColour (theme.text);
    g.drawText (parameter.format ((float) currentValue()), area.removeFromTop (18.0f), juce::Justification::centred);
}

//==============================================================================
EffectEditor::EffectEditor (const Theme& t, Project& projectToEdit, juce::String insertToEdit)
    : theme (t), project (projectToEdit), insertId (std::move (insertToEdit)), projectState (projectToEdit.getState()),
      onOff (t, "On", t.accent)
{
    const auto insert = mixer::findInsert (project, insertId);
    type = dsp::findEffectType (insert[ids::type].toString());

    onOff.isOn = [this] { return ! (bool) mixer::findInsert (project, insertId)[ids::bypass]; };
    onOff.onClick = [this] { edit::setInsertBypassed (project, insertId, ! (bool) mixer::findInsert (project, insertId)[ids::bypass]); };
    onOff.setTooltip ("Switch the effect on or off");
    addChildComponent (onOff);
    onOff.setVisible (type == nullptr || ! type->isInstrument);   // an instrument is silenced with the track's mute

    int width = 240;

    // Instruments: a preset menu at the top.
    if (type != nullptr && type->isInstrument && ! type->presets.empty())
    {
        for (int i = 0; i < (int) type->presets.size(); ++i)
            presetBox.addItem (type->presets[(size_t) i].name, i + 1);

        presetBox.setTextWhenNothingSelected ("(your own settings)");
        presetBox.setTooltip ("Ready-made sounds. Choosing one changes all the settings below (you can undo it).");
        presetBox.onChange = [this]
        {
            const auto trackId = mixer::findInsert (project, insertId).getParent()[ids::id].toString();
            edit::setInstrumentPreset (project, trackId, presetBox.getText());
        };
        addAndMakeVisible (presetBox);
    }

    if (type != nullptr)
    {
        for (const auto& parameter : type->parameters)
        {
            if (parameter.isChoice())
            {
                auto box = std::make_unique<juce::ComboBox>();
                for (int i = 0; i < parameter.choices.size(); ++i)
                    box->addItem (parameter.choices[i], i + 1);

                const auto* p = &parameter;
                box->onChange = [this, p, raw = box.get()]
                {
                    edit::setInsertParameter (project, insertId, p->id, (float) (raw->getSelectedId() - 1));
                };
                box->setTooltip (parameter.name);
                addAndMakeVisible (*box);
                choices.push_back (std::move (box));
                choiceParameters.push_back (&parameter);
                continue;
            }

            auto knob = std::make_unique<ParameterKnob> (theme, parameter);
            const auto* p = &parameter;
            const auto name = type->name;
            knob->getValue = [this, p] { return (double) mixer::getInsertParameter (mixer::findInsert (project, insertId), p->id); };
            knob->onGestureStart = [this, p, name]
            {
                project.getUndoManager().beginNewTransaction ("Change " + name + " " + p->name.toLowerCase());
            };
            knob->onValueChange = [this, p] (double value) { edit::setInsertParameter (project, insertId, p->id, (float) value, false); };
            addAndMakeVisible (*knob);
            knobs.push_back (std::move (knob));
        }

        width = std::max (width, margin * 2 + knobWidth * std::min (knobsPerRow, (int) knobs.size()));
    }

    const auto choiceRows = (int) choices.size();
    const auto knobRows = std::max (1, ((int) knobs.size() + knobsPerRow - 1) / knobsPerRow);
    setSize (width, titleHeight + (presetBox.getNumItems() > 0 ? presetHeight : 0) + knobRows * knobHeight + choiceRows * 30 + margin);
    projectState.addListener (this);
    refresh();
}

EffectEditor::~EffectEditor()
{
    projectState.removeListener (this);
}

void EffectEditor::show (const Theme& theme, Project& project, const juce::String& insertId, juce::Rectangle<int> screenArea)
{
    auto editor = std::make_unique<EffectEditor> (theme, project, insertId);
    juce::CallOutBox::launchAsynchronously (std::move (editor), screenArea, nullptr);
}

void EffectEditor::refresh()
{
    const auto insert = mixer::findInsert (project, insertId);

    for (size_t i = 0; i < choices.size(); ++i)
        choices[i]->setSelectedId ((int) std::lround (mixer::getInsertParameter (insert, choiceParameters[i]->id)) + 1,
                                   juce::dontSendNotification);

    if (presetBox.getNumItems() > 0)
    {
        // Shows the preset while the settings still match it; "(your own settings)" once changed.
        presetBox.setSelectedId (0, juce::dontSendNotification);

        for (int i = 0; type != nullptr && i < (int) type->presets.size(); ++i)
        {
            const auto& preset = type->presets[(size_t) i];
            bool matches = preset.name == insert[ids::preset].toString();

            for (size_t p = 0; matches && p < type->parameters.size() && p < preset.values.size(); ++p)
                matches = std::abs (mixer::getInsertParameter (insert, type->parameters[p].id) - preset.values[p]) < 1.0e-3f;

            if (matches)
                presetBox.setSelectedId (i + 1, juce::dontSendNotification);
        }
    }

    repaint();
}

void EffectEditor::closeIfGone()
{
    if (mixer::findInsert (project, insertId).isValid())
        return;

    if (auto* box = findParentComponentOfClass<juce::CallOutBox>())
        box->dismiss();
}

void EffectEditor::paint (juce::Graphics& g)
{
    const auto bypassed = (bool) mixer::findInsert (project, insertId)[ids::bypass];
    auto title = getLocalBounds().removeFromTop (titleHeight).reduced (margin, 8);

    g.setColour (theme.text);
    g.setFont (juce::FontOptions (16.0f, juce::Font::bold));
    g.drawText (type != nullptr ? type->name : juce::String ("Unknown effect"), title, juce::Justification::centredLeft);

    if (bypassed)
    {
        g.setColour (theme.warning);
        g.setFont (juce::FontOptions (12.0f));
        g.drawText ("switched off", title.withTrimmedRight (56), juce::Justification::centredRight);
    }

    // Choice labels, to the left of their boxes.
    g.setFont (juce::FontOptions (12.5f));
    g.setColour (theme.textDim);

    for (size_t i = 0; i < choices.size(); ++i)
        g.drawText (choiceParameters[i]->name, choices[i]->getBounds().withX (margin).withWidth (90), juce::Justification::centredLeft);
}

void EffectEditor::resized()
{
    auto area = getLocalBounds();
    auto title = area.removeFromTop (titleHeight).reduced (margin, 8);
    onOff.setBounds (title.removeFromRight (44).withSizeKeepingCentre (44, 22));

    if (presetBox.getNumItems() > 0)
        presetBox.setBounds (area.removeFromTop (presetHeight).reduced (margin, 4));

    juce::Rectangle<int> knobRow;
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        if (i % knobsPerRow == 0)
            knobRow = area.removeFromTop (knobHeight).withTrimmedLeft (margin);

        knobs[i]->setBounds (knobRow.removeFromLeft (knobWidth));
    }

    if (knobs.empty())
        area.removeFromTop (knobHeight);

    for (auto& box : choices)
        box->setBounds (area.removeFromTop (30).withTrimmedLeft (margin + 92).withTrimmedRight (margin).reduced (0, 3));
}

} // namespace aether::ui
