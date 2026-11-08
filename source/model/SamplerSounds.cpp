// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "SamplerSounds.h"

#include "MidiClips.h"
#include "Mixer.h"
#include "dsp/DrumSounds.h"
#include "dsp/EffectTypes.h"
#include "dsp/Sampler.h"

namespace aether
{

namespace samplersound
{

namespace
{
    struct Prefix { Kind kind; const char* text; };
    constexpr Prefix prefixes[] = { { Kind::builtIn, "builtin:" }, { Kind::recording, "recording:" },
                                    { Kind::drum, "drum:" }, { Kind::sfz, "sfz:" } };

    /** A file's name without its folder or extension (by hand: stored paths use "/" everywhere). */
    juce::String stemOf (const juce::String& path)
    {
        const auto name = path.replaceCharacter ('\\', '/').fromLastOccurrenceOf ("/", false, false);
        return name.containsChar ('.') ? name.upToLastOccurrenceOf (".", false, false) : name;
    }
}

juce::String Reference::toString() const
{
    for (const auto& p : prefixes)
        if (p.kind == kind)
            return p.text + path;

    return {};
}

Reference parse (const juce::String& text)
{
    for (const auto& p : prefixes)
        if (text.startsWith (p.text) && text.length() > (int) std::strlen (p.text))
            return { p.kind, text.substring ((int) std::strlen (p.text)) };

    return {};
}

juce::String defaultName (const Reference& reference)
{
    switch (reference.kind)
    {
        case Kind::builtIn:   return reference.path.fromLastOccurrenceOf ("/", false, false);
        case Kind::drum:
            if (const auto* info = dsp::drums::findSound (reference.path))
                return info->name;
            return reference.path;
        case Kind::recording:
        case Kind::sfz:       return stemOf (reference.path);
        case Kind::none:      break;
    }

    return "(no sound)";
}

Reference of (const Project& project, const juce::String& trackId)
{
    const auto instrument = midi::instrumentOf (project, trackId);
    if (instrument[ids::type].toString() != dsp::sampler::typeId)
        return {};

    return parse (instrument[property].toString());
}

juce::String nameOf (const Project& project, const juce::String& trackId)
{
    const auto instrument = midi::instrumentOf (project, trackId);
    const auto name = instrument[nameProperty].toString();
    return name.isNotEmpty() ? name : defaultName (of (project, trackId));
}

int rootKeyOf (const Project& project, const juce::String& trackId)
{
    return juce::jlimit (0, 127, (int) midi::instrumentOf (project, trackId).getProperty (rootProperty, 60));
}

} // namespace samplersound

//==============================================================================
bool edit::setSamplerSound (Project& project, const juce::String& trackId, const samplersound::Reference& sound,
                            const juce::String& name, int rootKey, const std::map<juce::String, float>& settings)
{
    auto track = project.findTrack (trackId);

    if (track.getKind() != TrackKind::midi || sound.kind == samplersound::Kind::none)
        return false;

    const auto displayName = name.isNotEmpty() ? name : samplersound::defaultName (sound);
    auto& undo = project.getUndoManager();
    undo.beginNewTransaction ("Use " + displayName);

    auto state = track.getState();
    auto instrument = state.getChildWithName (ids::Instrument);

    if (instrument[ids::type].toString() != dsp::sampler::typeId)
    {
        if (instrument.isValid())
            state.removeChild (instrument, &undo);

        instrument = midi::makeInstrument (dsp::sampler::typeId, {});
        state.appendChild (instrument, &undo);
    }

    instrument.setProperty (samplersound::property, sound.toString(), &undo);
    instrument.setProperty (samplersound::nameProperty, displayName, &undo);
    instrument.setProperty (ids::preset, displayName, &undo);

    if (rootKey >= 0)
        instrument.setProperty (samplersound::rootProperty, juce::jlimit (0, 127, rootKey), &undo);
    else
        instrument.removeProperty (samplersound::rootProperty, &undo);

    if (const auto* type = dsp::findEffectType (dsp::sampler::typeId))
        for (const auto& [id, value] : settings)
            for (const auto& parameter : type->parameters)
                if (parameter.id.toString() == id)
                    instrument.setProperty (parameter.id, parameter.clamp (value), &undo);

    return true;
}

bool edit::setSamplerRootKey (Project& project, const juce::String& trackId, int rootKey)
{
    auto instrument = midi::instrumentOf (project, trackId);
    rootKey = juce::jlimit (0, 127, rootKey);

    if (instrument[ids::type].toString() != dsp::sampler::typeId || samplersound::rootKeyOf (project, trackId) == rootKey)
        return false;

    project.getUndoManager().beginNewTransaction ("Root key " + midi::noteName (rootKey));
    instrument.setProperty (samplersound::rootProperty, rootKey, &project.getUndoManager());
    return true;
}

} // namespace aether
