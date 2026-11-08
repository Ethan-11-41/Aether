// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <functional>
#include <memory>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

#include "EffectEditor.h"
#include "KeyboardPanel.h"
#include "Theme.h"
#include "engine/SamplerLibrary.h"
#include "model/Project.h"

namespace aether::ui
{

/** The sampler's window (Milestone 12): what it plays (Aether's recorded instruments, one of its
    drum sounds, or your own recordings), a picture of which keys each recording covers, its
    settings, and a keyboard to play it while you change them. Every change is undoable and
    follows undo/redo. */
class SamplerEditor final : public juce::Component,
                            public juce::FileDragAndDropTarget,
                            private juce::ValueTree::Listener,
                            private juce::Timer
{
public:
    SamplerEditor (const Theme& theme, Project& project, const juce::String& trackId);
    ~SamplerEditor() override;

    void setTrack (const juce::String& trackId);
    juce::String getTrackId() const   { return trackId; }

    juce::String getTitle() const;
    std::function<void()> onTitleChanged;

    /** Plays a note on this track (the keyboard at the bottom). */
    std::function<void (int pitch, int velocity, bool on)> playNote;

    /** How many of the sound's recordings are loaded, out of how many (the app knows). */
    std::function<std::pair<int, int>()> loadingProgress;

    /** Where the project is (for sounds kept in it). */
    std::function<juce::File()> projectFolder;

    /** Your own sounds: files dropped on the window, or chosen ("Your own sound..."). The app
        copies them into the project and calls back setSound(). */
    std::function<void (const juce::Array<juce::File>& files)> onImportFiles;
    std::function<void()> onChooseFiles;

    void focusKeyboard();
    KeyboardPanel& getKeyboard() noexcept   { return *keyboard; }

    // Choosing sounds (the menu does these; the tests too) -----------------------------------
    void chooseBuiltIn (const samplerlibrary::Entry& entry);
    void chooseDrumSound (const juce::String& drumSoundId);
    void chooseOffset (int delta);   // the previous (-1) or next (+1) recorded instrument

    // For the tests ---------------------------------------------------------------------------
    ParameterKnob* findKnob (const juce::String& parameterId) const;
    juce::ComboBox& getVoicesBox() noexcept      { return voicesBox; }
    juce::String getSoundName() const;
    juce::String getStatusText() const;
    bool isShowingRootKey() const                { return rootLabel.isVisible(); }
    juce::TextButton& getRootDownButton() noexcept   { return rootDown; }
    juce::TextButton& getRootUpButton() noexcept     { return rootUp; }

    void paint (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;
    void fileDragEnter (const juce::StringArray&, int, int) override   { dropping = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override              { dropping = false; repaint(); }

    static constexpr int headerHeight = 56;

private:
    class Card;
    class ZoneMap;

    juce::ValueTree instrument() const;
    bool isSampler() const;
    void showSoundMenu();
    void refresh();
    ParameterKnob* makeKnob (const juce::String& id);

    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override   { refresh(); }
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override               { refresh(); }
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override        { refresh(); }
    void timerCallback() override;

    const Theme& theme;
    Project& project;
    juce::String trackId;
    juce::ValueTree projectState;
    bool dropping = false, refreshing = false;

    juce::Label titleLabel, nameLabel, descriptionLabel, statusLabel, rootLabel, notSamplerLabel;
    juce::TextButton soundButton { "Choose a sound" }, previousButton { "<" }, nextButton { ">" },
                     ownButton { "Your own sound..." }, rootDown { "-" }, rootUp { "+" }, useSamplerButton { "Use the sampler" };
    juce::ComboBox voicesBox;

    std::unique_ptr<ZoneMap> zoneMap;
    std::unique_ptr<juce::Component> voicesControl;
    std::vector<std::unique_ptr<Card>> cards;
    std::vector<std::unique_ptr<ParameterKnob>> knobs;
    std::unique_ptr<KeyboardPanel> keyboard;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SamplerEditor)
};

} // namespace aether::ui
