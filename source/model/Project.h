// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <juce_data_structures/juce_data_structures.h>

#include "Ids.h"
#include "Track.h"

namespace aether
{

/** A project (a song): all of its data, plus its undo history.

    The data lives in a juce::ValueTree, which lets the user interface watch for changes and
    gives undo/redo for free. All changes must go through the UndoManager (the functions in
    EditCommands.h do this), so that every edit can be undone.

    Use from the UI (message) thread only. The audio thread never reads this directly.
*/
class Project final : private juce::ValueTree::Listener
{
public:
    /** A new, empty project. */
    explicit Project (const juce::String& name = "Untitled");

    /** A project made from loaded data. `state` must be a "Project" tree (see ProjectFormat). */
    static std::unique_ptr<Project> fromState (juce::ValueTree state);

    ~Project() override;

    juce::ValueTree& getState() noexcept              { return state; }
    const juce::ValueTree& getState() const noexcept  { return state; }
    juce::UndoManager& getUndoManager() noexcept      { return undoManager; }

    juce::String getName() const                      { return state[ids::name].toString(); }

    //==============================================================================
    // Tracks

    int getNumTracks() const;
    Track getTrack (int index) const;
    Track findTrack (const juce::String& trackId) const;
    int indexOfTrack (const juce::String& trackId) const;
    juce::ValueTree getTracksNode() const             { return state.getChildWithName (ids::Tracks); }

    //==============================================================================
    // "Has this changed since it was last saved?"

    /** Increases every time anything in the project changes (including undo and redo). */
    juce::uint64 getChangeCount() const noexcept      { return changeCount; }

    bool hasUnsavedChanges() const noexcept           { return forcedUnsaved || changeCount != savedChangeCount; }
    void markAsSaved() noexcept                       { savedChangeCount = changeCount; forcedUnsaved = false; }

    /** Treats the project as changed even though nothing has been edited yet (used for a project
        recovered from an autosave, which differs from the saved file). */
    void markAsUnsaved() noexcept                     { forcedUnsaved = true; }

private:
    explicit Project (juce::ValueTree loadedState, bool);

    void ensureStructure();
    void attachListener();

    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override  { ++changeCount; }
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override               { ++changeCount; }
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override        { ++changeCount; }
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override                { ++changeCount; }

    juce::ValueTree state;
    juce::UndoManager undoManager;
    juce::uint64 changeCount = 0, savedChangeCount = 0;
    bool forcedUnsaved = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Project)
};

} // namespace aether
