// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "AudioImport.h"

#include <algorithm>
#include <cstring>

#include "model/MidiClips.h"

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

namespace
{
    /** Copies a file into the Audio folder (unless it's already there); an empty File if that failed. */
    juce::File copyIntoProject (const juce::File& audioFolder, const juce::File& file)
    {
        if (file.isAChildOf (audioFolder))
            return file;

        auto destination = audioFolder.getChildFile (projectfiles::safeFileName (file.getFileNameWithoutExtension())
                                                     + file.getFileExtension());
        if (destination.exists())
            destination = destination.getNonexistentSibling (true);

        return file.copyFileTo (destination) ? destination : juce::File();
    }
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
        const auto destination = copyIntoProject (audioFolder, file);

        if (destination == juce::File())
        {
            result.problems.add (file.getFileName() + ": couldn't copy it into the project folder (is the disk full?)");
            continue;
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

juce::String importPadSound (Project& project, const ProjectLocation& location, juce::AudioFormatManager& formats,
                             const juce::File& file, const juce::String& trackId, int pad)
{
    {
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));

        if (reader == nullptr || reader->sampleRate <= 0.0 || reader->lengthInSamples <= 0)
            return file.getFileName() + " isn't an audio file Aether can read (it can use WAV, AIFF, FLAC, MP3 and Ogg Vorbis).";
    }

    const auto audioFolder = location.getAudioFolder();
    audioFolder.createDirectory();
    const auto destination = copyIntoProject (audioFolder, file);

    if (destination == juce::File())
        return "Couldn't copy " + file.getFileName() + " into the project folder (is the disk full?)";

    if (! edit::setPadSound (project, trackId, pad, relativePathInProject (location, destination)))
        return "That track isn't a drum track.";

    return {};
}

namespace
{
    constexpr const char* reversedSuffix = " (reversed)";

    /** True if `file` is a readable recording with this rate and length (so a reversed copy made
        earlier can be used again). */
    bool matches (juce::AudioFormatManager& formats, const juce::File& file, double rate, juce::int64 length)
    {
        if (! file.existsAsFile())
            return false;

        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
        return reader != nullptr && reader->lengthInSamples == length && std::abs (reader->sampleRate - rate) < 0.5;
    }

    /** Writes `source` backwards into `target` (a 32-bit float WAV), a block at a time from its end. */
    juce::String writeReversed (juce::AudioFormatManager& formats, const juce::File& source, const juce::File& target)
    {
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (source));

        if (reader == nullptr)
            return source.getFileName() + ": Aether couldn't read the recording.";

        target.getParentDirectory().createDirectory();
        const auto temporary = target.getSiblingFile (target.getFileNameWithoutExtension() + ".writing.tmp");
        temporary.deleteFile();

        {
            juce::WavAudioFormat wav;
            std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (temporary);
            const auto channels = (int) reader->numChannels;
            auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions{}
                                                           .withSampleRate (reader->sampleRate)
                                                           .withNumChannels (channels)
                                                           .withBitsPerSample (32)
                                                           .withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint));
            if (writer == nullptr)
                return target.getFileName() + ": Aether couldn't write the reversed copy.";

            constexpr int blockSize = 65536;
            juce::AudioBuffer<float> block (channels, blockSize);

            for (auto end = reader->lengthInSamples; end > 0;)
            {
                const auto start = std::max<juce::int64> (0, end - blockSize);
                const auto count = (int) (end - start);
                block.clear();

                if (! reader->read (&block, 0, count, start, true, true))
                    return source.getFileName() + ": Aether couldn't read the recording.";

                for (int c = 0; c < channels; ++c)
                    std::reverse (block.getWritePointer (c), block.getWritePointer (c) + count);

                if (! writer->writeFromAudioSampleBuffer (block, 0, count))
                    return target.getFileName() + ": Aether couldn't write the reversed copy (is the disk full?).";

                end = start;
            }
        }

        if (! temporary.moveFileTo (target))
            return target.getFileName() + ": Aether couldn't write the reversed copy.";

        return {};
    }
}

juce::StringArray reverseClips (Project& project, const ProjectLocation& location, juce::AudioFormatManager& formats,
                                const juce::StringArray& clipIds)
{
    juce::StringArray problems;
    bool started = false;

    for (const auto& id : clipIds)
    {
        auto clip = clips::findClip (project, id);

        if (! clip.isValid() || clip.isMidi())
            continue;

        // The reversed copy (or, for a reversed one, its original), next to the recording if it's
        // in the project, otherwise in the Audio folder.
        const auto source = resolveClipFile (location, clip.getFile());
        const auto stem = source.getFileNameWithoutExtension();
        const auto backToOriginal = stem.endsWith (reversedSuffix);
        const auto folder = source.isAChildOf (location.getFolder()) ? source.getParentDirectory() : location.getAudioFolder();
        const auto rate = clip.getSourceRate();
        const auto length = clip.getSourceLength();

        juce::File target;

        if (backToOriginal)
        {
            for (const auto& candidate : folder.findChildFiles (juce::File::findFiles, false, stem.dropLastCharacters ((int) std::strlen (reversedSuffix)) + ".*"))
                if (matches (formats, candidate, rate, length))
                    target = candidate;
        }

        if (target == juce::File())
        {
            const auto name = backToOriginal ? stem.dropLastCharacters ((int) std::strlen (reversedSuffix)) : stem + reversedSuffix;
            target = folder.getChildFile (name + ".wav");

            if (! matches (formats, target, rate, length))
            {
                if (target.exists())
                    target = target.getNonexistentSibling (true);

                if (const auto problem = writeReversed (formats, source, target); problem.isNotEmpty())
                {
                    problems.add (problem);
                    continue;
                }

                juce::Logger::writeToLog ("Reversed " + source.getFullPathName() + " into " + target.getFullPathName());
            }
        }

        if (! started)
        {
            project.getUndoManager().beginNewTransaction (clipIds.size() == 1 ? "Reverse clip" : "Reverse clips");
            started = true;
        }

        // The same part of the recording, backwards: [offset, offset + content) becomes
        // [length - offset - content, length - offset); a loop's phase runs the other way too.
        auto& undo = project.getUndoManager();
        auto state = clip.getState();
        const auto content = clip.getContentLength();
        const auto offset = clip.getOffset();
        const auto duration = clip.getDuration();

        state.setProperty (ids::file, relativePathInProject (location, target), &undo);
        state.setProperty (ids::offset, (juce::int64) std::max<juce::int64> (0, length - offset - content), &undo);

        if (clip.isLooping())
            state.setProperty (ids::phase, (juce::int64) ((content - (clip.getPhase() + duration) % content) % content), &undo);

        if (clip.getNumTakes() > 1)
        {
            state.removeProperty (ids::takes, &undo);
            state.removeProperty (ids::take, &undo);
            state.removeProperty (ids::takeLength, &undo);
        }

        const auto name = clip.getName();
        state.setProperty (ids::name, name.endsWith (reversedSuffix) ? name.dropLastCharacters ((int) std::strlen (reversedSuffix))
                                                                     : name + reversedSuffix, &undo);
    }

    return problems;
}

juce::File copyIntoAudioFolder (const ProjectLocation& location, const juce::File& file, const juce::String& subfolder)
{
    auto folder = location.getAudioFolder();
    if (subfolder.isNotEmpty())
        folder = folder.getChildFile (projectfiles::safeFileName (subfolder));

    if (! folder.createDirectory())
        return {};

    return copyIntoProject (folder, file);
}

} // namespace aether
