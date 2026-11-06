// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
//
// Recording for real: the engine is fed a known signal as if from a sound card, the Recorder
// writes it to WAV files, and the clips must hold exactly that sound, in exactly the right place.
// Also input monitoring, and measuring latency through a simulated cable.

#include <cmath>
#include <random>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "engine/LatencyProbe.h"
#include "engine/Recorder.h"
#include "engine/TimelineSync.h"
#include "io/AudioImport.h"
#include "model/ClipEdits.h"
#include "model/EditCommands.h"
#include "model/Mixer.h"
#include "model/Recording.h"
#include "model/TimelineEdits.h"

using namespace aether;
using Catch::Matchers::WithinAbs;

namespace
{
    constexpr double rate = 48000.0;
    constexpr Tick q = ticksPerQuarter;   // 24000 samples at 120 BPM
    constexpr int block = 256;

    /** The "microphone": a slow ramp, so every sample is different and its position is known. */
    float micSample (std::int64_t t)   { return (float) ((t % 100000) + 1) * 1.0e-6f; }

    struct Fixture
    {
        Fixture()
        {
            folder.createDirectory();
            location = ProjectLocation (folder.getChildFile ("Song.aether"));
            vocals = edit::addTrack (project, TrackKind::audio, -1, true, "Vocals").getId();
            edit::setArmed (project, { vocals }, true);
            engine.prepare (rate, block);
            sync = std::make_unique<TimelineSync> (project, engine, cache, [this] { return location; });
            recorder = std::make_unique<Recorder> (project, engine, *sync, [this] { return location; },
                                                   [this] { return latency; }, [] { return 2; });
        }

        ~Fixture()
        {
            recorder = nullptr;
            sync = nullptr;
            folder.deleteRecursively();
        }

        /** Runs the engine for `samples`, with the microphone on input 1, and returns the left output. */
        std::vector<float> run (int samples)
        {
            std::vector<float> out;
            std::vector<float> mic (block), silence (block), left (block), right (block);

            for (int done = 0; done < samples; done += block)
            {
                for (int i = 0; i < block; ++i)
                    mic[(size_t) i] = micSample (clock + i);

                const float* inputs[] = { mic.data(), silence.data() };
                float* outputs[] = { left.data(), right.data() };
                engine.process (inputs, 2, outputs, 2, block);
                engine.collectGarbage();
                clock += block;
                out.insert (out.end(), left.begin(), left.end());
            }

            return out;
        }

        void stopAndFinish()
        {
            recorder->stop();
            run (block);
            for (int i = 0; i < 3 && recorder->isActive(); ++i)
                recorder->poll();
        }

        juce::ScopedJuceInitialiser_GUI juce;
        juce::File folder = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                .getChildFile ("aether-recording-test-" + juce::Uuid().toString());
        ProjectLocation location;
        Project project;
        juce::String vocals;
        AudioEngine engine;
        AudioFileCache cache;
        std::unique_ptr<TimelineSync> sync;
        std::unique_ptr<Recorder> recorder;
        int latency = 0;
        std::int64_t clock = 0;   // samples the "sound card" has delivered
    };

    std::vector<float> readWav (const juce::File& file)
    {
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatReader> reader (wav.createReaderFor (new juce::FileInputStream (file), true));
        REQUIRE (reader != nullptr);
        juce::AudioBuffer<float> buffer ((int) reader->numChannels, (int) reader->lengthInSamples);
        reader->read (&buffer, 0, (int) reader->lengthInSamples, 0, true, true);
        return { buffer.getReadPointer (0), buffer.getReadPointer (0) + buffer.getNumSamples() };
    }
}

TEST_CASE ("Recording writes exactly what came in, and places it corrected for latency", "[recording][engine]")
{
    Fixture f;
    f.latency = 100;
    f.run (1024);                        // the sound card has been running for a while

    REQUIRE (f.recorder->start().isEmpty());
    CHECK (f.recorder->isActive());
    f.run (48000);
    CHECK (f.engine.getRecordingStatus().recording);
    CHECK (f.engine.getRecordingStatus().startSample == 0);

    juce::StringArray finishedClips;
    f.recorder->onFinished = [&] (const juce::StringArray& ids) { finishedClips = ids; };
    f.stopAndFinish();

    CHECK_FALSE (f.recorder->isActive());
    REQUIRE (finishedClips.size() == 1);

    // The file holds the microphone, sample for sample, from the moment recording started.
    const auto clip = clips::findClip (f.project, finishedClips[0]);
    const auto file = resolveClipFile (f.location, clip.getFile());
    CHECK (file.getFileName() == "Vocals 1.wav");
    const auto recorded = readWav (file);
    REQUIRE (recorded.size() == (48000 + block - 1) / block * block);   // whole blocks, until the stop arrived

    for (size_t i = 0; i < recorded.size(); i += 997)
        REQUIRE_THAT (recorded[i], WithinAbs (micSample (1024 + (std::int64_t) i), 2.0e-7));

    // The clip starts where recording started, reading the file 100 samples in.
    CHECK (clip.getStart() == 0);
    CHECK (clip.getOffset() == 100);
    CHECK (clip.getDuration() == (juce::int64) recorded.size() - 100);
    CHECK (f.project.getUndoManager().getUndoDescription() == "Record \"Vocals\"");

    // The next recording gets a new file.
    f.engine.locate (0);
    REQUIRE (f.recorder->start().isEmpty());
    f.run (24000);
    f.stopAndFinish();
    CHECK (f.folder.getChildFile ("Audio").getChildFile ("Vocals 2.wav").existsAsFile());
}

TEST_CASE ("Loop recording turns each pass into a take", "[recording][engine][takes]")
{
    Fixture f;
    edit::setLoopRange (f.project, q, 2 * q);    // one beat: 24000 samples
    edit::setLoopEnabled (f.project, true);

    juce::StringArray clipIds;
    f.recorder->onFinished = [&] (const juce::StringArray& ids) { clipIds = ids; };
    REQUIRE (f.recorder->start().isEmpty());
    f.run (24000 + 3 * 24000 + 2048);        // up to the loop, three passes, and a little more
    f.stopAndFinish();

    REQUIRE (clipIds.size() == 1);
    const auto clip = clips::findClip (f.project, clipIds[0]);
    CHECK (clip.getStart() == q);
    CHECK (clip.getDuration() == 24000);
    CHECK (clip.getNumTakes() == 3);
    CHECK (clip.getTake() == 2);
    CHECK (clip.getOffset() == 24000 + 2 * 24000);
}

TEST_CASE ("Stopping during the count-in records nothing and leaves no files", "[recording][engine]")
{
    Fixture f;
    f.engine.setCountInBars (1);
    REQUIRE (f.recorder->start().isEmpty());
    f.run (4096);
    CHECK_FALSE (f.engine.getRecordingStatus().recording);

    f.recorder->stop();
    f.run (block);
    f.recorder->poll();
    f.recorder->poll();
    CHECK_FALSE (f.recorder->isActive());
    CHECK (clips::allClips (f.project).isEmpty());
    CHECK (f.folder.getChildFile ("Audio").findChildFiles (juce::File::findFiles, false, "*.wav").isEmpty());
}

TEST_CASE ("Recording explains what's missing instead of failing silently", "[recording][engine]")
{
    Fixture f;
    edit::setArmed (f.project, { f.vocals }, false);
    CHECK (f.recorder->start().contains ("R button"));

    edit::setArmed (f.project, { f.vocals }, true);
    edit::setInput (f.project, { f.vocals }, 2, false);   // input 3, on a card with 2 inputs
    CHECK (f.recorder->start().contains ("only has 2 inputs"));
    CHECK_FALSE (f.recorder->isActive());
}

TEST_CASE ("Monitoring lets you hear the input through the track", "[recording][engine][monitoring]")
{
    Fixture f;
    f.run (block);
    CHECK (f.run (block)[10] == 0.0f);         // not monitoring: silence

    edit::setMonitoring (f.project, { f.vocals }, true);
    const auto heard = f.run (block);
    CHECK (heard[10] == micSample (f.clock - block + 10));

    edit::setVolume (f.project, f.vocals, mixer::silentDb);   // through the track's fader
    f.run (block * 4);
    CHECK (f.run (block)[10] == 0.0f);
}

TEST_CASE ("An armed track's meter shows its input level, even when not monitoring", "[recording][engine][meters]")
{
    Fixture f;
    MixerResources resources;
    f.sync = nullptr;   // drive the engine directly, with our own resources to read the meters from
    f.engine.setTimeline (buildTimelineSnapshot (f.project, rate, {}, &resources));
    f.run (block);
    CHECK (resources.takePeaks (f.vocals).first > 0.0f);     // armed: the microphone shows
    CHECK (f.run (block)[10] == 0.0f);                       // but isn't heard

    edit::setArmed (f.project, { f.vocals }, false);
    f.engine.setTimeline (buildTimelineSnapshot (f.project, rate, {}, &resources));
    f.run (block);
    resources.takePeaks (f.vocals);
    f.run (block);
    CHECK (resources.takePeaks (f.vocals).first == 0.0f);
}

//==============================================================================
namespace
{
    /** Runs a latency measurement through a simulated cable that delays the sound by `delay`
        samples (plus optional noise). */
    LatencyMeasurement measureThroughCable (int delay, float noise, float gain = 0.5f)
    {
        AudioEngine engine;
        engine.prepare (rate, block);
        auto capture = std::make_shared<LatencyCapture> (rate, 0);
        engine.startLatencyCapture (capture);

        std::vector<float> played;   // everything that came out of output 1
        std::vector<float> in (block), left (block), right (block);
        std::mt19937 random (42);
        std::uniform_real_distribution<float> hiss (-noise, noise);

        for (int guard = 0; guard < 2000 && ! capture->finished; ++guard)
        {
            const auto now = (std::int64_t) played.size();

            for (int i = 0; i < block; ++i)
            {
                const auto t = now + i - delay;
                in[(size_t) i] = (t >= 0 && t < (std::int64_t) played.size() ? played[(size_t) t] * gain : 0.0f) + hiss (random);
            }

            const float* inputs[] = { in.data() };
            float* outputs[] = { left.data(), right.data() };
            engine.process (inputs, 1, outputs, 2, block);
            played.insert (played.end(), left.begin(), left.end());
        }

        engine.collectGarbage();
        return analyseLatencyCapture (*capture);
    }
}

TEST_CASE ("Latency is measured to the exact sample", "[recording][latency]")
{
    // (A real round trip is always at least one block: sound can't come back in the block it was played in.)
    for (const int delay : { 300, 1234, 4800 })
    {
        INFO ("delay " << delay);
        const auto result = measureThroughCable (delay, 0.0f);
        REQUIRE (result.ok);
        CHECK (result.roundTripSamples == delay);
    }

    // Through speakers and a microphone: quieter, with room noise.
    const auto noisy = measureThroughCable (2000, 0.02f, 0.1f);
    REQUIRE (noisy.ok);
    CHECK (noisy.roundTripSamples == 2000);
}

TEST_CASE ("A measurement that hears nothing says so", "[recording][latency]")
{
    const auto result = measureThroughCable (500, 0.0f, 0.0f);
    CHECK_FALSE (result.ok);
    CHECK (result.problem.contains ("couldn't hear"));
}
