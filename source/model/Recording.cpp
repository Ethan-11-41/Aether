// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "Recording.h"

#include <algorithm>
#include <cmath>

#include "MidiClips.h"
#include "TimelineEdits.h"

namespace aether
{

namespace recording
{

std::optional<Placement> place (const Facts& f, double sampleRate)
{
    const auto minimumLength = std::max<std::int64_t> (1, (std::int64_t) std::llround (sampleRate * 0.01));
    const auto latency = (std::int64_t) std::max (0, f.latencySamples);
    const auto loopLength = f.loopEnd - f.loopStart;

    // Loop recording: the playhead went round the loop at least once while recording.
    const auto firstPass = f.loopEnd - f.startSample;   // file samples until the first jump back
    const auto wentRound = f.loopEnabled && loopLength >= minimumLength && f.startSample < f.loopEnd
                        && f.lengthInSamples > firstPass;

    if (wentRound)
    {
        // Where in the file each pass reaches the loop start. If recording began inside the
        // loop, that first pass is incomplete, so the takes start with the second.
        const auto takeZero = f.startSample <= f.loopStart ? firstPass - loopLength : firstPass;
        int count = 0;

        for (;; ++count)
        {
            const auto begin = takeZero + count * loopLength + latency;
            const auto recorded = f.lengthInSamples - begin;

            if (recorded >= loopLength)
                continue;   // a complete pass

            // A pass you stopped part-way through is kept if at least a quarter of it was played.
            if (recorded * 4 >= loopLength)
                ++count;

            break;
        }

        if (count > 0)
        {
            Placement p;
            p.timelineStart = f.loopStart;
            p.duration = loopLength;
            p.takes = count;
            p.takeLength = loopLength;
            p.take = count - 1;
            p.offset = takeZero + p.take * loopLength + latency;
            return p;
        }
    }

    // An ordinary recording, from where it started to where it stopped (or, if it went round the
    // loop without completing a take, just the first pass).
    auto from = f.startSample;
    auto to = f.startSample + f.lengthInSamples - latency;

    if (wentRound)
        to = std::min (to, f.loopEnd);

    if (f.punch && loopLength > 0)
    {
        from = std::max (from, f.loopStart);
        to = std::min (to, f.loopEnd);
    }

    if (to - from < minimumLength)
        return std::nullopt;

    Placement p;
    p.timelineStart = from;
    p.offset = from - f.startSample + latency;
    p.duration = to - from;
    return p;
}

std::optional<MidiPlacement> placeMidi (const Facts& f, const std::vector<NoteData>& notes, double sampleRate)
{
    const auto minimumLength = std::max<std::int64_t> (1, (std::int64_t) std::llround (sampleRate * 0.01));
    const auto loopLength = f.loopEnd - f.loopStart;
    const auto wentRound = f.loopEnabled && loopLength >= minimumLength && f.startSample < f.loopEnd
                        && f.lengthInSamples > f.loopEnd - f.startSample;

    // Where on the timeline the playhead was when each sample of the recording was captured.
    auto position = [&] (std::int64_t index)
    {
        auto p = f.startSample + index;
        if (wentRound && p >= f.loopEnd)
            p = f.loopStart + (p - f.loopEnd) % loopLength;
        return p;
    };

    MidiPlacement result;
    auto to = f.startSample + f.lengthInSamples;

    if (wentRound)
    {
        result.timelineStart = f.loopStart;
        to = f.loopEnd;
    }
    else
    {
        result.timelineStart = f.startSample;

        if (f.punch && loopLength > 0)
        {
            result.timelineStart = std::max (result.timelineStart, f.loopStart);
            to = std::min (to, f.loopEnd);
        }
    }

    result.duration = to - result.timelineStart;

    if (result.duration < minimumLength)
        return std::nullopt;

    for (const auto& n : notes)
    {
        // Played in time with what was heard `latency` samples earlier.
        const auto at = position (n.start) - f.latencySamples;

        if (at < result.timelineStart || at >= to)
            continue;

        result.notes.push_back ({ n.pitch, n.velocity, (Tick) (at - result.timelineStart),
                                  (Tick) std::max<std::int64_t> (1, std::min<std::int64_t> (n.length, to - at)) });
    }

    if (result.notes.empty())
        return std::nullopt;

    std::sort (result.notes.begin(), result.notes.end(), [] (const NoteData& a, const NoteData& b) { return a.start < b.start; });
    return result;
}

juce::String describeInput (int firstChannel, bool stereo)
{
    if (firstChannel < 0)
        return "No input";

    if (stereo)
        return "Inputs " + juce::String (firstChannel + 1) + "+" + juce::String (firstChannel + 2);

    return "Input " + juce::String (firstChannel + 1);
}

namespace
{
    juce::StringArray armedOfKind (const Project& project, TrackKind kind)
    {
        juce::StringArray ids;

        for (int i = 0; i < project.getNumTracks(); ++i)
        {
            const auto track = project.getTrack (i);

            if (track.isArmed() && track.getKind() == kind)
                ids.add (track.getId());
        }

        return ids;
    }
}

juce::StringArray armedTracks (const Project& project)       { return armedOfKind (project, TrackKind::audio); }
juce::StringArray armedMidiTracks (const Project& project)   { return armedOfKind (project, TrackKind::midi); }

} // namespace recording

//==============================================================================
namespace
{
    void setSwitch (Project& project, const juce::StringArray& trackIds, const juce::Identifier& property, bool value)
    {
        for (const auto& id : trackIds)
        {
            auto track = project.findTrack (id);

            // Not an undo step (see the header). Audio and MIDI tracks record (not buses), but
            // anything can be switched off.
            if (track.isValid() && (! track.isBus() || ! value))
                track.getState().setProperty (property, value, nullptr);
        }
    }
}

void edit::setArmed (Project& project, const juce::StringArray& trackIds, bool shouldBeArmed)
{
    setSwitch (project, trackIds, ids::armed, shouldBeArmed);
}

void edit::setMonitoring (Project& project, const juce::StringArray& trackIds, bool shouldMonitor)
{
    setSwitch (project, trackIds, ids::monitor, shouldMonitor);
}

bool edit::setInput (Project& project, const juce::StringArray& trackIds, int firstChannel, bool stereo)
{
    firstChannel = std::max (-1, firstChannel);
    stereo = stereo && firstChannel >= 0;
    auto& undo = project.getUndoManager();
    bool changed = false;

    for (const auto& id : trackIds)
    {
        auto track = project.findTrack (id);

        if (! track.isValid() || (track.getInputChannel() == firstChannel && track.isInputStereo() == stereo))
            continue;

        if (! changed)
            undo.beginNewTransaction ("Change input to " + recording::describeInput (firstChannel, stereo));

        track.getState().setProperty (ids::input, firstChannel, &undo);
        track.getState().setProperty (ids::inputStereo, stereo, &undo);
        changed = true;
    }

    return changed;
}

bool edit::setPunch (Project& project, bool shouldPunch)
{
    auto state = project.getState();

    if ((bool) state[ids::punch] == shouldPunch)
        return false;

    auto& undo = project.getUndoManager();
    undo.beginNewTransaction (shouldPunch ? "Punch in/out on" : "Punch in/out off");
    state.setProperty (ids::punch, shouldPunch, &undo);
    return true;
}

bool edit::clearRange (Project& project, const juce::String& trackId, Tick from, Tick to, bool newUndoStep)
{
    const auto track = project.findTrack (trackId);

    if (! track.isValid() || to <= from)
        return false;

    const auto tempo = timeline::buildTempoMap (project);
    const auto fromSeconds = tempo.ticksToSeconds (from);
    const auto toSeconds = tempo.ticksToSeconds (to);
    auto& undo = project.getUndoManager();
    bool changed = false;

    auto beginChange = [&]
    {
        if (! changed && newUndoStep)
            undo.beginNewTransaction ("Clear");

        changed = true;
    };

    for (const auto& clip : clips::clipsOnTrack (track.getState()))
    {
        const auto start = clip.getStartSeconds (tempo);
        const auto end = clip.isMidi() ? tempo.ticksToSeconds (clip.getEnd (tempo)) : start + clip.getDurationSeconds();

        if (end <= fromSeconds || start >= toSeconds)
            continue;   // not in the range

        // How much would be left on each side; slivers shorter than the minimum go too.
        const auto minimum = clip.isMidi() ? tempo.ticksToSeconds (clip.getStart() + midi::minimumClipLength) - start
                                           : (double) clips::minimumLength (clip) / clip.getSourceRate();
        const auto keepLeft = fromSeconds - start >= minimum;
        const auto keepRight = end - toSeconds >= minimum;
        const auto id = clip.getId();
        beginChange();

        if (keepLeft && keepRight)
        {
            edit::splitClip (project, id, to, false);
            edit::trimClipEnd (project, id, from, false);
        }
        else if (keepLeft)
        {
            edit::trimClipEnd (project, id, from, false);
        }
        else if (keepRight)
        {
            edit::trimClipStart (project, id, to, false);
        }
        else
        {
            auto state = clip.getState();
            state.getParent().removeChild (state, &undo);
        }
    }

    return changed;
}

juce::StringArray edit::addRecordings (Project& project, const std::vector<RecordedTake>& recorded)
{
    juce::StringArray clipIds;

    if (recorded.empty())
        return clipIds;

    auto& undo = project.getUndoManager();
    undo.beginNewTransaction (recorded.size() == 1 ? "Record \"" + project.findTrack (recorded[0].trackId).getName() + "\""
                                                   : "Record " + juce::String ((int) recorded.size()) + " tracks");

    for (const auto& take : recorded)
    {
        if (! project.findTrack (take.trackId).isValid())
            continue;

        if (take.isMidi)
        {
            clearRange (project, take.trackId, take.midiStart, take.midiStart + take.midiLength, false);
            const auto id = addMidiClip (project, take.trackId, project.findTrack (take.trackId).getName(),
                                         take.midiStart, take.midiLength, take.notes, false);
            if (id.isNotEmpty())
                clipIds.add (id);

            continue;
        }

        const auto tempo = timeline::buildTempoMap (project);
        const auto rate = take.source.sampleRate;
        const auto& p = take.placement;
        const auto start = tempo.secondsToTicks ((double) p.timelineStart / rate);
        const auto end = tempo.secondsToTicks ((double) (p.timelineStart + p.duration) / rate);

        // The new recording replaces whatever was there.
        clearRange (project, take.trackId, start, end, false);

        const auto id = addAudioClip (project, take.trackId, take.source, start, false);
        auto clip = clips::findClip (project, id).getState();

        if (! clip.isValid())
            continue;

        clip.setProperty (ids::offset, (juce::int64) p.offset, &undo);
        clip.setProperty (ids::contentLength, (juce::int64) p.duration, &undo);
        clip.setProperty (ids::duration, (juce::int64) p.duration, &undo);

        if (p.takes > 1)
        {
            clip.setProperty (ids::takes, p.takes, &undo);
            clip.setProperty (ids::takeLength, (juce::int64) p.takeLength, &undo);
            clip.setProperty (ids::take, p.take, &undo);
        }

        clipIds.add (id);
    }

    return clipIds;
}

bool edit::setClipTake (Project& project, const juce::String& clipId, int take)
{
    const auto clip = clips::findClip (project, clipId);

    if (! clip.isValid() || clip.getNumTakes() < 2 || clip.getTakeLength() <= 0)
        return false;

    take = juce::jlimit (0, clip.getNumTakes() - 1, take);

    if (take == clip.getTake())
        return false;

    auto& undo = project.getUndoManager();
    undo.beginNewTransaction ("Choose take " + juce::String (take + 1));

    // The same part of a different pass: moving by whole takes keeps any trimming or splitting.
    auto state = clip.getState();
    const auto offset = clip.getOffset() + (take - clip.getTake()) * clip.getTakeLength();
    state.setProperty (ids::offset, (juce::int64) std::max<juce::int64> (0, offset), &undo);
    state.setProperty (ids::take, take, &undo);
    return true;
}

} // namespace aether
