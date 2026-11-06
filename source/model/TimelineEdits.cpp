// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "TimelineEdits.h"

#include <cmath>
#include <limits>

namespace aether
{

namespace
{
    juce::ValueTree tempoNode (const Project& p)       { return p.getState().getChildWithName (ids::Tempo); }
    juce::ValueTree signatureNode (const Project& p)   { return p.getState().getChildWithName (ids::TimeSignatures); }

    Tick tickOf (const juce::ValueTree& point)         { return (Tick) (juce::int64) point[ids::tick]; }

    juce::String curveToString (TempoCurve c)          { return c == TempoCurve::ramp ? "ramp" : "jump"; }
    TempoCurve curveFromString (const juce::String& s) { return s == "ramp" ? TempoCurve::ramp : TempoCurve::jump; }

    double cleanBpm (double bpm)
    {
        if (! std::isfinite (bpm))
            bpm = 120.0;

        // Keep tempos tidy: at most 3 decimal places.
        return std::round (juce::jlimit (TempoMap::minBpm, TempoMap::maxBpm, bpm) * 1000.0) / 1000.0;
    }

    bool isValidDenominator (int d)
    {
        return d == 1 || d == 2 || d == 4 || d == 8 || d == 16 || d == 32;
    }

    void startStep (Project& project, const juce::String& name, bool newUndoStep)
    {
        if (newUndoStep)
            project.getUndoManager().beginNewTransaction (name);
    }
}

//==============================================================================
TempoMap timeline::buildTempoMap (const Project& project)
{
    std::vector<TempoPoint> points;
    for (const auto& p : tempoNode (project))
        points.push_back ({ tickOf (p), (double) p[ids::bpm], curveFromString (p[ids::curve].toString()) });

    std::vector<TimeSignature> signatures;
    for (const auto& s : signatureNode (project))
        signatures.push_back ({ (int) s[ids::bar], (int) s[ids::numerator], (int) s[ids::denominator] });

    return TempoMap (std::move (points), std::move (signatures));
}

LoopRange timeline::getLoop (const Project& project)
{
    const auto& state = project.getState();
    return { (Tick) (juce::int64) state[ids::loopStart], (Tick) (juce::int64) state[ids::loopEnd], (bool) state[ids::loopEnabled] };
}

int timeline::getNumTempoChanges (const Project& project)
{
    return tempoNode (project).getNumChildren();
}

//==============================================================================
bool edit::setTempo (Project& project, int pointIndex, double bpm, bool newUndoStep)
{
    auto point = tempoNode (project).getChild (pointIndex);
    bpm = cleanBpm (bpm);

    if (! point.isValid() || (double) point[ids::bpm] == bpm)
        return false;

    startStep (project, "Change tempo", newUndoStep);
    point.setProperty (ids::bpm, bpm, &project.getUndoManager());
    return true;
}

int edit::addTempoChange (Project& project, Tick tick, double bpm, TempoCurve curve)
{
    auto tempo = tempoNode (project);
    tick = std::max<Tick> (0, tick);
    bpm = cleanBpm (bpm);
    auto& undo = project.getUndoManager();

    // Already a change exactly there? Just set its tempo.
    for (int i = 0; i < tempo.getNumChildren(); ++i)
        if (tickOf (tempo.getChild (i)) == tick)
        {
            setTempo (project, i, bpm);
            return i;
        }

    int index = 0;
    while (index < tempo.getNumChildren() && tickOf (tempo.getChild (index)) < tick)
        ++index;

    undo.beginNewTransaction ("Add tempo change");
    juce::ValueTree point (ids::TempoPoint);
    point.setProperty (ids::tick, (juce::int64) tick, nullptr);
    point.setProperty (ids::bpm, bpm, nullptr);
    point.setProperty (ids::curve, curveToString (curve), nullptr);
    tempo.addChild (point, index, &undo);
    return index;
}

bool edit::moveTempoChange (Project& project, int pointIndex, Tick newTick, bool newUndoStep)
{
    auto tempo = tempoNode (project);
    auto point = tempo.getChild (pointIndex);

    if (pointIndex <= 0 || ! point.isValid())
        return false;   // the first tempo is always at the start

    // Stay strictly between the neighbours.
    const auto earliest = tickOf (tempo.getChild (pointIndex - 1)) + 1;
    const auto next = tempo.getChild (pointIndex + 1);
    const auto latest = next.isValid() ? tickOf (next) - 1 : std::numeric_limits<Tick>::max() / 4;
    newTick = juce::jlimit (earliest, latest, newTick);

    if (newTick == tickOf (point))
        return false;

    startStep (project, "Move tempo change", newUndoStep);
    point.setProperty (ids::tick, (juce::int64) newTick, &project.getUndoManager());
    return true;
}

bool edit::setTempoCurve (Project& project, int pointIndex, TempoCurve curve)
{
    auto point = tempoNode (project).getChild (pointIndex);

    if (! point.isValid() || curveFromString (point[ids::curve].toString()) == curve)
        return false;

    project.getUndoManager().beginNewTransaction (curve == TempoCurve::ramp ? "Make tempo ramp" : "Make tempo jump");
    point.setProperty (ids::curve, curveToString (curve), &project.getUndoManager());
    return true;
}

bool edit::deleteTempoChange (Project& project, int pointIndex)
{
    auto tempo = tempoNode (project);

    if (pointIndex <= 0 || pointIndex >= tempo.getNumChildren())
        return false;

    project.getUndoManager().beginNewTransaction ("Delete tempo change");
    tempo.removeChild (pointIndex, &project.getUndoManager());
    return true;
}

//==============================================================================
bool edit::setTimeSignature (Project& project, int bar, int numerator, int denominator)
{
    if (bar < 0 || numerator < 1 || numerator > 32 || ! isValidDenominator (denominator))
        return false;

    auto signatures = signatureNode (project);
    auto& undo = project.getUndoManager();

    int index = 0;
    while (index < signatures.getNumChildren() && (int) signatures.getChild (index)[ids::bar] < bar)
        ++index;

    auto existing = signatures.getChild (index);

    if (existing.isValid() && (int) existing[ids::bar] == bar)
    {
        if ((int) existing[ids::numerator] == numerator && (int) existing[ids::denominator] == denominator)
            return false;

        undo.beginNewTransaction ("Change time signature");
        existing.setProperty (ids::numerator, numerator, &undo);
        existing.setProperty (ids::denominator, denominator, &undo);
        return true;
    }

    undo.beginNewTransaction ("Add time signature change");
    juce::ValueTree sig (ids::TimeSignature);
    sig.setProperty (ids::bar, bar, nullptr);
    sig.setProperty (ids::numerator, numerator, nullptr);
    sig.setProperty (ids::denominator, denominator, nullptr);
    signatures.addChild (sig, index, &undo);
    return true;
}

bool edit::deleteTimeSignature (Project& project, int bar)
{
    if (bar <= 0)
        return false;   // the song always has a starting signature

    auto signatures = signatureNode (project);
    auto sig = signatures.getChildWithProperty (ids::bar, bar);

    if (! sig.isValid())
        return false;

    project.getUndoManager().beginNewTransaction ("Delete time signature change");
    signatures.removeChild (sig, &project.getUndoManager());
    return true;
}

//==============================================================================
bool edit::setLoopRange (Project& project, Tick start, Tick end, bool newUndoStep)
{
    start = std::max<Tick> (0, start);

    if (end <= start)
        return false;

    const auto current = timeline::getLoop (project);

    if (current.start == start && current.end == end)
        return false;

    startStep (project, "Set loop range", newUndoStep);
    auto& state = project.getState();
    state.setProperty (ids::loopStart, (juce::int64) start, &project.getUndoManager());
    state.setProperty (ids::loopEnd, (juce::int64) end, &project.getUndoManager());
    return true;
}

bool edit::setLoopEnabled (Project& project, bool enabled, bool newUndoStep)
{
    if (timeline::getLoop (project).enabled == enabled)
        return false;

    startStep (project, enabled ? "Turn loop on" : "Turn loop off", newUndoStep);
    project.getState().setProperty (ids::loopEnabled, enabled, &project.getUndoManager());
    return true;
}

} // namespace aether
