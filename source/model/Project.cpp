// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "Project.h"

#include "MidiClips.h"
#include "dsp/EffectTypes.h"

namespace aether
{

Project::Project (const juce::String& name)
    : state (ids::Project)
{
    state.setProperty (ids::name, name, nullptr);
    ensureStructure();
    attachListener();
}

Project::Project (juce::ValueTree loadedState, bool)
    : state (std::move (loadedState))
{
    ensureStructure();
    attachListener();
}

std::unique_ptr<Project> Project::fromState (juce::ValueTree loadedState)
{
    if (! loadedState.hasType (ids::Project))
        return nullptr;

    return std::unique_ptr<Project> (new Project (std::move (loadedState), true));
}

Project::~Project()
{
    state.removeListener (this);
}

void Project::ensureStructure()
{
    // Fill in anything a (possibly hand-edited or older) file left out.
    if (! state.hasProperty (ids::name))
        state.setProperty (ids::name, "Untitled", nullptr);

    if (! state.getChildWithName (ids::Tracks).isValid())
        state.appendChild (juce::ValueTree (ids::Tracks), nullptr);

    // Timeline defaults: 120 BPM, 4/4, a 4-bar loop (switched off). Projects saved before
    // Milestone 2 don't have these, and get them here.
    auto tempo = state.getChildWithName (ids::Tempo);
    if (! tempo.isValid())
    {
        tempo = juce::ValueTree (ids::Tempo);
        state.appendChild (tempo, nullptr);
    }

    if (tempo.getNumChildren() == 0)
    {
        juce::ValueTree point (ids::TempoPoint);
        point.setProperty (ids::tick, (juce::int64) 0, nullptr);
        point.setProperty (ids::bpm, 120.0, nullptr);
        point.setProperty (ids::curve, "jump", nullptr);
        tempo.appendChild (point, nullptr);
    }

    auto signatures = state.getChildWithName (ids::TimeSignatures);
    if (! signatures.isValid())
    {
        signatures = juce::ValueTree (ids::TimeSignatures);
        state.appendChild (signatures, nullptr);
    }

    if (signatures.getNumChildren() == 0)
    {
        juce::ValueTree sig (ids::TimeSignature);
        sig.setProperty (ids::bar, 0, nullptr);
        sig.setProperty (ids::numerator, 4, nullptr);
        sig.setProperty (ids::denominator, 4, nullptr);
        signatures.appendChild (sig, nullptr);
    }

    // The master channel (from Milestone 4).
    if (! state.getChildWithName (ids::Master).isValid())
    {
        juce::ValueTree master (ids::Master);
        master.setProperty (ids::volume, 0.0, nullptr);
        state.appendChild (master, nullptr);
    }

    if (! state.hasProperty (ids::loopStart))   state.setProperty (ids::loopStart, (juce::int64) 0, nullptr);
    if (! state.hasProperty (ids::loopEnd))     state.setProperty (ids::loopEnd, (juce::int64) (16 * 960000), nullptr);
    if (! state.hasProperty (ids::loopEnabled)) state.setProperty (ids::loopEnabled, false, nullptr);

    auto tracks = getTracksNode();

    for (auto track : tracks)
    {
        if (track.hasType (ids::Track) && track[ids::id].toString().isEmpty())
            track.setProperty (ids::id, juce::Uuid().toString(), nullptr);

        // Every track has a (possibly empty) list of clips.
        if (track.hasType (ids::Track) && ! track.getChildWithName (ids::Clips).isValid())
            track.appendChild (juce::ValueTree (ids::Clips), nullptr);

        // Every MIDI track has an instrument (projects from before Milestone 6 get the default).
        if (track.hasType (ids::Track) && track[ids::kind].toString() == "midi" && ! track.getChildWithName (ids::Instrument).isValid())
            track.appendChild (midi::makeInstrument (dsp::defaultInstrument, dsp::defaultInstrumentPreset), nullptr);
    }
}

void Project::attachListener()
{
    state.addListener (this);
    changeCount = 0;
    savedChangeCount = 0;
}

int Project::getNumTracks() const
{
    return getTracksNode().getNumChildren();
}

Track Project::getTrack (int index) const
{
    return Track (getTracksNode().getChild (index));
}

Track Project::findTrack (const juce::String& trackId) const
{
    return Track (getTracksNode().getChildWithProperty (ids::id, trackId));
}

int Project::indexOfTrack (const juce::String& trackId) const
{
    auto tracks = getTracksNode();
    return tracks.indexOf (tracks.getChildWithProperty (ids::id, trackId));
}

} // namespace aether
