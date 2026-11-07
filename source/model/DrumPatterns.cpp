// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "DrumPatterns.h"

#include <cmath>

#include "ClipEdits.h"
#include "EditCommands.h"
#include "NoteEdits.h"
#include "dsp/DrumKit.h"

namespace aether
{

namespace drumpattern
{

bool isDrumTrack (const Project& project, const juce::String& trackId)
{
    return midi::instrumentOf (project, trackId)[ids::type].toString() == dsp::drumkit::typeId;
}

bool isDrumClip (const Project& project, const Clip& clip)
{
    return clip.isValid() && clip.isMidi() && isDrumTrack (project, clip.getTrackState()[ids::id].toString());
}

Tick stepLengthOf (const Clip& clip)
{
    const auto length = (Tick) (juce::int64) clip.getState().getProperty (ids::stepLength, (juce::int64) defaultStepLength);
    return std::max<Tick> (ticksPerQuarter / 16, length);
}

double swingOf (const Clip& clip)
{
    return std::clamp ((double) clip.getState().getProperty (ids::swing, 0.0), 0.0, 1.0);
}

int numSteps (const Clip& clip)
{
    const auto pattern = clip.isLooping() ? clip.getPatternLength() : clip.getLengthTicks();
    return (int) std::clamp<Tick> (pattern / stepLengthOf (clip), 1, maxSteps);
}

Tick stepTime (int step, Tick stepLength, double swing) noexcept
{
    const auto swingDelay = (step % 2 == 1) ? (Tick) std::llround (std::clamp (swing, 0.0, 1.0) * (double) stepLength / 3.0) : 0;
    return step * stepLength + swingDelay;
}

namespace
{
    juce::ValueTree findStepAt (const Clip& clip, int pad, int step, Tick stepLength, double swing)
    {
        const auto pitch = dsp::drums::noteForPad (pad);
        const auto at = stepTime (step, stepLength, swing);
        const auto start = clip.isLooping() ? Tick (0) : clip.getContentStart();

        for (const auto& note : clip.getState())
            if (note.hasType (ids::Note) && (int) note[ids::pitch] == pitch
                && std::abs ((Tick) (juce::int64) note[ids::tick] - start - at) * 2 < stepLength)
                return note;

        return {};
    }
}

juce::ValueTree findStep (const Clip& clip, int pad, int step)
{
    return findStepAt (clip, pad, step, stepLengthOf (clip), swingOf (clip));
}

int stepVelocity (const Clip& clip, int pad, int step)
{
    const auto note = findStep (clip, pad, step);
    return note.isValid() ? (int) note[ids::velocity] : 0;
}

} // namespace drumpattern

//==============================================================================
Track edit::addDrumTrack (Project& project, int insertIndex, const juce::String& kitName)
{
    auto& undo = project.getUndoManager();
    undo.beginNewTransaction ("Add drum track");

    auto track = edit::addTrack (project, TrackKind::midi, insertIndex, false, "Drums");
    auto state = track.getState();

    if (auto old = state.getChildWithName (ids::Instrument); old.isValid())
        state.removeChild (old, &undo);

    const auto& kits = dsp::drums::kits();
    const auto kit = kitName.isNotEmpty() ? kitName : (kits.empty() ? juce::String() : juce::String (kits.front().name));
    state.appendChild (midi::makeInstrument (dsp::drumkit::typeId, kit), &undo);
    return track;
}

juce::String edit::addPattern (Project& project, const juce::String& trackId, Tick start, Tick length, const juce::String& name)
{
    const auto track = project.findTrack (trackId);

    if (track.getKind() != TrackKind::midi)
        return {};

    project.getUndoManager().beginNewTransaction ("Add pattern");
    const auto id = edit::addMidiClip (project, trackId, name.isNotEmpty() ? name : track.getName(), start, length, {}, false);
    auto clip = clips::findClip (project, id);

    if (clip.isValid())
    {
        clip.getState().setProperty (ids::loop, true, &project.getUndoManager());
        clip.getState().setProperty (ids::patternLength, (juce::int64) clip.getLengthTicks(), &project.getUndoManager());
    }

    return id;
}

bool edit::setStep (Project& project, const juce::String& clipId, int pad, int step, int velocity, bool newUndoStep)
{
    const auto clip = clips::findClip (project, clipId);

    if (! clip.isMidi() || ! juce::isPositiveAndBelow (pad, dsp::drums::numPads) || ! juce::isPositiveAndBelow (step, drumpattern::numSteps (clip)))
        return false;

    velocity = juce::jlimit (0, 127, velocity);
    const auto existing = drumpattern::findStep (clip, pad, step);

    if (velocity == 0)
    {
        if (! existing.isValid())
            return false;

        if (newUndoStep)
            project.getUndoManager().beginNewTransaction ("Clear step");

        edit::deleteNotes (project, { existing }, false);
        return true;
    }

    if (existing.isValid())
    {
        if ((int) existing[ids::velocity] == velocity)
            return false;

        if (newUndoStep)
            project.getUndoManager().beginNewTransaction ("Change step velocity");

        edit::setNoteVelocity (project, { existing }, velocity, false);
        return true;
    }

    if (newUndoStep)
        project.getUndoManager().beginNewTransaction ("Add step");

    const auto length = drumpattern::stepLengthOf (clip);
    const auto start = (clip.isLooping() ? Tick (0) : clip.getContentStart())
                     + drumpattern::stepTime (step, length, drumpattern::swingOf (clip));
    edit::addNote (project, clipId, { dsp::drums::noteForPad (pad), velocity, start, length }, false);
    return true;
}

bool edit::setStepLength (Project& project, const juce::String& clipId, Tick stepLength)
{
    auto clip = clips::findClip (project, clipId);
    stepLength = std::max<Tick> (ticksPerQuarter / 16, stepLength);

    if (! clip.isMidi() || drumpattern::stepLengthOf (clip) == stepLength)
        return false;

    project.getUndoManager().beginNewTransaction ("Change step size");
    clip.getState().setProperty (ids::stepLength, (juce::int64) stepLength, &project.getUndoManager());
    return true;
}

bool edit::setSwing (Project& project, const juce::String& clipId, double swing, bool newUndoStep)
{
    auto clip = clips::findClip (project, clipId);
    swing = std::round (std::clamp (swing, 0.0, 1.0) * 100.0) / 100.0;

    if (! clip.isMidi() || drumpattern::swingOf (clip) == swing)
        return false;

    auto& undo = project.getUndoManager();
    if (newUndoStep)
        undo.beginNewTransaction ("Change swing");

    // Move the notes on the odd steps from where the old swing put them to where the new one does.
    const auto length = drumpattern::stepLengthOf (clip);
    const auto oldSwing = drumpattern::swingOf (clip);
    const auto origin = clip.isLooping() ? Tick (0) : clip.getContentStart();
    const auto steps = drumpattern::numSteps (clip);

    for (auto note : clip.getState())
    {
        if (! note.hasType (ids::Note))
            continue;

        const auto at = (Tick) (juce::int64) note[ids::tick] - origin;

        for (int step = 1; step < steps; step += 2)
            if (std::abs (at - drumpattern::stepTime (step, length, oldSwing)) * 2 < length)
            {
                note.setProperty (ids::tick, (juce::int64) (origin + drumpattern::stepTime (step, length, swing)), &undo);
                break;
            }
    }

    clip.getState().setProperty (ids::swing, swing, &undo);
    return true;
}

} // namespace aether
