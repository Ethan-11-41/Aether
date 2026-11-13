// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "Warp.h"

#include <algorithm>
#include <cmath>

#include "ClipEdits.h"
#include "TimelineEdits.h"

namespace aether
{

namespace
{
    constexpr double minimumBpm = 20.0, maximumBpm = 400.0;
    constexpr Tick minimumGap = ticksPerQuarter / 64;   // markers stay at least a 256th note apart

    double floorMod (double a, double b)
    {
        return a - b * std::floor (a / b);
    }

    void set (Project& project, juce::ValueTree node, const juce::Identifier& property, const juce::var& value)
    {
        if (node[property] != value)
            node.setProperty (property, value, &project.getUndoManager());
    }

    void writeMarkers (Project& project, juce::ValueTree clip, const std::vector<warp::Marker>& markers)
    {
        auto& undo = project.getUndoManager();

        for (int i = clip.getNumChildren(); --i >= 0;)
            if (clip.getChild (i).hasType (ids::WarpMarker))
                clip.removeChild (i, &undo);

        for (const auto& m : markers)
        {
            juce::ValueTree node (ids::WarpMarker);
            node.setProperty (ids::sample, m.sample, nullptr);
            node.setProperty (ids::tick, (juce::int64) m.tick, nullptr);
            clip.appendChild (node, &undo);
        }
    }

    /** Markers kept in order: samples and beats both rising, a little apart. */
    std::vector<warp::Marker> tidy (std::vector<warp::Marker> markers)
    {
        std::sort (markers.begin(), markers.end(), [] (const auto& a, const auto& b) { return a.sample < b.sample; });
        std::vector<warp::Marker> kept;

        for (const auto& m : markers)
            if (kept.empty() || (m.sample > kept.back().sample && m.tick >= kept.back().tick + minimumGap))
                kept.push_back (m);

        return kept;
    }

    bool isUsableAudioClip (const Clip& clip)
    {
        return clip.isValid() && ! clip.isMidi();
    }

    /** Makes a clip follow the tempo so it sounds exactly as it does now (see setFollowTempo). */
    void followFromNow (Project& project, Clip& clip, const TempoMap& tempo)
    {
        const warp::ClipTiming asNow (clip);
        const auto rate = clip.getSourceRate();
        const auto start = clip.getStart();
        const auto end = asNow.end (tempo);
        const auto bpm = std::clamp (tempo.tempoAt (start), minimumBpm, maximumBpm);
        const auto pace = bpm / 60.0 * (double) ticksPerQuarter / rate;
        const auto offset = (double) clip.getOffset();

        // Where the tempo changes inside the clip (and, in a ramp, every beat), the file is pinned
        // to its beat, so the clip keeps sounding as it does.
        std::vector<Tick> pins;
        const auto& points = tempo.getTempoPoints();

        for (size_t i = 0; i < points.size(); ++i)
        {
            if (points[i].tick > start && points[i].tick < end)
                pins.push_back (points[i].tick);

            if (points[i].curve == TempoCurve::ramp && i + 1 < points.size())
                for (auto t = std::max (points[i].tick, start); t < std::min (points[i + 1].tick, end); t += ticksPerQuarter)
                    pins.push_back (t);
        }

        std::vector<warp::Marker> markers;

        if (! pins.empty() && ! clip.isLooping())
        {
            pins.push_back (start);
            pins.push_back (end);
            const auto offsetBeat = offset * pace;

            for (const auto t : pins)
                markers.push_back ({ (juce::int64) std::llround (offset + asNow.playedAt (tempo, (double) t)),
                                     (Tick) std::llround (offsetBeat + (double) (t - start)) });
        }

        auto state = clip.getState();
        set (project, state, ids::warpBpm, std::round (bpm * 1000.0) / 1000.0);
        writeMarkers (project, state, tidy (markers));
    }
}

//==============================================================================
bool warp::followsTempo (const Clip& clip)
{
    return ! clip.isMidi() && (bool) clip.getState()[ids::followTempo];
}

double warp::bpmOf (const Clip& clip)
{
    const auto bpm = (double) clip.getState().getProperty (ids::warpBpm, 120.0);
    return std::isfinite (bpm) && bpm > 0.0 ? std::clamp (bpm, minimumBpm, maximumBpm) : 120.0;
}

std::vector<warp::Marker> warp::markersOf (const Clip& clip)
{
    std::vector<Marker> markers;

    for (const auto& node : clip.getState())
        if (node.hasType (ids::WarpMarker))
            markers.push_back ({ (juce::int64) node[ids::sample], (Tick) (juce::int64) node[ids::tick] });

    return tidy (std::move (markers));
}

double warp::transposeOf (const Clip& clip)
{
    const auto semitones = (double) clip.getState()[ids::transpose];
    return std::isfinite (semitones) ? std::clamp (semitones, -24.0, 24.0) : 0.0;
}

//==============================================================================
warp::ClipTiming::ClipTiming (const Clip& clip)
    : follows (warp::followsTempo (clip)),
      looping (clip.isLooping()),
      start (clip.getStart()),
      rate (clip.getSourceRate()),
      offset ((double) clip.getOffset()),
      contentLength ((double) clip.getContentLength()),
      phase (clip.isLooping() ? (double) clip.getPhase() : 0.0),
      duration ((double) clip.getDuration()),
      pace (bpmOf (clip) / 60.0 * (double) ticksPerQuarter / clip.getSourceRate()),
      markers (follows ? markersOf (clip) : std::vector<Marker>())
{
}

double warp::ClipTiming::beatOf (double s) const
{
    if (markers.empty())
        return s * pace;

    if (s <= (double) markers.front().sample)
        return (double) markers.front().tick - ((double) markers.front().sample - s) * pace;

    if (s >= (double) markers.back().sample)
        return (double) markers.back().tick + (s - (double) markers.back().sample) * pace;

    const auto after = std::upper_bound (markers.begin(), markers.end(), s,
                                         [] (double value, const Marker& m) { return value < (double) m.sample; });
    const auto& b = *after;
    const auto& a = *(after - 1);
    return (double) a.tick + (s - (double) a.sample) * (double) (b.tick - a.tick) / (double) (b.sample - a.sample);
}

double warp::ClipTiming::fileSampleOf (double beat) const
{
    if (markers.empty())
        return beat / pace;

    if (beat <= (double) markers.front().tick)
        return (double) markers.front().sample - ((double) markers.front().tick - beat) / pace;

    if (beat >= (double) markers.back().tick)
        return (double) markers.back().sample + (beat - (double) markers.back().tick) / pace;

    const auto after = std::upper_bound (markers.begin(), markers.end(), beat,
                                         [] (double value, const Marker& m) { return value < (double) m.tick; });
    const auto& b = *after;
    const auto& a = *(after - 1);
    return (double) a.sample + (beat - (double) a.tick) * (double) (b.sample - a.sample) / (double) (b.tick - a.tick);
}

double warp::ClipTiming::contentBeats (double x) const
{
    const auto first = beatOf (offset);

    if (! looping)
        return beatOf (offset + x) - first;

    const auto repeat = std::floor (x / contentLength);
    const auto within = x - repeat * contentLength;
    return repeat * (beatOf (offset + contentLength) - first) + beatOf (offset + within) - first;
}

double warp::ClipTiming::contentSample (double beats) const
{
    const auto first = beatOf (offset);

    if (! looping)
        return fileSampleOf (first + beats) - offset;

    const auto length = std::max (1.0e-9, beatOf (offset + contentLength) - first);
    const auto repeat = std::floor (beats / length);
    return repeat * contentLength + fileSampleOf (first + beats - repeat * length) - offset;
}

double warp::ClipTiming::tickAt (const TempoMap& tempo, double played) const
{
    if (follows)
        return (double) start + contentBeats (phase + played) - contentBeats (phase);

    return (double) tempo.secondsToTicks (tempo.ticksToSeconds (start) + played / rate);
}

double warp::ClipTiming::playedAt (const TempoMap& tempo, double tick) const
{
    if (follows)
        return contentSample (tick - (double) start + contentBeats (phase)) - phase;

    return (tempo.ticksToSeconds ((Tick) std::llround (tick)) - tempo.ticksToSeconds (start)) * rate;
}

Tick warp::ClipTiming::end (const TempoMap& tempo) const
{
    return (Tick) std::llround (tickAt (tempo, duration));
}

double warp::ClipTiming::fileSampleAt (double played) const
{
    return looping ? offset + floorMod (phase + played, contentLength) : offset + played;
}

//==============================================================================
std::vector<warp::TimePoint> warp::timeMap (const Clip& clip, const TempoMap& tempo)
{
    const ClipTiming timing (clip);
    const auto duration = (double) clip.getDuration();
    const auto start = clip.getStart();
    const auto startSeconds = tempo.ticksToSeconds (start);
    std::vector<double> played { 0.0, duration };

    if (timing.followsTempo())
    {
        const auto end = timing.end (tempo);

        // Every 16th note (curves in tempo ramps are close enough at this spacing)...
        constexpr Tick step = ticksPerQuarter / 4;
        for (auto t = (start / step + 1) * step; t < end; t += step)
            played.push_back (timing.playedAt (tempo, (double) t));

        // ...and exactly where the tempo changes,
        for (const auto& p : tempo.getTempoPoints())
            if (p.tick > start && p.tick < end)
                played.push_back (timing.playedAt (tempo, (double) p.tick));

        // where the content repeats, and at the warp markers (in every repeat).
        const auto contentLength = (double) clip.getContentLength();
        const auto phase = clip.isLooping() ? (double) clip.getPhase() : 0.0;
        const auto repeats = clip.isLooping() ? (int) std::ceil ((phase + duration) / contentLength) : 1;

        const auto markers = markersOf (clip);

        for (int r = 0; r < repeats && r < 100000; ++r)
        {
            if (r > 0)
                played.push_back (r * contentLength - phase);

            for (const auto& m : markers)
                played.push_back (r * contentLength + (double) (m.sample - clip.getOffset()) - phase);
        }
    }

    std::sort (played.begin(), played.end());
    std::vector<TimePoint> map;

    for (const auto p : played)
    {
        if (p < 0.0 || p > duration || (! map.empty() && p - map.back().played < 0.5))
            continue;

        map.push_back ({ tempo.ticksToSeconds ((Tick) std::llround (timing.tickAt (tempo, p))) - startSeconds, p });
    }

    // Seconds never go back (rounding at the tick could make two close points cross).
    for (size_t i = 1; i < map.size(); ++i)
        map[i].seconds = std::max (map[i].seconds, map[i - 1].seconds);

    return map;
}

double warp::bpmFromFileName (const juce::String& name, double seconds)
{
    const auto lower = name.toLowerCase();
    const auto isSeparator = [] (juce::juce_wchar c) { return c == ' ' || c == '_' || c == '-' || c == '.' || c == '(' || c == ')' || c == '[' || c == ']'; };

    // "90 BPM", "90bpm", "90_bpm", "bpm90", "BPM 90".
    for (auto at = lower.indexOf ("bpm"); at >= 0; at = lower.indexOf (at + 1, "bpm"))
    {
        auto before = at;
        while (before > 0 && isSeparator (lower[before - 1]))
            --before;

        auto digits = before;
        while (digits > 0 && (juce::CharacterFunctions::isDigit (lower[digits - 1]) || lower[digits - 1] == '.'))
            --digits;

        auto value = lower.substring (digits, before).getDoubleValue();

        if (value <= 0.0)
        {
            auto after = at + 3;
            while (after < lower.length() && isSeparator (lower[after]))
                ++after;
            value = lower.substring (after).getDoubleValue();
        }

        if (value >= 40.0 && value <= 300.0)
            return value;
    }

    // A bare number, if the recording is a whole number of bars (of 4 beats) at that tempo.
    juce::String token;
    for (auto c : lower + " ")
    {
        if (juce::CharacterFunctions::isDigit (c))
        {
            token += c;
            continue;
        }

        if (token.isNotEmpty())
        {
            const auto value = token.getDoubleValue();
            const auto beats = seconds * value / 60.0;
            const auto whole = std::round (beats);

            if (value >= 60.0 && value <= 200.0 && whole >= 4.0 && (int) whole % 4 == 0 && std::abs (beats - whole) < 0.01 * whole
                && (isSeparator (c) || c == ' '))
                return value;
        }

        token.clear();
    }

    return 0.0;
}

bool warp::needsStretching (const Clip& clip, const TempoMap& tempo)
{
    if (clip.isMidi())
        return false;

    if (std::abs (transposeOf (clip)) >= 0.005)
        return true;

    if (! followsTempo (clip))
        return false;

    const auto rate = clip.getSourceRate();

    for (const auto& point : timeMap (clip, tempo))
        if (std::abs (point.seconds * rate - point.played) > 1.0)
            return true;

    return false;
}

//==============================================================================
bool edit::setFollowTempo (Project& project, const juce::StringArray& clipIds, bool follow, bool newUndoStep)
{
    const auto tempo = timeline::buildTempoMap (project);
    std::vector<Clip> changing;

    for (const auto& id : clipIds)
        if (auto clip = clips::findClip (project, id); isUsableAudioClip (clip) && warp::followsTempo (clip) != follow)
            changing.push_back (clip);

    if (changing.empty())
        return false;

    if (newUndoStep)
        project.getUndoManager().beginNewTransaction (follow ? "Follow the song's tempo" : "Play at its own speed");

    for (auto& clip : changing)
    {
        if (follow && ! clip.getState().hasProperty (ids::warpBpm))
            followFromNow (project, clip, tempo);

        set (project, clip.getState(), ids::followTempo, follow);
    }

    return true;
}

bool edit::followTempoFromNow (Project& project, const juce::String& clipId, bool newUndoStep)
{
    auto clip = clips::findClip (project, clipId);

    if (! isUsableAudioClip (clip))
        return false;

    if (newUndoStep)
        project.getUndoManager().beginNewTransaction ("Follow the song's tempo");

    set (project, clip.getState(), ids::followTempo, false);   // (worked out as it sounds now)
    followFromNow (project, clip, timeline::buildTempoMap (project));
    set (project, clip.getState(), ids::followTempo, true);
    return true;
}

bool edit::setClipBpm (Project& project, const juce::String& clipId, double bpm)
{
    auto clip = clips::findClip (project, clipId);

    if (! isUsableAudioClip (clip) || ! std::isfinite (bpm))
        return false;

    bpm = std::round (std::clamp (bpm, minimumBpm, maximumBpm) * 1000.0) / 1000.0;

    if (clip.getState().hasProperty (ids::warpBpm) && warp::bpmOf (clip) == bpm)
        return false;

    project.getUndoManager().beginNewTransaction ("Set the clip's tempo");
    set (project, clip.getState(), ids::warpBpm, bpm);
    return true;
}

bool edit::setClipTranspose (Project& project, const juce::StringArray& clipIds, double semitones)
{
    semitones = std::isfinite (semitones) ? std::round (std::clamp (semitones, -24.0, 24.0) * 100.0) / 100.0 : 0.0;
    bool started = false;

    for (const auto& id : clipIds)
    {
        auto clip = clips::findClip (project, id);

        if (! isUsableAudioClip (clip) || warp::transposeOf (clip) == semitones)
            continue;

        if (! started)
            project.getUndoManager().beginNewTransaction (semitones == 0.0 ? "Back to the original pitch" : "Transpose");

        started = true;

        if (semitones == 0.0)
            clip.getState().removeProperty (ids::transpose, &project.getUndoManager());
        else
            set (project, clip.getState(), ids::transpose, semitones);
    }

    return started;
}

bool edit::addWarpMarker (Project& project, const juce::String& clipId, juce::int64 fileSample)
{
    auto clip = clips::findClip (project, clipId);

    if (! isUsableAudioClip (clip) || clip.isLooping())
        return false;

    fileSample = std::clamp<juce::int64> (fileSample, clip.getOffset(), clip.getOffset() + clip.getDuration());
    project.getUndoManager().beginNewTransaction ("Add warp marker");

    // Following the tempo from now (if it wasn't), so nothing moves.
    if (! warp::followsTempo (clip))
    {
        if (! clip.getState().hasProperty (ids::warpBpm))
            followFromNow (project, clip, timeline::buildTempoMap (project));

        set (project, clip.getState(), ids::followTempo, true);
    }

    const warp::ClipTiming timing (clip);
    auto markers = warp::markersOf (clip);

    // The first marker also pins the clip's start (so moving the marker stretches what's
    // between, rather than sliding everything).
    const auto pin = [&] (juce::int64 sample)
    {
        if (std::none_of (markers.begin(), markers.end(), [sample] (const warp::Marker& m) { return m.sample == sample; }))
            markers.push_back ({ sample, (Tick) std::llround (timing.beatOf ((double) sample)) });
    };

    if (markers.empty() && fileSample > clip.getOffset())
        pin (clip.getOffset());

    pin (fileSample);
    markers = tidy (std::move (markers));

    // Too close to a marker that's already there: nothing to add.
    if (std::none_of (markers.begin(), markers.end(), [fileSample] (const warp::Marker& m) { return m.sample == fileSample; })
        || markers == warp::markersOf (clip))
    {
        project.getUndoManager().undoCurrentTransactionOnly();
        return false;
    }

    writeMarkers (project, clip.getState(), markers);
    return true;
}

bool edit::moveWarpMarker (Project& project, const juce::String& clipId, int index, Tick tick, bool newUndoStep)
{
    auto clip = clips::findClip (project, clipId);
    auto markers = warp::markersOf (clip);

    if (! isUsableAudioClip (clip) || ! warp::followsTempo (clip) || ! juce::isPositiveAndBelow (index, (int) markers.size()))
        return false;

    // From a timeline position to a beat position of the file (the clip's start stays put).
    const warp::ClipTiming timing (clip);
    const auto startBeat = timing.beatOf ((double) clip.getOffset());
    auto beat = (Tick) std::llround (startBeat + (double) (tick - clip.getStart()));

    if (index > 0)
        beat = std::max (beat, markers[(size_t) index - 1].tick + minimumGap);
    if (index + 1 < (int) markers.size())
        beat = std::min (beat, markers[(size_t) index + 1].tick - minimumGap);

    // A marker at the clip's start can't move it (drag the clip for that).
    if (markers[(size_t) index].sample <= clip.getOffset())
        return false;

    if (beat == markers[(size_t) index].tick)
        return false;

    if (newUndoStep)
        project.getUndoManager().beginNewTransaction ("Move warp marker");

    markers[(size_t) index].tick = beat;
    writeMarkers (project, clip.getState(), markers);
    return true;
}

bool edit::removeWarpMarker (Project& project, const juce::String& clipId, int index)
{
    auto clip = clips::findClip (project, clipId);
    auto markers = warp::markersOf (clip);

    if (! isUsableAudioClip (clip) || ! juce::isPositiveAndBelow (index, (int) markers.size()))
        return false;

    project.getUndoManager().beginNewTransaction ("Remove warp marker");
    markers.erase (markers.begin() + index);
    writeMarkers (project, clip.getState(), markers);
    return true;
}

bool edit::clearWarpMarkers (Project& project, const juce::String& clipId)
{
    auto clip = clips::findClip (project, clipId);

    if (! isUsableAudioClip (clip) || warp::markersOf (clip).empty())
        return false;

    project.getUndoManager().beginNewTransaction ("Remove warp markers");
    writeMarkers (project, clip.getState(), {});
    return true;
}

void edit::setWarp (Project& project, const juce::String& clipId, double bpm, const std::vector<warp::Marker>& markers,
                    const juce::String& undoName, bool newUndoStep)
{
    auto clip = clips::findClip (project, clipId);

    if (! isUsableAudioClip (clip))
        return;

    if (newUndoStep)
        project.getUndoManager().beginNewTransaction (undoName);

    set (project, clip.getState(), ids::warpBpm, std::round (std::clamp (bpm, minimumBpm, maximumBpm) * 1000.0) / 1000.0);
    set (project, clip.getState(), ids::followTempo, true);
    writeMarkers (project, clip.getState(), tidy (markers));
}

} // namespace aether
