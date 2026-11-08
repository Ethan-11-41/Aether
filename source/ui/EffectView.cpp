// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "EffectView.h"

#include <map>

#include "dsp/ColourEffects.h"
#include "dsp/Dynamics.h"
#include "dsp/Eq.h"
#include "dsp/SpaceEffects.h"
#include "model/Mixer.h"

namespace aether::ui
{

namespace
{
    constexpr int margin = 14, headerHeight = 58, presetRowHeight = 36, groupTitleHeight = 20,
                  controlHeight = 88, groupPadding = 8, groupGap = 8, minimumPictureHeight = 190,
                  idealPictureHeight = 250;
    constexpr int knobWidth = 70, boxWidth = 128, toggleWidth = 58;

    struct GroupSpec
    {
        juce::String title;
        std::vector<std::pair<juce::String, juce::String>> controls;   // parameter id, label (empty: its name)
    };

    /** How each effect's settings are grouped in its window. Settings not listed here go in a
        last group, so every setting always has a control. */
    std::vector<GroupSpec> groupsFor (const juce::String& typeId)
    {
        if (typeId == dsp::eq::typeId)
            return { { "Low cut",   { { "lowCutOn", "On" }, { "lowCut", "Frequency" } } },
                     { "Bass",      { { "lowShelfFreq", "Frequency" }, { "lowShelfGain", "Gain" } } },
                     { "Low mids",  { { "band1Freq", "Frequency" }, { "band1Gain", "Gain" }, { "band1Q", "Width" } } },
                     { "Mids",      { { "band2Freq", "Frequency" }, { "band2Gain", "Gain" }, { "band2Q", "Width" } } },
                     { "High mids", { { "band3Freq", "Frequency" }, { "band3Gain", "Gain" }, { "band3Q", "Width" } } },
                     { "Treble",    { { "highShelfFreq", "Frequency" }, { "highShelfGain", "Gain" } } },
                     { "High cut",  { { "highCutOn", "On" }, { "highCut", "Frequency" } } },
                     { "Output",    { { "output", "Level" } } } };

        if (typeId == dsp::compressor::typeId)
            return { { "When and how much", { { "threshold", {} }, { "ratio", {} }, { "knee", {} } } },
                     { "How fast",          { { "attack", {} }, { "release", {} } } },
                     { "Output",            { { "makeup", {} }, { "mix", {} } } } };

        if (typeId == dsp::limiter::typeId)
            return { { "Loudness", { { "gain", {} }, { "ceiling", {} } } },
                     { "How fast", { { "release", {} } } } };

        if (typeId == dsp::gate::typeId)
            return { { "When and how much", { { "threshold", {} }, { "range", {} } } },
                     { "How fast",          { { "attack", {} }, { "hold", {} }, { "release", {} } } } };

        if (typeId == dsp::deesser::typeId)
            return { { "\"S\" sounds", { { "frequency", {} }, { "threshold", {} }, { "amount", {} } } } };

        if (typeId == dsp::saturation::typeId)
            return { { "Character", { { "character", {} }, { "drive", {} } } },
                     { "Tone",      { { "tone", {} } } },
                     { "Output",    { { "mix", {} }, { "output", {} } } } };

        if (typeId == dsp::filter::typeId)
            return { { "Filter", { { "filterType", {} }, { "cutoff", {} }, { "resonance", {} } } },
                     { "Sweep",  { { "lfoRate", "Rate" }, { "lfoDepth", "Amount" } } },
                     { "Output", { { "mix", {} } } } };

        if (typeId == dsp::chorus::typeId)
            return { { "Movement", { { "rate", {} }, { "depth", {} }, { "delay", {} } } },
                     { "Output",   { { "width", {} }, { "mix", {} } } } };

        if (typeId == dsp::echo::typeId)
            return { { "Echoes", { { "time", {} }, { "feedback", {} }, { "pingPong", {} } } },
                     { "Tone",   { { "tone", {} }, { "lowCut", {} } } },
                     { "Output", { { "mix", {} } } } };

        if (typeId == dsp::reverb::typeId)
            return { { "Room",   { { "size", {} }, { "damping", {} }, { "width", {} } } },
                     { "Timing and tone", { { "predelay", {} }, { "lowCut", {} } } },
                     { "Output", { { "mix", {} } } } };

        return {};
    }

    bool isOnOff (const dsp::EffectParameter& p)
    {
        return p.choices.size() == 2 && p.choices[0] == "Off" && p.choices[1] == "On";
    }

    /** The preset menu's sections, in order. */
    const std::vector<std::pair<juce::String, juce::String>>& presetSections()
    {
        static const std::vector<std::pair<juce::String, juce::String>> sections {
            { "General", "Starting points" }, { "Vocals", "Vocals" }, { "Instruments", "Instruments" },
            { "Master", "The whole song (on the master)" },
        };
        return sections;
    }
}

juce::Component* EffectView::Control::component() const
{
    if (knob != nullptr) return knob.get();
    if (box != nullptr) return box.get();
    return toggle.get();
}

//==============================================================================
EffectView::EffectView (const Theme& t, Project& p, juce::String id)
    : theme (t), project (p), insertId (std::move (id)), projectState (p.getState()),
      onOff (t, "On", t.accent)
{
    type = dsp::findEffectType (insert()[ids::type].toString());

    onOff.isOn = [this] { return ! (bool) insert()[ids::bypass]; };
    onOff.onClick = [this] { edit::setInsertBypassed (project, insertId, ! (bool) insert()[ids::bypass]); };
    onOff.setTooltip ("Switch the effect on or off, to hear the difference it makes");
    addAndMakeVisible (onOff);

    previousButton.setTooltip ("The previous preset");
    nextButton.setTooltip ("The next preset");
    previousButton.onClick = [this] { choosePresetOffset (-1); };
    nextButton.onClick = [this] { choosePresetOffset (1); };
    addAndMakeVisible (previousButton);
    addAndMakeVisible (nextButton);

    buildPresetBox();
    addAndMakeVisible (presetBox);

    picture = createEffectPicture (theme, project, insertId);
    picture->meter = [this] { return meter != nullptr ? meter() : 0.0f; };
    addAndMakeVisible (*picture);

    buildControls();

    setSize (idealWidth, getIdealHeight());
    projectState.addListener (this);
    lastTitle = getTitle();
    refresh();
    startTimerHz (30);
}

EffectView::~EffectView()
{
    projectState.removeListener (this);
}

juce::ValueTree EffectView::insert() const
{
    return mixer::findInsert (project, insertId);
}

juce::String EffectView::getTitle() const
{
    const auto node = insert();
    const auto channel = node.getParent().getParent();
    const auto channelName = channel == mixer::masterNode (project) ? juce::String ("Master")
                                                                    : project.findTrack (channel[ids::id].toString()).getName();
    return (type != nullptr ? type->name : juce::String ("Effect")) + (channelName.isNotEmpty() ? " - " + channelName : juce::String());
}

void EffectView::buildPresetBox()
{
    presetBox.setTooltip ("Ready-made settings, for vocals, instruments and the whole song. Choosing one changes all "
                          "the settings below (you can undo it).");

    if (type == nullptr)
        return;

    // By section; any other category goes at the end.
    std::vector<std::pair<juce::String, juce::String>> sections = presetSections();
    for (const auto& preset : type->presets)
        if (std::none_of (sections.begin(), sections.end(), [&] (const auto& s) { return s.first == preset.category; }))
            sections.push_back ({ preset.category, preset.category.isEmpty() ? juce::String ("Other") : preset.category });

    for (const auto& [category, heading] : sections)
    {
        bool first = true;

        for (size_t i = 0; i < type->presets.size(); ++i)
        {
            if (type->presets[i].category != category)
                continue;

            if (first)
                presetBox.addSectionHeading (heading);

            first = false;
            presetBox.addItem (type->presets[i].name, (int) i + 1);
            presetOrder.push_back ((int) i);
        }
    }

    presetBox.onChange = [this]
    {
        if (const auto id = presetBox.getSelectedId(); id > 0 && type != nullptr)
            edit::setInsertPreset (project, insertId, type->presets[(size_t) id - 1].name);
    };
}

void EffectView::buildControls()
{
    if (type == nullptr)
        return;

    auto specs = groupsFor (type->id);

    // Anything not in a group: a last group, so nothing is left out.
    GroupSpec rest { specs.empty() ? juce::String() : juce::String ("More"), {} };
    for (const auto& parameter : type->parameters)
    {
        const auto listed = std::any_of (specs.begin(), specs.end(), [&] (const GroupSpec& spec)
        {
            return std::any_of (spec.controls.begin(), spec.controls.end(), [&] (const auto& c) { return c.first == parameter.id.toString(); });
        });

        if (! listed)
            rest.controls.push_back ({ parameter.id.toString(), {} });
    }

    if (! rest.controls.empty())
        specs.push_back (rest);

    controls.reserve (type->parameters.size());

    for (const auto& spec : specs)
    {
        Group group { spec.title, {}, {} };

        for (const auto& [parameterId, label] : spec.controls)
        {
            const auto it = std::find_if (type->parameters.begin(), type->parameters.end(),
                                          [&] (const dsp::EffectParameter& p) { return p.id.toString() == parameterId; });
            if (it == type->parameters.end())
                continue;

            const auto* parameter = &*it;
            Control control;
            control.parameter = parameter;
            control.label = label.isNotEmpty() ? label : parameter->name;

            if (isOnOff (*parameter))
            {
                control.toggle = std::make_unique<ToggleBox> (theme, "On", theme.accent);
                control.toggle->isOn = [this, parameter] { return mixer::getInsertParameter (insert(), parameter->id) >= 0.5f; };
                control.toggle->onClick = [this, parameter]
                {
                    const auto on = mixer::getInsertParameter (insert(), parameter->id) >= 0.5f;
                    edit::setInsertParameter (project, insertId, parameter->id, on ? 0.0f : 1.0f);
                };
                control.toggle->setTooltip ("Switch the " + parameter->name.toLowerCase() + " on or off");
                control.width = toggleWidth;
            }
            else if (parameter->isChoice())
            {
                control.box = std::make_unique<juce::ComboBox>();
                for (int i = 0; i < parameter->choices.size(); ++i)
                    control.box->addItem (parameter->choices[i], i + 1);

                control.box->onChange = [this, parameter, raw = control.box.get()]
                {
                    edit::setInsertParameter (project, insertId, parameter->id, (float) (raw->getSelectedId() - 1));
                };
                control.box->setTooltip (parameter->name);
                control.width = boxWidth;
            }
            else
            {
                control.knob = std::make_unique<ParameterKnob> (theme, *parameter);
                control.knob->label = control.label;
                const auto name = type->name;
                control.knob->getValue = [this, parameter] { return (double) mixer::getInsertParameter (insert(), parameter->id); };
                control.knob->onGestureStart = [this, parameter, name]
                {
                    project.getUndoManager().beginNewTransaction ("Change " + name + " " + parameter->name.toLowerCase());
                };
                control.knob->onValueChange = [this, parameter] (double value)
                {
                    edit::setInsertParameter (project, insertId, parameter->id, (float) value, false);
                };
                control.width = knobWidth;
            }

            addAndMakeVisible (*control.component());
            group.controls.push_back (controls.size());
            controls.push_back (std::move (control));
        }

        if (! group.controls.empty())
            groups.push_back (std::move (group));
    }
}

std::vector<juce::Rectangle<int>> EffectView::groupBounds (int width, int top) const
{
    std::vector<juce::Rectangle<int>> bounds;
    int x = margin, y = top;
    const auto groupHeight = groupTitleHeight + controlHeight + groupPadding;

    for (const auto& group : groups)
    {
        int groupWidth = groupPadding * 2;
        for (const auto index : group.controls)
            groupWidth += controls[index].width;

        if (x > margin && x + groupWidth > width - margin)
        {
            x = margin;
            y += groupHeight + groupGap;
        }

        bounds.push_back ({ x, y, groupWidth, groupHeight });
        x += groupWidth + groupGap;
    }

    return bounds;
}

int EffectView::heightOf (const std::vector<juce::Rectangle<int>>& bounds, int top)
{
    int bottom = top;
    for (const auto& b : bounds)
        bottom = std::max (bottom, b.getBottom());

    return bottom - top;
}

int EffectView::getIdealHeight() const
{
    return headerHeight + presetRowHeight + idealPictureHeight + margin + heightOf (groupBounds (idealWidth, 0), 0) + margin;
}

void EffectView::resized()
{
    auto area = getLocalBounds();
    auto header = area.removeFromTop (headerHeight).reduced (margin, 0);
    onOff.setBounds (header.removeFromRight (56).withSizeKeepingCentre (56, 26));

    auto presetRow = area.removeFromTop (presetRowHeight).reduced (margin, 4);
    presetRow.removeFromLeft (52);   // "Preset" is written here
    previousButton.setBounds (presetRow.removeFromLeft (28));
    presetRow.removeFromLeft (4);
    presetBox.setBounds (presetRow.removeFromLeft (std::min (340, presetRow.getWidth() - 40)));
    presetRow.removeFromLeft (4);
    nextButton.setBounds (presetRow.removeFromLeft (28));

    // The groups along the bottom; the picture takes the rest.
    const auto groupsHeight = heightOf (groupBounds (getWidth(), 0), 0);
    const auto groupsTop = std::max (area.getY() + minimumPictureHeight + margin, getHeight() - margin - groupsHeight);
    const auto bounds = groupBounds (getWidth(), groupsTop);
    for (size_t i = 0; i < groups.size(); ++i)
        groups[i].bounds = bounds[i];

    picture->setBounds (juce::Rectangle<int> (margin, area.getY() + 4, getWidth() - margin * 2, groupsTop - margin - area.getY() - 4));

    for (const auto& group : groups)
    {
        auto inner = group.bounds.reduced (groupPadding, 0).withTrimmedTop (groupTitleHeight);

        for (const auto index : group.controls)
        {
            const auto& control = controls[index];
            auto slot = inner.removeFromLeft (control.width);

            if (control.knob != nullptr)
                control.knob->setBounds (slot.withHeight (controlHeight - 4));
            else if (control.label == "On")
                control.component()->setBounds (slot.withHeight (48).withSizeKeepingCentre (slot.getWidth() - 12, 26));   // level with the knobs
            else
                control.component()->setBounds (slot.withTrimmedTop (30).withHeight (26).reduced (control.box != nullptr ? 4 : 6, 0));
        }
    }
}

void EffectView::paint (juce::Graphics& g)
{
    g.fillAll (theme.background);

    auto header = getLocalBounds().removeFromTop (headerHeight).reduced (margin, 6).withTrimmedRight (70);
    const auto channel = getTitle().fromFirstOccurrenceOf (" - ", false, false);

    g.setColour (theme.text);
    g.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    const auto name = type != nullptr ? type->name : juce::String ("Unknown effect");
    const auto nameWidth = juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), name) + 12;
    g.drawText (name, header.removeFromTop (26), juce::Justification::centredLeft);

    g.setColour (theme.textDim);
    g.setFont (juce::FontOptions (13.0f));
    if (channel.isNotEmpty())
        g.drawText ("on " + channel, juce::Rectangle<int> (margin + nameWidth, 6, 300, 26), juce::Justification::centredLeft);

    if (type != nullptr)
        g.drawText (type->description, header, juce::Justification::centredLeft, true);

    if ((bool) insert()[ids::bypass])
    {
        g.setColour (theme.warning);
        g.setFont (juce::FontOptions (12.5f));
        g.drawText ("switched off", juce::Rectangle<int> (getWidth() - margin - 170, 6, 104, 26), juce::Justification::centredRight);
    }

    // "Preset", before its menu.
    g.setColour (theme.textDim);
    g.setFont (juce::FontOptions (13.0f));
    g.drawText ("Preset", juce::Rectangle<int> (margin, headerHeight, 50, presetRowHeight), juce::Justification::centredLeft);

    // The groups' panels and titles.
    for (const auto& group : groups)
    {
        g.setColour (theme.panel);
        g.fillRoundedRectangle (group.bounds.toFloat(), 6.0f);

        if (group.title.isNotEmpty())
        {
            g.setColour (theme.textDim);
            g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
            g.drawText (group.title.toUpperCase(), group.bounds.reduced (groupPadding, 0).withHeight (groupTitleHeight),
                        juce::Justification::centredLeft, true);
        }

        // Labels above the switches and menus (knobs write their own).
        g.setFont (juce::FontOptions (12.0f));
        auto inner = group.bounds.reduced (groupPadding, 0).withTrimmedTop (groupTitleHeight);
        for (const auto index : group.controls)
        {
            const auto& control = controls[index];
            const auto slot = inner.removeFromLeft (control.width);

            if (control.knob == nullptr && control.label != "On")
            {
                g.setColour (theme.textDim);
                g.drawText (control.label, slot.withHeight (26).translated (0, 2), juce::Justification::centred, true);
            }
        }
    }
}

void EffectView::refresh()
{
    if (gone)
        return;

    const auto node = insert();

    for (auto& control : controls)
        if (control.box != nullptr)
            control.box->setSelectedId ((int) std::lround (mixer::getInsertParameter (node, control.parameter->id)) + 1,
                                        juce::dontSendNotification);

    // The preset while the settings still match it, "(changed)" once they don't.
    if (const auto index = mixer::currentPresetIndex (node); index >= 0)
    {
        presetBox.setSelectedId (index + 1, juce::dontSendNotification);
    }
    else
    {
        const auto chosen = node[ids::preset].toString();
        presetBox.setSelectedId (0, juce::dontSendNotification);
        presetBox.setTextWhenNothingSelected (chosen.isNotEmpty() ? chosen + " (changed)" : juce::String ("Your own settings"));
    }

    if (const auto title = getTitle(); title != lastTitle)
    {
        lastTitle = title;
        if (onTitleChanged != nullptr)
            onTitleChanged();
    }

    repaint();
    picture->repaint();
    for (auto& control : controls)
        control.component()->repaint();
}

void EffectView::checkStillThere()
{
    if (gone || insert().isValid())
        return;

    // Removed (or its track deleted): the window closes.
    gone = true;
    stopTimer();

    if (onEffectRemoved != nullptr)
        onEffectRemoved();
}

juce::String EffectView::getPresetText() const
{
    return presetBox.getSelectedId() > 0 ? presetBox.getText() : presetBox.getTextWhenNothingSelected();
}

void EffectView::choosePresetOffset (int delta)
{
    if (type == nullptr || presetOrder.empty())
        return;

    // From the current preset (or the one last chosen, if the settings have changed since).
    auto current = mixer::currentPresetIndex (insert());
    if (current < 0)
        current = dsp::findPresetIndex (*type, insert()[ids::preset].toString());

    const auto it = std::find (presetOrder.begin(), presetOrder.end(), current);
    const auto position = it != presetOrder.end() ? (int) (it - presetOrder.begin()) : (delta > 0 ? -1 : 0);
    const auto count = (int) presetOrder.size();
    const auto next = presetOrder[(size_t) (((position + delta) % count + count) % count)];
    edit::setInsertPreset (project, insertId, type->presets[(size_t) next].name);
}

void EffectView::timerCallback()
{
    if (! gone && isShowing())
        picture->tick();
}

} // namespace aether::ui
