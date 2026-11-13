// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "AudioImport.h"

#include <algorithm>
#include <cstring>

#include "dsp/SoundEffects.h"
#include "model/EditCommands.h"
#include "model/MidiClips.h"
#include "model/TimelineEdits.h"

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

namespace
{
    /** Where a sound effect goes and how long it is (see addSoundEffect). */
    struct EffectPlace
    {
        Tick start = 0;
        double seconds = 0.0;   // for effects made to a length
        Tick bar = 0;           // the bar line it belongs to: where a build-up ends, anything else starts
    };

    EffectPlace placeEffect (const TempoMap& tempo, const dsp::soundfx::Info& info, int bars, Tick position)
    {
        // Build-ups end on the nearest bar line; the rest start where asked (a downlifter on the
        // nearest bar line too).
        auto nearestBar = [&tempo] (Tick tick)
        {
            const auto bar = tempo.ticksToBarBeat (std::max<Tick> (0, tick)).bar;
            return tick - tempo.barToTicks (bar) <= tempo.barToTicks (bar + 1) - tick ? bar : bar + 1;
        };

        EffectPlace place { std::max<Tick> (0, position), 0.0, std::max<Tick> (0, position) };

        if (info.buildsUp)
        {
            const auto endBar = std::max (1, nearestBar (position));
            place.start = tempo.barToTicks (std::max (0, endBar - bars));
            place.bar = tempo.barToTicks (endBar);
            place.seconds = tempo.ticksToSeconds (place.bar) - tempo.ticksToSeconds (place.start);
        }
        else if (info.hasLength)
        {
            const auto startBar = nearestBar (position);
            place.start = place.bar = tempo.barToTicks (startBar);
            place.seconds = tempo.ticksToSeconds (tempo.barToTicks (startBar + bars)) - tempo.ticksToSeconds (place.start);
        }

        return place;
    }

    /** The sound effect's file in the project, made once per length (and sample rate). */
    struct EffectFile
    {
        AudioSourceInfo source;
        juce::String problem;
    };

    EffectFile effectFile (const ProjectLocation& location, const dsp::soundfx::Info& info, double seconds, double rate)
    {
        const auto folder = location.getAudioFolder().getChildFile ("Sound effects");
        const auto fileName = juce::String (info.name) + (info.hasLength ? " " + juce::String (seconds, 2) + " s" : juce::String())
                              + " " + juce::String (juce::roundToInt (rate / 100.0) / 10.0) + " kHz.wav";
        const auto file = folder.getChildFile (projectfiles::safeFileName (fileName));
        juce::int64 length = 0;

        {
            juce::AudioFormatManager formats;
            formats.registerBasicFormats();

            if (std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file)); reader != nullptr)
                length = reader->lengthInSamples;
        }

        if (length <= 0)
        {
            const auto sound = dsp::soundfx::render (info.id, seconds, rate);
            folder.createDirectory();
            file.deleteFile();

            juce::WavAudioFormat wav;
            std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (file);
            auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate (rate)
                                                                                     .withNumChannels (sound.getNumChannels())
                                                                                     .withBitsPerSample (24));
            if (writer == nullptr || ! writer->writeFromAudioSampleBuffer (sound, 0, sound.getNumSamples()))
                return { {}, "Aether couldn't write " + file.getFullPathName() + " (is the disk full?)." };

            length = sound.getNumSamples();
        }

        return { { relativePathInProject (location, file), info.name, rate, length }, {} };
    }

    void rememberEffect (Project& project, const juce::String& clipId, const juce::String& effectId, int bars, const EffectPlace& place)
    {
        auto clip = clips::findClip (project, clipId).getState();
        clip.setProperty (ids::soundEffect, effectId, &project.getUndoManager());
        clip.setProperty (ids::effectBars, bars, &project.getUndoManager());
        clip.setProperty (ids::effectBar, (juce::int64) (place.bar - place.start), &project.getUndoManager());
    }
}

SoundEffectResult addSoundEffect (Project& project, const ProjectLocation& location, const juce::String& effectId, int bars,
                                  const juce::String& trackId, Tick position, double sampleRate)
{
    SoundEffectResult result;
    const auto* info = dsp::soundfx::find (effectId);

    if (info == nullptr)
    {
        result.problem = "There's no sound effect called " + effectId + ".";
        return result;
    }

    const auto rate = sampleRate > 0.0 ? sampleRate : 48000.0;
    bars = std::clamp (bars, 1, 64);
    const auto place = placeEffect (timeline::buildTempoMap (project), *info, bars, position);
    const auto made = effectFile (location, *info, place.seconds, rate);

    if (made.problem.isNotEmpty())
    {
        result.problem = made.problem;
        return result;
    }

    // On the audio track asked for, or a new one below it.
    project.getUndoManager().beginNewTransaction (juce::String ("Add ") + info->name);
    auto track = project.findTrack (trackId);

    if (! track.isValid() || track.getKind() != TrackKind::audio)
    {
        const auto below = project.indexOfTrack (trackId);
        track = edit::addTrack (project, TrackKind::audio, below >= 0 ? below + 1 : -1, false, "Sound effects");
    }

    result.trackId = track.getId();
    result.clipId = edit::addAudioClip (project, result.trackId, made.source, place.start, false);

    if (result.clipId.isNotEmpty())
        rememberEffect (project, result.clipId, effectId, bars, place);

    return result;
}

const dsp::soundfx::Info* soundEffectOf (const Project& project, const juce::String& clipId)
{
    const auto clip = clips::findClip (project, clipId);
    return clip.isValid() && ! clip.isMidi() ? dsp::soundfx::find (clip.getState()[ids::soundEffect].toString()) : nullptr;
}

int soundEffectBars (const Project& project, const juce::String& clipId)
{
    return std::clamp ((int) clips::findClip (project, clipId).getState().getProperty (ids::effectBars, 1), 1, 64);
}

juce::String changeSoundEffect (Project& project, const ProjectLocation& location, const juce::String& clipId,
                                const juce::String& effectId, int bars, double sampleRate, const juce::String& undoName)
{
    const auto* was = soundEffectOf (project, clipId);
    const auto* info = dsp::soundfx::find (effectId);

    if (was == nullptr)
        return "This clip isn't one of Aether's sound effects.";

    if (info == nullptr)
        return "There's no sound effect called " + effectId + ".";

    // The bar line it belongs to stays: where a build-up ends (what it rises into), where
    // anything else starts. It's remembered in ticks from the clip's start, so it moves with the
    // clip and stays on its bar when the tempo changes.
    const auto clip = clips::findClip (project, clipId);
    const auto tempo = timeline::buildTempoMap (project);
    const auto anchor = clip.getState().hasProperty (ids::effectBar)
                            ? clip.getStart() + (Tick) (juce::int64) clip.getState()[ids::effectBar]
                            : (was->buildsUp ? clip.getEnd (tempo) : clip.getStart());
    const auto rate = sampleRate > 0.0 ? sampleRate : 48000.0;
    bars = std::clamp (bars, 1, 64);
    const auto place = placeEffect (tempo, *info, bars, anchor);
    const auto made = effectFile (location, *info, place.seconds, rate);

    if (made.problem.isNotEmpty())
        return made.problem;

    auto& undo = project.getUndoManager();
    undo.beginNewTransaction (undoName.isNotEmpty() ? undoName : juce::String ("Change sound effect"));

    // The same clip (its volume, fades and track stay), playing the new sound from its start.
    auto state = clip.getState();
    const auto length = made.source.lengthInSamples;
    state.setProperty (ids::name, made.source.name, &undo);
    state.setProperty (ids::file, made.source.file, &undo);
    state.setProperty (ids::start, (juce::int64) place.start, &undo);
    state.setProperty (ids::sourceRate, made.source.sampleRate, &undo);
    state.setProperty (ids::sourceLength, length, &undo);
    state.setProperty (ids::offset, (juce::int64) 0, &undo);
    state.setProperty (ids::contentLength, length, &undo);
    state.setProperty (ids::duration, length, &undo);
    state.setProperty (ids::phase, (juce::int64) 0, &undo);
    state.setProperty (ids::loop, false, &undo);

    for (const auto& id : { ids::takes, ids::take, ids::takeLength })
        state.removeProperty (id, &undo);

    // Fades fit inside the new length.
    const auto fadeIn = std::min (clip.getFadeIn(), length);
    state.setProperty (ids::fadeIn, (juce::int64) fadeIn, &undo);
    state.setProperty (ids::fadeOut, (juce::int64) std::min (clip.getFadeOut(), length - fadeIn), &undo);

    rememberEffect (project, clipId, effectId, bars, place);
    return {};
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
