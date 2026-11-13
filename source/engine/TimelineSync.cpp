// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "TimelineSync.h"

#include <algorithm>
#include <cmath>

#include "DrumSoundCache.h"
#include "dsp/Sampler.h"
#include "SamplerLibrary.h"
#include "MixerRouting.h"
#include "dsp/DrumKit.h"
#include "io/AudioImport.h"
#include "model/ClipEdits.h"
#include "model/MidiClips.h"
#include "model/Automation.h"
#include "model/Mixer.h"
#include "model/TimelineEdits.h"
#include "model/Warp.h"

namespace aether
{

namespace
{

    void addInserts (TrackPlayback& channel, const Project& project, const juce::String& channelId,
                     double sampleRate, MixerResources& resources, juce::StringArray* insertIds = nullptr)
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
            playback.state = resources.insertState (insert[ids::id].toString(), ! playback.bypassed);

            for (size_t i = 0; i < type->parameters.size() && i < playback.parameters.size(); ++i)
                playback.parameters[i] = mixer::getInsertParameter (insert, type->parameters[i].id);

            if (playback.effect != nullptr)
            {
                channel.inserts.push_back (std::move (playback));
                if (insertIds != nullptr)
                    insertIds->add (insert[ids::id].toString());
            }
        }
    }

    /** The track's lanes with points, pointed at its sends, effects and instrument. */
    void addAutomation (TrackPlayback& channel, const Project& project, const TimelineSnapshot& snapshot, const Track& track,
                        double sampleRate, const juce::StringArray& sendIds, const juce::StringArray& insertIds)
    {
        using Kind = automation::Target::Kind;
        const auto trackId = track.getId();

        for (const auto& lane : automation::lanesOf (project, trackId))
        {
            const auto target = automation::Target::fromString (lane[ids::setting].toString());
            const auto range = automation::rangeOf (project, trackId, target);
            const auto points = automation::pointsOf (lane);

            if (! range || points.empty())
                continue;

            AutomationPlayback playback;
            playback.range = *range;

            switch (target.kind)
            {
                case Kind::volume:   playback.kind = AutomationPlayback::Kind::volume; break;
                case Kind::pan:      playback.kind = AutomationPlayback::Kind::pan; break;
                case Kind::mute:     playback.kind = AutomationPlayback::Kind::mute; break;
                case Kind::send:     playback.kind = AutomationPlayback::Kind::send; playback.index = sendIds.indexOf (target.objectId); break;
                case Kind::effectOn: playback.kind = AutomationPlayback::Kind::effectOn; playback.index = insertIds.indexOf (target.objectId); break;

                case Kind::effect:
                {
                    playback.kind = AutomationPlayback::Kind::effect;
                    playback.index = insertIds.indexOf (target.objectId);
                    const auto* type = dsp::findEffectType (mixer::findInsert (project, target.objectId)[ids::type].toString());
                    for (size_t i = 0; type != nullptr && i < type->parameters.size(); ++i)
                        if (type->parameters[i].id.toString() == target.parameterId)
                            playback.parameter = (int) i;
                    break;
                }

                case Kind::instrument:
                {
                    playback.kind = AutomationPlayback::Kind::instrument;
                    playback.index = 0;
                    const auto* type = dsp::findEffectType (midi::instrumentOf (project, trackId)[ids::type].toString());
                    for (size_t i = 0; type != nullptr && i < type->parameters.size(); ++i)
                        if (type->parameters[i].id.toString() == target.parameterId)
                            playback.parameter = (int) i;
                    break;
                }

                case Kind::none:
                    continue;
            }

            // A send or effect that isn't playing (e.g. a send to a bus that would make a loop).
            if (playback.index < 0 && target.kind != Kind::volume && target.kind != Kind::pan && target.kind != Kind::mute)
                continue;

            if ((target.kind == Kind::effect || target.kind == Kind::instrument) && playback.parameter < 0)
                continue;

            playback.points.reserve (points.size());
            for (const auto& p : points)
                playback.points.push_back ({ (std::int64_t) std::llround (snapshot.tempo.ticksToSeconds (p.tick) * sampleRate),
                                             range->toPosition (p.value), p.bend });

            channel.automation.push_back (std::move (playback));
        }
    }

    void addClips (TrackPlayback& playback, TimelineSnapshot& snapshot, const Track& track, double sampleRate,
                   const AudioResolver& resolver, const StretchResolver& stretcher)
    {
        for (const auto& clip : clips::clipsOnTrack (track.getState()))
        {
            auto audio = resolver (clip.getFile());

            if (audio == nullptr || audio->sampleRate != sampleRate || audio->samples.getNumSamples() == 0)
                continue;   // not loaded yet (or missing): silent for now

            // A clip following the tempo (or transposed): its stretched audio, from its start to
            // its end, played straight through.
            if (stretcher && warp::needsStretching (clip, snapshot.tempo))
            {
                if (auto stretched = stretcher (StretchJob::make (clip, snapshot.tempo, audio));
                    stretched != nullptr && stretched->sampleRate == sampleRate && stretched->samples.getNumSamples() > 0)
                {
                    const warp::ClipTiming timing (clip);
                    const auto startSeconds = clip.getStartSeconds (snapshot.tempo);
                    const auto outputAt = [&] (double played)
                    {
                        return (std::int64_t) std::llround ((snapshot.tempo.ticksToSeconds ((Tick) std::llround (timing.tickAt (snapshot.tempo, played)))
                                                             - startSeconds) * sampleRate);
                    };

                    const auto length = (std::int64_t) stretched->samples.getNumSamples();
                    ClipPlayback c;
                    c.audio = stretched.get();
                    c.start = (std::int64_t) std::llround (startSeconds * sampleRate);
                    c.duration = length;
                    c.offset = 0;
                    c.contentLength = length;
                    c.gain = std::pow (10.0f, (float) clip.getGainDb() / 20.0f);
                    c.fadeIn = std::clamp<std::int64_t> (outputAt ((double) clip.getFadeIn()), 0, length);
                    c.fadeOut = std::clamp<std::int64_t> (length - outputAt ((double) (clip.getDuration() - clip.getFadeOut())), 0, length);
                    c.fadeInCurve = clip.getFadeInCurve();
                    c.fadeOutCurve = clip.getFadeOutCurve();

                    playback.clips.push_back (c);
                    snapshot.keepAlive.push_back (std::move (stretched));
                    snapshot.keepAlive.push_back (std::move (audio));
                    continue;
                }
            }

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
                  double sampleRate, MixerResources& resources, const juce::String& focusedTrackId, const AudioResolver& resolver,
                  const juce::File& projectFolder)
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

        // A drum kit's sounds: built in (rendered at the engine's rate), or files in the project
        // (decoded by the audio file cache; until they're ready, those pads are silent).
        if (typeId == dsp::drumkit::typeId)
        {
            for (int pad = 0; pad < dsp::drums::numPads; ++pad)
            {
                const auto reference = instrument[dsp::drumkit::soundProperty (pad)].toString();
                std::shared_ptr<const AudioData> data;

                if (dsp::drumkit::isBuiltIn (reference))
                    data = builtInDrumSound (dsp::drumkit::builtInId (reference), sampleRate);
                else if (reference.isNotEmpty() && resolver)
                    data = resolver (reference);

                if (data != nullptr && data->samples.getNumSamples() > 1)
                {
                    channel.soundData[(size_t) pad] = data;
                    channel.sounds.sounds[(size_t) pad] = &data->samples;
                }
            }
        }

        // A sampler's recordings: built-in drum sounds rendered at the engine's rate, files from the
        // audio file cache (zones whose audio isn't loaded yet are left out until it is).
        if (typeId == dsp::sampler::typeId)
        {
            const auto sound = samplersound::of (project, track.getId());
            const auto regions = samplerlibrary::regionsOf (sound, samplersound::rootKeyOf (project, track.getId()), projectFolder);

            for (const auto& r : regions)
            {
                std::shared_ptr<const AudioData> data;

                if (r.sample.startsWith ("drum:"))
                    data = builtInDrumSound (r.sample.substring (5), sampleRate);
                else if (resolver)
                    data = resolver (r.sample);

                channel.sampleAudio.push_back (data);
            }

            channel.sampleMap = resources.sampleMap (instrument[ids::id].toString(), samplerlibrary::buildMap (regions, channel.sampleAudio));
            channel.sounds.map = channel.sampleMap.get();
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

        // The clips' fades (only if one of them has any), and how long it stays silent after each
        // fade-out: until the next clip starts, at most a few seconds.
        {
            std::vector<MidiFadePlayback> fades;
            bool anyFade = false;

            for (const auto& clip : clips::clipsOnTrack (track.getState()))
            {
                if (! clip.isMidi())
                    continue;

                const auto length = clip.getLengthTicks();
                const auto fadeIn = std::clamp<Tick> ((Tick) clip.getFadeIn(), 0, length);
                const auto fadeOut = std::clamp<Tick> ((Tick) clip.getFadeOut(), 0, length - fadeIn);
                const auto end = clip.getStart() + length;

                MidiFadePlayback f;
                f.start = toSample (clip.getStart());
                f.end = toSample (end);
                f.fadeIn = fadeIn > 0 ? std::max<std::int64_t> (1, toSample (clip.getStart() + fadeIn) - f.start) : 0;
                f.fadeOut = fadeOut > 0 ? std::max<std::int64_t> (1, f.end - toSample (end - fadeOut)) : 0;
                f.fadeInCurve = clip.getFadeInCurve();
                f.fadeOutCurve = clip.getFadeOutCurve();
                fades.push_back (f);
                anyFade = anyFade || fadeIn > 0 || fadeOut > 0;
            }

            if (anyFade)
            {
                std::sort (fades.begin(), fades.end(), [] (const auto& a, const auto& b) { return a.start < b.start; });
                const auto longest = (std::int64_t) (MidiFadePlayback::quietSeconds * sampleRate);

                for (auto& f : fades)
                {
                    f.quietUntil = f.end + longest;
                    for (const auto& other : fades)
                        if (other.start >= f.end)
                            f.quietUntil = std::min (f.quietUntil, other.start);
                }

                channel.midiFades = std::move (fades);
            }
        }

        // What's played on a MIDI keyboard goes to armed or monitoring MIDI tracks, and to the
        // selected one (so you can just select a track and play).
        channel.liveInput = track.isArmed() || track.isMonitoring() || track.getId() == focusedTrackId;
        channel.idHash = AudioEngine::hashTrackId (track.getId());
    }
}

std::unique_ptr<TimelineSnapshot> buildTimelineSnapshot (const Project& project, double sampleRate, const AudioResolver& resolver,
                                                         MixerResources* resources, const juce::String& focusedTrackId,
                                                         const PartLoop& partLoop, const juce::File& projectFolder,
                                                         const StretchResolver& stretcher)
{
    auto snapshot = std::make_unique<TimelineSnapshot>();
    snapshot->tempo = timeline::buildTempoMap (project);

    const auto loop = timeline::getLoop (project);
    const auto looping = partLoop.isActive() && project.findTrack (partLoop.trackId).isValid();
    snapshot->loopEnabled = looping || loop.enabled;
    snapshot->loopStart = looping ? partLoop.start : loop.start;
    snapshot->loopEnd = looping ? partLoop.end : loop.end;
    snapshot->sampleRate = sampleRate;

    // Playing one part on its own: that track is heard as if it were the only one soloed, the
    // others not. (The part being played is heard even if its track is muted.)
    const auto alone = looping && partLoop.onlyThisTrack;

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
        soloed[(size_t) t] = alone ? track.getId() == partLoop.trackId : track.isSoloed();

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
        channel.volumeDb = (float) track.getVolumeDb();
        channel.pan = (float) track.getPan();
        channel.muteFixed = looping && track.getId() == partLoop.trackId;
        channel.muteGain = track.isMuted() && ! channel.muteFixed ? 0.0f : 1.0f;

        const auto out = outputConnection[(size_t) t];
        const auto to = connections[out].to;
        channel.destination = routes.kept[out] && to != n ? position[(size_t) to] : -1;
        channel.outputGain = routes.audible[out] ? 1.0f : 0.0f;

        juce::StringArray sendIds, insertIds;

        for (const auto& send : sends)
        {
            if (send.track != t || ! routes.kept[send.connection])
                continue;

            SendPlayback playback;
            playback.destination = position[(size_t) connections[send.connection].to];
            playback.audible = routes.audible[send.connection];
            playback.gain = playback.audible ? mixer::dbToGain ((double) send.node[ids::level]) : 0.0f;
            sendIds.add (send.node[ids::id].toString());
            playback.preFader = (bool) send.node[ids::preFader];
            playback.state = res.send (send.node[ids::id].toString());
            channel.sends.push_back (std::move (playback));
        }

        addInserts (channel, project, track.getId(), sampleRate, res, &insertIds);

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
            addClips (channel, *snapshot, track, sampleRate, resolver, stretcher);

        if (track.getKind() == TrackKind::midi)
            addMidi (channel, project, *snapshot, track, sampleRate, res, focusedTrackId, resolver, projectFolder);

        addAutomation (channel, project, *snapshot, track, sampleRate, sendIds, insertIds);

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
    stretchCache.addChangeListener (this);

    if (engine.getCurrentSampleRate() > 0.0)
        cache.setTargetSampleRate (engine.getCurrentSampleRate());

    update();
    engine.locate (0);
    startTimerHz (10);
}

TimelineSync::~TimelineSync()
{
    stopTimer();
    stretchCache.removeChangeListener (this);
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

    if (++timerTicks % 50 == 0)   // every 5 seconds
        releaseUnusedAudio (juce::Time::getMillisecondCounterHiRes() * 0.001);
}

void TimelineSync::update()
{
    // Rebuilding is cheap (small lists, no audio is copied), so do it on every change: dragging
    // a clip or a tempo point then updates the sound immediately.
    const auto where = location();
    std::set<juce::String> requested;
    AudioResolver resolver = [this, where, &requested] (const juce::String& relativePath)
    {
        const auto file = resolveClipFile (where, relativePath);
        requested.insert (file.getFullPathName());
        return cache.getAudio (file);
    };

    std::set<juce::String> stretches;
    StretchResolver stretcher = [this, &stretches] (const StretchJob& job)
    {
        stretches.insert (job.key);
        return stretchCache.get (job).audio;
    };

    auto snapshot = buildTimelineSnapshot (project, cache.getTargetSampleRate(), resolver, &mixerResources, focusedTrack, partLoop,
                                           where.getFolder(), stretcher);
    snapshot->recordRing = recordRing;
    engine.setTimeline (std::move (snapshot));

    const auto now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    for (const auto& path : requested)
        filesLastUsed[path] = now;
    filesInUse = std::move (requested);

    for (const auto& key : stretches)
        stretchesLastUsed[key] = now;
    stretchesInUse = std::move (stretches);
}

void TimelineSync::releaseUnusedAudio (double now, double graceSeconds)
{
    // Kept: what the engine uses now, and what it used a moment ago (so going back and forth
    // between two instruments doesn't load them again each time).
    std::set<juce::String> keep = filesInUse;

    for (auto it = filesLastUsed.begin(); it != filesLastUsed.end();)
    {
        if (now - it->second < graceSeconds)
        {
            keep.insert (it->first);
            ++it;
        }
        else
        {
            it = filesLastUsed.erase (it);
        }
    }

    cache.keepOnly (keep);

    // The same for stretched clips (a moment ago: e.g. before an undo).
    std::set<juce::String> keepStretches = stretchesInUse;

    for (auto it = stretchesLastUsed.begin(); it != stretchesLastUsed.end();)
    {
        if (now - it->second < graceSeconds)
        {
            keepStretches.insert (it->first);
            ++it;
        }
        else
        {
            it = stretchesLastUsed.erase (it);
        }
    }

    stretchCache.keepOnly (keepStretches);
}

} // namespace aether
