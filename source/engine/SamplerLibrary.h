// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <map>
#include <memory>
#include <vector>

#include "AudioFileCache.h"
#include "dsp/SampleMap.h"
#include "io/SfzReader.h"
#include "model/SamplerSounds.h"

/** Finding and reading what samplers play (Milestone 12): Aether's own recorded instruments (the
    "Sounds" folder installed next to Aether), SFZ files, single recordings and drum sounds. */
namespace aether::samplerlibrary
{

/** Aether's recorded instruments: the folder set with setBuiltInFolder(), else the AETHER_SOUNDS
    environment variable, else "Sounds" next to the program. */
juce::File builtInFolder();
void setBuiltInFolder (const juce::File& folder);

/** One of Aether's recorded instruments, as listed in the Sounds folder's library.json. */
struct Entry
{
    juce::String category, name, description;
    samplersound::Reference reference;
};

/** Every recorded instrument, in library order (empty if the Sounds folder isn't there). */
std::vector<Entry> list();

/** The SFZ file of a built-in or SFZ sound (none for other kinds). */
juce::File sfzFileOf (const samplersound::Reference& sound, const juce::File& projectFolder);

/** A parsed SFZ file, read once and kept (read again if the file changes). Null if missing. */
std::shared_ptr<const sfz::Instrument> readSfz (const juce::File& file);

/** A sound's recordings, ready to load: each region's `sample` is a path for the AudioResolver
    (absolute, or relative to the project folder), or "drum:<id>" for one of Aether's drum
    sounds. */
std::vector<sfz::Region> regionsOf (const samplersound::Reference& sound, int rootKey, const juce::File& projectFolder);

/** The settings a sound starts with: its SFZ envelope, and Slide for 808s and basses. */
std::map<juce::String, float> suggestedSettings (const samplersound::Reference& sound, const juce::File& projectFolder);

/** Every audio file the project's samplers use (absolute), e.g. to load before exporting. */
juce::Array<juce::File> filesUsed (const Project& project, const juce::File& projectFolder);

/** How many of a sampler track's recordings are loaded, out of how many (0 of 0: none found). */
std::pair<int, int> loadingProgress (const Project& project, const juce::String& trackId, const juce::File& projectFolder,
                                     AudioFileCache& cache);

/** A map of zones from regions and their audio (null audio: that zone is left out for now).
    Offsets and loops are converted from the file's rate to the audio's. */
dsp::SampleMap buildMap (const std::vector<sfz::Region>& regions, const std::vector<std::shared_ptr<const AudioData>>& audio);

} // namespace aether::samplerlibrary
