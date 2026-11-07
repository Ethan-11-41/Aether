// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "MidiClips.h"

#include <algorithm>

#include "ClipEdits.h"
#include "dsp/DrumKit.h"
#include "dsp/EffectTypes.h"

namespace aether
{

namespace midi
{

std::vector<NoteData> notesOf (const Clip& clip)
{
    std::vector<NoteData> notes;

    for (const auto& node : clip.getState())
    {
        if (! node.hasType (ids::Note))
            continue;

        NoteData n;
        n.pitch = juce::jlimit (0, 127, (int) node[ids::pitch]);
        n.velocity = juce::jlimit (1, 127, (int) node[ids::velocity]);
        n.start = (Tick) (juce::int64) node[ids::tick];
        n.length = std::max<Tick> (1, (Tick) (juce::int64) node[ids::length]);
        notes.push_back (n);
    }

    std::sort (notes.begin(), notes.end(), [] (const NoteData& a, const NoteData& b)
    {
        return a.start != b.start ? a.start < b.start : a.pitch < b.pitch;
    });

    return notes;
}

std::vector<NoteData> audibleNotes (const Clip& clip)
{
    std::vector<NoteData> result;
    const auto from = clip.getContentStart();
    const auto to = from + clip.getLengthTicks();

    for (auto n : notesOf (clip))
    {
        // A note belongs to where it starts: one starting before the clip's range isn't played.
        if (n.start < from || n.start >= to)
            continue;

        n.length = std::min (n.length, to - n.start);
        n.start -= from;
        result.push_back (n);
    }

    return result;
}

juce::ValueTree instrumentOf (const Project& project, const juce::String& trackId)
{
    const auto track = project.findTrack (trackId);
    return track.getKind() == TrackKind::midi ? track.getState().getChildWithName (ids::Instrument) : juce::ValueTree();
}

juce::ValueTree makeInstrument (const juce::String& typeId, const juce::String& presetName)
{
    juce::ValueTree instrument (ids::Instrument);
    instrument.setProperty (ids::id, juce::Uuid().toString(), nullptr);
    instrument.setProperty (ids::type, typeId, nullptr);
    instrument.setProperty (ids::bypass, false, nullptr);

    if (const auto* type = dsp::findEffectType (typeId))
    {
        for (const auto& parameter : type->parameters)
            instrument.setProperty (parameter.id, parameter.defaultValue, nullptr);

        for (const auto& preset : type->presets)
        {
            if (preset.name != presetName)
                continue;

            for (size_t i = 0; i < type->parameters.size() && i < preset.values.size(); ++i)
                instrument.setProperty (type->parameters[i].id, preset.values[i], nullptr);

            instrument.setProperty (ids::preset, presetName, nullptr);
        }
    }

    // A drum kit's pads also get the kit's sounds.
    if (typeId == dsp::drumkit::typeId)
    {
        const auto* kit = dsp::drums::findKit (presetName);
        if (kit == nullptr && ! dsp::drums::kits().empty())
            kit = &dsp::drums::kits().front();

        if (kit != nullptr)
            for (int pad = 0; pad < dsp::drums::numPads; ++pad)
                instrument.setProperty (dsp::drumkit::soundProperty (pad), dsp::drumkit::builtInReference (kit->pads[(size_t) pad].sound), nullptr);
    }

    return instrument;
}

juce::String noteName (int pitch, bool flats)
{
    static const char* const sharpNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    static const char* const flatNames[] = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };
    pitch = juce::jlimit (0, 127, pitch);
    return juce::String ((flats ? flatNames : sharpNames)[pitch % 12]) + juce::String (pitch / 12 - 1);
}

void NoteCollector::noteOn (std::int64_t time, int pitch, int velocity)
{
    if (! juce::isPositiveAndBelow (pitch, 128))
        return;

    if (velocity <= 0)
    {
        noteOff (time, pitch);
        return;
    }

    end (time, pitch);   // the same key again: the previous one stops here
    held[(size_t) pitch] = { time, juce::jlimit (1, 127, velocity), false };
}

void NoteCollector::noteOff (std::int64_t time, int pitch)
{
    if (! juce::isPositiveAndBelow (pitch, 128) || held[(size_t) pitch].start < 0)
        return;

    if (pedal)
        held[(size_t) pitch].released = true;   // keeps sounding until the pedal comes up
    else
        end (time, pitch);
}

void NoteCollector::sustain (std::int64_t time, bool down)
{
    if (pedal && ! down)
        for (int p = 0; p < 128; ++p)
            if (held[(size_t) p].start >= 0 && held[(size_t) p].released)
                end (time, p);

    pedal = down;
}

void NoteCollector::end (std::int64_t time, int pitch)
{
    auto& h = held[(size_t) pitch];

    if (h.start < 0)
        return;

    notes.push_back ({ pitch, h.velocity, (Tick) h.start, (Tick) std::max<std::int64_t> (1, time - h.start) });
    h = {};
}

std::vector<NoteData> NoteCollector::finish (std::int64_t time)
{
    for (int p = 0; p < 128; ++p)
        end (time, p);

    pedal = false;
    auto result = std::move (notes);
    notes.clear();

    std::sort (result.begin(), result.end(), [] (const NoteData& a, const NoteData& b)
    {
        return a.start != b.start ? a.start < b.start : a.pitch < b.pitch;
    });

    return result;
}

void NoteCollector::add (std::int64_t time, const juce::MidiMessage& message)
{
    if (message.isNoteOn())
        noteOn (time, message.getNoteNumber(), message.getVelocity());
    else if (message.isNoteOff())
        noteOff (time, message.getNoteNumber());
    else if (message.isSustainPedalOn())
        sustain (time, true);
    else if (message.isSustainPedalOff())
        sustain (time, false);
    else if (message.isAllNotesOff() || message.isAllSoundOff())
        for (int p = 0; p < 128; ++p)
            end (time, p);
}

} // namespace midi

//==============================================================================
juce::String edit::addMidiClip (Project& project, const juce::String& trackId, const juce::String& name,
                                Tick start, Tick length, const std::vector<NoteData>& notes, bool newUndoStep)
{
    const auto track = project.findTrack (trackId);

    if (track.getKind() != TrackKind::midi)
        return {};

    if (newUndoStep)
        project.getUndoManager().beginNewTransaction ("Add MIDI clip");

    const auto id = juce::Uuid().toString();
    juce::ValueTree clip (ids::Clip);
    clip.setProperty (ids::id, id, nullptr);
    clip.setProperty (ids::kind, "midi", nullptr);
    clip.setProperty (ids::name, name, nullptr);
    clip.setProperty (ids::start, (juce::int64) std::max<Tick> (0, start), nullptr);
    clip.setProperty (ids::length, (juce::int64) std::max (midi::minimumClipLength, length), nullptr);
    clip.setProperty (ids::contentStart, (juce::int64) 0, nullptr);

    for (const auto& n : notes)
    {
        juce::ValueTree note (ids::Note);
        note.setProperty (ids::tick, (juce::int64) n.start, nullptr);
        note.setProperty (ids::length, (juce::int64) std::max<Tick> (1, n.length), nullptr);
        note.setProperty (ids::pitch, juce::jlimit (0, 127, n.pitch), nullptr);
        note.setProperty (ids::velocity, juce::jlimit (1, 127, n.velocity), nullptr);
        clip.appendChild (note, nullptr);
    }

    clips::clipsNodeOf (track.getState()).appendChild (clip, &project.getUndoManager());
    return id;
}

bool edit::setInstrumentType (Project& project, const juce::String& trackId, const juce::String& typeId,
                              const juce::String& presetName)
{
    auto track = project.findTrack (trackId);
    const auto* type = dsp::findEffectType (typeId);

    if (track.getKind() != TrackKind::midi || type == nullptr || ! type->isInstrument)
        return false;

    auto& undo = project.getUndoManager();
    undo.beginNewTransaction ("Use " + type->name);

    auto state = track.getState();
    if (auto old = state.getChildWithName (ids::Instrument); old.isValid())
        state.removeChild (old, &undo);

    const auto preset = presetName.isNotEmpty() || type->presets.empty() ? presetName : type->presets.front().name;
    state.appendChild (midi::makeInstrument (typeId, preset), &undo);
    return true;
}

bool edit::setPadSound (Project& project, const juce::String& trackId, int pad, const juce::String& reference)
{
    auto instrument = midi::instrumentOf (project, trackId);

    if (instrument[ids::type].toString() != dsp::drumkit::typeId || ! juce::isPositiveAndBelow (pad, dsp::drums::numPads))
        return false;

    auto& undo = project.getUndoManager();
    undo.beginNewTransaction ("Pad " + juce::String (pad + 1) + ": " + dsp::drumkit::describeSound (reference));
    instrument.setProperty (dsp::drumkit::soundProperty (pad), reference, &undo);
    return true;
}

bool edit::setInstrumentPreset (Project& project, const juce::String& trackId, const juce::String& presetName)
{
    auto instrument = midi::instrumentOf (project, trackId);
    const auto* type = instrument.isValid() ? dsp::findEffectType (instrument[ids::type].toString()) : nullptr;

    if (type == nullptr)
        return false;

    for (const auto& preset : type->presets)
    {
        if (preset.name != presetName)
            continue;

        auto& undo = project.getUndoManager();
        undo.beginNewTransaction ("Choose \"" + presetName + "\"");

        for (size_t i = 0; i < type->parameters.size() && i < preset.values.size(); ++i)
            instrument.setProperty (type->parameters[i].id, preset.values[i], &undo);

        // A drum kit: its sounds too.
        if (const auto* kit = type->id == dsp::drumkit::typeId ? dsp::drums::findKit (presetName) : nullptr)
            for (int pad = 0; pad < dsp::drums::numPads; ++pad)
                instrument.setProperty (dsp::drumkit::soundProperty (pad), dsp::drumkit::builtInReference (kit->pads[(size_t) pad].sound), &undo);

        instrument.setProperty (ids::preset, presetName, &undo);
        return true;
    }

    return false;
}

} // namespace aether
