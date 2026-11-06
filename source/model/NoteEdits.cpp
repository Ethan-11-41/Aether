// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "NoteEdits.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "ClipEdits.h"
#include "Scales.h"
#include "TimelineEdits.h"

namespace aether
{

namespace
{
    Tick tickOf (const juce::ValueTree& n)     { return (Tick) (juce::int64) n[ids::tick]; }
    Tick lengthOf (const juce::ValueTree& n)   { return std::max<Tick> (1, (Tick) (juce::int64) n[ids::length]); }

    juce::ValueTree makeNote (const NoteData& n)
    {
        juce::ValueTree note (ids::Note);
        note.setProperty (ids::tick, (juce::int64) std::max<Tick> (0, n.start), nullptr);
        note.setProperty (ids::length, (juce::int64) std::max<Tick> (1, n.length), nullptr);
        note.setProperty (ids::pitch, juce::jlimit (0, 127, n.pitch), nullptr);
        note.setProperty (ids::velocity, juce::jlimit (1, 127, n.velocity), nullptr);
        return note;
    }

    /** Grows a clip so every note fits, to the end of the bar the last note ends in. */
    void growClipToFit (Project& project, juce::ValueTree clipNode)
    {
        const Clip clip (clipNode);
        Tick lastEnd = 0;

        for (const auto& n : clipNode)
            if (n.hasType (ids::Note))
                lastEnd = std::max (lastEnd, tickOf (n) + lengthOf (n));

        if (lastEnd <= clip.getContentStart() + clip.getLengthTicks())
            return;

        const auto tempo = timeline::buildTempoMap (project);
        const auto endOnTimeline = clip.getStart() - clip.getContentStart() + lastEnd;
        auto barEnd = tempo.barToTicks (tempo.ticksToBarBeat (endOnTimeline).bar);

        if (barEnd < endOnTimeline)
            barEnd = tempo.barToTicks (tempo.ticksToBarBeat (endOnTimeline).bar + 1);

        clipNode.setProperty (ids::length, (juce::int64) (barEnd - clip.getStart()), &project.getUndoManager());
    }

    void grow (Project& project, const NoteNodes& changed)
    {
        juce::Array<juce::ValueTree> clipsDone;

        for (const auto& n : changed)
        {
            const auto clip = n.getParent();
            if (clip.isValid() && ! clipsDone.contains (clip))
            {
                clipsDone.add (clip);
                growClipToFit (project, clip);
            }
        }
    }

    void begin (Project& project, bool newUndoStep, const juce::String& name)
    {
        if (newUndoStep)
            project.getUndoManager().beginNewTransaction (name);
    }

    juce::String plural (const NoteNodes& notes, const juce::String& one, const juce::String& many)
    {
        return notes.size() == 1 ? one : many;
    }
}

//==============================================================================
NoteData notes::read (const juce::ValueTree& note)
{
    NoteData n;
    n.pitch = juce::jlimit (0, 127, (int) note[ids::pitch]);
    n.velocity = juce::jlimit (1, 127, (int) note[ids::velocity]);
    n.start = tickOf (note);
    n.length = lengthOf (note);
    return n;
}

NoteNodes notes::nodesOf (const Clip& clip)
{
    NoteNodes nodes;

    for (const auto& n : clip.getState())
        if (n.hasType (ids::Note))
            nodes.push_back (n);

    std::stable_sort (nodes.begin(), nodes.end(), [] (const juce::ValueTree& a, const juce::ValueTree& b)
    {
        return tickOf (a) != tickOf (b) ? tickOf (a) < tickOf (b) : (int) a[ids::pitch] < (int) b[ids::pitch];
    });

    return nodes;
}

Clip notes::clipOf (const juce::ValueTree& note)
{
    return Clip (note.getParent());
}

Tick notes::quantizeTick (Tick tick, Tick step, double strength, double swing) noexcept
{
    if (step <= 0)
        return tick;

    const auto index = (Tick) std::llround ((double) tick / (double) step);
    auto target = index * step;

    // Swing: the off-beat steps (every second one) come later; at 1, two steps feel like a triplet.
    if (index % 2 != 0)
        target += (Tick) std::llround (std::clamp (swing, 0.0, 1.0) * (double) step / 3.0);

    return tick + (Tick) std::llround ((double) (target - tick) * std::clamp (strength, 0.0, 1.0));
}

//==============================================================================
juce::ValueTree edit::addNote (Project& project, const juce::String& clipId, const NoteData& note, bool newUndoStep)
{
    auto clip = clips::findClip (project, clipId);

    if (! clip.isValid() || ! clip.isMidi())
        return {};

    begin (project, newUndoStep, "Add note");
    auto node = makeNote (note);
    clip.getState().appendChild (node, &project.getUndoManager());
    growClipToFit (project, clip.getState());
    return node;
}

NoteNodes edit::addNotes (Project& project, const juce::String& clipId, const std::vector<NoteData>& notes,
                          const juce::String& undoName)
{
    auto clip = clips::findClip (project, clipId);
    NoteNodes added;

    if (! clip.isValid() || ! clip.isMidi() || notes.empty())
        return added;

    project.getUndoManager().beginNewTransaction (undoName);

    for (const auto& n : notes)
    {
        auto node = makeNote (n);
        clip.getState().appendChild (node, &project.getUndoManager());
        added.push_back (node);
    }

    growClipToFit (project, clip.getState());
    return added;
}

void edit::deleteNotes (Project& project, const NoteNodes& notes)
{
    if (notes.empty())
        return;

    auto& undo = project.getUndoManager();
    undo.beginNewTransaction (plural (notes, "Delete note", "Delete notes"));

    for (auto n : notes)
        if (auto parent = n.getParent(); parent.isValid())
            parent.removeChild (n, &undo);
}

void edit::moveNotes (Project& project, const NoteNodes& notes, Tick deltaTicks, int deltaPitch, bool newUndoStep)
{
    if (notes.empty() || (deltaTicks == 0 && deltaPitch == 0))
        return;

    // Keep the whole group together: limit the move so no note goes before the content's start
    // or off the keyboard.
    Tick earliest = std::numeric_limits<Tick>::max();
    int lowest = 127, highest = 0;

    for (const auto& n : notes)
    {
        earliest = std::min (earliest, tickOf (n));
        lowest = std::min (lowest, (int) n[ids::pitch]);
        highest = std::max (highest, (int) n[ids::pitch]);
    }

    deltaTicks = std::max (deltaTicks, -earliest);
    deltaPitch = juce::jlimit (-lowest, 127 - highest, deltaPitch);

    begin (project, newUndoStep, plural (notes, "Move note", "Move notes"));
    auto& undo = project.getUndoManager();

    for (auto n : notes)
    {
        if (deltaTicks != 0)  n.setProperty (ids::tick, (juce::int64) (tickOf (n) + deltaTicks), &undo);
        if (deltaPitch != 0)  n.setProperty (ids::pitch, (int) n[ids::pitch] + deltaPitch, &undo);
    }

    grow (project, notes);
}

void edit::resizeNotes (Project& project, const NoteNodes& notes, Tick deltaLength, Tick minimumLength, bool newUndoStep)
{
    if (notes.empty() || deltaLength == 0)
        return;

    begin (project, newUndoStep, plural (notes, "Change note length", "Change note lengths"));
    auto& undo = project.getUndoManager();

    for (auto n : notes)
        n.setProperty (ids::length, (juce::int64) std::max (std::max<Tick> (1, minimumLength), lengthOf (n) + deltaLength), &undo);

    grow (project, notes);
}

void edit::setNoteVelocity (Project& project, const NoteNodes& notes, int velocity, bool newUndoStep)
{
    if (notes.empty())
        return;

    begin (project, newUndoStep, "Change velocity");

    for (auto n : notes)
        n.setProperty (ids::velocity, juce::jlimit (1, 127, velocity), &project.getUndoManager());
}

void edit::changeNoteVelocity (Project& project, const NoteNodes& notes, int delta, bool newUndoStep)
{
    if (notes.empty() || delta == 0)
        return;

    begin (project, newUndoStep, "Change velocity");

    for (auto n : notes)
        n.setProperty (ids::velocity, juce::jlimit (1, 127, (int) n[ids::velocity] + delta), &project.getUndoManager());
}

void edit::quantizeNotes (Project& project, const NoteNodes& notes, Tick step, double strength, double swing, bool ends)
{
    if (notes.empty() || step <= 0)
        return;

    project.getUndoManager().beginNewTransaction ("Quantize");
    auto& undo = project.getUndoManager();

    for (auto n : notes)
    {
        const auto start = tickOf (n);
        const auto end = start + lengthOf (n);
        const auto newStart = std::max<Tick> (0, notes::quantizeTick (start, step, strength, swing));
        auto newEnd = ends ? notes::quantizeTick (end, step, strength, swing) : newStart + (end - start);

        if (newEnd <= newStart)
            newEnd = newStart + std::max<Tick> (1, ends ? step : end - start);

        n.setProperty (ids::tick, (juce::int64) newStart, &undo);
        n.setProperty (ids::length, (juce::int64) (newEnd - newStart), &undo);
    }

    grow (project, notes);
}

void edit::humanizeNotes (Project& project, const NoteNodes& notes, Tick timing, int velocity, juce::Random& random)
{
    if (notes.empty() || (timing <= 0 && velocity <= 0))
        return;

    project.getUndoManager().beginNewTransaction ("Humanize");
    auto& undo = project.getUndoManager();

    for (auto n : notes)
    {
        if (timing > 0)
        {
            const auto shift = (Tick) std::llround ((random.nextDouble() * 2.0 - 1.0) * (double) timing);
            n.setProperty (ids::tick, (juce::int64) std::max<Tick> (0, tickOf (n) + shift), &undo);
        }

        if (velocity > 0)
            n.setProperty (ids::velocity, juce::jlimit (1, 127, (int) n[ids::velocity] + random.nextInt ({ -velocity, velocity + 1 })), &undo);
    }

    grow (project, notes);
}

NoteNodes edit::duplicateNotes (Project& project, const NoteNodes& notes, Tick step)
{
    NoteNodes copies;

    if (notes.empty())
        return copies;

    Tick first = std::numeric_limits<Tick>::max(), lastEnd = 0;

    for (const auto& n : notes)
    {
        first = std::min (first, tickOf (n));
        lastEnd = std::max (lastEnd, tickOf (n) + lengthOf (n));
    }

    auto shift = lastEnd - first;
    if (step > 0)
        shift = (shift + step - 1) / step * step;

    project.getUndoManager().beginNewTransaction (plural (notes, "Duplicate note", "Duplicate notes"));

    for (const auto& n : notes)
    {
        auto parent = n.getParent();
        if (! parent.isValid())
            continue;

        auto data = notes::read (n);
        data.start += shift;
        auto copy = makeNote (data);
        parent.appendChild (copy, &project.getUndoManager());
        copies.push_back (copy);
    }

    grow (project, copies);
    return copies;
}

void edit::setScale (Project& project, int key, const juce::String& scaleId)
{
    auto& undo = project.getUndoManager();
    undo.beginNewTransaction ("Choose scale");
    project.getState().setProperty (ids::songKey, ((key % 12) + 12) % 12, &undo);
    project.getState().setProperty (ids::scale, scales::find (scaleId).id, &undo);
}

} // namespace aether
