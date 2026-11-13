// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "MidiTiming.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <optional>

#include "ClipEdits.h"
#include "NoteEdits.h"
#include "TimelineEdits.h"
#include "Warp.h"

namespace aether
{

namespace
{
    //==============================================================================
    // How clips were played. The first timing fix remembers, on each clip, when each moment of
    // it was played (a map from ticks, counted from the clip's origin, to seconds) and the song's
    // tempo before and after. Later fixes start from that, not from the fixed notes, so choices
    // don't pile up on each other. Notes moved by hand since keep their place among the notes
    // around them; a clip moved by hand is taken as it is now.

    juce::String tempoText (const std::vector<TempoPoint>& points)
    {
        juce::StringArray parts;
        for (const auto& p : points)
            parts.add (juce::String ((juce::int64) p.tick) + ":" + juce::String (p.bpm, 3) + ":"
                       + (p.curve == TempoCurve::ramp ? "ramp" : "jump"));

        return parts.joinIntoString (" ");
    }

    std::vector<TempoPoint> tempoFromText (const juce::String& text)
    {
        std::vector<TempoPoint> points;

        for (const auto& part : juce::StringArray::fromTokens (text, " ", {}))
        {
            const auto fields = juce::StringArray::fromTokens (part, ":", {});
            if (fields.size() == 3 && fields[1].getDoubleValue() > 0.0)
                points.push_back ({ (Tick) fields[0].getLargeIntValue(), fields[1].getDoubleValue(),
                                    fields[2] == "ramp" ? TempoCurve::ramp : TempoCurve::jump });
        }

        return points;
    }

    juce::String songTempoText (const Project& project)
    {
        return tempoText (timeline::buildTempoMap (project).getTempoPoints());
    }

    /** When each moment of a clip was played: (tick from the clip's origin, seconds), ticks
        rising. Between them it's in proportion; beyond them at the overall pace. */
    struct PlayedMap
    {
        std::vector<std::pair<double, double>> points;

        double secondsAt (double tick) const
        {
            const auto& first = points.front();
            const auto& last = points.back();
            const auto pace = last.first > first.first ? (last.second - first.second) / (last.first - first.first)
                                                       : 0.5 / (double) ticksPerQuarter;   // 120 BPM

            if (tick <= first.first)
                return first.second + (tick - first.first) * pace;

            if (tick >= last.first)
                return last.second + (tick - last.first) * pace;

            const auto after = std::upper_bound (points.begin(), points.end(), tick,
                                                 [] (double t, const auto& p) { return t < p.first; });
            const auto& b = *after;
            const auto& a = *(after - 1);
            return b.first > a.first ? a.second + (tick - a.first) * (b.second - a.second) / (b.first - a.first) : a.second;
        }

        juce::String toText() const
        {
            juce::String text;
            text.preallocateBytes (points.size() * 24);

            for (const auto& [tick, seconds] : points)
                text << (juce::int64) std::llround (tick) << ":" << juce::String (seconds, 6) << " ";

            return text.trimEnd();
        }

        static PlayedMap fromText (const juce::String& text)
        {
            PlayedMap map;

            for (const auto& part : juce::StringArray::fromTokens (text, " ", {}))
            {
                const auto colon = part.indexOfChar (':');
                if (colon > 0)
                    map.points.emplace_back ((double) part.substring (0, colon).getLargeIntValue(),
                                             part.substring (colon + 1).getDoubleValue());
            }

            map.tidy();
            return map;
        }

        /** Ticks rising, one point per tick, and seconds never going back. */
        void tidy()
        {
            std::stable_sort (points.begin(), points.end(), [] (const auto& a, const auto& b) { return a.first < b.first; });
            std::vector<std::pair<double, double>> kept;

            for (const auto& p : points)
                if (kept.empty() || (p.first > kept.back().first && p.second >= kept.back().second))
                    kept.push_back (p);

            points = std::move (kept);
        }
    };

    Tick originOf (const Clip& clip)    { return clip.getStart() - clip.getContentStart(); }

    /** The clip's remembered playing, if it has one that still fits (the clip wasn't moved). */
    std::optional<PlayedMap> playedMapOf (const Clip& clip)
    {
        const auto record = clip.getState().getChildWithName (ids::PlayedTiming);

        if (! clip.isMidi() || ! record.isValid() || (Tick) (juce::int64) record[ids::origin] != originOf (clip))
            return std::nullopt;

        auto map = PlayedMap::fromText (record[ids::timeMap].toString());
        return map.points.size() >= 2 ? std::optional<PlayedMap> (std::move (map)) : std::nullopt;
    }

    /** An audio clip's record of its first timing fix (a recording is always "as played": the
        record says where it was and what the song's tempo was), if the clip hasn't moved since. */
    juce::ValueTree audioRecordOf (const Clip& clip)
    {
        const auto record = clip.getState().getChildWithName (ids::PlayedTiming);

        if (clip.isMidi() || ! record.isValid() || ! record.hasProperty (ids::fixedStart)
            || (Tick) (juce::int64) record[ids::fixedStart] != clip.getStart())
            return {};

        return record;
    }

    juce::String markersText (const std::vector<warp::Marker>& markers)
    {
        juce::StringArray parts;
        for (const auto& m : markers)
            parts.add (juce::String (m.sample) + ":" + juce::String ((juce::int64) m.tick));
        return parts.joinIntoString (" ");
    }

    std::vector<warp::Marker> markersFromText (const juce::String& text)
    {
        std::vector<warp::Marker> markers;
        for (const auto& part : juce::StringArray::fromTokens (text, " ", {}))
            if (const auto colon = part.indexOfChar (':'); colon > 0)
                markers.push_back ({ part.substring (0, colon).getLargeIntValue(), (Tick) part.substring (colon + 1).getLargeIntValue() });
        return markers;
    }

    /** How an audio clip was before its first timing fix: where it started, and its warp. */
    struct AudioOriginal
    {
        Tick start = 0;
        juce::int64 offset = 0, duration = 0;   // the part of the recording it played
        bool follows = false, hasBpm = false;
        double bpm = 120.0;
        juce::String markers;
    };

    AudioOriginal originalOf (const Clip& clip)
    {
        if (const auto record = audioRecordOf (clip); record.isValid())
            return { (Tick) (juce::int64) record[ids::origin], (juce::int64) record[ids::offset], (juce::int64) record[ids::duration],
                     (bool) record[ids::followTempo], record.hasProperty (ids::warpBpm),
                     (double) record.getProperty (ids::warpBpm, 120.0), record[ids::warpMarkers].toString() };

        return { clip.getStart(), clip.getOffset(), clip.getDuration(), warp::followsTempo (clip),
                 clip.getState().hasProperty (ids::warpBpm), warp::bpmOf (clip), markersText (warp::markersOf (clip)) };
    }

    /** When a clip and its notes were played, in seconds, before anything changes. */
    struct NoteTimes
    {
        juce::ValueTree node;   // (none for an audio clip's hits)
        double start = 0.0, end = 0.0;
        bool audible = false;   // starts inside the clip's edges
        int pitch = 60, velocity = 100;
    };

    struct ClipTimes
    {
        juce::ValueTree clip;
        double start = 0.0, end = 0.0;
        bool remembered = false;   // from the clip's remembered playing (not as the song plays it now)
        std::vector<NoteTimes> notes;

        // Audio clips: the recording at its own speed, from where it starts.
        bool audio = false;
        juce::int64 offset = 0, duration = 0;
        double rate = 48000.0;
    };

    std::vector<ClipTimes> timesOf (const Project& project, const juce::StringArray& clipIds, const TempoMap& tempo,
                                    const miditiming::AudioHits& hits)
    {
        std::vector<ClipTimes> result;

        for (const auto& id : clipIds)
        {
            const auto clip = clips::findClip (project, id);

            if (! clip.isMidi())
            {
                // A recording: where it started (before any timing fix), then at its own speed.
                ClipTimes c;
                c.clip = clip.getState();
                c.audio = true;
                c.rate = clip.getSourceRate();

                const auto record = audioRecordOf (clip);
                const auto original = originalOf (clip);
                c.remembered = record.isValid();
                c.offset = original.offset;
                c.duration = original.duration;
                c.start = c.remembered ? TempoMap (tempoFromText (record[ids::tempoBefore].toString()), tempo.getTimeSignatures())
                                             .ticksToSeconds ((Tick) (juce::int64) record[ids::origin])
                                       : tempo.ticksToSeconds (clip.getStart());
                c.end = c.start + (double) c.duration / c.rate;

                if (hits)
                {
                    std::vector<audioonsets::Hit> inside;
                    for (const auto& h : hits (clip))
                        if (h.sample >= c.offset && h.sample < c.offset + c.duration)
                            inside.push_back (h);

                    for (const auto& n : audioonsets::asNotes (inside, c.rate, c.start - (double) c.offset / c.rate))
                        c.notes.push_back ({ {}, n.seconds, n.seconds + n.length, true, n.pitch, n.velocity });
                }

                result.push_back (std::move (c));
                continue;
            }

            const auto origin = originOf (clip);
            const auto played = playedMapOf (clip);
            const auto secondsOf = [&] (Tick at)
            {
                return played ? played->secondsAt ((double) (at - origin)) : tempo.ticksToSeconds (at);
            };

            ClipTimes c;
            c.clip = clip.getState();
            c.start = secondsOf (clip.getStart());
            c.end = secondsOf (clip.getStart() + clip.getLengthTicks());
            c.remembered = played.has_value();

            for (const auto& n : clip.getState())
            {
                if (! n.hasType (ids::Note))
                    continue;

                const auto tick = (Tick) (juce::int64) n[ids::tick];
                const auto at = origin + tick;
                const auto length = std::max<Tick> (1, (Tick) (juce::int64) n[ids::length]);
                const auto audible = tick >= clip.getContentStart() && tick < clip.getContentStart() + clip.getLengthTicks();
                c.notes.push_back ({ n, secondsOf (at), secondsOf (at + length), audible,
                                     juce::jlimit (0, 127, (int) n[ids::pitch]), juce::jlimit (1, 127, (int) n[ids::velocity]) });
            }

            result.push_back (std::move (c));
        }

        return result;
    }

    /** What a timing change starts from: the song's tempo (as it was before the last fix, if
        that changed it and nothing has changed it since) and when the clips were played. */
    struct Basis
    {
        TempoMap tempo;
        std::optional<std::vector<TempoPoint>> restore;   // the song's tempo goes back to this first
        std::vector<ClipTimes> clips;
    };

    Basis basisOf (const Project& project, const juce::StringArray& usable, const miditiming::AudioHits& hits = {})
    {
        const auto current = timeline::buildTempoMap (project);
        Basis basis { current, std::nullopt, timesOf (project, usable, current, hits) };
        const auto now = tempoText (current.getTempoPoints());

        for (const auto& id : usable)
        {
            const auto record = clips::findClip (project, id).getState().getChildWithName (ids::PlayedTiming);

            if (record.isValid() && record[ids::tempoAfter].toString() == now && record[ids::tempoBefore].toString() != now)
            {
                auto points = tempoFromText (record[ids::tempoBefore].toString());

                if (! points.empty())
                {
                    basis.tempo = TempoMap (points, current.getTimeSignatures());
                    basis.restore = std::move (points);
                    break;
                }
            }
        }

        return basis;
    }

    /** The notes as they were played, in seconds (for finding the beat). */
    std::vector<tempodetect::PlayedNote> notesPlayed (const Basis& basis)
    {
        std::vector<tempodetect::PlayedNote> result;

        for (const auto& c : basis.clips)
            for (const auto& n : c.notes)
                if (n.audible)
                    result.push_back ({ n.start, std::max (0.001, std::min (n.end, c.end) - n.start), n.pitch, n.velocity });

        std::stable_sort (result.begin(), result.end(), [] (const auto& a, const auto& b) { return a.seconds < b.seconds; });
        return result;
    }

    /** Where the earliest of the clips starts, in seconds as played. */
    double earliestPlayed (const Basis& basis)
    {
        auto earliest = std::numeric_limits<double>::max();
        for (const auto& c : basis.clips)
            earliest = std::min (earliest, c.start);

        return basis.clips.empty() ? 0.0 : earliest;
    }

    /** Puts a clip and its notes where `toTick` says their times go. The clip keeps all its
        notes (also any hidden before or after its edges); nothing goes before the song's start. */
    PlayedMap place (Project& project, const ClipTimes& c, const std::function<Tick (double)>& toTick)
    {
        auto& undo = project.getUndoManager();
        auto clip = c.clip;

        const auto start = std::max<Tick> (0, toTick (c.start));
        const auto end = std::max (start + midi::minimumClipLength, toTick (c.end));

        std::vector<std::pair<Tick, Tick>> placed;
        auto origin = start;

        for (const auto& n : c.notes)
        {
            const auto from = toTick (n.start);
            const auto to = std::max (from + 1, toTick (n.end));
            placed.emplace_back (from, to);
            origin = std::min (origin, from);
        }

        origin = std::max<Tick> (0, origin);

        clip.setProperty (ids::start, (juce::int64) start, &undo);
        clip.setProperty (ids::length, (juce::int64) (end - start), &undo);
        clip.setProperty (ids::contentStart, (juce::int64) (start - origin), &undo);

        // When each moment was played, from where it now is (for the next timing change). Only
        // what went exactly where its time said (not what was held back at the song's start, or
        // made longer to be a note at all).
        PlayedMap map;

        if (toTick (c.start) >= 0)
            map.points.emplace_back ((double) (start - origin), c.start);

        if (end == toTick (c.end))
            map.points.emplace_back ((double) (end - origin), c.end);

        for (size_t i = 0; i < c.notes.size(); ++i)
        {
            auto node = c.notes[i].node;
            const auto [from, to] = placed[i];
            const auto tick = std::max<Tick> (0, from - origin);
            const auto length = std::max<Tick> (1, to - std::max (from, origin));
            node.setProperty (ids::tick, (juce::int64) tick, &undo);
            node.setProperty (ids::length, (juce::int64) length, &undo);

            if (from >= origin)
            {
                map.points.emplace_back ((double) tick, c.notes[i].start);

                if (to == toTick (c.notes[i].end))
                    map.points.emplace_back ((double) (tick + length), c.notes[i].end);
            }
        }

        map.tidy();
        return map;
    }

    /** Puts an audio clip's recording where `toTick` says its times go: the clip starts there,
        and warp markers (at the clip's start and end, and at each of `pins`, seconds as played)
        stretch the recording to fit. It remembers where it was first (see audioRecordOf). */
    void placeAudio (Project& project, const ClipTimes& c, const std::function<Tick (double)>& toTick,
                     const std::vector<double>& pins, const TempoMap& tempoAfter, const juce::String& tempoBefore)
    {
        auto& undo = project.getUndoManager();
        auto state = c.clip;
        const Clip clip (state);
        const auto original = originalOf (clip);

        // Anything that would be before the song's start (e.g. silence before the first beat,
        // when bar 1 of the playing goes on the song's first bar) is trimmed off.
        auto offset = c.offset, duration = c.duration;
        auto from = c.start;

        if (toTick (c.start) < 0)
        {
            double early = c.start, late = c.end;
            for (int i = 0; i < 60; ++i)
            {
                const auto middle = 0.5 * (early + late);
                (toTick (middle) < 0 ? early : late) = middle;
            }

            const auto cut = std::min (duration - 1, (juce::int64) std::ceil ((late - c.start) * c.rate));
            offset += cut;
            duration -= cut;
            from = c.start + (double) cut / c.rate;
        }

        const auto start = std::max<Tick> (0, toTick (from));

        std::vector<warp::Marker> markers { { offset, 0 } };
        for (const auto seconds : pins)
            if (seconds > from + 0.002 && seconds < c.end - 0.002)
                markers.push_back ({ c.offset + (juce::int64) std::llround ((seconds - c.start) * c.rate), toTick (seconds) - start });

        markers.push_back ({ offset + duration, toTick (c.end) - start });

        edit::setWarp (project, clip.getId(), tempoAfter.tempoAt (toTick (c.end)), markers, {}, false);
        state.setProperty (ids::start, (juce::int64) start, &undo);
        state.setProperty (ids::offset, offset, &undo);
        state.setProperty (ids::duration, duration, &undo);
        state.setProperty (ids::contentLength, duration, &undo);

        // What it was before the first fix, to start from (and put back) later.
        if (const auto old = state.getChildWithName (ids::PlayedTiming); old.isValid())
            state.removeChild (old, &undo);

        juce::ValueTree record (ids::PlayedTiming);
        record.setProperty (ids::origin, (juce::int64) original.start, nullptr);
        record.setProperty (ids::offset, original.offset, nullptr);
        record.setProperty (ids::duration, original.duration, nullptr);
        record.setProperty (ids::fixedStart, (juce::int64) start, nullptr);
        record.setProperty (ids::tempoBefore, tempoBefore, nullptr);
        record.setProperty (ids::tempoAfter, songTempoText (project), nullptr);
        record.setProperty (ids::followTempo, original.follows, nullptr);
        if (original.hasBpm)
            record.setProperty (ids::warpBpm, original.bpm, nullptr);
        record.setProperty (ids::warpMarkers, original.markers, nullptr);
        state.appendChild (record, &undo);
    }

    /** Remembers how a clip was played (see above), replacing what it remembered before. */
    void remember (Project& project, juce::ValueTree clip, const PlayedMap& map, const juce::String& tempoBefore)
    {
        auto& undo = project.getUndoManager();

        if (const auto old = clip.getChildWithName (ids::PlayedTiming); old.isValid())
            clip.removeChild (old, &undo);

        if (map.points.size() < 2)
            return;

        juce::ValueTree record (ids::PlayedTiming);
        record.setProperty (ids::origin, (juce::int64) ((Tick) (juce::int64) clip[ids::start] - (Tick) (juce::int64) clip[ids::contentStart]), nullptr);
        record.setProperty (ids::timeMap, map.toText(), nullptr);
        record.setProperty (ids::tempoBefore, tempoBefore, nullptr);
        record.setProperty (ids::tempoAfter, songTempoText (project), nullptr);
        clip.appendChild (record, &undo);
    }

    /** Where each beat goes: beat `anchorIndex` at `anchor`, the others a beat of the time
        signature apart. */
    std::vector<Tick> beatTicks (const TempoMap& tempo, Tick anchor, int anchorIndex, int count)
    {
        std::vector<Tick> ticks ((size_t) count);
        ticks[(size_t) anchorIndex] = anchor;

        for (int k = anchorIndex + 1; k < count; ++k)
            ticks[(size_t) k] = ticks[(size_t) k - 1] + tempo.beatLengthAt (ticks[(size_t) k - 1]);

        for (int k = anchorIndex - 1; k >= 0; --k)
            ticks[(size_t) k] = ticks[(size_t) k + 1] - tempo.beatLengthAt (std::max<Tick> (0, ticks[(size_t) k + 1] - 1));

        return ticks;
    }

    /** The bar whose start is nearest `tick` (never before bar 0). */
    int nearestBar (const TempoMap& tempo, Tick tick)
    {
        if (tick <= 0)
            return 0;

        const auto bar = tempo.ticksToBarBeat (tick).bar;
        return (tick - tempo.barToTicks (bar) <= tempo.barToTicks (bar + 1) - tick) ? bar : bar + 1;
    }

    bool isBarLine (const TempoMap& tempo, Tick tick)
    {
        const auto position = tempo.ticksToBarBeat (tick);
        return position.beat == 0 && position.tickInBeat == 0;
    }

    //==============================================================================
    // Making it steady

    struct SteadyPlan
    {
        std::vector<double> beats;   // seconds, as played
        std::vector<Tick> ticks;     // where each goes
        int startBar = -1;
    };

    Tick toSteady (const SteadyPlan& plan, double seconds)
    {
        const auto position = tempodetect::beatPosition (plan.beats, seconds);
        const auto k = std::clamp<std::ptrdiff_t> ((std::ptrdiff_t) std::floor (position), 0, (std::ptrdiff_t) plan.ticks.size() - 2);
        const auto from = (double) plan.ticks[(size_t) k], to = (double) plan.ticks[(size_t) k + 1];
        return (Tick) std::llround (from + (position - (double) k) * (to - from));
    }

    TempoMap steadyTempo (const TempoMap& current, double songBpm)
    {
        return songBpm > 0.0 ? TempoMap ({ TempoPoint { 0, songBpm, TempoCurve::jump } }, current.getTimeSignatures()) : current;
    }

    std::optional<SteadyPlan> planSteady (const Basis& basis, const tempodetect::Result& beat,
                                          const miditiming::SteadyOptions& options)
    {
        const auto count = (int) beat.beats.size();

        if (! beat.found || count < 2 || basis.clips.empty())
            return std::nullopt;

        const auto& current = basis.tempo;
        const auto target = steadyTempo (current, options.songBpm);
        const auto downbeat = std::clamp (beat.downbeat, 0, count - 1);

        // Bar 1 of the playing goes on the bar line nearest where it would be if the time from
        // the clip's start to it stayed the same (in the target tempo).
        const auto clipStartSeconds = earliestPlayed (basis);
        const auto clipStart = current.secondsToTicks (clipStartSeconds);
        const auto afterStart = beat.beats[(size_t) downbeat] - clipStartSeconds;
        auto bar = nearestBar (target, target.secondsToTicks (target.ticksToSeconds (clipStart) + afterStart));

        double firstNote = std::numeric_limits<double>::max();
        for (const auto& n : notesPlayed (basis))
            firstNote = std::min (firstNote, n.seconds);

        const auto strength = std::clamp (options.strength, 0.0, 1.0);
        const auto& smooth = beat.smoothBeats.size() == beat.beats.size() ? beat.smoothBeats : beat.beats;

        for (int attempt = 0; attempt < 512; ++attempt, ++bar)
        {
            SteadyPlan plan;
            plan.beats = beat.beats;
            plan.ticks = beatTicks (target, target.barToTicks (bar), downbeat, count);
            plan.startBar = bar;

            // Less than full strength: each beat keeps part of how far it was from the player's
            // smoothed tempo.
            if (strength < 1.0)
            {
                const auto grid = plan.ticks;

                for (int k = 0; k < count; ++k)
                {
                    const auto next = k + 1 < count ? k + 1 : k, previous = k + 1 < count ? k : k - 1;
                    const auto length = smooth[(size_t) next] - smooth[(size_t) previous];
                    const auto off = std::clamp ((beat.beats[(size_t) k] - smooth[(size_t) k]) / length, -0.3, 0.3);
                    const auto beatLength = (double) (grid[(size_t) next] - grid[(size_t) previous]);
                    plan.ticks[(size_t) k] = grid[(size_t) k] + (Tick) std::llround ((1.0 - strength) * off * beatLength);

                    if (k > 0)
                        plan.ticks[(size_t) k] = std::max (plan.ticks[(size_t) k], plan.ticks[(size_t) k - 1] + 1);
                }
            }

            // The first note mustn't land before the start of the song (a pick-up needs room).
            if (firstNote == std::numeric_limits<double>::max() || toSteady (plan, firstNote) >= 0)
                return plan;
        }

        return std::nullopt;
    }

    //==============================================================================
    // Making the tempo follow

    struct FollowPlan
    {
        std::vector<TempoPoint> points;
        int startBar = -1;
    };

    std::optional<FollowPlan> planFollow (const Basis& basis, const tempodetect::Result& beat,
                                          const miditiming::FollowOptions& options)
    {
        const auto count = (int) beat.beats.size();

        if (! beat.found || count < 2 || basis.clips.empty())
            return std::nullopt;

        const auto& current = basis.tempo;
        const auto& beats = beat.beats;
        const auto downbeat = std::clamp (beat.downbeat, 0, count - 1);
        auto bar = nearestBar (current, current.secondsToTicks (beats[(size_t) downbeat]));
        std::vector<Tick> ticks;

        // Bar 1 of the playing on the nearest bar line where the beats before it fit after the
        // start of the song, with some time before the first beat for a lead-in to take up. If
        // the playing starts so soon after the start of the song that even a first bar at the
        // fastest tempo couldn't fit before it, it starts on the song's first bar line instead
        // (its first beat then lasts that little bit longer).
        const auto quickest = (double) current.barToTicks (1) / (double) ticksPerQuarter * 60.0 / TempoMap::maxBpm;

        for (int attempt = 0; attempt < 512; ++attempt, ++bar)
        {
            ticks = beatTicks (current, current.barToTicks (bar), downbeat, count);

            if (ticks.front() > 0 || (ticks.front() == 0 && beats[0] <= std::max (0.1 * (beats[1] - beats[0]), quickest)))
                break;
        }

        if (ticks.front() < 0)
            return std::nullopt;

        FollowPlan plan;
        plan.startBar = bar;

        // When the first beat will really come: the lead-in (or the song's start) decides.
        double reached = 0.0;

        // The lead-in: from the bar line before the first beat, the tempo that gets there at the
        // moment it was played. (Earlier tempo changes stay; if that would be impossibly fast,
        // the lead-in starts a bar earlier.)
        if (ticks.front() > 0)
        {
            auto leadBar = current.ticksToBarBeat (ticks.front() - 1).bar;
            const auto& existing = current.getTempoPoints();

            for (;; --leadBar)
            {
                const auto leadStart = current.barToTicks (leadBar);
                std::vector<TempoPoint> kept;

                for (const auto& p : existing)
                    if (p.tick < leadStart)
                        kept.push_back (p);

                if (! kept.empty())
                    kept.back().curve = TempoCurve::jump;   // a ramp would now lead somewhere else

                auto withLead = kept;
                withLead.push_back ({ leadStart, kept.empty() ? 120.0 : kept.back().bpm, TempoCurve::jump });
                const auto leadSeconds = beats[0] - TempoMap (withLead, current.getTimeSignatures()).ticksToSeconds (leadStart);
                const auto leadBpm = leadSeconds > 0.0 ? (double) (ticks.front() - leadStart) / ticksPerQuarter * 60.0 / leadSeconds
                                                       : std::numeric_limits<double>::max();

                if (leadBpm <= TempoMap::maxBpm || leadBar <= 0)
                {
                    const auto bpm = std::clamp (leadBpm, TempoMap::minBpm, TempoMap::maxBpm);
                    plan.points = kept;
                    plan.points.push_back ({ leadStart, bpm, TempoCurve::jump });
                    reached = beats[0] - leadSeconds + (double) (ticks.front() - leadStart) / ticksPerQuarter * 60.0 / bpm;
                    break;
                }
            }
        }

        // The playing: a tempo change on each bar line (or beat), lasting until the next.
        std::vector<int> marks { 0 };
        for (int k = 1; k < count - 1; ++k)
            if (options.everyBeat || isBarLine (current, ticks[(size_t) k]))
                marks.push_back (k);

        marks.push_back (count - 1);

        // Each change lasts from where the last one really got to (normally exactly its beat)
        // until its own last beat, so one that couldn't get there exactly is caught up by the next.
        for (size_t i = 0; i + 1 < marks.size(); ++i)
        {
            const auto from = (size_t) marks[i], to = (size_t) marks[i + 1];
            const auto quarters = (double) (ticks[to] - ticks[from]) / ticksPerQuarter;
            const auto seconds = beats[to] - reached;
            const auto bpm = std::clamp (seconds > 0.0 ? quarters * 60.0 / seconds : TempoMap::maxBpm, TempoMap::minBpm, TempoMap::maxBpm);
            plan.points.push_back ({ ticks[from], bpm, TempoCurve::jump });
            reached += quarters * 60.0 / bpm;
        }

        // After the playing: the last tempo carries on, up to any later tempo changes the song had.
        for (const auto& p : current.getTempoPoints())
            if (p.tick > ticks.back())
                plan.points.push_back (p);

        return plan;
    }
}

//==============================================================================
juce::StringArray miditiming::usableClips (const Project& project, const juce::StringArray& clipIds)
{
    juce::StringArray result;

    for (const auto& id : clipIds)
    {
        const auto clip = clips::findClip (project, id);
        if (clip.isValid() && ! clip.isLooping())
            result.addIfNotAlreadyThere (id);
    }

    return result;
}

std::vector<tempodetect::PlayedNote> miditiming::playedNotes (const Project& project, const juce::StringArray& clipIds,
                                                              const AudioHits& hits)
{
    return notesPlayed (basisOf (project, usableClips (project, clipIds), hits));
}

tempodetect::Result miditiming::findBeat (const Project& project, const juce::StringArray& clipIds, double hintBpm,
                                          const AudioHits& hits)
{
    const auto usable = usableClips (project, clipIds);

    if (usable.isEmpty())
        return {};

    const auto basis = basisOf (project, usable, hits);
    tempodetect::Options options;
    options.hintBpm = hintBpm;
    options.beatsPerBar = basis.tempo.signatureAt (basis.tempo.secondsToTicks (earliestPlayed (basis))).numerator;
    return tempodetect::analyse (notesPlayed (basis), options);
}

int miditiming::steadyStartBar (const Project& project, const juce::StringArray& clipIds, const tempodetect::Result& beat,
                                const SteadyOptions& options, const AudioHits& hits)
{
    const auto plan = planSteady (basisOf (project, usableClips (project, clipIds), hits), beat, options);
    return plan ? plan->startBar : -1;
}

int miditiming::followStartBar (const Project& project, const juce::StringArray& clipIds, const tempodetect::Result& beat,
                                const AudioHits& hits)
{
    const auto plan = planFollow (basisOf (project, usableClips (project, clipIds), hits), beat, {});
    return plan ? plan->startBar : -1;
}

double miditiming::songTempoAt (const Project& project, const juce::StringArray& clipIds, const AudioHits& hits)
{
    const auto basis = basisOf (project, usableClips (project, clipIds), hits);
    return basis.tempo.tempoAt (basis.clips.empty() ? 0 : basis.tempo.secondsToTicks (earliestPlayed (basis)));
}

int miditiming::songTempoChanges (const Project& project, const juce::StringArray& clipIds, const AudioHits& hits)
{
    return (int) basisOf (project, usableClips (project, clipIds), hits).tempo.getTempoPoints().size();
}

bool miditiming::remembersPlaying (const Project& project, const juce::StringArray& clipIds)
{
    for (const auto& id : usableClips (project, clipIds))
    {
        const auto clip = clips::findClip (project, id);
        if (playedMapOf (clip) || audioRecordOf (clip).isValid())
            return true;
    }

    return false;
}

bool miditiming::wasFixed (const Project& project, const juce::StringArray& clipIds, const AudioHits& hits)
{
    const auto basis = basisOf (project, usableClips (project, clipIds), hits);

    if (basis.restore)
        return true;

    return std::any_of (basis.clips.begin(), basis.clips.end(), [] (const ClipTimes& c) { return c.remembered; });
}

//==============================================================================
bool edit::makeTimingSteady (Project& project, const juce::StringArray& clipIds, const tempodetect::Result& beat,
                             const miditiming::SteadyOptions& options, const miditiming::AudioHits& hits)
{
    const auto usable = miditiming::usableClips (project, clipIds);
    const auto basis = basisOf (project, usable, hits);
    const auto plan = planSteady (basis, beat, options);

    if (! plan)
        return false;

    project.getUndoManager().beginNewTransaction ("Make timing steady");
    const auto tempoBefore = tempoText (basis.tempo.getTempoPoints());

    if (options.songBpm > 0.0)
        edit::replaceTempo (project, { TempoPoint { 0, options.songBpm, TempoCurve::jump } }, {}, false);
    else if (basis.restore)
        edit::replaceTempo (project, *basis.restore, {}, false);

    const auto toTick = [&] (double seconds) { return toSteady (*plan, seconds); };
    const auto tempoAfter = timeline::buildTempoMap (project);
    std::vector<std::pair<juce::ValueTree, PlayedMap>> remembered;

    for (const auto& c : basis.clips)
    {
        if (c.audio)
            placeAudio (project, c, toTick, plan->beats, tempoAfter, tempoBefore);   // a marker on every beat
        else
            remembered.emplace_back (c.clip, place (project, c, toTick));
    }

    for (const auto& [clip, map] : remembered)
        remember (project, clip, map, tempoBefore);

    return true;
}

bool edit::makeTempoFollow (Project& project, const juce::StringArray& clipIds, const tempodetect::Result& beat,
                            const miditiming::FollowOptions& options, const miditiming::AudioHits& hits)
{
    const auto usable = miditiming::usableClips (project, clipIds);
    const auto basis = basisOf (project, usable, hits);
    const auto plan = planFollow (basis, beat, options);

    if (! plan)
        return false;

    project.getUndoManager().beginNewTransaction ("Make the tempo follow the playing");
    const auto tempoBefore = tempoText (basis.tempo.getTempoPoints());
    edit::replaceTempo (project, plan->points, {}, false);

    // The notes (and recordings) keep the time they were played at.
    const auto tempo = timeline::buildTempoMap (project);
    const auto toTick = [&] (double seconds) { return tempo.secondsToTicks (seconds); };
    std::vector<double> tempoChanges;
    for (const auto& p : tempo.getTempoPoints())
        tempoChanges.push_back (tempo.ticksToSeconds (p.tick));

    std::vector<std::pair<juce::ValueTree, PlayedMap>> remembered;

    for (const auto& c : basis.clips)
    {
        if (c.audio)
            placeAudio (project, c, toTick, tempoChanges, tempo, tempoBefore);   // in the bars from now on
        else
            remembered.emplace_back (c.clip, place (project, c, toTick));
    }

    for (const auto& [clip, map] : remembered)
        remember (project, clip, map, tempoBefore);

    return true;
}

bool edit::putBackAsPlayed (Project& project, const juce::StringArray& clipIds, const miditiming::AudioHits& hits)
{
    const auto usable = miditiming::usableClips (project, clipIds);

    if (! miditiming::wasFixed (project, usable, hits))
        return false;

    const auto basis = basisOf (project, usable, hits);
    auto& undo = project.getUndoManager();
    undo.beginNewTransaction ("Put the timing back as played");

    if (basis.restore)
        edit::replaceTempo (project, *basis.restore, {}, false);

    // The notes go back to when they were played (with the song's tempo as it was then, they're
    // exactly where they were); recordings to where they were, with their warp as it was.
    const auto tempo = timeline::buildTempoMap (project);

    for (const auto& c : basis.clips)
    {
        auto clip = c.clip;

        if (c.audio && c.remembered)
        {
            const auto original = originalOf (Clip (clip));
            edit::setWarp (project, Clip (clip).getId(), original.bpm, markersFromText (original.markers), {}, false);
            clip.setProperty (ids::followTempo, original.follows, &undo);
            if (! original.hasBpm)
                clip.removeProperty (ids::warpBpm, &undo);
            clip.setProperty (ids::start, (juce::int64) std::max<Tick> (0, tempo.secondsToTicks (c.start)), &undo);
            clip.setProperty (ids::offset, original.offset, &undo);
            clip.setProperty (ids::duration, original.duration, &undo);
            clip.setProperty (ids::contentLength, original.duration, &undo);
        }
        else if (c.remembered)
        {
            place (project, c, [&] (double seconds) { return tempo.secondsToTicks (seconds); });
        }

        if (const auto record = clip.getChildWithName (ids::PlayedTiming); record.isValid())
            clip.removeChild (record, &undo);
    }

    return true;
}

bool edit::quantizeAudioClips (Project& project, const juce::StringArray& clipIds, const miditiming::AudioHits& hits,
                               GridSize grid, double strength, double swing)
{
    if (! hits)
        return false;

    const auto tempo = timeline::buildTempoMap (project);
    const auto size = grid == GridSize::off ? GridSize::sixteenth : grid;
    bool started = false;

    // On the song's grid: lines counted from the start of the bar the position is in.
    const auto onGrid = [&] (Tick position)
    {
        const auto barStart = tempo.barToTicks (tempo.ticksToBarBeat (position).bar);
        return barStart + notes::quantizeTick (position - barStart, gridStepAt (tempo, position, size), strength, swing);
    };

    for (const auto& id : clipIds)
    {
        const auto clip = clips::findClip (project, id);

        if (! clip.isValid() || clip.isMidi() || clip.isLooping())
            continue;

        // Each clear hit (not the faintest) where it plays now, moved towards the grid; the clip's
        // start and end stay where they are.
        const warp::ClipTiming timing (clip);
        const auto start = clip.getStart();
        const auto offset = clip.getOffset(), duration = clip.getDuration();
        const auto rate = clip.getSourceRate();
        std::vector<warp::Marker> markers { { offset, 0 } };

        for (const auto& h : hits (clip))
        {
            const auto played = (double) (h.sample - offset);
            if (h.strength < 0.1f || played < 0.01 * rate || played > (double) duration - 0.01 * rate)
                continue;

            const auto now = (Tick) std::llround (timing.tickAt (tempo, played));
            markers.push_back ({ h.sample, onGrid (now) - start });
        }

        markers.push_back ({ offset + duration, timing.end (tempo) - start });

        if (markers.size() <= 2)
            continue;

        if (! started)
            project.getUndoManager().beginNewTransaction ("Quantize");

        started = true;
        edit::setWarp (project, id, warp::followsTempo (clip) ? warp::bpmOf (clip) : tempo.tempoAt (start), markers, {}, false);
    }

    return started;
}

} // namespace aether
