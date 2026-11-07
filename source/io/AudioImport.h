// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

#include "ProjectFiles.h"
#include "model/ClipEdits.h"

namespace aether
{

struct ImportResult
{
    juce::StringArray clipIds;     // the new clips
    juce::StringArray trackIds;    // the track each clip went on (new or existing)
    juce::StringArray problems;    // files that couldn't be imported, with the reason
};

/** Imports audio files into a project, as one undo step:

    - each file is copied into the project's Audio folder (so the project keeps working if the
      original is moved or deleted, and Save As can copy everything); files already in that
      folder are used where they are;
    - the first file goes on `trackId` if that's an audio track, otherwise on a new audio track;
    - with several files, each goes on its own new audio track (named after the file), all
      starting at `start`, which is what you want for stems or multitrack recordings.

    Files Aether can't read are skipped and listed in `problems`.
*/
ImportResult importAudioFiles (Project& project, const ProjectLocation& location, juce::AudioFormatManager& formats,
                               const juce::Array<juce::File>& files, const juce::String& trackId, Tick start);

/** Puts an audio file on a drum pad (copied into the project's Audio folder like any import), as
    one undo step. Returns an empty string, or what went wrong in plain language. */
juce::String importPadSound (Project& project, const ProjectLocation& location, juce::AudioFormatManager& formats,
                             const juce::File& file, const juce::String& trackId, int pad);

/** The project-relative path of a file in the project folder, e.g. "Audio/Guitar.wav". */
juce::String relativePathInProject (const ProjectLocation& location, const juce::File& file);

/** The file a clip refers to. */
juce::File resolveClipFile (const ProjectLocation& location, const juce::String& relativePath);

} // namespace aether
