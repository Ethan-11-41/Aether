// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
//
// Audio files: sample-rate conversion, loading into memory, waveform peaks and importing.
// The tests write their own small WAV files, so they need no sound files in the repository.

#include <cmath>
#include <numbers>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "engine/AudioFileCache.h"
#include "engine/Resampler.h"
#include "io/AudioImport.h"
#include "model/EditCommands.h"

using namespace aether;
using Catch::Matchers::WithinAbs;

namespace
{
    struct TempFolder
    {
        TempFolder()
            : folder (juce::File::getSpecialLocation (juce::File::tempDirectory)
                          .getChildFile ("aether-tests").getNonexistentChildFile ("audio", {}, false))
        {
            folder.createDirectory();
        }

        ~TempFolder()  { folder.deleteRecursively(); }
        juce::File folder;
    };

    /** Writes a WAV file whose channel `c` contains `fill(c, i)`. */
    juce::File writeWav (const juce::File& file, double rate, int channels, int length,
                         const std::function<float (int, int)>& fill)
    {
        juce::AudioBuffer<float> buffer (channels, length);
        for (int c = 0; c < channels; ++c)
            for (int i = 0; i < length; ++i)
                buffer.setSample (c, i, fill (c, i));

        file.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (file);
        auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate (rate)
                                                                                 .withNumChannels (channels)
                                                                                 .withBitsPerSample (24));
        REQUIRE (writer != nullptr);
        writer->writeFromAudioSampleBuffer (buffer, 0, length);
        return file;
    }

    float sine (double frequency, double rate, int i)
    {
        return (float) (0.5 * std::sin (2.0 * std::numbers::pi * frequency * i / rate));
    }

    int countUpwardZeroCrossings (const float* data, int length)
    {
        int count = 0;
        for (int i = 1; i < length; ++i)
            if (data[i - 1] < 0.0f && data[i] >= 0.0f)
                ++count;
        return count;
    }
}

//==============================================================================
TEST_CASE ("Resampling changes the length by the ratio of the rates", "[audio][resample]")
{
    CHECK (resampler::outputLength (44100, 44100.0, 48000.0) == 48000);
    CHECK (resampler::outputLength (48000, 48000.0, 44100.0) == 44100);
    CHECK (resampler::outputLength (1, 44100.0, 96000.0) == 3);
    CHECK (resampler::outputLength (0, 44100.0, 48000.0) == 0);
}

TEST_CASE ("Resampling keeps the pitch and the level", "[audio][resample]")
{
    for (auto [from, to] : { std::pair { 44100.0, 48000.0 }, std::pair { 48000.0, 44100.0 }, std::pair { 48000.0, 96000.0 } })
    {
        std::vector<float> input (44100 * 2);
        for (size_t i = 0; i < input.size(); ++i)
            input[i] = sine (1000.0, from, (int) i);

        const auto output = resampler::process (input.data(), (std::int64_t) input.size(), from, to);
        INFO (from << " -> " << to);

        // One second of output: 1000 cycles, still at about 0.5 amplitude.
        CHECK (std::abs (countUpwardZeroCrossings (output.data() + (int) to / 2, (int) to) - 1000) <= 1);

        float peak = 0.0f;
        for (int i = (int) to / 2; i < (int) to * 3 / 2; ++i)
            peak = std::max (peak, std::abs (output[(size_t) i]));
        CHECK_THAT (peak, WithinAbs (0.5, 0.01));
    }
}

TEST_CASE ("Resampling has no delay: a click stays at the same moment in time", "[audio][resample]")
{
    std::vector<float> input (44100, 0.0f);
    input[22050] = 1.0f;   // a click at exactly 0.5 s

    const auto output = resampler::process (input.data(), (std::int64_t) input.size(), 44100.0, 48000.0);
    const auto loudest = std::max_element (output.begin(), output.end()) - output.begin();
    CHECK (loudest == 24000);   // 0.5 s at 48 kHz
}

TEST_CASE ("Resampling at the same rate copies exactly", "[audio][resample]")
{
    std::vector<float> input { 0.1f, -0.2f, 0.3f };
    CHECK (resampler::process (input.data(), 3, 48000.0, 48000.0) == input);
}

//==============================================================================
TEST_CASE ("The cache loads audio, makes peaks and converts to the engine rate", "[audio][cache]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    TempFolder temp;

    // Stereo, 44.1 kHz, 1 second: left is a ramp from -1 to 1, right is silent.
    const auto file = writeWav (temp.folder.getChildFile ("ramp.wav"), 44100.0, 2, 44100,
                                [] (int c, int i) { return c == 0 ? (float) (-1.0 + 2.0 * i / 44099.0) * 0.9f : 0.0f; });

    AudioFileCache cache;
    cache.setTargetSampleRate (48000.0);

    const auto info = cache.readInfo (file);
    REQUIRE (info.has_value());
    CHECK (info->sampleRate == 44100.0);
    CHECK (info->lengthInSamples == 44100);
    CHECK (info->numChannels == 2);

    CHECK (cache.getAudio (file) == nullptr);     // starts loading
    cache.waitUntilIdle();
    CHECK (cache.getState (file) == AudioFileCache::State::ready);

    const auto audio = cache.getAudio (file);
    REQUIRE (audio != nullptr);
    CHECK (audio->sampleRate == 48000.0);
    CHECK (audio->sourceRate == 44100.0);
    CHECK (audio->samples.getNumChannels() == 2);
    CHECK (audio->samples.getNumSamples() == 48000);

    const auto peaks = cache.getPeaks (file);
    REQUIRE (peaks != nullptr);
    CHECK (peaks->getNumChannels() == 2);
    CHECK (peaks->sourceLength == 44100);
    const auto [lo, hi] = peaks->range (0, 0, 44100);
    CHECK_THAT (lo, WithinAbs (-0.9, 0.001));
    CHECK_THAT (hi, WithinAbs (0.9, 0.001));
    CHECK (peaks->range (1, 0, 44100) == std::pair { 0.0f, 0.0f });

    // Changing the engine's rate reconverts the audio.
    cache.setTargetSampleRate (96000.0);
    CHECK (cache.getAudio (file) == nullptr);
    cache.waitUntilIdle();
    REQUIRE (cache.getAudio (file) != nullptr);
    CHECK (cache.getAudio (file)->samples.getNumSamples() == 96000);
}

TEST_CASE ("Missing or unreadable files are reported, not crashed on", "[audio][cache]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    TempFolder temp;
    AudioFileCache cache;

    const auto missing = temp.folder.getChildFile ("nope.wav");
    cache.getAudio (missing);
    cache.waitUntilIdle();
    CHECK (cache.getState (missing) == AudioFileCache::State::failed);
    CHECK (cache.getError (missing).contains ("missing"));

    const auto junk = temp.folder.getChildFile ("junk.wav");
    junk.replaceWithText ("this is not audio");
    cache.getAudio (junk);
    cache.waitUntilIdle();
    CHECK (cache.getState (junk) == AudioFileCache::State::failed);
    CHECK_FALSE (cache.readInfo (junk).has_value());
}

//==============================================================================
TEST_CASE ("Importing copies files into the project and makes clips, in one undo step", "[audio][import]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    TempFolder temp;
    AudioFileCache cache;

    ProjectLocation location;
    REQUIRE (projectfiles::createProjectFolder (temp.folder, "Song", location).wasOk());
    Project project ("Song");
    const auto audioTrack = edit::addTrack (project, TrackKind::audio).getId();
    project.getUndoManager().clearUndoHistory();

    const auto outside = temp.folder.getChildFile ("elsewhere");
    outside.createDirectory();
    const auto kick = writeWav (outside.getChildFile ("Kick.wav"), 48000.0, 1, 4800, [] (int, int i) { return sine (60.0, 48000.0, i); });
    const auto snare = writeWav (outside.getChildFile ("Snare.wav"), 44100.0, 2, 4410, [] (int, int i) { return sine (200.0, 44100.0, i); });
    const auto junk = outside.getChildFile ("notes.txt");
    junk.replaceWithText ("hello");

    const auto before = project.getState().toXmlString();
    const auto result = importAudioFiles (project, location, cache.getFormats(), { kick, snare, junk }, audioTrack, 4 * ticksPerQuarter);

    CHECK (result.clipIds.size() == 2);
    REQUIRE (result.problems.size() == 1);
    CHECK (result.problems[0].contains ("notes.txt"));

    // The first file went on the chosen track, the second on a new track named after it.
    CHECK (result.trackIds[0] == audioTrack);
    CHECK (project.getNumTracks() == 2);
    CHECK (project.getTrack (1).getName() == "Snare");

    // Both were copied into the project's Audio folder.
    CHECK (location.getAudioFolder().getChildFile ("Kick.wav").existsAsFile());
    CHECK (location.getAudioFolder().getChildFile ("Snare.wav").existsAsFile());

    const auto kickClip = clips::findClip (project, result.clipIds[0]);
    CHECK (kickClip.getFile() == "Audio/Kick.wav");
    CHECK (kickClip.getStart() == 4 * ticksPerQuarter);
    CHECK (kickClip.getSourceRate() == 48000.0);
    CHECK (kickClip.getDuration() == 4800);
    CHECK (resolveClipFile (location, kickClip.getFile()) == location.getAudioFolder().getChildFile ("Kick.wav"));

    // The whole import is a single undo step.
    CHECK (project.getUndoManager().getUndoDescription() == "Import audio files");
    project.getUndoManager().undo();
    CHECK (project.getState().toXmlString() == before);

    // Importing the same file again doesn't overwrite the first copy.
    importAudioFiles (project, location, cache.getFormats(), { kick }, audioTrack, 0);
    CHECK (location.getAudioFolder().getNumberOfChildFiles (juce::File::findFiles, "Kick*.wav") == 2);
}

TEST_CASE ("Reversing a clip plays the same part of its recording backwards", "[audio][import]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    TempFolder temp;
    const ProjectLocation location (temp.folder.getChildFile ("Song.aether"));
    location.getAudioFolder().createDirectory();

    // A recording that rises steadily: sample i is i / 100000 (left) and its negative (right).
    constexpr int length = 100000;
    const auto file = writeWav (location.getAudioFolder().getChildFile ("Rise.wav"), 48000.0, 2, length,
                                [] (int c, int i) { return (c == 0 ? 1.0f : -1.0f) * (float) i / (float) length; });

    Project project;
    const auto track = edit::addTrack (project, TrackKind::audio).getId();
    const auto clipId = edit::addAudioClip (project, track, { "Audio/Rise.wav", "Rise", 48000.0, length }, 0);
    auto clip = clips::findClip (project, clipId);

    // It plays samples 20000 to 50000 of the recording.
    edit::trimClipStart (project, clipId, clip.getStart() + ticksPerQuarter / 4);
    clip = clips::findClip (project, clipId);
    auto state = clip.getState();
    state.setProperty (ids::offset, (juce::int64) 20000, nullptr);
    state.setProperty (ids::contentLength, (juce::int64) 30000, nullptr);
    state.setProperty (ids::duration, (juce::int64) 30000, nullptr);

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    CHECK (reverseClips (project, location, formats, { clipId }).isEmpty());
    CHECK (project.getUndoManager().getUndoDescription() == "Reverse clip");

    // The reversed copy: the recording backwards, exactly, in both channels.
    const auto reversedFile = location.getAudioFolder().getChildFile ("Rise (reversed).wav");
    REQUIRE (reversedFile.existsAsFile());
    {
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (reversedFile));
        REQUIRE (reader != nullptr);
        CHECK (reader->lengthInSamples == length);
        juce::AudioBuffer<float> b (2, length);
        reader->read (&b, 0, length, 0, true, true);
        for (const int i : { 0, 1, 777, 65535, 65536, 99999 })
        {
            CHECK_THAT (b.getSample (0, i), WithinAbs ((float) (length - 1 - i) / (float) length, 1.0e-6));
            CHECK_THAT (b.getSample (1, i), WithinAbs (-(float) (length - 1 - i) / (float) length, 1.0e-6));
        }
    }

    // The clip plays the same part, backwards: 50000 down to 20000 of the original is 50000 to
    // 80000 of the reversed copy.
    clip = clips::findClip (project, clipId);
    CHECK (clip.getFile() == "Audio/Rise (reversed).wav");
    CHECK (clip.getOffset() == length - 20000 - 30000);
    CHECK (clip.getContentLength() == 30000);
    CHECK (clip.getName() == "Rise (reversed)");

    // Undo puts the original back (the copy stays, for redo).
    project.getUndoManager().undo();
    CHECK (clips::findClip (project, clipId).getFile() == "Audio/Rise.wav");
    CHECK (clips::findClip (project, clipId).getOffset() == 20000);
    CHECK (reversedFile.existsAsFile());

    // Reversed twice is the original again: no new file, the copy is reused the first time too.
    CHECK (reverseClips (project, location, formats, { clipId }).isEmpty());
    CHECK (reverseClips (project, location, formats, { clipId }).isEmpty());
    clip = clips::findClip (project, clipId);
    CHECK (clip.getFile() == "Audio/Rise.wav");
    CHECK (clip.getOffset() == 20000);
    CHECK (clip.getName() == "Rise");
    CHECK (location.getAudioFolder().findChildFiles (juce::File::findFiles, false).size() == 2);
    CHECK (file.existsAsFile());
}

TEST_CASE ("Reversing a looping clip keeps its loop in step; a loop-recorded one keeps its take", "[audio][import]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    TempFolder temp;
    const ProjectLocation location (temp.folder.getChildFile ("Song.aether"));
    location.getAudioFolder().createDirectory();
    writeWav (location.getAudioFolder().getChildFile ("Loop.wav"), 48000.0, 1, 1000, [] (int, int i) { return (float) i / 1000.0f; });

    Project project;
    const auto track = edit::addTrack (project, TrackKind::audio).getId();
    const auto clipId = edit::addAudioClip (project, track, { "Audio/Loop.wav", "Loop", 48000.0, 1000 }, 0);
    auto state = clips::findClip (project, clipId).getState();
    state.setProperty (ids::loop, true, nullptr);
    state.setProperty (ids::phase, (juce::int64) 300, nullptr);
    state.setProperty (ids::duration, (juce::int64) 2500, nullptr);
    state.setProperty (ids::takes, 3, nullptr);
    state.setProperty (ids::take, 1, nullptr);
    state.setProperty (ids::takeLength, (juce::int64) 300, nullptr);

    // What the clip plays at each moment, read from its file through its offset, loop and phase.
    const auto played = [&] (int at)
    {
        const auto clip = clips::findClip (project, clipId);
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (resolveClipFile (location, clip.getFile())));
        juce::AudioBuffer<float> b (1, 1);
        reader->read (&b, 0, 1, clip.getOffset() + (clip.getPhase() + at) % clip.getContentLength(), true, true);
        return b.getSample (0, 0);
    };

    std::vector<float> forwards;
    for (int at = 0; at < 2500; at += 7)
        forwards.push_back (played (at));

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    REQUIRE (reverseClips (project, location, formats, { clipId }).isEmpty());

    // Backwards: the clip's moment `at` is what was at 2499 - at.
    size_t k = 0;
    for (int at = 0; at < 2500; at += 7, ++k)
        CHECK_THAT (played (2499 - at), WithinAbs (forwards[k], 1.0e-4));

    CHECK (clips::findClip (project, clipId).getNumTakes() == 1);
}
