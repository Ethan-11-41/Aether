// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
//
// Performance with a big project: 50 tracks (36 audio tracks with 8 clips each, 10 MIDI tracks
// playing chords on the starter synth, 4 buses with reverb and delay), as Phase 1 promises.
//
// Each test prints how long things take. The time limits are only checked in optimised (Release)
// builds, which is what users run: a Debug build is many times slower and says little.

#include <iostream>

#include <catch2/catch_test_macros.hpp>

#include "engine/AudioEngine.h"
#include "engine/AudioFileCache.h"
#include "engine/TimelineSync.h"
#include "model/ClipEdits.h"
#include "model/EditCommands.h"
#include "model/MidiClips.h"
#include "model/Mixer.h"
#include "ui/MixerComponent.h"
#include "ui/TimelineComponent.h"
#include "ui/TrackListComponent.h"

using namespace aether;

namespace
{
   #if defined (NDEBUG)
    constexpr bool checkTimes = true;
   #else
    constexpr bool checkTimes = false;
   #endif

    constexpr double rate = 48000.0;
    constexpr int blockSize = 256;
    constexpr Tick bar = 4 * ticksPerQuarter;
    constexpr juce::int64 fourBars = 4 * 2 * 48000;   // 4 bars at 120 BPM: 8 seconds

    double nowMs()   { return juce::Time::getMillisecondCounterHiRes(); }

    /** Average and slowest time of `work`, run `times` times (after one warm-up run). */
    template <typename Work>
    std::pair<double, double> measure (int times, Work&& work)
    {
        work();
        double total = 0.0, slowest = 0.0;

        for (int i = 0; i < times; ++i)
        {
            const auto start = nowMs();
            work();
            const auto elapsed = nowMs() - start;
            total += elapsed;
            slowest = std::max (slowest, elapsed);
        }

        return { total / times, slowest };
    }

    void report (const juce::String& what, double averageMs, double slowestMs, const juce::String& extra = {})
    {
        std::cout << "[performance] " << what << ": " << juce::String (averageMs, 2) << " ms on average, slowest "
                  << juce::String (slowestMs, 2) << " ms" << extra << (checkTimes ? "" : " (Debug build: not checked)")
                  << std::endl;
    }

    std::shared_ptr<AudioData> makeSound (int seed)
    {
        auto data = std::make_shared<AudioData>();
        data->samples.setSize (2, (int) fourBars);
        juce::Random random (seed);

        for (int ch = 0; ch < 2; ++ch)
        {
            auto* d = data->samples.getWritePointer (ch);
            for (int i = 0; i < (int) fourBars; ++i)
                d[i] = 0.2f * std::sin ((float) i * 0.01f * (float) (seed + 1)) + 0.05f * (random.nextFloat() - 0.5f);
        }

        return data;
    }

    /** The 50-track project. */
    struct BigProject
    {
        BigProject()
        {
            auto bus = [this] (const juce::String& name, const juce::String& effect)
            {
                const auto id = edit::addTrack (project, TrackKind::bus, -1, false, name).getId();
                if (effect.isNotEmpty())
                    edit::addInsert (project, id, effect);
                return id;
            };

            const auto reverbBus = bus ("Reverb", "reverb");
            const auto delayBus = bus ("Echo", "delay");
            const auto drumsBus = bus ("Drums", {});
            const auto guitarsBus = bus ("Guitars", {});

            for (int i = 0; i < 6; ++i)
                files.set ("Audio/Take " + juce::String (i) + ".wav", makeSound (i));

            for (int i = 0; i < 36; ++i)
            {
                const auto id = edit::addTrack (project, TrackKind::audio, -1, false).getId();
                const auto file = "Audio/Take " + juce::String (i % 6) + ".wav";

                for (int c = 0; c < 8; ++c)
                {
                    const auto clip = edit::addAudioClip (project, id, { file, "Take", rate, fourBars }, c * 4 * bar, false);
                    edit::setClipFades (project, clip, 4800, 9600, false);
                }

                edit::addSend (project, id, reverbBus);
                if (i % 2 == 0)
                    edit::addSend (project, id, delayBus);
                if (i % 4 == 0)
                    edit::addInsert (project, id, "delay");

                edit::setOutput (project, { id }, i < 12 ? drumsBus : i < 24 ? guitarsBus : juce::String());
            }

            for (int i = 0; i < 10; ++i)
            {
                const auto id = edit::addTrack (project, TrackKind::midi, -1, false).getId();

                // Four-note chords on every eighth note, for 8 bars, twice.
                std::vector<NoteData> notes;
                for (int step = 0; step < 64; ++step)
                    for (int n = 0; n < 4; ++n)
                        notes.push_back ({ 48 + i + n * 4 + (step % 4), 90, step * ticksPerQuarter / 2, ticksPerQuarter / 2 });

                for (int c = 0; c < 2; ++c)
                    edit::addMidiClip (project, id, "Chords", c * 8 * bar, 8 * bar, notes, false);

                edit::addSend (project, id, reverbBus);
            }

            REQUIRE (project.getNumTracks() == 50);
        }

        AudioResolver resolver()
        {
            return [this] (const juce::String& file) { return files.contains (file) ? files[file] : nullptr; };
        }

        Project project;
        juce::HashMap<juce::String, std::shared_ptr<AudioData>> files;
    };
}

TEST_CASE ("The audio engine plays 50 tracks well within real time", "[performance][engine]")
{
    BigProject big;
    AudioEngine engine;
    MixerResources resources;
    engine.prepare (rate, blockSize);
    engine.setTimeline (buildTimelineSnapshot (big.project, rate, big.resolver(), &resources));
    engine.play();

    std::vector<float> left (blockSize), right (blockSize);
    float* outputs[] = { left.data(), right.data() };

    // 6 seconds of sound, one sound-card buffer at a time.
    const auto blocks = (int) (6.0 * rate / blockSize);
    const auto [average, slowest] = measure (blocks, [&] { engine.process (nullptr, 0, outputs, 2, blockSize); });
    engine.collectGarbage();

    const auto budget = 1000.0 * blockSize / rate;   // 5.3 ms
    report ("Engine, 50 tracks, one 256-sample buffer", average, slowest,
            " (" + juce::String (juce::roundToInt (100.0 * average / budget)) + "% of the " + juce::String (budget, 1)
                + " ms available)");

    // Something is playing (the test isn't measuring silence).
    CHECK (engine.takeOutputPeak (0) > 0.01f);

    if (checkTimes)
        CHECK (average < budget * 0.5);   // leaves plenty of room for the rest of the computer
}

TEST_CASE ("Changing a 50-track project updates the engine quickly", "[performance][engine]")
{
    BigProject big;
    MixerResources resources;

    // Runs on the UI thread after every edit (including each step of dragging a clip).
    const auto [average, slowest] = measure (20, [&] { buildTimelineSnapshot (big.project, rate, big.resolver(), &resources); });
    report ("Rebuilding the engine's copy of a 50-track project", average, slowest);

    if (checkTimes)
        CHECK (average < 15.0);
}

TEST_CASE ("The timeline, track list and mixer draw 50 tracks quickly", "[performance][ui]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    BigProject big;
    const auto theme = ui::Theme::dark();
    AudioEngine engine;
    AudioFileCache cache;

    ui::TrackListComponent trackList { theme, big.project };
    juce::Viewport viewport;
    viewport.setViewedComponent (&trackList, false);
    viewport.setBounds (0, 0, 300, 800);
    trackList.setSize (300, trackList.getIdealHeight());

    ui::TimelineComponent timeline (theme, big.project, engine, cache, [] { return ProjectLocation(); }, viewport, trackList);
    timeline.setBounds (0, 0, 1600, 800);

    ui::MixerComponent mixer (theme, big.project, trackList);
    mixer.setBounds (0, 0, 1900, 340);

    auto paintTime = [] (juce::Component& c)
    {
        juce::Image image (juce::Image::ARGB, c.getWidth(), c.getHeight(), true);
        return measure (10, [&]
        {
            juce::Graphics g (image);
            c.paintEntireComponent (g, false);
        });
    };

    const auto [timelineAverage, timelineSlowest] = paintTime (timeline);
    report ("Drawing the timeline (1600 x 800, 50 tracks, 308 clips)", timelineAverage, timelineSlowest);

    const auto [listAverage, listSlowest] = paintTime (viewport);
    report ("Drawing the track list", listAverage, listSlowest);

    const auto [mixerAverage, mixerSlowest] = paintTime (mixer);
    report ("Drawing the mixer (50 tracks)", mixerAverage, mixerSlowest);

    // The meters are updated 30 times a second.
    auto peaks = [] (const juce::String&) { return std::pair<float, float> { 0.5f, 0.4f }; };
    const auto [meterAverage, meterSlowest] = measure (30, [&]
    {
        trackList.updateMeters (peaks, 1.0f / 30.0f);
        mixer.updateMeters (peaks, 1.0f / 30.0f);
    });
    report ("Updating 100 meters", meterAverage, meterSlowest);

    if (checkTimes)
    {
        CHECK (timelineAverage < 25.0);
        CHECK (listAverage < 25.0);
        CHECK (mixerAverage < 25.0);
        CHECK (meterAverage < 2.0);
    }
}
