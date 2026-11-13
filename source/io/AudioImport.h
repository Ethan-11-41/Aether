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

/** Copies a file into the project's Audio folder (or a folder inside it), unless it's already
    there, without overwriting anything. The copy, or an empty File if it couldn't be copied. */
juce::File copyIntoAudioFolder (const ProjectLocation& location, const juce::File& file, const juce::String& subfolder = {});

/** The project-relative path of a file in the project folder, e.g. "Audio/Guitar.wav". */
juce::String relativePathInProject (const ProjectLocation& location, const juce::File& file);

/** The file a clip refers to. */
juce::File resolveClipFile (const ProjectLocation& location, const juce::String& relativePath);

/** Reverses audio clips (plays them backwards), as one undo step: each clip then plays a reversed
    copy of its recording, "<name> (reversed).wav" (32-bit float, at the recording's rate) in the
    project's Audio folder, made once and reused; reversing it again goes back to the original.
    The original recording is never changed. The part of the recording a clip plays is the same
    part, backwards; its fades stay where they are on the timeline. A loop-recorded clip keeps
    only the take it plays. Returns what went wrong, if anything, in plain words (one line per
    clip that couldn't be reversed). */
juce::StringArray reverseClips (Project& project, const ProjectLocation& location, juce::AudioFormatManager& formats,
                                const juce::StringArray& clipIds);

} // namespace aether
