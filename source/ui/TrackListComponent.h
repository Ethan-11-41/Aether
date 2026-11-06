// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <functional>
#include <memory>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"
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
    All changes go through the undoable functions in model/EditCommands.h and model/Mixer.h.
*/
class TrackListComponent final : public juce::Component,
                                 private juce::ValueTree::Listener
{
public:
    static constexpr int rowHeight = 56;

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

    /** Opens a MIDI track's instrument settings in a pop-up pointing at `screenArea`. */
    void showInstrumentEditor (const juce::String& trackId, juce::Rectangle<int> screenArea);

    /** Called whenever the selected track changes. */
    std::function<void()> onSelectionChanged;

    /** How many inputs the sound card has (for the Input menu). */
    std::function<int()> inputChannelCount;

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
    friend class Header;

    void rebuild();
    void selectionChanged();
    void showColourMenu (const juce::String& trackId);
    void showContextMenu (const juce::String& trackId);
    int insertIndexForY (int y) const;

    // Only tracks being added, removed or moved need new headers (not clips, sends or effects).
    void valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree&) override        { if (parent == tracksNode) rebuild(); }
    void valueTreeChildRemoved (juce::ValueTree& parent, juce::ValueTree&, int) override { if (parent == tracksNode) rebuild(); }
    void valueTreeChildOrderChanged (juce::ValueTree& parent, int, int) override         { if (parent == tracksNode) rebuild(); }
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override  { repaintHeaders(); }
    void repaintHeaders();

    const Theme& theme;
    Project& project;
    juce::ValueTree tracksNode;
    std::vector<std::unique_ptr<Header>> headers;
    juce::String selectedTrackId, anchorTrackId;
    juce::StringArray selectedIds;

    juce::String draggingTrackId;
    int dropIndex = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackListComponent)
};

} // namespace aether::ui
