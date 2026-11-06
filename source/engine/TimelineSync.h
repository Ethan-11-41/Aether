// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <functional>

#include <juce_events/juce_events.h>

#include "AudioEngine.h"
#include "AudioFileCache.h"
#include "TimelineSnapshot.h"
#include "io/ProjectFiles.h"
#include "model/Project.h"

namespace aether
{

/** Finds the decoded audio for a clip's file (a path relative to the project folder).
    Returns null if it isn't ready yet. */
using AudioResolver = std::function<std::shared_ptr<const AudioData> (const juce::String& relativePath)>;

/** Builds the audio engine's view of a project: tempo, loop and (if `resolver` is given) every
    clip that has its audio ready, with positions in samples at `sampleRate`. */
std::unique_ptr<TimelineSnapshot> buildTimelineSnapshot (const Project& project, double sampleRate = 0.0,
                                                         const AudioResolver& resolver = {});

/** Keeps the audio engine up to date with a project: whenever the tempo, loop or clips change,
    or an audio file finishes loading, a new TimelineSnapshot is built and handed to the engine.
    Also tells the file cache the engine's sample rate, and tidies up old snapshots.
    UI thread only.

    When it is destroyed (the project closes), playback stops and the engine goes back to an
    empty timeline. */
class TimelineSync final : private juce::ValueTree::Listener,
                           private juce::ChangeListener,
                           private juce::Timer
{
public:
    TimelineSync (Project& project, AudioEngine& engine, AudioFileCache& cache,
                  std::function<ProjectLocation()> location);
    ~TimelineSync() override;

    /** Sends the current timeline to the engine now. */
    void update();

private:
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override  { update(); }
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override               { update(); }
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override        { update(); }
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override                { update(); }
    void changeListenerCallback (juce::ChangeBroadcaster*) override                       { update(); }
    void timerCallback() override;

    Project& project;
    AudioEngine& engine;
    AudioFileCache& cache;
    std::function<ProjectLocation()> location;
    juce::ValueTree state;
};

} // namespace aether
