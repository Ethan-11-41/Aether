// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <map>

#include "Project.h"

/** What a sampler track plays (Milestone 12), as stored on its Instrument node:

    - `sound`: where the recordings come from, one of
        "builtin:Pianos/Grand piano"   one of Aether's recorded instruments (in its Sounds folder)
        "recording:Audio/My 808.wav"   one recording, played across the keyboard
        "drum:kick-808"                one of Aether's own drum sounds, played across the keyboard
        "sfz:Audio/My piano/My piano.sfz"  an SFZ instrument
      Paths are relative to the project folder, or absolute for files elsewhere.
    - `rootKey`: for a single recording or drum sound, the key it plays at its own pitch.
    - `soundName`: what it's called (shown in the interface).

    The sampler's settings (envelope, tone...) are its parameters, like any instrument's. */
namespace aether
{

namespace samplersound
{
    inline const juce::Identifier property { "sound" };
    inline const juce::Identifier rootProperty { "rootKey" };
    inline const juce::Identifier nameProperty { "soundName" };

    enum class Kind { none, builtIn, recording, drum, sfz };

    struct Reference
    {
        Kind kind = Kind::none;
        juce::String path;   // the instrument's name, the file, or the drum sound's id

        juce::String toString() const;
        bool operator== (const Reference& o) const noexcept   { return kind == o.kind && path == o.path; }
    };

    Reference parse (const juce::String& text);

    inline Reference builtIn (const juce::String& name)      { return { Kind::builtIn, name }; }
    inline Reference recording (const juce::String& path)    { return { Kind::recording, path }; }
    inline Reference drum (const juce::String& soundId)      { return { Kind::drum, soundId }; }
    inline Reference sfzFile (const juce::String& path)      { return { Kind::sfz, path }; }

    /** A name for a sound if none was stored: the instrument's or file's name. */
    juce::String defaultName (const Reference& reference);

    /** What a track's sampler plays (an empty Reference if it isn't a sampler). */
    Reference of (const Project& project, const juce::String& trackId);
    juce::String nameOf (const Project& project, const juce::String& trackId);
    int rootKeyOf (const Project& project, const juce::String& trackId);
}

namespace edit
{
    /** Chooses what a MIDI track's sampler plays (switching the track to the sampler if it isn't
        one), and applies `settings` (parameter id -> value, e.g. the instrument's own envelope)
        in the same undo step. `rootKey` is for single recordings and drum sounds. */
    bool setSamplerSound (Project& project, const juce::String& trackId, const samplersound::Reference& sound,
                          const juce::String& name = {}, int rootKey = -1, const std::map<juce::String, float>& settings = {},
                          bool newUndoStep = true);

    /** Adds a MIDI track (named after the sound) with a sampler playing `sound`, as one undo step. */
    Track addSamplerTrack (Project& project, const samplersound::Reference& sound, const juce::String& name, int rootKey,
                           const std::map<juce::String, float>& settings, int insertIndex = -1);

    /** Which key a single recording plays at its own pitch. */
    bool setSamplerRootKey (Project& project, const juce::String& trackId, int rootKey);
}

} // namespace aether
