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
