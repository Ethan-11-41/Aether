// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "ClipEdits.h"

#include <cmath>
#include <limits>

#include "MidiClips.h"
#include "TimelineEdits.h"

namespace aether
{

namespace
{
    constexpr double minGainDb = -60.0, maxGainDb = 18.0;

    juce::int64 positiveMod (juce::int64 a, juce::int64 b)
    {
        const auto r = a % b;
        return r < 0 ? r + b : r;
    }

    void startStep (Project& project, const juce::String& name, bool newUndoStep)
    {
        if (newUndoStep)
            project.getUndoManager().beginNewTransaction (name);
    }

    void set (Project& project, Clip& clip, const juce::Identifier& property, const juce::var& value)
    {
        if (clip.getState()[property] != value)
            clip.getState().setProperty (property, value, &project.getUndoManager());
    }

    /** Keeps the fades inside the clip after its length changes. */
    void limitFades (Project& project, Clip& clip)
    {
        if (clip.isMidi() && ! clip.hasFades())
            return;   // (MIDI clips usually have none: don't add the properties)

        const auto duration = clip.getFadeSpan();
        auto fadeIn = std::clamp<juce::int64> (clip.getFadeIn(), 0, duration);
        auto fadeOut = std::clamp<juce::int64> (clip.getFadeOut(), 0, duration);

        if (fadeIn + fadeOut > duration)
        {
            const auto scale = (double) duration / (double) (fadeIn + fadeOut);
            fadeIn = (juce::int64) ((double) fadeIn * scale);
            fadeOut = duration - fadeIn;
        }

        set (project, clip, ids::fadeIn, fadeIn);
        set (project, clip, ids::fadeOut, fadeOut);
    }

    juce::ValueTree findTrackState (const Project& project, const juce::String& trackId)
    {
        return project.getTracksNode().getChildWithProperty (ids::id, trackId);
    }
}

//==============================================================================
juce::ValueTree clips::clipsNodeOf (const juce::ValueTree& trackState)
{
    auto node = trackState.getChildWithName (ids::Clips);

    if (! node.isValid() && trackState.isValid())
    {
        node = juce::ValueTree (ids::Clips);
        juce::ValueTree (trackState).appendChild (node, nullptr);   // structure, not an edit
    }

    return node;
}

Clip clips::findClip (const Project& project, const juce::String& clipId)
{
    if (clipId.isEmpty())
        return {};

    for (const auto& track : project.getTracksNode())
        if (auto clip = track.getChildWithName (ids::Clips).getChildWithProperty (ids::id, clipId); clip.isValid())
            return Clip (clip);

    return {};
}

juce::Array<Clip> clips::clipsOnTrack (const juce::ValueTree& trackState)
{
    juce::Array<Clip> result;
    for (const auto& clip : trackState.getChildWithName (ids::Clips))
        if (clip.hasType (ids::Clip))
            result.add (Clip (clip));
    return result;
}

juce::Array<Clip> clips::allClips (const Project& project)
{
    juce::Array<Clip> result;
    for (const auto& track : project.getTracksNode())
        result.addArray (clipsOnTrack (track));
    return result;
}

juce::int64 clips::minimumLength (const Clip& clip)
{
    if (clip.isMidi())
        return midi::minimumClipLength;

    return std::max<juce::int64> (1, (juce::int64) (clip.getSourceRate() * 0.01));
}

//==============================================================================
juce::String edit::addAudioClip (Project& project, const juce::String& trackId, const AudioSourceInfo& source,
                                 Tick start, bool newUndoStep)
{
    auto track = findTrackState (project, trackId);

    if (! track.isValid() || source.lengthInSamples <= 0)
        return {};

    startStep (project, "Add clip", newUndoStep);

    const auto id = juce::Uuid().toString();
    juce::ValueTree clip (ids::Clip);
    clip.setProperty (ids::id, id, nullptr);
    clip.setProperty (ids::name, source.name, nullptr);
    clip.setProperty (ids::file, source.file, nullptr);
    clip.setProperty (ids::start, (juce::int64) std::max<Tick> (0, start), nullptr);
    clip.setProperty (ids::sourceRate, source.sampleRate, nullptr);
    clip.setProperty (ids::sourceLength, source.lengthInSamples, nullptr);
    clip.setProperty (ids::offset, (juce::int64) 0, nullptr);
    clip.setProperty (ids::contentLength, source.lengthInSamples, nullptr);
    clip.setProperty (ids::duration, source.lengthInSamples, nullptr);
    clip.setProperty (ids::phase, (juce::int64) 0, nullptr);
    clip.setProperty (ids::loop, false, nullptr);
    clip.setProperty (ids::gain, 0.0, nullptr);
    clip.setProperty (ids::fadeIn, (juce::int64) 0, nullptr);
    clip.setProperty (ids::fadeOut, (juce::int64) 0, nullptr);
    clip.setProperty (ids::fadeInCurve, "smooth", nullptr);
    clip.setProperty (ids::fadeOutCurve, "smooth", nullptr);

    clips::clipsNodeOf (track).appendChild (clip, &project.getUndoManager());
    return id;
}

bool edit::setClipPosition (Project& project, const juce::String& clipId, Tick newStart,
                            const juce::String& trackId, bool newUndoStep)
{
    auto clip = clips::findClip (project, clipId);
    auto targetTrack = findTrackState (project, trackId);

    if (! clip.isValid() || ! targetTrack.isValid())
        return false;

    // Audio clips only go on audio tracks, MIDI clips on MIDI tracks.
    if (Track (targetTrack).getKind() != (clip.isMidi() ? TrackKind::midi : TrackKind::audio))
        targetTrack = clip.getTrackState();

    newStart = std::max<Tick> (0, newStart);
    const auto moveTrack = targetTrack != clip.getTrackState();

    if (newStart == clip.getStart() && ! moveTrack)
        return false;

    startStep (project, "Move clip", newUndoStep);
    auto& undo = project.getUndoManager();
    set (project, clip, ids::start, (juce::int64) newStart);

    if (moveTrack)
    {
        auto state = clip.getState();
        auto oldClips = state.getParent();
        oldClips.removeChild (state, &undo);
        clips::clipsNodeOf (targetTrack).appendChild (state, &undo);
    }

    return true;
}

namespace
{
    // MIDI clips: everything is in ticks, and the notes stay where they are in time.
    bool trimMidiClipStart (Project& project, Clip& clip, Tick newStart, bool newUndoStep)
    {
        newStart = std::clamp<Tick> (newStart, 0, clip.getEnd ({}) - midi::minimumClipLength);
        const auto delta = newStart - clip.getStart();

        if (delta == 0)
            return false;

        startStep (project, "Trim clip", newUndoStep);
        // A repeating pattern can be stretched left too: it starts part-way through a repeat.
        const auto contentStart = clip.isLooping() ? positiveMod (clip.getContentStart() + delta, clip.getPatternLength())
                                                   : clip.getContentStart() + delta;
        set (project, clip, ids::contentStart, (juce::int64) contentStart);
        set (project, clip, ids::length, (juce::int64) (clip.getLengthTicks() - delta));
        set (project, clip, ids::start, (juce::int64) newStart);
        limitFades (project, clip);
        return true;
    }

    bool trimMidiClipEnd (Project& project, Clip& clip, Tick newEnd, bool newUndoStep)
    {
        const auto newLength = std::max (midi::minimumClipLength, newEnd - clip.getStart());

        if (newLength == clip.getLengthTicks())
            return false;

        startStep (project, "Trim clip", newUndoStep);
        set (project, clip, ids::length, (juce::int64) newLength);
        limitFades (project, clip);
        return true;
    }

    juce::String splitMidiClip (Project& project, Clip& clip, Tick position, bool newUndoStep)
    {
        const auto leftLength = position - clip.getStart();

        if (leftLength < midi::minimumClipLength || leftLength > clip.getLengthTicks() - midi::minimumClipLength)
            return {};

        startStep (project, "Split clip", newUndoStep);
        auto& undo = project.getUndoManager();
        const auto cut = clip.getContentStart() + leftLength;   // where the cut is, in the notes

        // The right-hand part: a copy holding the notes that start after the cut.
        auto rightState = clip.getState().createCopy();
        const auto rightId = juce::Uuid().toString();
        rightState.setProperty (ids::id, rightId, nullptr);
        rightState.setProperty (ids::start, (juce::int64) position, nullptr);
        rightState.setProperty (ids::length, (juce::int64) (clip.getLengthTicks() - leftLength), nullptr);
        auto left = clip.getState();

        // A fade-in stays at the start of the left part, a fade-out at the end of the right one.
        if (clip.hasFades())
            rightState.setProperty (ids::fadeIn, (juce::int64) 0, nullptr);

        if (clip.isLooping())
        {
            // Both halves keep the whole pattern; the right one starts part-way through a repeat.
            rightState.setProperty (ids::contentStart, (juce::int64) positiveMod (cut, clip.getPatternLength()), nullptr);
        }
        else
        {
            rightState.setProperty (ids::contentStart, (juce::int64) cut, nullptr);

            for (int i = rightState.getNumChildren(); --i >= 0;)
                if (rightState.getChild (i).hasType (ids::Note) && (Tick) (juce::int64) rightState.getChild (i)[ids::tick] < cut)
                    rightState.removeChild (i, nullptr);

            // The left-hand part keeps the notes that start before it.
            for (int i = left.getNumChildren(); --i >= 0;)
                if (left.getChild (i).hasType (ids::Note) && (Tick) (juce::int64) left.getChild (i)[ids::tick] >= cut)
                    left.removeChild (i, &undo);
        }

        set (project, clip, ids::length, (juce::int64) leftLength);

        if (clip.hasFades())
        {
            set (project, clip, ids::fadeOut, (juce::int64) 0);
            limitFades (project, clip);
        }

        auto clipsNode = left.getParent();
        clipsNode.addChild (rightState, clipsNode.indexOf (left) + 1, &undo);

        if (Clip right (rightState); right.hasFades())
            limitFades (project, right);

        return rightId;
    }
}

bool edit::trimClipStart (Project& project, const juce::String& clipId, Tick newStart, bool newUndoStep)
{
    auto clip = clips::findClip (project, clipId);

    if (! clip.isValid())
        return false;

    if (clip.isMidi())
        return trimMidiClipStart (project, clip, newStart, newUndoStep);

    const auto tempo = timeline::buildTempoMap (project);
    const auto rate = clip.getSourceRate();
    const auto oldStartSeconds = clip.getStartSeconds (tempo);
    auto delta = (juce::int64) std::llround ((tempo.ticksToSeconds (std::max<Tick> (0, newStart)) - oldStartSeconds) * rate);

    // Limits: at least the minimum length, and (unless looping) only as far left as the file goes.
    delta = std::min (delta, clip.getDuration() - clips::minimumLength (clip));
    if (! clip.isLooping())
        delta = std::max (delta, -clip.getOffset());

    if (delta == 0)
        return false;

    startStep (project, "Trim clip", newUndoStep);
    const auto newDuration = clip.getDuration() - delta;

    if (clip.isLooping())
    {
        set (project, clip, ids::phase, positiveMod (clip.getPhase() + delta, clip.getContentLength()));
    }
    else
    {
        set (project, clip, ids::offset, clip.getOffset() + delta);
        set (project, clip, ids::contentLength, newDuration);
    }

    set (project, clip, ids::duration, newDuration);

    // Place the edge exactly where the audio now starts.
    set (project, clip, ids::start, (juce::int64) std::max<Tick> (0, tempo.secondsToTicks (oldStartSeconds + (double) delta / rate)));
    limitFades (project, clip);
    return true;
}

bool edit::trimClipEnd (Project& project, const juce::String& clipId, Tick newEnd, bool newUndoStep)
{
    auto clip = clips::findClip (project, clipId);

    if (! clip.isValid())
        return false;

    if (clip.isMidi())
        return trimMidiClipEnd (project, clip, newEnd, newUndoStep);

    const auto tempo = timeline::buildTempoMap (project);
    const auto rate = clip.getSourceRate();
    auto newDuration = (juce::int64) std::llround ((tempo.ticksToSeconds (newEnd) - clip.getStartSeconds (tempo)) * rate);

    newDuration = std::max (newDuration, clips::minimumLength (clip));
    if (! clip.isLooping())
        newDuration = std::min (newDuration, clip.getSourceLength() - clip.getOffset());

    if (newDuration == clip.getDuration())
        return false;

    startStep (project, "Trim clip", newUndoStep);
    set (project, clip, ids::duration, newDuration);

    if (! clip.isLooping())
        set (project, clip, ids::contentLength, newDuration);

    limitFades (project, clip);
    return true;
}

juce::String edit::splitClip (Project& project, const juce::String& clipId, Tick position, bool newUndoStep)
{
    auto clip = clips::findClip (project, clipId);

    if (! clip.isValid())
        return {};

    if (clip.isMidi())
        return splitMidiClip (project, clip, position, newUndoStep);

    const auto tempo = timeline::buildTempoMap (project);
    const auto rate = clip.getSourceRate();
    const auto startSeconds = clip.getStartSeconds (tempo);
    const auto leftLength = (juce::int64) std::llround ((tempo.ticksToSeconds (position) - startSeconds) * rate);
    const auto minLength = clips::minimumLength (clip);

    if (leftLength < minLength || leftLength > clip.getDuration() - minLength)
        return {};   // too close to an edge (or outside the clip)

    startStep (project, "Split clip", newUndoStep);
    auto& undo = project.getUndoManager();

    // The right-hand part: a copy that starts where the cut is.
    auto rightState = clip.getState().createCopy();
    const auto rightId = juce::Uuid().toString();
    Clip right (rightState);
    rightState.setProperty (ids::id, rightId, nullptr);
    rightState.setProperty (ids::start, (juce::int64) tempo.secondsToTicks (startSeconds + (double) leftLength / rate), nullptr);
    rightState.setProperty (ids::duration, clip.getDuration() - leftLength, nullptr);
    rightState.setProperty (ids::fadeIn, (juce::int64) 0, nullptr);

    if (clip.isLooping())
    {
        rightState.setProperty (ids::phase, positiveMod (clip.getPhase() + leftLength, clip.getContentLength()), nullptr);
    }
    else
    {
        rightState.setProperty (ids::offset, clip.getOffset() + leftLength, nullptr);
        rightState.setProperty (ids::contentLength, clip.getDuration() - leftLength, nullptr);
    }

    // The left-hand part: the original, shortened.
    set (project, clip, ids::duration, leftLength);
    set (project, clip, ids::fadeOut, (juce::int64) 0);
    if (! clip.isLooping())
        set (project, clip, ids::contentLength, leftLength);
    limitFades (project, clip);

    auto clipsNode = clip.getState().getParent();
    clipsNode.addChild (rightState, clipsNode.indexOf (clip.getState()) + 1, &undo);
    limitFades (project, right);
    return rightId;
}

juce::StringArray edit::duplicateClips (Project& project, const juce::StringArray& clipIds)
{
    const auto tempo = timeline::buildTempoMap (project);
    juce::Array<Clip> originals;
    Tick earliest = std::numeric_limits<Tick>::max(), latest = 0;

    for (const auto& id : clipIds)
        if (auto clip = clips::findClip (project, id); clip.isValid())
        {
            originals.add (clip);
            earliest = std::min (earliest, clip.getStart());
            latest = std::max (latest, clip.getEnd (tempo));
        }

    juce::StringArray newIds;

    if (originals.isEmpty())
        return newIds;

    project.getUndoManager().beginNewTransaction (originals.size() == 1 ? "Duplicate clip" : "Duplicate clips");
    const auto shift = latest - earliest;

    for (const auto& clip : originals)
    {
        auto copy = clip.getState().createCopy();
        const auto id = juce::Uuid().toString();
        copy.setProperty (ids::id, id, nullptr);
        copy.setProperty (ids::start, (juce::int64) (clip.getStart() + shift), nullptr);
        clip.getState().getParent().appendChild (copy, &project.getUndoManager());
        newIds.add (id);
    }

    return newIds;
}

bool edit::deleteClips (Project& project, const juce::StringArray& clipIds, const juce::String& undoName)
{
    juce::Array<Clip> toDelete;
    for (const auto& id : clipIds)
        if (auto clip = clips::findClip (project, id); clip.isValid())
            toDelete.add (clip);

    if (toDelete.isEmpty())
        return false;

    project.getUndoManager().beginNewTransaction (undoName.isNotEmpty() ? undoName
                                                  : toDelete.size() == 1 ? "Delete clip" : "Delete clips");

    for (auto& clip : toDelete)
        clip.getState().getParent().removeChild (clip.getState(), &project.getUndoManager());

    return true;
}

ClipClipboard clips::copy (const Project& project, const juce::StringArray& clipIds)
{
    ClipClipboard clipboard;
    Tick earliest = std::numeric_limits<Tick>::max();
    int topmost = std::numeric_limits<int>::max();

    for (const auto& id : clipIds)
        if (const auto clip = findClip (project, id); clip.isValid())
        {
            earliest = std::min (earliest, clip.getStart());
            topmost = std::min (topmost, project.indexOfTrack (clip.getTrackState()[ids::id].toString()));
        }

    for (const auto& id : clipIds)
        if (const auto clip = findClip (project, id); clip.isValid())
        {
            const auto trackId = clip.getTrackState()[ids::id].toString();
            clipboard.push_back ({ clip.getState().createCopy(), trackId, project.indexOfTrack (trackId) - topmost,
                                   clip.getStart() - earliest });
        }

    return clipboard;
}

juce::StringArray edit::pasteClips (Project& project, const ClipClipboard& clipboard, Tick position, const juce::String& trackId)
{
    juce::StringArray newIds;
    const auto targetIndex = std::max (0, project.indexOfTrack (trackId));
    auto& undo = project.getUndoManager();
    bool started = false;

    for (const auto& item : clipboard)
    {
        const auto kind = Clip (item.clip).isMidi() ? TrackKind::midi : TrackKind::audio;
        auto fits = [&] (const Track& t) { return t.isValid() && t.getKind() == kind; };

        auto track = project.getTrack (targetIndex + item.trackOffset);
        if (! fits (track))
            track = project.findTrack (item.trackId);   // back where it came from
        if (! fits (track))
            continue;

        if (! started)
        {
            undo.beginNewTransaction (clipboard.size() == 1 ? "Paste clip" : "Paste clips");
            started = true;
        }

        auto copy = item.clip.createCopy();
        const auto id = juce::Uuid().toString();
        copy.setProperty (ids::id, id, nullptr);
        copy.setProperty (ids::start, (juce::int64) std::max<Tick> (0, position + item.offset), nullptr);
        clips::clipsNodeOf (track.getState()).appendChild (copy, &undo);
        newIds.add (id);
    }

    return newIds;
}

bool edit::setClipFades (Project& project, const juce::String& clipId, juce::int64 fadeIn, juce::int64 fadeOut, bool newUndoStep)
{
    auto clip = clips::findClip (project, clipId);

    if (! clip.isValid())
        return false;

    const auto duration = clip.getFadeSpan();
    fadeIn = std::clamp<juce::int64> (fadeIn, 0, duration);
    fadeOut = std::clamp<juce::int64> (fadeOut, 0, duration - fadeIn);

    if (fadeIn == clip.getFadeIn() && fadeOut == clip.getFadeOut())
        return false;

    startStep (project, "Change fade", newUndoStep);
    set (project, clip, ids::fadeIn, fadeIn);
    set (project, clip, ids::fadeOut, fadeOut);
    return true;
}

bool edit::setClipFadeCurve (Project& project, const juce::String& clipId, bool fadeIn, FadeCurve curve)
{
    auto clip = clips::findClip (project, clipId);

    if (! clip.isValid() || (fadeIn ? clip.getFadeInCurve() : clip.getFadeOutCurve()) == curve)
        return false;

    project.getUndoManager().beginNewTransaction ("Change fade shape");
    set (project, clip, fadeIn ? ids::fadeInCurve : ids::fadeOutCurve, toString (curve));
    return true;
}

bool edit::setClipGain (Project& project, const juce::String& clipId, double gainDb, bool newUndoStep)
{
    auto clip = clips::findClip (project, clipId);
    gainDb = std::round (std::clamp (std::isfinite (gainDb) ? gainDb : 0.0, minGainDb, maxGainDb) * 10.0) / 10.0;

    if (! clip.isValid() || clip.getGainDb() == gainDb)
        return false;

    startStep (project, "Change clip volume", newUndoStep);
    set (project, clip, ids::gain, gainDb);
    return true;
}

bool edit::setClipLooping (Project& project, const juce::String& clipId, bool shouldLoop)
{
    auto clip = clips::findClip (project, clipId);

    if (! clip.isValid() || clip.isLooping() == shouldLoop)
        return false;

    if (clip.isMidi())
        return setPatternLooping (project, clipId, shouldLoop);

    project.getUndoManager().beginNewTransaction (shouldLoop ? "Loop clip" : "Stop looping clip");

    if (shouldLoop)
    {
        // What you can see becomes the part that repeats.
        set (project, clip, ids::contentLength, clip.getDuration());
        set (project, clip, ids::phase, (juce::int64) 0);
    }
    else
    {
        // Back to a single play-through of the file, starting where the clip currently starts.
        const auto newOffset = std::clamp<juce::int64> (clip.getOffset() + clip.getPhase(), 0, clip.getSourceLength() - 1);
        const auto newDuration = std::min (clip.getDuration(), clip.getSourceLength() - newOffset);
        set (project, clip, ids::offset, newOffset);
        set (project, clip, ids::phase, (juce::int64) 0);
        set (project, clip, ids::duration, newDuration);
        set (project, clip, ids::contentLength, newDuration);
        limitFades (project, clip);
    }

    set (project, clip, ids::loop, shouldLoop);
    return true;
}

bool edit::renameClip (Project& project, const juce::String& clipId, const juce::String& newName)
{
    auto clip = clips::findClip (project, clipId);
    const auto trimmed = newName.trim();

    if (! clip.isValid() || trimmed.isEmpty() || trimmed == clip.getName())
        return false;

    project.getUndoManager().beginNewTransaction ("Rename clip");
    set (project, clip, ids::name, trimmed);
    return true;
}

} // namespace aether
