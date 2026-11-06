// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <vector>

#include <juce_audio_formats/juce_audio_formats.h>

#include "io/ProjectFiles.h"
#include "model/Project.h"
#include "time/TempoMap.h"

namespace aether
{

enum class ExportFormat { wav, flac, mp3 };

/** What to export, and how. */
struct ExportOptions
{
    ExportFormat format = ExportFormat::wav;
    int bitDepth = 24;              // WAV: 16, 24 or 32 (floating point); FLAC: 16 or 24
    int mp3Kbps = 320;              // MP3: 128, 192, 256 or 320
    double sampleRate = 48000.0;

    bool mix = true;                // the whole song, as one file
    bool stems = false;             // one file per track (each as you'd hear it soloed)
    bool loopRegionOnly = false;    // otherwise: from the start to the end of the last clip
    bool includeTail = true;        // let reverbs, echoes and released notes ring out at the end
    bool normalize = false;         // the mix: loudest peak at -1 dB (stems are never normalized)

    juce::File folder;              // where the files go
    juce::String name;              // the mix's file name (without extension); stems add " - track name"

    juce::String extension() const;
};

/** The part of the song to export, in ticks. */
struct ExportRange
{
    Tick start = 0, end = 0;
};

/** The whole song (start to the end of the last clip), or the loop region. Empty (end == start)
    if there is nothing to export. */
ExportRange exportRange (const Project& project, bool loopRegionOnly);

/** Renders a project offline, faster than real time, with its own private engine: exactly what
    you'd hear from `range.start` to `range.end` (plus the tail, if asked: up to 10 s, stopping
    once it's silent). Not on the UI thread (it waits for audio files to load).
    `progress` is called with 0..1 and returns false to cancel. */
juce::Result renderProject (const Project& project, const ProjectLocation& location, double sampleRate,
                            ExportRange range, bool includeTail, juce::AudioBuffer<float>& output,
                            const std::function<bool (float)>& progress = {});

/** Writes stereo audio in the chosen format (16-bit is dithered). */
juce::Result writeAudioFile (const juce::AudioBuffer<float>& audio, const ExportOptions& options, const juce::File& file);

/** An export: prepared on the UI thread (it copies the project), then run on a background thread. */
class ExportJob
{
public:
    /** Copies what's needed from the project (UI thread). */
    ExportJob (const Project& project, const ProjectLocation& location, const ExportOptions& options);

    /** Renders and writes every file (call on a background thread). */
    juce::Result run();

    void cancel() noexcept                     { cancelled = true; }
    float getProgress() const noexcept         { return progress.load(); }
    juce::String getCurrentStep() const;

    /** The files written (after run()). */
    const juce::Array<juce::File>& getFiles() const noexcept   { return files; }

    /** Notes for the person exporting, e.g. that the mix clipped. */
    const juce::StringArray& getWarnings() const noexcept      { return warnings; }

    /** The tracks that get a stem: audio and MIDI tracks with clips, not muted. */
    static juce::StringArray stemTracks (const Project& project);

private:
    struct Render
    {
        juce::String label;
        juce::ValueTree state;    // a private copy of the project, set up for this render
        juce::File file;
    };

    juce::File uniqueFile (const juce::String& baseName) const;

    ExportOptions options;
    ProjectLocation location;
    ExportRange range;
    std::vector<Render> renders;
    std::atomic<float> progress { 0.0f };
    std::atomic<bool> cancelled { false };
    std::atomic<int> currentRender { 0 };
    juce::Array<juce::File> files;
    juce::StringArray warnings;
};

} // namespace aether
