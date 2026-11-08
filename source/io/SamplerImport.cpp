// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "SamplerImport.h"

#include "AudioImport.h"
#include "SampleMapping.h"
#include "SfzReader.h"
#include "dsp/DrumSounds.h"
#include "dsp/PitchDetect.h"

namespace aether
{

int findRootKey (juce::AudioFormatManager& formats, const juce::File& file)
{
    if (const auto named = samplemapping::noteInName (file.getFileName()); named >= 0)
        return named;

    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr || reader->sampleRate <= 0.0)
        return -1;

    // Two seconds is plenty to hear the pitch.
    const auto length = (int) std::min<juce::int64> (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 2.0));
    juce::AudioBuffer<float> sound ((int) std::min (2u, reader->numChannels), length);
    reader->read (&sound, 0, length, 0, true, sound.getNumChannels() > 1);

    if (const auto key = dsp::pitch::detectKey (sound, reader->sampleRate))
        return *key;

    return -1;
}

int findDrumRootKey (const juce::String& drumSoundId)
{
    constexpr double rate = 48000.0;
    if (const auto key = dsp::pitch::detectKey (dsp::drums::render (drumSoundId, rate), rate))
        return *key;
    return -1;
}

SamplerImport importSamplerSound (const ProjectLocation& location, juce::AudioFormatManager& formats,
                                  const juce::Array<juce::File>& chosen)
{
    SamplerImport result;

    // A folder stands for the audio files in it.
    juce::Array<juce::File> files;
    juce::String folderName;

    for (const auto& f : chosen)
    {
        if (f.isDirectory())
        {
            folderName = f.getFileName();
            for (const auto& entry : juce::RangedDirectoryIterator (f, true, "*", juce::File::findFiles))
                files.add (entry.getFile());
        }
        else
        {
            files.add (f);
        }
    }

    // An SFZ instrument: used where it is.
    for (const auto& f : files)
    {
        if (f.hasFileExtension ("sfz"))
        {
            const auto inside = f.isAChildOf (location.getFolder());
            const auto path = (inside ? f.getRelativePathFrom (location.getFolder()) : f.getFullPathName()).replaceCharacter ('\\', '/');

            if (sfz::read (f).regions.empty())
            {
                result.problem = f.getFileName() + " has no recordings Aether can play.";
                return result;
            }

            result.sound = samplersound::sfzFile (path);
            result.name = f.getFileNameWithoutExtension();
            return result;
        }
    }

    // Audio files only (others are left out).
    juce::Array<juce::File> audio;
    for (const auto& f : files)
        if (formats.findFormatForFileExtension (f.getFileExtension()) != nullptr)
            audio.add (f);

    if (audio.isEmpty())
    {
        result.problem = "There's no audio file there Aether can play (it can use WAV, AIFF, FLAC, MP3 and Ogg Vorbis, or an SFZ instrument).";
        return result;
    }

    // One recording: across the keyboard, at its own pitch on its own key.
    if (audio.size() == 1)
    {
        const auto copy = copyIntoAudioFolder (location, audio[0]);
        if (copy == juce::File())
        {
            result.problem = "Couldn't copy " + audio[0].getFileName() + " into the project folder (is the disk full?)";
            return result;
        }

        const auto key = findRootKey (formats, copy);
        result.sound = samplersound::recording (relativePathInProject (location, copy));
        result.name = audio[0].getFileNameWithoutExtension();
        result.rootKey = key >= 0 ? key : 60;
        result.keyFound = key >= 0;
        return result;
    }

    // Several: copied together into a folder, laid out by their names, with an SFZ beside them.
    const auto name = folderName.isNotEmpty() ? folderName : audio[0].getParentDirectory().getFileName();
    juce::Array<juce::File> copies;

    for (const auto& f : audio)
    {
        const auto copy = copyIntoAudioFolder (location, f, name);
        if (copy == juce::File())
        {
            result.problem = "Couldn't copy " + f.getFileName() + " into the project folder (is the disk full?)";
            return result;
        }
        copies.add (copy);
    }

    const auto folder = copies[0].getParentDirectory();
    const auto instrument = samplemapping::mapByNames (copies, folder);
    auto sfzFile = folder.getChildFile (projectfiles::safeFileName (name) + ".sfz");
    if (sfzFile.exists())
        sfzFile = sfzFile.getNonexistentSibling (true);

    if (! sfzFile.replaceWithText (sfz::write (instrument)))
    {
        result.problem = "Couldn't write the instrument file in the project folder.";
        return result;
    }

    result.sound = samplersound::sfzFile (relativePathInProject (location, sfzFile));
    result.name = name;
    return result;
}

} // namespace aether
