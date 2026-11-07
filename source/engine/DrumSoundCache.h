// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <memory>

#include "AudioFileCache.h"

namespace aether
{

/** Aether's built-in drum sounds (dsp/DrumSounds.h), each rendered once per sample rate and
    shared by everything that plays it (the engine, exports). Safe to call from the UI thread and
    the export thread at the same time; never from the audio thread. Null for an unknown sound. */
std::shared_ptr<const AudioData> builtInDrumSound (const juce::String& soundId, double sampleRate);

} // namespace aether
