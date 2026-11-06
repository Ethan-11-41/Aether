// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <algorithm>

#include <juce_data_structures/juce_data_structures.h>

#include "Ids.h"
#include "time/TempoMap.h"

namespace aether
{

/** The shape of a fade. */
enum class FadeCurve
{
    linear,   // a straight line
    smooth,   // an S-shape: gentle at both ends (the default)
    fast,     // changes quickly at first
    slow      // changes slowly at first
};

juce::String toString (FadeCurve curve);
FadeCurve fadeCurveFromString (const juce::String& text);

/** The gain (0..1) of a fade `fraction` of the way through (0 = silent end, 1 = full volume). */
float fadeShape (FadeCurve curve, double fraction) noexcept;

/** A lightweight, typed view of one audio clip in the project tree.

    How a clip refers to its audio file:
        - the clip starts at `start` on the timeline (a musical position, so it moves with the bar
          when the tempo changes) and lasts `duration` samples of the file (a real-time length);
        - the audio used is the part of the file from `offset`, `contentLength` samples long;
        - normally duration == contentLength. When looping, the content repeats to fill the
          duration, starting `phase` samples into it.
    All lengths are in samples of the audio file (at `sourceRate`).
*/
class Clip
{
public:
    Clip() = default;
    explicit Clip (juce::ValueTree clipState) : state (std::move (clipState)) {}

    bool isValid() const                     { return state.isValid() && state.hasType (ids::Clip); }

    juce::String getId() const               { return state[ids::id].toString(); }
    juce::String getName() const             { return state[ids::name].toString(); }
    juce::String getFile() const             { return state[ids::file].toString(); }

    Tick getStart() const                    { return (Tick) (juce::int64) state[ids::start]; }
    double getSourceRate() const             { const auto r = (double) state[ids::sourceRate]; return r > 0.0 ? r : 48000.0; }
    juce::int64 getSourceLength() const      { return (juce::int64) state[ids::sourceLength]; }
    juce::int64 getOffset() const            { return (juce::int64) state[ids::offset]; }
    juce::int64 getContentLength() const     { return std::max<juce::int64> (1, (juce::int64) state[ids::contentLength]); }
    juce::int64 getDuration() const          { return std::max<juce::int64> (1, (juce::int64) state[ids::duration]); }
    juce::int64 getPhase() const             { return (juce::int64) state[ids::phase]; }
    bool isLooping() const                   { return (bool) state[ids::loop]; }
    double getGainDb() const                 { return (double) state[ids::gain]; }
    juce::int64 getFadeIn() const            { return (juce::int64) state[ids::fadeIn]; }
    juce::int64 getFadeOut() const           { return (juce::int64) state[ids::fadeOut]; }
    FadeCurve getFadeInCurve() const         { return fadeCurveFromString (state[ids::fadeInCurve].toString()); }
    FadeCurve getFadeOutCurve() const        { return fadeCurveFromString (state[ids::fadeOutCurve].toString()); }

    double getDurationSeconds() const        { return (double) getDuration() / getSourceRate(); }
    double getStartSeconds (const TempoMap& tempo) const   { return tempo.ticksToSeconds (getStart()); }

    /** Where the clip ends on the timeline. */
    Tick getEnd (const TempoMap& tempo) const
    {
        return tempo.secondsToTicks (getStartSeconds (tempo) + getDurationSeconds());
    }

    /** The track this clip is on (its "Track" node). */
    juce::ValueTree getTrackState() const    { return state.getParent().getParent(); }

    juce::ValueTree getState() const         { return state; }

    bool operator== (const Clip& other) const   { return state == other.state; }

private:
    juce::ValueTree state;
};

} // namespace aether
