// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "TempoMap.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace aether
{

namespace
{
    constexpr double ticksPerQuarterD = (double) ticksPerQuarter;

    bool isValidDenominator (int d) noexcept
    {
        return d == 1 || d == 2 || d == 4 || d == 8 || d == 16 || d == 32;
    }

    /** Floor division that works for negative numbers too. */
    Tick floorDiv (Tick a, Tick b) noexcept
    {
        const auto q = a / b;
        return (a % b != 0 && ((a < 0) != (b < 0))) ? q - 1 : q;
    }

    /** True if the ramp between two tempos is so slight that it is effectively constant. */
    bool isFlat (double b0, double b1) noexcept
    {
        return std::abs (b1 - b0) < 1.0e-9;
    }
}

TempoMap::TempoMap()
    : TempoMap ({ TempoPoint {} }, { TimeSignature {} })
{
}

TempoMap::TempoMap (std::vector<TempoPoint> tempoPoints, std::vector<TimeSignature> sigs)
    : points (std::move (tempoPoints)), signatures (std::move (sigs))
{
    prepare();
}

void TempoMap::prepare()
{
    // Tempo points: sorted by position, one per tick (the last one wins), tempos in range,
    // and always one at tick 0.
    for (auto& p : points)
    {
        p.tick = std::max<Tick> (0, p.tick);
        p.bpm = std::isfinite (p.bpm) ? std::clamp (p.bpm, minBpm, maxBpm) : 120.0;
    }

    std::stable_sort (points.begin(), points.end(), [] (const auto& a, const auto& b) { return a.tick < b.tick; });

    std::vector<TempoPoint> unique;
    for (const auto& p : points)
    {
        if (! unique.empty() && unique.back().tick == p.tick)
            unique.back() = p;
        else
            unique.push_back (p);
    }
    points = std::move (unique);

    if (points.empty() || points.front().tick != 0)
        points.insert (points.begin(), TempoPoint { 0, points.empty() ? 120.0 : points.front().bpm, TempoCurve::jump });

    // The last point has nothing to ramp to.
    points.back().curve = TempoCurve::jump;

    pointSeconds.assign (points.size(), 0.0);

    for (size_t i = 1; i < points.size(); ++i)
    {
        const auto& p = points[i - 1];
        const auto beats = (double) (points[i].tick - p.tick) / ticksPerQuarterD;
        const auto b0 = p.bpm, b1 = points[i].bpm;

        pointSeconds[i] = pointSeconds[i - 1]
                          + ((p.curve == TempoCurve::ramp && ! isFlat (b0, b1))
                                 ? 60.0 * beats / (b1 - b0) * std::log (b1 / b0)
                                 : beats * 60.0 / b0);
    }

    // Time signatures: valid values, sorted by bar, one per bar, always one at bar 0.
    for (auto& s : signatures)
    {
        s.bar = std::max (0, s.bar);

        if (s.numerator < 1 || s.numerator > 32 || ! isValidDenominator (s.denominator))
        {
            s.numerator = 4;
            s.denominator = 4;
        }
    }

    std::stable_sort (signatures.begin(), signatures.end(), [] (const auto& a, const auto& b) { return a.bar < b.bar; });

    std::vector<TimeSignature> uniqueSigs;
    for (const auto& s : signatures)
    {
        if (! uniqueSigs.empty() && uniqueSigs.back().bar == s.bar)
            uniqueSigs.back() = s;
        else
            uniqueSigs.push_back (s);
    }
    signatures = std::move (uniqueSigs);

    if (signatures.empty() || signatures.front().bar != 0)
        signatures.insert (signatures.begin(), TimeSignature { 0, 4, 4 });

    spans.clear();
    Tick start = 0;

    for (size_t i = 0; i < signatures.size(); ++i)
    {
        const auto& s = signatures[i];

        if (i > 0)
            start += (Tick) (s.bar - spans.back().bar) * spans.back().barTicks;

        SignatureSpan span;
        span.startTick = start;
        span.bar = s.bar;
        span.numerator = s.numerator;
        span.beatTicks = 4 * ticksPerQuarter / s.denominator;
        span.barTicks = span.beatTicks * s.numerator;
        spans.push_back (span);
    }
}

//==============================================================================
int TempoMap::segmentIndexForTick (Tick tick) const noexcept AETHER_NONBLOCKING
{
    // The last point at or before `tick`.
    auto it = std::upper_bound (points.begin(), points.end(), tick,
                                [] (Tick t, const TempoPoint& p) { return t < p.tick; });
    return std::max (0, (int) (it - points.begin()) - 1);
}

int TempoMap::segmentIndexForSeconds (double seconds) const noexcept AETHER_NONBLOCKING
{
    auto it = std::upper_bound (pointSeconds.begin(), pointSeconds.end(), seconds);
    return std::max (0, (int) (it - pointSeconds.begin()) - 1);
}

int TempoMap::tempoPointIndexAt (Tick tick) const noexcept AETHER_NONBLOCKING
{
    return segmentIndexForTick (tick);
}

double TempoMap::ticksToSeconds (Tick tick) const noexcept AETHER_NONBLOCKING
{
    const auto i = (size_t) segmentIndexForTick (tick);
    const auto& p = points[i];
    const auto beats = (double) (tick - p.tick) / ticksPerQuarterD;   // negative before tick 0

    if (p.curve == TempoCurve::ramp && i + 1 < points.size() && tick > p.tick)
    {
        const auto& next = points[i + 1];
        const auto length = (double) (next.tick - p.tick) / ticksPerQuarterD;
        const auto b0 = p.bpm, b1 = next.bpm;

        if (! isFlat (b0, b1))
        {
            const auto bpmHere = b0 + (b1 - b0) * beats / length;
            return pointSeconds[i] + 60.0 * length / (b1 - b0) * std::log (bpmHere / b0);
        }
    }

    return pointSeconds[i] + beats * 60.0 / p.bpm;
}

Tick TempoMap::secondsToTicks (double seconds) const noexcept AETHER_NONBLOCKING
{
    const auto i = (size_t) segmentIndexForSeconds (seconds);
    const auto& p = points[i];
    const auto elapsed = seconds - pointSeconds[i];
    double beats = elapsed * p.bpm / 60.0;

    if (p.curve == TempoCurve::ramp && i + 1 < points.size() && elapsed > 0.0)
    {
        const auto& next = points[i + 1];
        const auto length = (double) (next.tick - p.tick) / ticksPerQuarterD;
        const auto b0 = p.bpm, b1 = next.bpm;

        if (! isFlat (b0, b1))
        {
            const auto bpmHere = b0 * std::exp (elapsed * (b1 - b0) / (60.0 * length));
            beats = std::clamp ((bpmHere - b0) * length / (b1 - b0), 0.0, length);
        }
    }

    return p.tick + (Tick) std::llround (beats * ticksPerQuarterD);
}

double TempoMap::tempoAt (Tick tick) const noexcept AETHER_NONBLOCKING
{
    const auto i = (size_t) segmentIndexForTick (tick);
    const auto& p = points[i];

    if (p.curve == TempoCurve::ramp && i + 1 < points.size() && tick > p.tick)
    {
        const auto& next = points[i + 1];
        const auto fraction = (double) (tick - p.tick) / (double) (next.tick - p.tick);
        return p.bpm + (next.bpm - p.bpm) * std::min (1.0, fraction);
    }

    return p.bpm;
}

//==============================================================================
int TempoMap::spanIndexForTick (Tick tick) const noexcept AETHER_NONBLOCKING
{
    auto it = std::upper_bound (spans.begin(), spans.end(), tick,
                                [] (Tick t, const SignatureSpan& s) { return t < s.startTick; });
    return std::max (0, (int) (it - spans.begin()) - 1);
}

int TempoMap::spanIndexForBar (int bar) const noexcept AETHER_NONBLOCKING
{
    auto it = std::upper_bound (spans.begin(), spans.end(), bar,
                                [] (int b, const SignatureSpan& s) { return b < s.bar; });
    return std::max (0, (int) (it - spans.begin()) - 1);
}

const TimeSignature& TempoMap::signatureAt (Tick tick) const noexcept AETHER_NONBLOCKING
{
    return signatures[(size_t) spanIndexForTick (tick)];
}

Tick TempoMap::beatLengthAt (Tick tick) const noexcept AETHER_NONBLOCKING
{
    return spans[(size_t) spanIndexForTick (tick)].beatTicks;
}

Tick TempoMap::barLengthAt (Tick tick) const noexcept AETHER_NONBLOCKING
{
    return spans[(size_t) spanIndexForTick (tick)].barTicks;
}

Tick TempoMap::barToTicks (int bar) const noexcept AETHER_NONBLOCKING
{
    const auto& span = spans[(size_t) spanIndexForBar (bar)];
    return span.startTick + (Tick) (bar - span.bar) * span.barTicks;
}

Tick TempoMap::barBeatToTicks (const BarBeat& position) const noexcept AETHER_NONBLOCKING
{
    const auto barStart = barToTicks (position.bar);
    return barStart + (Tick) position.beat * beatLengthAt (barStart) + position.tickInBeat;
}

BarBeat TempoMap::ticksToBarBeat (Tick tick) const noexcept AETHER_NONBLOCKING
{
    const auto& span = spans[(size_t) spanIndexForTick (tick)];
    const auto offset = tick - span.startTick;
    const auto barsIn = floorDiv (offset, span.barTicks);
    const auto inBar = offset - barsIn * span.barTicks;

    BarBeat result;
    result.bar = span.bar + (int) barsIn;
    result.beat = (int) (inBar / span.beatTicks);
    result.tickInBeat = inBar - (Tick) result.beat * span.beatTicks;
    return result;
}

Beat TempoMap::firstBeatAtOrAfter (Tick tick) const noexcept AETHER_NONBLOCKING
{
    const auto spanIndex = (size_t) spanIndexForTick (tick);
    const auto& span = spans[spanIndex];
    const auto offset = tick - span.startTick;
    auto beatsIn = floorDiv (offset, span.beatTicks);

    if (span.startTick + beatsIn * span.beatTicks < tick)
        ++beatsIn;

    auto beatTick = span.startTick + beatsIn * span.beatTicks;

    // Crossed into the next signature? Its first beat is its start.
    if (spanIndex + 1 < spans.size() && beatTick >= spans[spanIndex + 1].startTick)
        return Beat { spans[spanIndex + 1].startTick, spans[spanIndex + 1].bar, 0 };

    const auto barsIn = floorDiv (beatsIn, (Tick) span.numerator);
    return Beat { beatTick, span.bar + (int) barsIn, (int) (beatsIn - barsIn * span.numerator) };
}

Beat TempoMap::nextBeat (const Beat& beat) const noexcept AETHER_NONBLOCKING
{
    return firstBeatAtOrAfter (beat.tick + 1);
}

//==============================================================================
std::string formatBarBeat (const BarBeat& position)
{
    const auto sixteenth = (int) (position.tickInBeat / (ticksPerQuarter / 4));
    char text[64];
    std::snprintf (text, sizeof (text), "%d.%d.%d", position.bar + 1, position.beat + 1, sixteenth + 1);
    return text;
}

std::string formatTime (double seconds)
{
    if (! std::isfinite (seconds))
        seconds = 0.0;

    const auto negative = seconds < 0.0;
    const auto totalMs = (long long) std::llround (std::abs (seconds) * 1000.0);
    const auto minutes = totalMs / 60000;
    const auto secs = (totalMs / 1000) % 60;
    const auto ms = totalMs % 1000;

    char text[64];
    std::snprintf (text, sizeof (text), "%s%lld:%02lld.%03lld", negative ? "-" : "", minutes, secs, ms);
    return text;
}

} // namespace aether
