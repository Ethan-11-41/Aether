// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "MidiTiming.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <optional>

#include "ClipEdits.h"
#include "TimelineEdits.h"

namespace aether
{

namespace
{
    /** When a clip and its notes sound, in seconds, before anything changes. */
    struct NoteTimes
    {
        juce::ValueTree node;
        double start = 0.0, end = 0.0;
    };

    struct ClipTimes
    {
        juce::ValueTree clip;
        double start = 0.0, end = 0.0;
        std::vector<NoteTimes> notes;
    };

    std::vector<ClipTimes> timesOf (const Project& project, const juce::StringArray& clipIds, const TempoMap& tempo)
    {
        std::vector<ClipTimes> result;

        for (const auto& id : clipIds)
        {
            const auto clip = clips::findClip (project, id);
            const auto origin = clip.getStart() - clip.getContentStart();

            ClipTimes c;
            c.clip = clip.getState();
            c.start = tempo.ticksToSeconds (clip.getStart());
            c.end = tempo.ticksToSeconds (clip.getStart() + clip.getLengthTicks());

            for (const auto& n : clip.getState())
            {
                if (! n.hasType (ids::Note))
                    continue;

                const auto at = origin + (Tick) (juce::int64) n[ids::tick];
                const auto length = std::max<Tick> (1, (Tick) (juce::int64) n[ids::length]);
                c.notes.push_back ({ n, tempo.ticksToSeconds (at), tempo.ticksToSeconds (at + length) });
            }

            result.push_back (std::move (c));
        }

        return result;
    }

    /** Puts a clip and its notes where `toTick` says their times go. The clip keeps all its
        notes (also any hidden before or after its edges); nothing goes before the song's start. */
    void place (Project& project, const ClipTimes& c, const std::function<Tick (double)>& toTick)
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

        for (size_t i = 0; i < c.notes.size(); ++i)
        {
            auto node = c.notes[i].node;
            const auto [from, to] = placed[i];
            node.setProperty (ids::tick, (juce::int64) std::max<Tick> (0, from - origin), &undo);
            node.setProperty (ids::length, (juce::int64) std::max<Tick> (1, to - std::max (from, origin)), &undo);
        }
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

    Tick earliestStart (const Project& project, const juce::StringArray& clipIds)
    {
        auto earliest = std::numeric_limits<Tick>::max();
        for (const auto& id : clipIds)
            earliest = std::min (earliest, clips::findClip (project, id).getStart());

        return earliest;
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

    std::optional<SteadyPlan> planSteady (const Project& project, const juce::StringArray& clipIds,
                                          const tempodetect::Result& beat, const miditiming::SteadyOptions& options)
    {
        const auto count = (int) beat.beats.size();

        if (! beat.found || count < 2 || clipIds.isEmpty())
            return std::nullopt;

        const auto current = timeline::buildTempoMap (project);
        const auto target = steadyTempo (current, options.songBpm);
        const auto downbeat = std::clamp (beat.downbeat, 0, count - 1);

        // Bar 1 of the playing goes on the bar line nearest where it would be if the time from
        // the clip's start to it stayed the same (in the target tempo).
        const auto clipStart = earliestStart (project, clipIds);
        const auto afterStart = beat.beats[(size_t) downbeat] - current.ticksToSeconds (clipStart);
        auto bar = nearestBar (target, target.secondsToTicks (target.ticksToSeconds (clipStart) + afterStart));

        double firstNote = std::numeric_limits<double>::max();
        for (const auto& n : miditiming::playedNotes (project, clipIds))
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

    std::optional<FollowPlan> planFollow (const Project& project, const tempodetect::Result& beat,
                                          const miditiming::FollowOptions& options)
    {
        const auto count = (int) beat.beats.size();

        if (! beat.found || count < 2)
            return std::nullopt;

        const auto current = timeline::buildTempoMap (project);
        const auto& beats = beat.beats;
        const auto downbeat = std::clamp (beat.downbeat, 0, count - 1);
        auto bar = nearestBar (current, current.secondsToTicks (beats[(size_t) downbeat]));
        std::vector<Tick> ticks;

        // Bar 1 of the playing on the nearest bar line where the beats before it fit after the
        // start of the song, with some time before the first beat for a lead-in to take up.
        for (int attempt = 0; attempt < 512; ++attempt, ++bar)
        {
            ticks = beatTicks (current, current.barToTicks (bar), downbeat, count);

            if (ticks.front() > 0 || (ticks.front() == 0 && beats[0] <= 0.1 * (beats[1] - beats[0])))
                break;
        }

        if (ticks.front() < 0)
            return std::nullopt;

        FollowPlan plan;
        plan.startBar = bar;

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
                    plan.points = kept;
                    plan.points.push_back ({ leadStart, std::clamp (leadBpm, TempoMap::minBpm, TempoMap::maxBpm), TempoCurve::jump });
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

        for (size_t i = 0; i + 1 < marks.size(); ++i)
        {
            const auto from = (size_t) marks[i], to = (size_t) marks[i + 1];
            const auto bpm = (double) (ticks[to] - ticks[from]) / ticksPerQuarter * 60.0 / (beats[to] - beats[from]);
            plan.points.push_back ({ ticks[from], std::clamp (bpm, TempoMap::minBpm, TempoMap::maxBpm), TempoCurve::jump });
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
        if (clip.isValid() && clip.isMidi() && ! clip.isLooping())
            result.addIfNotAlreadyThere (id);
    }

    return result;
}

std::vector<tempodetect::PlayedNote> miditiming::playedNotes (const Project& project, const juce::StringArray& clipIds)
{
    const auto tempo = timeline::buildTempoMap (project);
    std::vector<tempodetect::PlayedNote> result;

    for (const auto& id : usableClips (project, clipIds))
    {
        const auto clip = clips::findClip (project, id);

        for (const auto& n : midi::audibleNotes (clip))
        {
            const auto at = clip.getStart() + n.start;
            const auto seconds = tempo.ticksToSeconds (at);
            result.push_back ({ seconds, tempo.ticksToSeconds (at + n.length) - seconds, n.pitch, n.velocity });
        }
    }

    return result;
}

tempodetect::Result miditiming::findBeat (const Project& project, const juce::StringArray& clipIds, double hintBpm)
{
    const auto usable = usableClips (project, clipIds);

    if (usable.isEmpty())
        return {};

    tempodetect::Options options;
    options.hintBpm = hintBpm;
    options.beatsPerBar = timeline::buildTempoMap (project).signatureAt (earliestStart (project, usable)).numerator;
    return tempodetect::analyse (playedNotes (project, usable), options);
}

int miditiming::steadyStartBar (const Project& project, const juce::StringArray& clipIds, const tempodetect::Result& beat,
                                const SteadyOptions& options)
{
    const auto plan = planSteady (project, usableClips (project, clipIds), beat, options);
    return plan ? plan->startBar : -1;
}

int miditiming::followStartBar (const Project& project, const juce::StringArray& clipIds, const tempodetect::Result& beat)
{
    if (usableClips (project, clipIds).isEmpty())
        return -1;

    const auto plan = planFollow (project, beat, {});
    return plan ? plan->startBar : -1;
}

double miditiming::songTempoAt (const Project& project, const juce::StringArray& clipIds)
{
    const auto usable = usableClips (project, clipIds);
    return timeline::buildTempoMap (project).tempoAt (usable.isEmpty() ? 0 : earliestStart (project, usable));
}

//==============================================================================
bool edit::makeTimingSteady (Project& project, const juce::StringArray& clipIds, const tempodetect::Result& beat,
                             const miditiming::SteadyOptions& options)
{
    const auto usable = miditiming::usableClips (project, clipIds);
    const auto plan = planSteady (project, usable, beat, options);

    if (! plan)
        return false;

    const auto times = timesOf (project, usable, timeline::buildTempoMap (project));

    project.getUndoManager().beginNewTransaction ("Make timing steady");

    if (options.songBpm > 0.0)
        edit::replaceTempo (project, { TempoPoint { 0, options.songBpm, TempoCurve::jump } }, {}, false);

    for (const auto& c : times)
        place (project, c, [&] (double seconds) { return toSteady (*plan, seconds); });

    return true;
}

bool edit::makeTempoFollow (Project& project, const juce::StringArray& clipIds, const tempodetect::Result& beat,
                            const miditiming::FollowOptions& options)
{
    const auto usable = miditiming::usableClips (project, clipIds);

    if (usable.isEmpty())
        return false;

    const auto plan = planFollow (project, beat, options);

    if (! plan)
        return false;

    const auto times = timesOf (project, usable, timeline::buildTempoMap (project));

    project.getUndoManager().beginNewTransaction ("Make the tempo follow the playing");
    edit::replaceTempo (project, plan->points, {}, false);

    // The notes keep the time they were played at.
    const auto tempo = timeline::buildTempoMap (project);
    for (const auto& c : times)
        place (project, c, [&] (double seconds) { return tempo.secondsToTicks (seconds); });

    return true;
}

} // namespace aether
