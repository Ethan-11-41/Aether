// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <memory>

#include <juce_core/juce_core.h>

namespace aether
{

/** Writes MP3 files, using the LAME encoder library (libmp3lame.dll next to Aether.exe; see
    cmake/AetherLame.cmake). The library is loaded only when needed, so Aether works without it:
    MP3 export is then simply unavailable. */
class Mp3Encoder
{
public:
    Mp3Encoder();
    ~Mp3Encoder();

    /** True if the LAME library can be loaded. */
    static bool isAvailable();

    /** e.g. "3.100", or empty if LAME isn't available. */
    static juce::String getLameVersion();

    /** Where to load LAME from (normally next to the program; the tests point elsewhere). */
    static void setLibraryFile (const juce::File& file);
    static juce::File getLibraryFile();

    /** Starts a file: constant bit rate (kbps), 1 or 2 channels. MP3 supports sample rates up to
        48 kHz; higher rates are converted by LAME. */
    juce::Result open (const juce::File& file, double sampleRate, int numChannels, int kbps);

    /** Adds sound (-1..1). `right` is ignored for mono. */
    juce::Result write (const float* left, const float* right, int numSamples);

    /** Finishes and closes the file. */
    juce::Result close();

private:
    struct Lame;
    std::unique_ptr<Lame> lame;
};

} // namespace aether
