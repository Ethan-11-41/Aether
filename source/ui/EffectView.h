// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <memory>
#include <vector>

#include "EffectEditor.h"
#include "EffectPictures.h"

namespace aether::ui
{

/** An effect's own window (Milestone 13): its name and what it's on, a preset menu (with
    previous/next), an on/off switch, a picture of what it does (the EQ's points can be dragged;
    the compressor, limiter, gate and de-esser show what they're doing as the song plays), and
    its settings as knobs in labelled groups.

    Every change is undoable; it follows undo/redo while open, and tells its window to close if
    the effect is removed. */
class EffectView final : public juce::Component,
                         private juce::ValueTree::Listener,
                         private juce::Timer
{
public:
    EffectView (const Theme& theme, Project& project, juce::String insertId);
    ~EffectView() override;

    static constexpr int idealWidth = 940;
    int getIdealHeight() const;

    const juce::String& getInsertId() const noexcept   { return insertId; }

    /** "EQ - Vocals" (the effect, and the track or the master it's on). */
    juce::String getTitle() const;

    /** What the effect is doing now, from the engine (see dsp::Effect::getMeter). */
    std::function<float()> meter;

    std::function<void()> onTitleChanged;
    std::function<void()> onEffectRemoved;

    /** Moves to the previous/next preset (in the menu's order), wrapping around. */
    void choosePresetOffset (int delta);

    // For the tests.
    EffectPicture& getPicture() noexcept        { return *picture; }
    int getNumControls() const noexcept         { return (int) controls.size(); }
    int getNumGroups() const noexcept           { return (int) groups.size(); }
    juce::String getPresetText() const;
    juce::ComboBox& getPresetBox() noexcept     { return presetBox; }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Control
    {
        const dsp::EffectParameter* parameter = nullptr;
        juce::String label;
        std::unique_ptr<ParameterKnob> knob;
        std::unique_ptr<juce::ComboBox> box;
        std::unique_ptr<ToggleBox> toggle;
        int width = 0;

        juce::Component* component() const;
    };

    struct Group
    {
        juce::String title;
        std::vector<size_t> controls;
        juce::Rectangle<int> bounds;
    };

    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override   { refresh(); }
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override       { checkStillThere(); }
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override               { checkStillThere(); }
    void timerCallback() override;

    void buildControls();
    void buildPresetBox();
    void refresh();
    void checkStillThere();

    /** Where each group goes, in rows `width` wide starting at `top`. */
    std::vector<juce::Rectangle<int>> groupBounds (int width, int top) const;
    static int heightOf (const std::vector<juce::Rectangle<int>>& bounds, int top);

    juce::ValueTree insert() const;
    juce::String trackIdOf() const;

    const Theme& theme;
    Project& project;
    juce::String insertId;
    const dsp::EffectType* type = nullptr;
    juce::ValueTree projectState;
    juce::String lastTitle;
    bool gone = false;

    ToggleBox onOff;
    juce::TextButton previousButton { "<" }, nextButton { ">" };
    juce::ComboBox presetBox;
    std::vector<int> presetOrder;        // preset indices, in the menu's order
    std::unique_ptr<EffectPicture> picture;
    std::vector<Control> controls;
    std::vector<Group> groups;
};

} // namespace aether::ui
