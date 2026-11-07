// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <functional>

#include <juce_gui_basics/juce_gui_basics.h>

#include "AppContext.h"
#include "engine/Recorder.h"
#include "engine/TimelineSync.h"
#include "model/Project.h"
#include "ui/KeyboardPanel.h"
#include "ui/MixerComponent.h"
#include "ui/PianoRoll.h"
#include "ui/SynthEditor.h"
#include "ui/TimelineComponent.h"
#include "ui/TrackListComponent.h"
#include "ui/TransportBar.h"

namespace aether
{

/** The window contents while a project is open: the toolbar and transport at the top, the
    track headers on the left, the timeline on the right, and the mixer (when shown) below. */
class ProjectView final : public juce::Component,
                          private juce::Timer
{
public:
    ProjectView (AppContext& context, Project& project);
    ~ProjectView() override;

    ui::TrackListComponent& getTrackList() noexcept   { return trackList; }
    ui::TimelineComponent& getTimeline() noexcept     { return *timeline; }
    ui::MixerComponent& getMixer() noexcept           { return *mixer; }
    Project& getProject() noexcept                    { return project; }

    /** Starts recording the armed tracks, or stops if already recording. Problems (e.g. no
        track armed) are explained in a message box. */
    void toggleRecording();
    void stopRecording()                              { recorder->stop(); }
    bool isRecording() const noexcept                 { return recorder->isActive(); }

    /** If recording, stops and adds the recorded clips now (before the project is closed). */
    void finishRecording()                            { recorder->finishNow(); }

    /** Shows or hides the mixer below the timeline (remembered between sessions). */
    void setMixerVisible (bool shouldBeVisible);
    bool isMixerVisible() const noexcept              { return mixerVisible; }

    /** Shows or hides the on-screen keyboard at the bottom (remembered between sessions). */
    void setKeyboardVisible (bool shouldBeVisible);
    bool isKeyboardVisible() const noexcept           { return keyboardVisible; }

    /** Imports audio files onto a track (or new tracks) at a position, and selects the new clips. */
    void importFiles (const juce::Array<juce::File>& files, const juce::String& trackId, Tick position);

    /** Asks which audio files to import, then imports them at `position` on `trackId`. */
    void chooseFilesToImport (const juce::String& trackId, Tick position);

    /** Opens (or brings forward) the piano roll window, showing a MIDI clip's notes. */
    void showPianoRoll (const juce::String& clipId);
    ui::PianoRoll* getPianoRoll() noexcept            { return pianoRoll; }

    /** Opens (or brings forward) the Synth's window for a MIDI track. */
    void showSynth (const juce::String& trackId);
    ui::SynthEditor* getSynthEditor() noexcept        { return synthEditor; }

    static constexpr const char* gridKey = "gridSize";
    static constexpr const char* mixerVisibleKey = "mixerVisible";
    static constexpr const char* mixerHeightKey = "mixerHeight";
    static constexpr const char* keyboardVisibleKey = "keyboardVisible";

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    /** A scrolling area that says when it has been scrolled, so the timeline lanes can follow. */
    struct TrackViewport final : public juce::Viewport
    {
        std::function<void()> onScroll;
        void visibleAreaChanged (const juce::Rectangle<int>&) override   { if (onScroll) onScroll(); }
    };

    /** The bar between the timeline and the mixer: drag it to change the mixer's height. */
    struct Divider final : public juce::Component, public juce::SettableTooltipClient
    {
        std::function<void (int)> onDrag;   // how far the mouse has moved up since the drag began
        std::function<void()> onDragStart;
        const ui::Theme* theme = nullptr;

        Divider()
        {
            setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
            setTooltip ("Drag up or down to make the mixer taller or shorter.");
        }
        void mouseDown (const juce::MouseEvent&) override   { if (onDragStart) onDragStart(); }
        void mouseDrag (const juce::MouseEvent& e) override { if (onDrag) onDrag (-e.getDistanceFromDragStartY()); }
        void paint (juce::Graphics& g) override
        {
            g.fillAll (theme->outline);
            g.setColour (theme->textDim.withAlpha (0.5f));
            g.fillRoundedRectangle (getLocalBounds().toFloat().withSizeKeepingCentre (40.0f, 2.0f), 1.0f);
        }
    };

    void timerCallback() override;

    void showProblem (const juce::String& title, const juce::String& message);

    AppContext& ctx;
    Project& project;
    TimelineSync timelineSync;
    std::unique_ptr<Recorder> recorder;

    juce::TextButton undoButton, redoButton, addAudioButton, addMidiButton, addBusButton, mixerButton;
    ui::TransportBar transportBar;
    TrackViewport trackViewport;
    ui::TrackListComponent trackList;
    std::unique_ptr<ui::TimelineComponent> timeline;
    std::unique_ptr<ui::MixerComponent> mixer;
    std::unique_ptr<ui::KeyboardPanel> keyboard;
    bool keyboardVisible = false;
    Divider divider;

    /** "MIDI" in the toolbar, lit for a moment whenever a MIDI message arrives. */
    struct MidiLight final : public juce::Component, public juce::SettableTooltipClient
    {
        const ui::Theme* theme = nullptr;
        bool lit = false;
        void paint (juce::Graphics& g) override
        {
            g.setColour (lit ? theme->accent : theme->panelRaised);
            g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (2.0f), 4.0f);
            g.setColour (lit ? theme->accentText : theme->textDim);
            g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
            g.drawText ("MIDI", getLocalBounds(), juce::Justification::centred);
        }
    } midiLight;
    std::uint32_t lastMidiActivity = 0;
    double midiLitUntil = 0.0;
    int tooltipRefresh = 0;
    bool mixerVisible = false;
    int mixerHeight = 340, heightAtDragStart = 340;
    double lastMeterTime = 0.0;
    std::unique_ptr<juce::DocumentWindow> pianoRollWindow;   // its own window (closes with the project)
    ui::PianoRoll* pianoRoll = nullptr;                      // inside pianoRollWindow
    std::unique_ptr<juce::DocumentWindow> synthWindow;       // the Synth's settings (closes with the project)
    ui::SynthEditor* synthEditor = nullptr;                  // inside synthWindow
    juce::ComboBox gridBox;
    juce::Rectangle<int> toolbarArea, laneHeaderArea;
    std::unique_ptr<juce::FileChooser> fileChooser;
    juce::ScopedMessageBox messageBox;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProjectView)
};

} // namespace aether
