// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "EditCommands.h"

#include "MidiClips.h"
#include "Mixer.h"
#include "TrackColours.h"
#include "dsp/EffectTypes.h"

namespace aether::edit
{

namespace
{
    juce::String baseName (TrackKind kind)
    {
        switch (kind)
        {
            case TrackKind::midi:  return "MIDI";
            case TrackKind::bus:   return "Bus";
            case TrackKind::audio: break;
        }

        return "Audio";
    }
}

juce::String nextTrackName (const Project& project, TrackKind kind)
{
    // Find the highest number already used by names like "Audio 3", then add one.
    const auto prefix = baseName (kind) + " ";
    int highest = 0;

    for (int i = 0; i < project.getNumTracks(); ++i)
    {
        const auto name = project.getTrack (i).getName();

        if (name.startsWith (prefix) && name.substring (prefix.length()).containsOnly ("0123456789"))
            highest = juce::jmax (highest, name.substring (prefix.length()).getIntValue());
    }

    return prefix + juce::String (highest + 1);
}

juce::Colour nextTrackColour (const Project& project)
{
    const auto& colours = trackColours::palette();

    for (const auto& colour : colours)
    {
        bool used = false;

        for (int i = 0; i < project.getNumTracks() && ! used; ++i)
            used = project.getTrack (i).getColour() == colour;

        if (! used)
            return colour;
    }

    return colours[(size_t) project.getNumTracks() % colours.size()];
}

Track addTrack (Project& project, TrackKind kind, int insertIndex, bool newUndoStep, const juce::String& name)
{
    auto& undo = project.getUndoManager();

    if (newUndoStep)
        undo.beginNewTransaction (kind == TrackKind::midi ? "Add MIDI track"
                                  : kind == TrackKind::bus ? "Add bus" : "Add audio track");

    juce::ValueTree track (ids::Track);
    track.setProperty (ids::id, juce::Uuid().toString(), nullptr);
    track.setProperty (ids::kind, toString (kind), nullptr);
    track.setProperty (ids::name, name.trim().isNotEmpty() ? name.trim() : nextTrackName (project, kind), nullptr);
    track.setProperty (ids::colour, Track::colourToString (nextTrackColour (project)), nullptr);
    track.appendChild (juce::ValueTree (ids::Clips), nullptr);

    if (kind == TrackKind::midi)
        track.appendChild (midi::makeInstrument (dsp::defaultInstrument, dsp::defaultInstrumentPreset), nullptr);

    auto tracks = project.getTracksNode();
    const auto index = (insertIndex < 0 || insertIndex > tracks.getNumChildren()) ? -1 : insertIndex;
    tracks.addChild (track, index, &undo);

    return Track (track);
}

bool deleteTrack (Project& project, const juce::String& trackId)
{
    return deleteTracks (project, { trackId }) > 0;
}

int deleteTracks (Project& project, const juce::StringArray& trackIds)
{
    juce::StringArray deleting;

    for (const auto& id : trackIds)
        if (project.findTrack (id).isValid())
            deleting.addIfNotAlreadyThere (id);

    if (deleting.isEmpty())
        return 0;

    auto& undo = project.getUndoManager();
    undo.beginNewTransaction (deleting.size() == 1 ? "Delete track \"" + project.findTrack (deleting[0]).getName() + "\""
                                                   : "Delete " + juce::String (deleting.size()) + " tracks");

    // Whatever played into a deleted bus goes to the master instead, and sends to it are removed
    // (all part of the same undo step, so undo brings the routing back too).
    for (const auto& id : deleting)
    {
        if (! project.findTrack (id).isBus())
            continue;

        for (int i = 0; i < project.getNumTracks(); ++i)
        {
            auto other = project.getTrack (i).getState();

            if (other[ids::output].toString() == id)
                other.removeProperty (ids::output, &undo);

            for (const auto& send : mixer::sendsOf (project, other[ids::id].toString()))
                if (send[ids::target].toString() == id)
                    send.getParent().removeChild (send, &undo);
        }
    }

    for (const auto& id : deleting)
        project.getTracksNode().removeChild (project.findTrack (id).getState(), &undo);

    return deleting.size();
}

bool renameTrack (Project& project, const juce::String& trackId, const juce::String& newName)
{
    auto track = project.findTrack (trackId);
    const auto trimmed = newName.trim();

    if (! track.isValid() || trimmed.isEmpty() || trimmed == track.getName())
        return false;

    auto& undo = project.getUndoManager();
    undo.beginNewTransaction ("Rename track");
    track.getState().setProperty (ids::name, trimmed, &undo);
    return true;
}

bool setTrackColour (Project& project, const juce::String& trackId, juce::Colour colour)
{
    return setTracksColour (project, { trackId }, colour);
}

bool setTracksColour (Project& project, const juce::StringArray& trackIds, juce::Colour colour)
{
    const auto text = Track::colourToString (colour.withAlpha (1.0f));
    auto& undo = project.getUndoManager();
    bool changed = false;

    for (const auto& id : trackIds)
    {
        auto track = project.findTrack (id);

        if (! track.isValid() || track.getState()[ids::colour].toString() == text)
            continue;

        if (! changed)
            undo.beginNewTransaction (trackIds.size() == 1 ? "Change track colour" : "Change track colours");

        track.getState().setProperty (ids::colour, text, &undo);
        changed = true;
    }

    return changed;
}

bool moveTrack (Project& project, const juce::String& trackId, int newIndex)
{
    auto tracks = project.getTracksNode();
    const auto currentIndex = project.indexOfTrack (trackId);

    if (currentIndex < 0)
        return false;

    newIndex = juce::jlimit (0, tracks.getNumChildren() - 1, newIndex);

    if (newIndex == currentIndex)
        return false;

    auto& undo = project.getUndoManager();
    undo.beginNewTransaction ("Move track");
    tracks.moveChild (currentIndex, newIndex, &undo);
    return true;
}

} // namespace aether::edit
