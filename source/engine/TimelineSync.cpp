// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "TimelineSync.h"

#include <algorithm>
#include <cmath>

#include "MixerRouting.h"
#include "io/AudioImport.h"
#include "model/ClipEdits.h"
#include "model/MidiClips.h"
#include "model/Mixer.h"
#include "model/TimelineEdits.h"

namespace aether
{

namespace
{
    /** Pan as a balance control: the centre leaves the sound unchanged; turning it to one side
        gradually turns the other side down (the side it points to stays at full level). */
    std::pair<float, float> panGains (double pan)
    {
        auto turnDown = [] (double amount) { return amount >= 1.0 ? 0.0f : (float) std::cos (amount * juce::MathConstants<double>::halfPi); };
        return { pan > 0.0 ? turnDown (pan) : 1.0f, pan < 0.0 ? turnDown (-pan) : 1.0f };
    }

    void addInserts (TrackPlayback& channel, const Project& project, const juce::String& channelId,
                     double sampleRate, MixerResources& resources)
    {
        for (const auto& insert : mixer::insertsOf (project, channelId))
        {
            const auto typeId = insert[ids::type].toString();
            const auto* type = dsp::findEffectType (typeId);

            if (type == nullptr)
                continue;   // an effect this version doesn't know: skipped (and kept in the file)

            InsertPlayback playback;
            playback.effect = resources.effect (insert[ids::id].toString(), typeId, sampleRate);
            playback.bypassed = (bool) insert[ids::bypass];

            for (size_t i = 0; i < type->parameters.size() && i < playback.parameters.size(); ++i)
                playback.parameters[i] = mixer::getInsertParameter (insert, type->parameters[i].id);

            if (playback.effect != nullptr)
                channel.inserts.push_back (std::move (playback));
        }
    }

    void addClips (TrackPlayback& playback, TimelineSnapshot& snapshot, const Track& track, double sampleRate,
                   const AudioResolver& resolver)
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
            c.start = (std::int64_t) std::llround (clip.getStartSeconds (snapshot.tempo) * sampleRate);
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
            snapshot.keepAlive.push_back (std::move (audio));
        }

        std::sort (playback.clips.begin(), playback.clips.end(),
                   [] (const ClipPlayback& a, const ClipPlayback& b) { return a.start < b.start; });
    }
}

namespace
{
    void addMidi (TrackPlayback& channel, const Project& project, const TimelineSnapshot& snapshot, const Track& track,
                  double sampleRate, MixerResources& resources, const juce::String& focusedTrackId)
    {
        // The instrument, with its settings.
        const auto instrument = midi::instrumentOf (project, track.getId());
        const auto typeId = instrument[ids::type].toString();

        if (const auto* type = dsp::findEffectType (typeId))
        {
            channel.instrument = resources.instrument (instrument[ids::id].toString(), typeId, sampleRate);

            for (size_t i = 0; i < type->parameters.size() && i < channel.instrumentParameters.size(); ++i)
                channel.instrumentParameters[i] = mixer::getInsertParameter (instrument, type->parameters[i].id);
        }

        // Every note, in samples (worked out through the tempo map, so notes follow tempo changes).
        auto toSample = [&snapshot, sampleRate] (Tick tick) { return (std::int64_t) std::llround (snapshot.tempo.ticksToSeconds (tick) * sampleRate); };

        for (const auto& clip : clips::clipsOnTrack (track.getState()))
        {
            if (! clip.isMidi())
                continue;

            for (const auto& n : midi::audibleNotes (clip))
            {
                const auto start = clip.getStart() + n.start;
                const auto from = toSample (start);
                channel.notes.push_back ({ from, std::max<std::int64_t> (1, toSample (start + n.length) - from),
                                           (std::uint8_t) n.pitch, (std::uint8_t) n.velocity });
            }
        }

        std::sort (channel.notes.begin(), channel.notes.end(),
                   [] (const NotePlayback& a, const NotePlayback& b) { return a.start < b.start; });

        // What's played on a MIDI keyboard goes to armed or monitoring MIDI tracks, and to the
        // selected one (so you can just select a track and play).
        channel.liveInput = track.isArmed() || track.isMonitoring() || track.getId() == focusedTrackId;
    }
}

std::unique_ptr<TimelineSnapshot> buildTimelineSnapshot (const Project& project, double sampleRate, const AudioResolver& resolver,
                                                         MixerResources* resources, const juce::String& focusedTrackId)
{
    auto snapshot = std::make_unique<TimelineSnapshot>();
    snapshot->tempo = timeline::buildTempoMap (project);

    const auto loop = timeline::getLoop (project);
    snapshot->loopEnabled = loop.enabled;
    snapshot->loopStart = loop.start;
    snapshot->loopEnd = loop.end;
    snapshot->sampleRate = sampleRate;

    if (sampleRate <= 0.0)
        return snapshot;

    MixerResources temporary;
    auto& res = resources != nullptr ? *resources : temporary;
    res.beginBuild();

    // The mixer's connections: each track's output, then its sends. The master is number n.
    const auto n = project.getNumTracks();
    std::vector<routing::Connection> connections;
    std::vector<size_t> outputConnection ((size_t) n);
    std::vector<bool> soloed ((size_t) n);

    struct SendInfo { int track; juce::ValueTree node; size_t connection; };
    std::vector<SendInfo> sends;

    auto busIndex = [&project] (const juce::String& id, int self)
    {
        const auto index = project.indexOfTrack (id);
        return index >= 0 && index != self && project.getTrack (index).isBus() ? index : -1;
    };

    for (int t = 0; t < n; ++t)
    {
        const auto track = project.getTrack (t);
        soloed[(size_t) t] = track.isSoloed();

        const auto output = busIndex (track.getOutput(), t);
        outputConnection[(size_t) t] = connections.size();
        connections.push_back ({ t, output >= 0 ? output : n });

        for (const auto& send : mixer::sendsOf (project, track.getId()))
        {
            const auto target = busIndex (send[ids::target].toString(), t);

            if (target >= 0)
            {
                sends.push_back ({ t, send, connections.size() });
                connections.push_back ({ t, target });
            }
        }
    }

    const auto routes = routing::compute (n, connections, soloed);

    std::vector<int> position ((size_t) n, -1);
    for (size_t i = 0; i < routes.order.size(); ++i)
        position[(size_t) routes.order[i]] = (int) i;

    snapshot->tracks.reserve ((size_t) n);

    for (const auto t : routes.order)
    {
        const auto track = project.getTrack (t);
        TrackPlayback channel (res.channel (track.getId()));

        const auto volume = mixer::dbToGain (track.getVolumeDb());
        const auto [left, right] = panGains (track.getPan());
        channel.leftGain = volume * left;
        channel.rightGain = volume * right;
        channel.muteGain = track.isMuted() ? 0.0f : 1.0f;

        const auto out = outputConnection[(size_t) t];
        const auto to = connections[out].to;
        channel.destination = routes.kept[out] && to != n ? position[(size_t) to] : -1;
        channel.outputGain = routes.audible[out] ? 1.0f : 0.0f;

        for (const auto& send : sends)
        {
            if (send.track != t || ! routes.kept[send.connection])
                continue;

            SendPlayback playback;
            playback.destination = position[(size_t) connections[send.connection].to];
            playback.gain = routes.audible[send.connection] ? mixer::dbToGain ((double) send.node[ids::level]) : 0.0f;
            playback.preFader = (bool) send.node[ids::preFader];
            playback.state = res.send (send.node[ids::id].toString());
            channel.sends.push_back (std::move (playback));
        }

        addInserts (channel, project, track.getId(), sampleRate, res);

        if (track.getKind() == TrackKind::audio && track.isMonitoring() && track.getInputChannel() >= 0)
        {
            channel.monitorChannel = track.getInputChannel();
            channel.monitorStereo = track.isInputStereo();
        }

        if (track.getKind() == TrackKind::audio && track.isArmed() && track.getInputChannel() >= 0)
        {
            channel.armedChannel = track.getInputChannel();
            channel.armedStereo = track.isInputStereo();
        }

        if (track.getKind() == TrackKind::audio && resolver)
            addClips (channel, *snapshot, track, sampleRate, resolver);

        if (track.getKind() == TrackKind::midi)
            addMidi (channel, project, *snapshot, track, sampleRate, res, focusedTrackId);

        snapshot->tracks.push_back (std::move (channel));
    }

    snapshot->master = TrackPlayback (res.channel (mixer::masterId));
    const auto masterVolume = mixer::dbToGain (mixer::getVolumeDb (project, mixer::masterId));
    snapshot->master.leftGain = snapshot->master.rightGain = masterVolume;
    addInserts (snapshot->master, project, mixer::masterId, sampleRate, res);

    res.endBuild();
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

    auto snapshot = buildTimelineSnapshot (project, cache.getTargetSampleRate(), resolver, &mixerResources, focusedTrack);
    snapshot->recordRing = recordRing;
    engine.setTimeline (std::move (snapshot));
}

} // namespace aether
