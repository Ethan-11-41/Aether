// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <functional>
#include <memory>

#include <juce_gui_basics/juce_gui_basics.h>

#include "AutomationLanes.h"
#include "Theme.h"
#include "TimelineView.h"
#include "TrackListComponent.h"
#include "engine/AudioEngine.h"
#include "engine/AudioFileCache.h"
#include "io/ProjectFiles.h"
#include "model/ClipEdits.h"
#include "model/Project.h"
#include "time/Grid.h"

namespace aether::ui
{

/** The timeline on the right of the project window. From top to bottom:

    - the ruler: a loop strip (drag to set the loop, double-click to switch it on/off) and the
      bar numbers (click or drag to move the playhead),
    - the time signature lane (click a signature to change it, right-click to add one),
    - the tempo lane (double-click to add a tempo change, drag points, right-click for options),
    - one lane per track, lined up with the track headers on the left, holding the clips (and
      under a track, its automation lanes, if shown: see AutomationLaneArea):
      drag a clip to move it (also to another track), drag its edges to trim it, drag the small
      squares at its top corners to fade in/out, right-click for more; drop audio files here,
    - a scroll bar.

    Ctrl + mouse wheel zooms, Shift + mouse wheel scrolls sideways, and the mouse wheel on its
    own scrolls the tracks up and down. Positions snap to the grid (hold Alt for exact). The
    view follows the playhead during playback, and when it jumps off screen (e.g. Go to start).
*/
class TimelineComponent final : public juce::Component,
                                public juce::FileDragAndDropTarget,
                                private juce::ValueTree::Listener,
                                private juce::ScrollBar::Listener,
                                private juce::ChangeListener,
                                private juce::Timer
{
public:
    static constexpr int rulerHeight = 36;
    static constexpr int signatureHeight = 22;
    static constexpr int tempoHeight = 66;
    static constexpr int headerHeight = rulerHeight + signatureHeight + tempoHeight;
    static constexpr int scrollBarHeight = 14;
    static constexpr double nudgeSeconds = 0.010;   // fine nudge: 10 ms

    TimelineComponent (const Theme& theme, Project& project, AudioEngine& engine, AudioFileCache& audioFiles,
                       std::function<ProjectLocation()> location, juce::Viewport& trackViewport,
                       TrackListComponent& trackList);
    ~TimelineComponent() override;

    TimelineView& getView() noexcept   { return view; }

    /** The automation lanes (for the tests). */
    AutomationLaneArea& getAutomationArea() noexcept   { return *automationArea; }

    void zoomIn();
    void zoomOut();

    /** Redraws the playhead where the engine says it is, scrolling to keep it in view (called
        about 30 times a second). */
    void updatePlayhead();

    /** A recording in progress, drawn growing on its tracks up to the playhead. */
    struct LiveRecording
    {
        juce::StringArray trackIds;
        Tick start = 0;
    };

    std::function<LiveRecording()> liveRecording;

    /** Call when the track list scrolls, so the lanes follow. */
    void tracksScrolled();

    //==============================================================================
    // Snap grid

    void setGrid (GridSize newGrid);
    GridSize getGrid() const noexcept   { return grid; }

    /** Switches snapping off, or back on to the last grid used. */
    void toggleSnap();

    std::function<void()> onGridChanged;

    //==============================================================================
    // Clips

    const juce::StringArray& getSelectedClips() const noexcept   { return selectedClips; }
    bool hasSelectedClips() const noexcept                       { return ! selectedClips.isEmpty(); }
    void selectClips (const juce::StringArray& clipIds);
    void clearClipSelection();
    void selectAllClips();

    /** Splits the selected clips (or, if none, the clips on the selected track) at the playhead. */
    bool splitClipsAtPlayhead();
    bool duplicateSelectedClips();
    bool deleteSelectedClips();

    /** Copy / cut the selected clips, and paste them at the playhead on the selected track. */
    bool copySelectedClips();
    bool cutSelectedClips();
    bool pasteClips();
    bool canPaste() const noexcept   { return ! clipboard.empty(); }

    /** Adds one of Aether's sound effects (see aether::addSoundEffect) and selects it. Returns
        its clip's id (empty if it couldn't, after saying why). */
    juce::String addSoundEffect (const juce::String& effectId, int bars, const juce::String& trackId, Tick position);

    /** Changes a sound effect clip in place: another effect, another length, or made again at
        the song's tempo (see aether::changeSoundEffect). False (after saying why) if it couldn't. */
    bool changeSoundEffect (const juce::String& clipId, const juce::String& effectId, int bars, const juce::String& undoName);

    /** Reverses audio clips (see aether::reverseClips); problems are shown in a message. */
    void reverseClips (const juce::StringArray& clipIds);

    /** Moves the selected clips (or, if none, the playhead) one grid step, or 10 ms if `fine`. */
    void nudge (int direction, bool fine);

    /** Audio files were dropped on the timeline (track id is empty below the last track). */
    std::function<void (const juce::Array<juce::File>&, const juce::String& trackId, Tick position)> onImportFiles;

    /** "Import audio file here..." was chosen from a lane's menu. */
    std::function<void (const juce::String& trackId, Tick position)> onImportRequested;

    std::function<void()> onClipSelectionChanged;

    /** Open a MIDI clip's notes in the piano roll (double-click a MIDI clip). */
    std::function<void (const juce::String& clipId)> onEditNotes;       // its own editor (drum patterns: the drum editor)
    std::function<void (const juce::String& clipId)> onOpenPianoRoll;   // the piano roll, whatever it is

    /** "Fix timing..." and "Quantize..." from a MIDI clip's menu, for these clips (Milestone 15). */
    std::function<void (const juce::StringArray& clipIds)> onFixTiming, onQuantize;

    /** True while a clip's stretched audio is being made (shown on the clip). */
    std::function<bool (const juce::String& clipId)> isStretching;

    /** Makes an empty one-bar MIDI clip on a MIDI track, at the bar `position` is in, and opens it
        in the piano roll. Returns its id (empty if the row isn't a MIDI track). */
    juce::String createMidiClipAt (int row, Tick position);

    //==============================================================================
    void resized() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray& files, int x, int y) override;
    void fileDragMove (const juce::StringArray& files, int x, int y) override;
    void fileDragExit (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

private:
    class Ruler;
    class SignatureLane;
    class TempoLane;
    class TrackLanes;
    friend class Ruler;
    friend class SignatureLane;
    friend class TempoLane;
    friend class TrackLanes;

    void rebuildTempoMap();
    void updateScrollBar();
    void repaintAll();
    int playheadX() const;
    Tick snap (Tick tick, const juce::ModifierKeys& mods) const;
    void drawGrid (juce::Graphics&, juce::Rectangle<int> area, bool strong) const;
    void setSelection (const juce::StringArray& ids);
    juce::File resolve (const juce::String& relativePath) const;
    int trackRowAt (int yInTimeline) const;

    void timerCallback() override;
    void scrollBarMoved (juce::ScrollBar*, double newRangeStart) override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override  { projectChanged(); }
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override               { projectChanged(); }
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override        { projectChanged(); }
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override                { projectChanged(); }
    void projectChanged();

    const Theme& theme;
    Project& project;
    AudioEngine& engine;
    AudioFileCache& audioFiles;
    std::function<ProjectLocation()> location;
    juce::Viewport& trackViewport;
    TrackListComponent& trackList;
    juce::ValueTree state;

    TimelineView view;
    TempoMap tempoMap;   // a copy of the project's tempo, for drawing

    GridSize grid = GridSize::beat, lastGrid = GridSize::beat;
    juce::StringArray selectedClips;
    ClipClipboard clipboard;   // per project view, so it never refers to another project's files

    std::unique_ptr<Ruler> ruler;
    std::unique_ptr<SignatureLane> signatureLane;
    std::unique_ptr<TempoLane> tempoLane;
    std::unique_ptr<TrackLanes> trackLanes;
    std::unique_ptr<AutomationLaneArea> automationArea;   // over the track lanes, on the automation rows
    juce::ScrollBar scrollBar { false };

    bool fileDragActive = false;
    int dropRow = -1;
    Tick dropTick = 0;

    int lastPlayheadX = -1000;
    Tick lastPlayheadPosition = 0;
    bool updatingScrollBar = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TimelineComponent)
};

} // namespace aether::ui
