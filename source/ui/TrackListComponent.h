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

    - Click to select; double-click the name (or press F2) to rename.
    - Click the coloured strip, or right-click, to change the colour.
    - Drag a track up or down to reorder it.
    All changes go through the undoable functions in model/EditCommands.h.
*/
class TrackListComponent final : public juce::Component,
                                 private juce::ValueTree::Listener
{
public:
    static constexpr int rowHeight = 56;

    TrackListComponent (const Theme& theme, Project& project);
    ~TrackListComponent() override;

    juce::String getSelectedTrackId() const     { return selectedTrackId; }
    void selectTrack (const juce::String& trackId);
    void selectRelative (int delta);

    /** Shows the rename box on the selected track. */
    void startRenamingSelected();

    /** Called whenever the selected track changes. */
    std::function<void()> onSelectionChanged;

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
    void showColourMenu (const juce::String& trackId);
    void showContextMenu (const juce::String& trackId);
    int insertIndexForY (int y) const;

    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override               { rebuild(); }
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override        { rebuild(); }
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override                { rebuild(); }
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override  { repaintHeaders(); }
    void repaintHeaders();

    const Theme& theme;
    Project& project;
    juce::ValueTree tracksNode;
    std::vector<std::unique_ptr<Header>> headers;
    juce::String selectedTrackId;

    juce::String draggingTrackId;
    int dropIndex = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackListComponent)
};

} // namespace aether::ui
