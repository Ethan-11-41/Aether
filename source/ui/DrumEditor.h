// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <functional>
#include <memory>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

#include "EffectEditor.h"
#include "Theme.h"
#include "dsp/DrumKit.h"
#include "engine/AudioEngine.h"
#include "model/DrumPatterns.h"

namespace aether::ui
{

/** The drum editor (Milestone 11): a step sequencer for one drum pattern, and the kit's pads.

    - Left: the 16 pads (kick, snares, hi-hats...). Click one to hear it and edit it on the right;
      drop an audio file on one to use that sound.
    - Middle: the steps. Click a step to turn it on or off (keep the button down and drag to do
      several), Shift+click for an accent (hit harder; again to undo it), right-click to remove a
      hit. Below: how hard each
      step of the selected pad is hit; drag the bars up or down.
    - Top: the kit, the pattern's length, the step size and swing.
    - Right: the selected pad's sound (built-in or from a file), tune, volume, pan, attack,
      length and choke group.

    Every change is undoable and follows undo/redo. */
class DrumEditor final : public juce::Component,
                         public juce::FileDragAndDropTarget,
                         private juce::ValueTree::Listener,
                         private juce::Timer
{
public:
    DrumEditor (const Theme& theme, Project& project, AudioEngine& engine);
    ~DrumEditor() override;

    /** Shows a pattern (and its track's kit). */
    void setClip (const juce::String& clipId);

    /** Shows a track's kit and its first pattern (if it has one). */
    void setTrack (const juce::String& trackId);

    juce::String getClipId() const      { return clipId; }
    juce::String getTrackId() const     { return trackId; }

    /** For the window title, e.g. "Drums - Beat". */
    juce::String getTitle() const;
    std::function<void()> onTitleChanged;

    /** How a pad is heard (normally through the engine; the tests listen here). */
    std::function<void (int note, int velocity)> playPad;

    /** Put a file on a pad (the app copies it into the project), or ask for one. */
    std::function<void (int pad, const juce::File& file)> onImportSound;
    std::function<void (int pad)> onChooseSoundFile;

    /** Make a new track that plays a pad's sound as notes (an 808 bass line). */
    std::function<void (int pad)> onPlayPadAsNotes;

    /** Open the pattern in the piano roll. */
    std::function<void (const juce::String& clipId)> onOpenPianoRoll;

    /** Play the pattern over and over (true), or stop (false). The app does the playing (on its
        own, or with the song if "With song" is on) and calls setPatternPlaying() to say so.
        Space in the drum editor does the same as the Play button. */
    std::function<void (bool play)> onPlayPattern;
    void togglePatternPlay();
    void setPatternPlaying (bool isPlaying);
    bool isPatternPlaying() const noexcept   { return patternPlaying; }
    bool playsWithSong() const noexcept      { return withSongButton.getToggleState(); }

    /** Where the pattern plays on the timeline: one time through it (empty if there's none). */
    juce::Range<Tick> getPatternRange() const;

    // Editing (the mouse does these; the tests too) -------------------------------------------
    void selectPad (int pad);
    int getSelectedPad() const noexcept   { return selectedPad; }
    void hearPad (int pad, int velocity = drumpattern::defaultVelocity);
    void chooseSound (int pad, const juce::String& reference);
    void addPatternAtPlayhead();
    int getNumSteps() const;

    // For the tests -----------------------------------------------------------------------------
    juce::Component& getStepArea() noexcept;
    juce::Component& getPadList() noexcept;
    juce::Point<float> stepPosition (int pad, int step) const;      // in the step area
    juce::Point<float> lanePosition (int step, int velocity) const; // in the step area
    juce::Point<float> padPosition (int pad) const;                 // in the pad list
    int getPlayingStep() const noexcept   { return playingStep; }
    ParameterKnob* findKnob (dsp::drumkit::PadSetting setting) const;
    ParameterKnob* getKitVolumeKnob() const noexcept   { return kitVolumeKnob.get(); }
    juce::ComboBox& getKitBox() noexcept      { return kitBox; }
    juce::ComboBox& getLengthBox() noexcept   { return lengthBox; }
    juce::ComboBox& getStepBox() noexcept     { return stepBox; }
    juce::ComboBox& getChokeBox() noexcept    { return chokeBox; }
    juce::Slider& getSwingSlider() noexcept   { return swingSlider; }
    juce::TextButton& getAddPatternButton() noexcept   { return actionButton; }
    juce::TextButton& getPlayButton() noexcept         { return playButton; }
    juce::TextButton& getWithSongButton() noexcept     { return withSongButton; }
    juce::TextButton& getAsNotesButton() noexcept      { return asNotesButton; }
    juce::String getPadName (int pad) const;
    bool isShowingSteps() const;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

    /** A pad's colour, from the kind of sound on it (kicks, snares, hi-hats...). */
    juce::Colour padColour (int pad) const;

    static constexpr int headerHeight = 56, padListWidth = 190, panelWidth = 272, rulerHeight = 24, laneHeight = 92;

private:
    class PadList;
    class StepArea;

    Clip clip() const;
    juce::ValueTree instrument() const;
    bool isDrumTrack() const;
    int rowOfPad (int pad) const;
    void refresh();
    void rebuildKnobs();
    void showSoundMenu (int pad);
    int padAt (juce::Point<int> positionInEditor) const;

    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override   { refresh(); }
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override               { refresh(); }
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override        { refresh(); }
    void timerCallback() override;

    const Theme& theme;
    Project& project;
    AudioEngine& engine;
    juce::ValueTree projectState;
    juce::String clipId, trackId;
    int selectedPad = 0, playingStep = -1;
    bool swingDragging = false, refreshing = false, patternPlaying = false;

    juce::Label titleLabel, messageLabel, padTitle, soundLabel;
    juce::ComboBox kitBox, lengthBox, stepBox, chokeBox;
    juce::Slider swingSlider;
    juce::TextButton pianoRollButton { "Piano roll" }, actionButton, soundButton { "Choose sound..." }, hearButton { "Hear" };
    juce::TextButton playButton { "Play" }, withSongButton { "With song" };
    juce::TextButton asNotesButton { "Play it as notes..." };

    std::unique_ptr<PadList> padList;
    std::unique_ptr<StepArea> stepArea;
    juce::Viewport stepViewport;
    std::vector<std::unique_ptr<ParameterKnob>> knobs;   // the selected pad's (tune, volume, pan, attack, length)
    std::unique_ptr<ParameterKnob> kitVolumeKnob;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumEditor)
};

} // namespace aether::ui
