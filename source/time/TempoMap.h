// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "engine/rt/RealtimeAnnotations.h"

namespace aether
{

/** A position in musical time. Timeline positions are whole numbers of "ticks":
    960,000 per quarter note, which divides exactly into every note length Aether offers
    (including triplets and quintuplets) and is fine enough for sample-accurate editing. */
using Tick = std::int64_t;
inline constexpr Tick ticksPerQuarter = 960000;

enum class TempoCurve
{
    jump,   // the tempo changes instantly at this point and stays until the next point
    ramp    // the tempo changes gradually from this point to the next one
};

struct TempoPoint
{
    Tick tick = 0;
    double bpm = 120.0;                     // quarter notes per minute
    TempoCurve curve = TempoCurve::jump;

    bool operator== (const TempoPoint&) const = default;
};

struct TimeSignature
{
    int bar = 0;            // the bar (counting from 0) where this signature starts
    int numerator = 4;      // beats per bar
    int denominator = 4;    // the note value of one beat: 4 = quarter note, 8 = eighth note

    bool operator== (const TimeSignature&) const = default;
};

/** A position as bars and beats (all counting from 0; add 1 for display). */
struct BarBeat
{
    int bar = 0;
    int beat = 0;
    Tick tickInBeat = 0;

    bool operator== (const BarBeat&) const = default;
};

/** One beat of the metronome grid. */
struct Beat
{
    Tick tick = 0;
    int bar = 0;
    int beatInBar = 0;      // 0 = the first beat of the bar ("downbeat")
};

/** The project's tempo and time signatures, and conversions between musical time (ticks,
    bars and beats) and real time (seconds).

    Built on the UI thread; all const methods are real-time safe (no allocation or locking),
    so the audio thread can use a TempoMap it has been handed.
*/
class TempoMap
{
public:
    static constexpr double minBpm = 20.0;
    static constexpr double maxBpm = 999.0;

    /** 120 BPM, 4/4. */
    TempoMap();

    /** Points and signatures are sorted and checked: a point at tick 0 and a signature at bar 0
        are added if missing, tempos are kept within minBpm..maxBpm, invalid signatures become 4/4,
        and duplicates (same tick or bar) keep the last one. */
    TempoMap (std::vector<TempoPoint> tempoPoints, std::vector<TimeSignature> signatures);

    const std::vector<TempoPoint>& getTempoPoints() const noexcept       { return points; }
    const std::vector<TimeSignature>& getTimeSignatures() const noexcept { return signatures; }

    //==============================================================================
    // Tempo

    /** Seconds from the start of the timeline (tick 0) to `tick`. */
    double ticksToSeconds (Tick tick) const noexcept AETHER_NONBLOCKING;

    /** The tick at `seconds` from the start, rounded to the nearest tick. */
    Tick secondsToTicks (double seconds) const noexcept AETHER_NONBLOCKING;

    /** The tempo (BPM) at a position, including part-way through a ramp. */
    double tempoAt (Tick tick) const noexcept AETHER_NONBLOCKING;

    /** Index of the tempo point in effect at `tick`. */
    int tempoPointIndexAt (Tick tick) const noexcept AETHER_NONBLOCKING;

    //==============================================================================
    // Bars and beats

    const TimeSignature& signatureAt (Tick tick) const noexcept AETHER_NONBLOCKING;
    Tick beatLengthAt (Tick tick) const noexcept AETHER_NONBLOCKING;   // ticks per beat
    Tick barLengthAt (Tick tick) const noexcept AETHER_NONBLOCKING;    // ticks per bar

    /** The tick where a bar starts (bar 0 starts at tick 0). */
    Tick barToTicks (int bar) const noexcept AETHER_NONBLOCKING;
    Tick barBeatToTicks (const BarBeat& position) const noexcept AETHER_NONBLOCKING;
    BarBeat ticksToBarBeat (Tick tick) const noexcept AETHER_NONBLOCKING;

    /** The first beat at or after `tick`, and the beat after a given beat. */
    Beat firstBeatAtOrAfter (Tick tick) const noexcept AETHER_NONBLOCKING;
    Beat nextBeat (const Beat& beat) const noexcept AETHER_NONBLOCKING;

private:
    struct SignatureSpan
    {
        Tick startTick = 0;
        int bar = 0;
        int numerator = 4;
        Tick beatTicks = ticksPerQuarter;
        Tick barTicks = 4 * ticksPerQuarter;
    };

    void prepare();
    int segmentIndexForTick (Tick tick) const noexcept AETHER_NONBLOCKING;
    int segmentIndexForSeconds (double seconds) const noexcept AETHER_NONBLOCKING;
    int spanIndexForTick (Tick tick) const noexcept AETHER_NONBLOCKING;
    int spanIndexForBar (int bar) const noexcept AETHER_NONBLOCKING;

    std::vector<TempoPoint> points;
    std::vector<double> pointSeconds;      // time at each tempo point
    std::vector<TimeSignature> signatures;
    std::vector<SignatureSpan> spans;
};

/** "3.2.1": bar . beat . sixteenth note within the beat, counting from 1 (as in the transport). */
std::string formatBarBeat (const BarBeat& position);

/** "1:05.250": minutes, seconds and milliseconds. */
std::string formatTime (double seconds);

} // namespace aether
