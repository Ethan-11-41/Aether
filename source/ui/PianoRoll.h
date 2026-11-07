// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <functional>
#include <memory>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"
#include "TimelineView.h"
#include "engine/AudioEngine.h"
#include "model/NoteEdits.h"
#include "time/Grid.h"

namespace aether::ui
{

/** The piano roll: edits the notes of one MIDI clip.

    - Click on an empty place to add a note there (as long as the last note you drew or resized;
      keep the mouse button down and drag to make it longer).
    - Drag a note to move it (Alt: without snapping), its right edge to change its length.
      Right-click a note to delete it. Ctrl+click adds to the selection, Ctrl+drag draws a box.
    - The velocity lane at the bottom: drag a note's stem up or down.
    - Keys: Delete, Ctrl+A, Esc, Ctrl+C / X / V, Ctrl+D (duplicate), Up / Down (a semitone,
      Shift: an octave), Left / Right (a grid step), Q (quantize). Other keys (Space, Ctrl+Z...)
      go on to Aether's shortcuts.

    The horizontal axis is the song's time (the same bars as the timeline); the clip's part is
    lit, the rest dimmed. */
class PianoRoll final : public juce::Component,
                        private juce::ValueTree::Listener,
                        private juce::ScrollBar::Listener,
                        private juce::Timer
{
public:
    PianoRoll (const Theme& theme, Project& project, AudioEngine& engine);
    ~PianoRoll() override;

    /** Shows a clip's notes (an empty id shows nothing). */
    void setClip (const juce::String& clipId);
    juce::String getClipId() const;

    /** Called when the clip shown changes, is renamed or disappears (e.g. to update the title). */
    std::function<void()> onClipChanged;

    // Settings ----------------------------------------------------------------------------------
    void setGrid (GridSize size);
    GridSize getGrid() const noexcept                 { return grid; }

    /** Hear notes as they're added, moved or changed. */
    void setPreviewEnabled (bool shouldPreview);
    bool isPreviewEnabled() const noexcept            { return previewEnabled; }

    /** How long new notes are: 0 = as long as the last note drawn or resized. */
    void setFixedNoteLength (Tick length);
    Tick getNewNoteLength() const;

    struct QuantizeSettings { double strength = 1.0, swing = 0.0; bool ends = false; };
    QuantizeSettings quantizeSettings;

    struct HumanizeSettings { double timingMs = 10.0; int velocity = 8; };
    HumanizeSettings humanizeSettings;

    /** Where previewed notes go (normally the engine; the tests listen here instead). */
    std::function<void (int pitch, int velocity, bool on)> previewNote;

    // Editing (also used by the keys and the toolbar) --------------------------------------------
    const NoteNodes& getSelection() const noexcept    { return selection; }
    void setSelection (NoteNodes notes);
    void selectAll();
    void deleteSelected();
    void duplicateSelected();
    void transposeSelected (int semitones);
    void nudgeSelected (int gridSteps, bool fine);
    void quantize();          // the selected notes, or all of them if none is selected
    void humanize();          // the same
    void copySelected();
    void cutSelected();
    void paste();             // at the playhead

    // Geometry (for the tests) --------------------------------------------------------------------
    juce::Component& getNoteGrid() noexcept;
    juce::Component& getVelocityLane() noexcept;
    juce::Component& getKeys() noexcept;
    TimelineView& getView() noexcept                  { return view; }

    /** Where a note position is drawn in the note grid (top-left of its cell). */
    juce::Point<int> positionOf (Tick contentTick, int pitch) const;
    int getRowHeight() const noexcept                 { return rowHeight; }

    /** Scrolls so a pitch is in the middle of the note grid. */
    void scrollToPitch (int pitch);

    bool keyPressed (const juce::KeyPress&) override;
    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int toolbarHeight = 44, rulerHeight = 24, keysWidth = 84, velocityHeight = 90, scrollBarSize = 12;

private:
    class Ruler;
    class Keys;
    class Grid;
    class VelocityLane;
    friend class Ruler;
    friend class Keys;
    friend class Grid;
    friend class VelocityLane;

    // Time and pitch <-> pixels (note grid coordinates).
    Tick toTimeline (Tick contentTick) const;
    Tick toContent (Tick timelineTick) const;
    int pitchToY (int pitch) const;
    int yToPitch (int y) const;
    Tick snapDown (Tick timelineTick, const juce::ModifierKeys& mods) const;
    Tick snapNearest (Tick timelineTick, const juce::ModifierKeys& mods) const;
    Tick stepAt (Tick timelineTick) const;   // the grid step (1/16 when the grid is off)

    Clip getClip() const;
    juce::String getTrackId() const;
    juce::Colour noteColour (int velocity, bool selected) const;
    int songKey() const;
    bool inScale (int pitch) const;
    bool scaleActive() const;
    bool namesWithFlats() const;   // notes named in the key's spelling (Bb in F major, A# in B major)

    void play (int pitch, int velocity, bool on, bool always = false);
    void releasePreview();
    void tidySelection();
    void updateScrollBars();
    void updateToolbar();
    void updateTempo();
    void showQuantizePanel();
    void showHumanizePanel();
    void zoomToClip();

    void timerCallback() override;
    void scrollBarMoved (juce::ScrollBar*, double newRangeStart) override;
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override   { modelChanged(); }
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override               { modelChanged(); }
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override        { modelChanged(); }
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override                { modelChanged(); }
    void modelChanged();

    const Theme& theme;
    Project& project;
    AudioEngine& engine;
    juce::ValueTree clipState;
    TempoMap tempo;
    TimelineView view;
    GridSize grid = GridSize::sixteenth;
    Tick lastLength = ticksPerQuarter / 4, fixedLength = 0;
    bool previewEnabled = true;
    int rowHeight = 14, scrollY = 0;
    NoteNodes selection;
    int sounding = -1;   // the note being previewed
    Tick lastPlayhead = -1;
    bool repaintPending = false;

    std::unique_ptr<Ruler> ruler;
    std::unique_ptr<Keys> keys;
    std::unique_ptr<Grid> noteGrid;
    std::unique_ptr<VelocityLane> velocityLane;
    juce::ScrollBar hScroll { false }, vScroll { true };

    juce::Label titleLabel;
    juce::ComboBox gridBox, lengthBox, keyBox, scaleBox;
    juce::TextButton quantizeButton { "Quantize" }, humanizeButton { "Humanize" }, previewButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PianoRoll)
};

} // namespace aether::ui
