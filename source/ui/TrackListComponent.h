// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <functional>
#include <memory>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"
#include "TrackRows.h"
#include "model/Project.h"

namespace aether::ui
{

/** The list of track headers on the left of the project window.

    - Click to select; Ctrl+click adds or removes a track from the selection, Shift+click selects
      a range. Double-click the name (or press F2) to rename.
    - Each header has mute (M), solo (S), volume, pan and a level meter; audio tracks also have
      record arm (R) and input monitoring (I). With several tracks selected, using a control on
      one of them changes them all (volume and pan move by the same amount).
    - Click the coloured strip, or right-click, to change the colour.
    - Drag a track up or down to reorder it.
    - The A button shows the track's automation lanes (Milestone 14), each with its own header
      under the track.
    All changes go through the undoable functions in model/EditCommands.h and model/Mixer.h.
*/
class TrackListComponent final : public juce::Component,
                                 public juce::FileDragAndDropTarget,
                                 private juce::ValueTree::Listener
{
public:
    static constexpr int rowHeight = TrackRows::trackHeight;

    TrackListComponent (const Theme& theme, Project& project);
    ~TrackListComponent() override;

    /** The selected track that has the focus (the one last clicked). */
    juce::String getSelectedTrackId() const     { return selectedTrackId; }

    /** Every selected track, top to bottom. */
    juce::StringArray getSelectedTrackIds() const;
    bool isTrackSelected (const juce::String& trackId) const   { return selectedIds.contains (trackId); }

    /** The tracks a control on `trackId` should change: all selected tracks if it is one of
        them, otherwise just that track. */
    juce::StringArray tracksToEdit (const juce::String& trackId) const;

    /** Selects just this track. */
    void selectTrack (const juce::String& trackId);

    /** Selects as a click with these modifier keys would: Ctrl adds/removes, Shift selects a range. */
    void selectTrack (const juce::String& trackId, juce::ModifierKeys mods);

    /** Gives a track the focus, keeping the selection if it's already part of it. */
    void focusTrack (const juce::String& trackId);

    void selectRelative (int delta);

    /** Updates the level meters, given each channel's peak levels since the last call. */
    void updateMeters (const std::function<std::pair<float, float> (const juce::String&)>& peaksOf, float elapsedSeconds);

    /** Shows the rename box on the selected track. */
    void startRenamingSelected();

    /** Shows a track's right-click menu (rename, colour, mute, solo, output, delete...). */
    void showTrackMenu (const juce::String& trackId)   { showContextMenu (trackId); }

    /** The automation menu (the A button): which settings' lanes are shown under the track. */
    void showAutomationMenu (const juce::String& trackId);

    /** The lane headers, for the tests. */
    int getNumLaneHeaders() const noexcept   { return (int) laneHeaders.size(); }

    /** Opens a MIDI track's instrument settings: the Synth's window (through onOpenInstrument), or
        for the starter synth a pop-up pointing at `screenArea`. */
    void showInstrumentEditor (const juce::String& trackId, juce::Rectangle<int> screenArea);

    /** Opens the Synth's window for a track (set by the app). */
    std::function<void (const juce::String& trackId)> onOpenInstrument;

    /** "Fix the timing..." and "Quantize..." from a MIDI track's menu, for its clips (Milestone 15). */
    std::function<void (const juce::StringArray& clipIds)> onFixTiming, onQuantize;

    /** Called whenever the selected track changes. */
    std::function<void()> onSelectionChanged;

    /** How many inputs the sound card has (for the Input menu). */
    std::function<int()> inputChannelCount;

    /** Which files can be dropped on the track list (audio and MIDI files). */
    std::function<bool (const juce::File&)> canImport;

    /** Files dropped on a track (`trackId`) or below the tracks (empty `trackId`: new tracks). */
    std::function<void (const juce::Array<juce::File>&, const juce::String& trackId)> onImportFiles;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray& files, int x, int y) override;
    void fileDragMove (const juce::StringArray& files, int x, int y) override;
    void fileDragExit (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

    /** The row a file drop would go to (-1: none; the number of tracks: a new track). For the tests. */
    int getFileDropRow() const noexcept   { return fileDropRow; }

    /** Height needed to show every track (for the scrolling area). */
    int getIdealHeight() const;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    // Drag to reorder: called by the track headers as the mouse moves (public for the tests).
    void dragStarted (const juce::String& trackId);
    void dragMoved (int yInList);
    void dragEnded();

    /** The rename box, while a track is being renamed (for the tests). */
    juce::TextEditor* getActiveRenameEditor() const;

private:
    class Header;
    class LaneHeader;
    friend class Header;
    friend class LaneHeader;

    void rebuild();
    void selectionChanged();
    void showColourMenu (const juce::String& trackId);
    void showContextMenu (const juce::String& trackId);
    int insertIndexForY (int y) const;

    // Only tracks being added, removed or moved need new headers (not clips, sends or effects);
    // lanes shown, hidden, added or removed need new lane headers.
    void valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree&) override        { childrenChanged (parent); }
    void valueTreeChildRemoved (juce::ValueTree& parent, juce::ValueTree&, int) override { childrenChanged (parent); }
    void valueTreeChildOrderChanged (juce::ValueTree& parent, int, int) override         { childrenChanged (parent); }
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void childrenChanged (const juce::ValueTree& parent);
    void rebuildLanes();
    void repaintHeaders();

    const Theme& theme;
    Project& project;
    juce::ValueTree tracksNode;
    std::vector<std::unique_ptr<Header>> headers;
    std::vector<std::unique_ptr<LaneHeader>> laneHeaders;
    juce::String selectedTrackId, anchorTrackId;
    juce::StringArray selectedIds;

    juce::String draggingTrackId;
    int dropIndex = -1;
    int fileDropRow = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackListComponent)
};

} // namespace aether::ui
