// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <juce_events/juce_events.h>

#include "AudioEngine.h"
#include "TimelineSnapshot.h"
#include "model/Project.h"

namespace aether
{

/** Builds the audio engine's view of a project's timeline. */
std::unique_ptr<TimelineSnapshot> buildTimelineSnapshot (const Project& project);

/** Keeps the audio engine up to date with a project: whenever the tempo, time signatures or
    loop change, a new TimelineSnapshot is built and handed to the engine. Also tidies up the
    snapshots the engine has finished with. UI thread only.

    When it is destroyed (the project closes), playback stops and the engine goes back to the
    default timeline. */
class TimelineSync final : private juce::ValueTree::Listener,
                           private juce::Timer
{
public:
    TimelineSync (Project& project, AudioEngine& engine);
    ~TimelineSync() override;

    /** Sends the current timeline to the engine now. */
    void update();

private:
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override  { update(); }
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override               { update(); }
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override        { update(); }
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override                { update(); }
    void timerCallback() override                                                         { engine.collectGarbage(); }

    Project& project;
    AudioEngine& engine;
    juce::ValueTree state;
};

} // namespace aether
