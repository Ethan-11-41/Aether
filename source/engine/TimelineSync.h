// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <functional>

#include <juce_events/juce_events.h>

#include "AudioEngine.h"
#include "AudioFileCache.h"
#include "MixerResources.h"
#include "TimelineSnapshot.h"
#include "io/ProjectFiles.h"
#include "model/Project.h"

namespace aether
{

/** Finds the decoded audio for a clip's file (a path relative to the project folder).
    Returns null if it isn't ready yet. */
using AudioResolver = std::function<std::shared_ptr<const AudioData> (const juce::String& relativePath)>;

/** Playing one part over and over while editing it (the drum editor's Space): the range loops
    instead of the project's loop, and (with `onlyThisTrack`) only that track is heard, as if it
    were soloed. Nothing in the project changes. */
struct PartLoop
{
    juce::String trackId;
    Tick start = 0, end = 0;
    bool onlyThisTrack = true;

    bool isActive() const noexcept   { return trackId.isNotEmpty() && end > start; }
    bool operator== (const PartLoop& o) const noexcept
    {
        return trackId == o.trackId && start == o.start && end == o.end && onlyThisTrack == o.onlyThisTrack;
    }
};

/** Builds the audio engine's view of a project: tempo, loop, the mixer (every track, bus and
    the master, with their settings, routing and effects) and (if `resolver` is given) every clip
    that has its audio ready, with positions in samples at `sampleRate`.

    `resources` keeps the mixer's working objects (effect tails, fades) from one build to the
    next; without it, everything starts afresh. With no sample rate, only the tempo and loop are
    filled in. `partLoop`, if active, replaces the loop (and the solos), see PartLoop. */
std::unique_ptr<TimelineSnapshot> buildTimelineSnapshot (const Project& project, double sampleRate = 0.0,
                                                         const AudioResolver& resolver = {},
                                                         MixerResources* resources = nullptr,
                                                         const juce::String& focusedTrackId = {},
                                                         const PartLoop& partLoop = {});

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

    /** A mixer channel's highest level (linear, left and right) since the last call, for meters.
        `channelId` is a track id or mixer::masterId. */
    std::pair<float, float> takeChannelPeaks (const juce::String& channelId)   { return mixerResources.takePeaks (channelId); }

    /** The selected track: if it's a MIDI track, what's played on a MIDI keyboard plays through it. */
    void setFocusedTrack (const juce::String& trackId)   { if (focusedTrack != trackId) { focusedTrack = trackId; update(); } }

    /** Gives the engine somewhere to record into (or takes it away, with null). */
    void setRecordingRing (std::shared_ptr<RecordingRing> ring)   { recordRing = std::move (ring); update(); }

    /** Loops one part on its own (or with the song) until set back to an empty PartLoop. */
    void setPartLoop (const PartLoop& loop)   { if (! (partLoop == loop)) { partLoop = loop; update(); } }
    const PartLoop& getPartLoop() const noexcept   { return partLoop; }

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
    MixerResources mixerResources;
    std::shared_ptr<RecordingRing> recordRing;
    juce::String focusedTrack;
    PartLoop partLoop;
};

} // namespace aether
