// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <functional>

#include <juce_events/juce_events.h>

#include "ProjectFiles.h"

namespace aether
{

/** Saves a copy of the project in the background every so often, so that if Aether or the
    computer crashes, at most a minute or so of work is lost.

    The copy goes to the project's Autosave/ folder; the real project file is only changed when
    the user saves. The project is turned into text on the UI thread (fast), and the file is
    written on a background thread so the interface never stutters.
*/
class Autosaver final : private juce::Timer
{
public:
    static constexpr int defaultIntervalSeconds = 60;

    Autosaver (Project& project, ProjectLocation location, int intervalSeconds = defaultIntervalSeconds);
    ~Autosaver() override;

    /** Writes an autosave now if anything changed since the last one. Returns true if it did. */
    bool autosaveNow();

    /** Deletes the autosave (after a real save, or when the user chose not to keep changes). */
    void discard();

    /** Waits until background writing has finished (used by tests and at shutdown). */
    void waitUntilIdle();

    juce::Time getLastAutosaveTime() const noexcept   { return lastAutosaveTime; }

    /** Called on the UI thread after each autosave is handed to the background writer. */
    std::function<void()> onAutosave;

private:
    void timerCallback() override   { autosaveNow(); }

    Project& project;
    ProjectLocation location;
    juce::uint64 lastAutosavedChangeCount;
    juce::Time lastAutosaveTime;
    juce::ThreadPool writer;
};

} // namespace aether
