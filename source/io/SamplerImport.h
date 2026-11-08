// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

#include "ProjectFiles.h"
#include "model/SamplerSounds.h"

/** Your own sounds in the sampler (Milestone 12): turning files you choose or drop into
    something a sampler track plays. */
namespace aether
{

struct SamplerImport
{
    samplersound::Reference sound;
    juce::String name;
    int rootKey = -1;          // single recordings: the key it plays at its own pitch
    bool keyFound = false;     // the key came from the name or the pitch (not a guess)
    juce::String problem;      // empty, or what went wrong in plain language
};

/** Makes a sampler sound from files:
    - one audio file: copied into the project's Audio folder and played across the keyboard; its
      key comes from its name ("Bass C2.wav") or its pitch (an 808's, say), or is C4 if neither;
    - several audio files (or a folder of them): copied into a folder in Audio and laid out over
      the keyboard by their names (see SampleMapping.h); an SFZ file is written beside them;
    - an SFZ file: used where it is (instruments can be big, so they aren't copied); kept by its
      path inside the project folder, or its full path if it's elsewhere. */
SamplerImport importSamplerSound (const ProjectLocation& location, juce::AudioFormatManager& formats,
                                  const juce::Array<juce::File>& files);

/** The key a recording plays at its own pitch: from its name, else its pitch. (-1 if neither.) */
int findRootKey (juce::AudioFormatManager& formats, const juce::File& file);

/** The same for one of Aether's own drum sounds (an 808 is about G1). */
int findDrumRootKey (const juce::String& drumSoundId);

} // namespace aether
