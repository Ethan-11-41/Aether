// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <functional>
#include <memory>

#include <juce_gui_basics/juce_gui_basics.h>

#include "NumberDragField.h"
#include "Theme.h"
#include "model/MidiTiming.h"

namespace aether::ui
{

/** The "Fix timing" window (Milestone 15): for MIDI clips played without the metronome.

    It finds the beat of the clips' notes and shows it: the notes as bars (like a small piano
    roll, in the time they were played), with a line on every beat found and brighter lines where
    bars start. If the beat is wrong, "Half as fast", "Twice as fast", or a tempo typed in look
    again; the arrows move where bar 1 starts (for a pick-up).

    Then one of two things, in one undo step:
      - Make it steady (the default): the notes move onto a steady beat, at the tempo played
        (the song's tempo becomes it) or the song's own tempo; a strength below 100% keeps some
        of the player's push and pull.
      - Make the song's tempo follow the playing: the notes stay exactly as played, and the song
        gets tempo changes every bar (or every beat) so its bars line up with them.

    A box at the bottom chooses whether the window opens by itself after recording MIDI without
    the metronome (the app keeps that setting).

    It always starts from how the clips were played, not from an earlier fix (the clips remember:
    see MidiTiming.h), so choices can be tried one after another; "Put it back as I played it"
    undoes the fixes.
*/
class TimingPanel final : public juce::Component
{
public:
    TimingPanel (const Theme& theme, Project& project, const juce::StringArray& clipIds, bool offeredAfterRecording);
    ~TimingPanel() override;

    /** The window should close (after Apply or Cancel). */
    std::function<void()> onClose;

    /** The "Offer this after recording without the metronome" box was changed. */
    std::function<void (bool)> onOfferChanged;

    enum class Mode { steady, follow };

    //==============================================================================
    // What the controls do (also for the tests).

    /** Looks for the beat again near a tempo (0: wherever it is). */
    void findBeatNear (double bpm);

    /** Moves the start of bar 1 by a number of beats (for a pick-up). */
    void moveBarStart (int beats);

    void setMode (Mode newMode);
    void setStrength (double newStrength);       // 0..1
    void setUseSongTempo (bool useSongTempo);    // steady: the song's tempo instead of the tempo played
    void setEveryBeat (bool everyBeat);          // follow: a tempo change on every beat instead of every bar

    /** Does it. Returns false if nothing could be changed. If the song was edited since the beat
        was found (the window doesn't block the main window), the beat is found again first. */
    bool apply();

    /** Puts the clips back as they were played (and the song's tempo as it was then), undoing
        earlier fixes, and closes. False if there was nothing to put back. */
    bool putBack();

    /** True if an earlier fix can be put back ("Put it back as I played it" shows). */
    bool canPutBack() const;

    const tempodetect::Result& getBeat() const noexcept   { return beat; }
    Mode getMode() const noexcept                          { return mode; }

    /** The tempo the steady option uses: the tempo played (whole BPM), or the song's. */
    double getSteadyTempo() const;

    /** What was found, in words (the line under the title), and where bar 1 will be. */
    juce::String getSummary() const;
    juce::String getOutcome() const;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class Picture;

    void analyse (double hintBpm);
    void refresh();

    const Theme& theme;
    Project& project;
    juce::StringArray clipIds;
    tempodetect::Result beat;
    double hint = 0.0;                 // the tempo the beat was looked for near (0: anywhere)
    int foundDownbeat = 0;             // where the analysis put bar 1 (before the arrows moved it)
    juce::uint64 analysedAt = 0;       // the project's change count when the beat was found
    Mode mode = Mode::steady;
    double strength = 1.0;
    bool useSongTempo = false, everyBeat = false;

    juce::Label title, summary, outcome, steadyText, followText, strengthLabel;
    juce::TextButton halfButton { "Half as fast" }, doubleButton { "Twice as fast" }, findButton { "Look again" };
    juce::Label nearLabel;
    NumberDragField nearField;
    juce::Label barStartLabel;
    juce::TextButton earlierButton { "<" }, laterButton { ">" };
    std::unique_ptr<Picture> picture;
    juce::Viewport pictureView;
    juce::ToggleButton steadyChoice { "Make it steady" }, followChoice { "Make the song's tempo follow your playing" };
    juce::ToggleButton playedTempoChoice, songTempoChoice;
    juce::Slider strengthSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::ToggleButton everyBarChoice { "A tempo change every bar (smoother)" }, everyBeatChoice { "Every beat (closer)" };
    juce::ToggleButton offerToggle { "Offer this after recording without the metronome" };
    juce::TextButton cancelButton { "Cancel" }, applyButton { "Apply" }, putBackButton { "Put it back as I played it" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TimingPanel)
};

} // namespace aether::ui
