// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "Project.h"

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

    auto tracks = getTracksNode();

    for (auto track : tracks)
        if (track.hasType (ids::Track) && track[ids::id].toString().isEmpty())
            track.setProperty (ids::id, juce::Uuid().toString(), nullptr);
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
