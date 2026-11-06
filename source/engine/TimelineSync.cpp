// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "TimelineSync.h"

#include <algorithm>
#include <cmath>

#include "io/AudioImport.h"
#include "model/ClipEdits.h"
#include "model/TimelineEdits.h"

namespace aether
{

std::unique_ptr<TimelineSnapshot> buildTimelineSnapshot (const Project& project, double sampleRate, const AudioResolver& resolver)
{
    auto snapshot = std::make_unique<TimelineSnapshot>();
    snapshot->tempo = timeline::buildTempoMap (project);

    const auto loop = timeline::getLoop (project);
    snapshot->loopEnabled = loop.enabled;
    snapshot->loopStart = loop.start;
    snapshot->loopEnd = loop.end;
    snapshot->sampleRate = sampleRate;

    if (sampleRate <= 0.0 || ! resolver)
        return snapshot;

    for (int t = 0; t < project.getNumTracks(); ++t)
    {
        const auto track = project.getTrack (t);
        TrackPlayback playback;

        if (track.getKind() == TrackKind::audio)
        {
            for (const auto& clip : clips::clipsOnTrack (track.getState()))
            {
                auto audio = resolver (clip.getFile());

                if (audio == nullptr || audio->sampleRate != sampleRate || audio->samples.getNumSamples() == 0)
                    continue;   // not loaded yet (or missing): silent for now

                // Everything in samples at the engine's rate.
                const auto ratio = sampleRate / clip.getSourceRate();
                auto toEngine = [ratio] (juce::int64 sourceSamples) { return (std::int64_t) std::llround ((double) sourceSamples * ratio); };

                ClipPlayback c;
                c.audio = audio.get();
                c.start = (std::int64_t) std::llround (clip.getStartSeconds (snapshot->tempo) * sampleRate);
                c.duration = std::max<std::int64_t> (1, toEngine (clip.getDuration()));
                c.offset = std::clamp<std::int64_t> (toEngine (clip.getOffset()), 0, audio->samples.getNumSamples() - 1);
                c.contentLength = std::max<std::int64_t> (1, toEngine (clip.getContentLength()));
                c.phase = toEngine (clip.getPhase()) % c.contentLength;
                c.loop = clip.isLooping();
                c.gain = std::pow (10.0f, (float) clip.getGainDb() / 20.0f);
                c.fadeIn = toEngine (clip.getFadeIn());
                c.fadeOut = toEngine (clip.getFadeOut());
                c.fadeInCurve = clip.getFadeInCurve();
                c.fadeOutCurve = clip.getFadeOutCurve();

                playback.clips.push_back (c);
                snapshot->keepAlive.push_back (std::move (audio));
            }

            std::sort (playback.clips.begin(), playback.clips.end(),
                       [] (const ClipPlayback& a, const ClipPlayback& b) { return a.start < b.start; });
        }

        snapshot->tracks.push_back (std::move (playback));
    }

    return snapshot;
}

//==============================================================================
TimelineSync::TimelineSync (Project& projectToFollow, AudioEngine& engineToUpdate, AudioFileCache& audioCache,
                            std::function<ProjectLocation()> projectLocation)
    : project (projectToFollow), engine (engineToUpdate), cache (audioCache),
      location (std::move (projectLocation)), state (projectToFollow.getState())
{
    state.addListener (this);
    cache.addChangeListener (this);

    if (engine.getCurrentSampleRate() > 0.0)
        cache.setTargetSampleRate (engine.getCurrentSampleRate());

    update();
    engine.locate (0);
    startTimerHz (10);
}

TimelineSync::~TimelineSync()
{
    stopTimer();
    cache.removeChangeListener (this);
    state.removeListener (this);
    engine.stop();
    engine.setTimeline (std::make_unique<TimelineSnapshot>());
}

void TimelineSync::timerCallback()
{
    // If the sound card's sample rate changed, the audio needs converting again.
    const auto rate = engine.getCurrentSampleRate();

    if (rate > 0.0 && rate != cache.getTargetSampleRate())
    {
        cache.setTargetSampleRate (rate);
        update();
    }

    engine.collectGarbage();
}

void TimelineSync::update()
{
    // Rebuilding is cheap (small lists, no audio is copied), so do it on every change: dragging
    // a clip or a tempo point then updates the sound immediately.
    const auto where = location();
    AudioResolver resolver = [this, where] (const juce::String& relativePath)
    {
        return cache.getAudio (resolveClipFile (where, relativePath));
    };

    engine.setTimeline (buildTimelineSnapshot (project, cache.getTargetSampleRate(), resolver));
}

} // namespace aether
