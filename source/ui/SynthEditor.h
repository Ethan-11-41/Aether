// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <functional>
#include <memory>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

#include "EffectEditor.h"
#include "Theme.h"
#include "model/Project.h"

namespace aether::ui
{

/** The Synth's window contents: presets (by category), a Simple page with the settings that
    matter most (and pictures of the waveform, the filter and the envelope), and an Advanced page
    with every setting. Every change is undoable and follows undo/redo. */
class SynthEditor final : public juce::Component,
                          private juce::ValueTree::Listener
{
public:
    SynthEditor (const Theme& theme, Project& project, const juce::String& trackId);
    ~SynthEditor() override;

    /** Shows another track's synth. */
    void setTrack (const juce::String& trackId);
    juce::String getTrackId() const   { return trackId; }

    /** Called when the track shown changes or is renamed (e.g. for the window title). */
    std::function<void()> onTitleChanged;
    juce::String getTitle() const;

    enum class Page { simple, advanced };
    void showPage (Page page);
    Page getPage() const noexcept   { return page; }

    // For the tests ---------------------------------------------------------------------------
    ParameterKnob* findKnob (const juce::String& parameterId) const;      // on the page shown
    juce::ComboBox* findChoice (const juce::String& parameterId) const;   // on the page shown
    juce::ComboBox& getPresetBox() noexcept   { return presetBox; }
    juce::String getPresetText() const;
    void choosePresetOffset (int delta);   // the previous (-1) or next (+1) preset

    /** A sentence explaining a setting (used for its tooltip). */
    static juce::String explain (const juce::String& parameterId);

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int headerHeight = 56;

private:
    class Card;
    class ChoiceControl;
    class WavePicture;
    class FilterPicture;
    class EnvelopePicture;
    class LfoPicture;

    juce::ValueTree instrument() const;
    juce::String instrumentId() const;
    bool isSynth() const;

    void build();
    void refresh();
    void updatePresetBox();
    juce::Component* makeControl (int parameterIndex);
    void layOutPage (std::vector<Card*>& cards, juce::Component& area, int columns, int width);

    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override;
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override;

    const Theme& theme;
    Project& project;
    juce::String trackId;
    juce::ValueTree projectState;
    Page page = Page::simple;

    juce::Label titleLabel, notSynthLabel;
    juce::ComboBox presetBox;
    juce::TextButton previousButton { "<" }, nextButton { ">" }, simpleButton { "Simple" }, advancedButton { "Advanced" };
    juce::TextButton useSynthButton { "Use the Synth on this track" };

    juce::OwnedArray<juce::Component> ownedControls;   // the knobs and drop-downs (shown inside the cards)
    juce::Component simplePage, advancedContent;
    juce::Viewport advancedPage;
    std::vector<std::unique_ptr<Card>> simpleCards, advancedCards;
    std::vector<ParameterKnob*> knobs;          // every knob (both pages), for repainting
    std::vector<ChoiceControl*> choiceControls;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SynthEditor)
};

} // namespace aether::ui
