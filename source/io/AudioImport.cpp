// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "AudioImport.h"

#include "model/EditCommands.h"

namespace aether
{

juce::String relativePathInProject (const ProjectLocation& location, const juce::File& file)
{
    // Always with forward slashes, so project files are the same on every system.
    return file.getRelativePathFrom (location.getFolder()).replaceCharacter ('\\', '/');
}

juce::File resolveClipFile (const ProjectLocation& location, const juce::String& relativePath)
{
    if (juce::File::isAbsolutePath (relativePath))
        return juce::File (relativePath);

    return location.getFolder().getChildFile (relativePath.replaceCharacter ('\\', '/'));
}

ImportResult importAudioFiles (Project& project, const ProjectLocation& location, juce::AudioFormatManager& formats,
                               const juce::Array<juce::File>& files, const juce::String& trackId, Tick start)
{
    ImportResult result;
    auto& undo = project.getUndoManager();
    const auto audioFolder = location.getAudioFolder();
    audioFolder.createDirectory();

    bool startedStep = false;
    auto targetTrack = project.findTrack (trackId);

    for (const auto& file : files)
    {
        // Can Aether read it?
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));

        if (reader == nullptr || reader->sampleRate <= 0.0 || reader->lengthInSamples <= 0)
        {
            result.problems.add (file.getFileName() + ": not an audio file Aether can read");
            continue;
        }

        AudioSourceInfo info;
        info.name = file.getFileNameWithoutExtension();
        info.sampleRate = reader->sampleRate;
        info.lengthInSamples = reader->lengthInSamples;
        reader = nullptr;   // close it before copying

        // Copy it into the project's Audio folder (unless it's already there).
        auto destination = file;

        if (! file.isAChildOf (audioFolder))
        {
            destination = audioFolder.getChildFile (projectfiles::safeFileName (file.getFileNameWithoutExtension())
                                                    + file.getFileExtension());
            if (destination.exists())
                destination = destination.getNonexistentSibling (true);

            if (! file.copyFileTo (destination))
            {
                result.problems.add (file.getFileName() + ": couldn't copy it into the project folder (is the disk full?)");
                continue;
            }
        }

        info.file = relativePathInProject (location, destination);

        if (! startedStep)
        {
            undo.beginNewTransaction (files.size() == 1 ? "Import audio file" : "Import audio files");
            startedStep = true;
        }

        // Which track? The chosen one for the first file (if it's an audio track), else a new one.
        juce::String clipTrack;

        if (result.clipIds.isEmpty() && targetTrack.isValid() && targetTrack.getKind() == TrackKind::audio)
        {
            clipTrack = targetTrack.getId();
        }
        else
        {
            const auto insertAt = result.trackIds.isEmpty()
                                      ? (targetTrack.isValid() ? project.indexOfTrack (targetTrack.getId()) + 1 : -1)
                                      : project.indexOfTrack (result.trackIds[result.trackIds.size() - 1]) + 1;
            clipTrack = edit::addTrack (project, TrackKind::audio, insertAt, false, info.name).getId();
        }

        const auto clipId = edit::addAudioClip (project, clipTrack, info, start, false);

        if (clipId.isNotEmpty())
        {
            result.clipIds.add (clipId);
            result.trackIds.add (clipTrack);
        }
    }

    return result;
}

} // namespace aether
