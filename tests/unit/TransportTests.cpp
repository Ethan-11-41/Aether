// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
//
// The transport and metronome, driven like a sound card would drive them. The key check: every
// click starts on exactly the right sample, whatever the block size, tempo changes or looping.

#include <atomic>
#include <cmath>
#include <thread>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "engine/AudioEngine.h"
#include "engine/rt/SnapshotExchange.h"

using namespace aether;

namespace
{
    constexpr double rate = 48000.0;
    constexpr Tick q = ticksPerQuarter;

    /** Renders `totalSamples` of mono output in blocks of `blockSize`. */
    std::vector<float> render (AudioEngine& engine, int totalSamples, int blockSize)
    {
        std::vector<float> out ((size_t) totalSamples, 0.0f);
        std::vector<float> block ((size_t) blockSize);

        for (int start = 0; start < totalSamples; start += blockSize)
        {
            const auto n = std::min (blockSize, totalSamples - start);
            float* channels[] = { block.data() };
            engine.process (nullptr, 0, channels, 1, n);
            std::copy (block.begin(), block.begin() + n, out.begin() + start);
        }

        return out;
    }

    /** Sample positions where a click starts: a non-silent sample after at least 1000 silent ones. */
    std::vector<int> clickStarts (const std::vector<float>& audio)
    {
        std::vector<int> starts;
        int silence = 1000;

        for (size_t i = 0; i < audio.size(); ++i)
        {
            if (std::abs (audio[i]) > 1.0e-6f)
            {
                if (silence >= 1000)
                    starts.push_back ((int) i);
                silence = 0;
            }
            else
            {
                ++silence;
            }
        }

        return starts;
    }

    std::unique_ptr<TimelineSnapshot> timelineWith (TempoMap tempo, bool loop = false, Tick loopStart = 0, Tick loopEnd = 0)
    {
        auto t = std::make_unique<TimelineSnapshot>();
        t->tempo = std::move (tempo);
        t->loopEnabled = loop;
        t->loopStart = loopStart;
        t->loopEnd = loopEnd;
        return t;
    }

    void startEngine (AudioEngine& engine, std::unique_ptr<TimelineSnapshot> timeline = nullptr)
    {
        engine.prepare (rate, 1024);
        if (timeline != nullptr)
            engine.setTimeline (std::move (timeline));
        engine.setMetronomeEnabled (true);
        engine.play();
    }
}

TEST_CASE ("Metronome clicks land on exactly the right sample at any block size", "[engine][transport]")
{
    for (const int blockSize : { 1, 31, 64, 480, 1000, 4096 })
    {
        AudioEngine engine;
        startEngine (engine);   // default: 120 BPM, 4/4 -> a beat every 24000 samples

        const auto starts = clickStarts (render (engine, 4 * 24000 + 100, blockSize));
        INFO ("block size " << blockSize);
        REQUIRE (starts.size() == 5);

        for (size_t i = 0; i < starts.size(); ++i)
            CHECK (starts[i] == (int) i * 24000);
    }
}

TEST_CASE ("The first beat of each bar has the accented click", "[engine][transport]")
{
    AudioEngine engine;
    startEngine (engine);
    const auto audio = render (engine, 4 * 24000 + 100, 512);

    // The accent starts louder than the other beats.
    CHECK (std::abs (audio[0]) > std::abs (audio[24000]) * 1.3f);
    CHECK (std::abs (audio[96000]) > std::abs (audio[72000]) * 1.3f);
}

TEST_CASE ("Clicks follow tempo changes and time signature changes", "[engine][transport]")
{
    // 60 BPM for the first bar (4 beats), then 120 BPM; 3/4 from bar 2.
    TempoMap tempo ({ { 0, 60.0, TempoCurve::jump }, { 4 * q, 120.0, TempoCurve::jump } }, { { 0, 4, 4 }, { 1, 3, 4 } });

    AudioEngine engine;
    startEngine (engine, timelineWith (tempo));
    const auto starts = clickStarts (render (engine, (int) (rate * 6.1), 333));

    std::vector<int> expected;
    for (Tick t = 0; t <= 8 * q; t += q)
        expected.push_back ((int) std::llround (tempo.ticksToSeconds (t) * rate));
    expected.resize (std::min<size_t> (expected.size(), starts.size()));

    CHECK (starts == expected);
    CHECK (starts[1] == 48000);       // 1 s per beat at 60 BPM
    CHECK (starts[5] - starts[4] == 24000);
}

TEST_CASE ("Clicks follow a tempo ramp exactly", "[engine][transport]")
{
    TempoMap tempo ({ { 0, 80.0, TempoCurve::ramp }, { 8 * q, 160.0, TempoCurve::jump } }, {});

    AudioEngine engine;
    startEngine (engine, timelineWith (tempo));
    const auto starts = clickStarts (render (engine, (int) std::ceil (tempo.ticksToSeconds (10 * q) * rate) + 10, 256));

    REQUIRE (starts.size() == 11);
    for (size_t i = 0; i < starts.size(); ++i)
        CHECK (starts[i] == (int) std::llround (tempo.ticksToSeconds ((Tick) i * q) * rate));
}

TEST_CASE ("Looping jumps back exactly at the loop end", "[engine][transport][loop]")
{
    // Loop beats 2-3 (one beat = 24000 samples at 120 BPM).
    AudioEngine engine;
    startEngine (engine, timelineWith (TempoMap(), true, 1 * q, 3 * q));
    engine.locate (q);
    const auto starts = clickStarts (render (engine, 24000 * 6 + 10, 700));

    // Beat 2, beat 3, then back to beat 2 ... every 24000 samples, never skipping or doubling.
    REQUIRE (starts.size() == 7);
    for (size_t i = 0; i < starts.size(); ++i)
        CHECK (starts[i] == (int) i * 24000);

    // The position stays inside the loop.
    const auto position = engine.getTransportStatus().position;
    CHECK (position >= q);
    CHECK (position < 3 * q);
}

TEST_CASE ("A playhead after the loop end plays on normally", "[engine][transport][loop]")
{
    AudioEngine engine;
    startEngine (engine, timelineWith (TempoMap(), true, 0, 2 * q));
    engine.locate (4 * q);
    render (engine, 48000, 512);
    CHECK (engine.getTransportStatus().position == 6 * q);
}

TEST_CASE ("Count-in clicks a bar before playback starts", "[engine][transport][countin]")
{
    AudioEngine engine;
    engine.prepare (rate, 512);
    engine.setMetronomeEnabled (true);
    engine.setCountInBars (1);
    engine.locate (8 * q);   // start at bar 3
    engine.play();

    const auto audio = render (engine, 24000 * 4 + 24000 + 10, 512);
    const auto starts = clickStarts (audio);

    // 4 count-in clicks, then playback clicks from the downbeat of bar 3 on, with no gap.
    REQUIRE (starts.size() == 6);
    for (size_t i = 0; i < starts.size(); ++i)
        CHECK (starts[i] == (int) i * 24000);

    // The playhead only started moving after the count-in.
    const auto status = engine.getTransportStatus();
    CHECK (status.playing);
    CHECK_FALSE (status.countingIn);
    CHECK (std::abs (status.position - (8 * q + q)) <= q / 1000);
}

TEST_CASE ("Count-in clicks even when the metronome is off", "[engine][transport][countin]")
{
    AudioEngine engine;
    engine.prepare (rate, 512);
    engine.setMetronomeEnabled (false);
    engine.setCountInBars (2);
    engine.play();

    const auto starts = clickStarts (render (engine, 24000 * 8 + 24000, 512));
    CHECK (starts.size() == 8);   // two bars of count-in, then silence
    CHECK (engine.getTransportStatus().playing);
}

TEST_CASE ("Stop, play and locate", "[engine][transport]")
{
    AudioEngine engine;
    engine.prepare (rate, 512);

    CHECK_FALSE (engine.getTransportStatus().playing);
    render (engine, 4800, 512);
    CHECK (engine.getTransportStatus().position == 0);   // stopped: doesn't move

    engine.play();
    render (engine, 24000, 512);
    CHECK (engine.getTransportStatus().playing);
    CHECK (engine.getTransportStatus().position == q);    // half a second at 120 BPM

    engine.stop();
    render (engine, 24000, 512);
    CHECK_FALSE (engine.getTransportStatus().playing);
    CHECK (engine.getTransportStatus().position == q);    // stays where it stopped

    engine.locate (16 * q);
    CHECK (engine.getTransportStatus().position == 16 * q);   // shown straight away
    render (engine, 512, 512);
    CHECK (engine.getTransportStatus().position == 16 * q);

    engine.locate (-q);                                   // can't go before the start
    render (engine, 512, 512);
    CHECK (engine.getTransportStatus().position == 0);
}

TEST_CASE ("Changing the sample rate keeps the playhead at the same moment", "[engine][transport]")
{
    AudioEngine engine;
    engine.prepare (44100.0, 512);
    engine.locate (8 * q);
    render (engine, 512, 512);

    engine.prepare (96000.0, 512);
    render (engine, 512, 512);
    CHECK (std::abs (engine.getTransportStatus().position - 8 * q) <= 20);
}

TEST_CASE ("Metronome volume and count-in settings are kept within range", "[engine][transport]")
{
    AudioEngine engine;
    engine.setMetronomeLevelDb (100.0f);
    CHECK (engine.getMetronomeLevelDb() == 6.0f);
    engine.setCountInBars (99);
    CHECK (engine.getCountInBars() == AudioEngine::maxCountInBars);
    engine.setCountInBars (-1);
    CHECK (engine.getCountInBars() == 0);
}

//==============================================================================
namespace
{
    struct Counted
    {
        static inline std::atomic<int> alive { 0 };
        explicit Counted (int v) : value (v) { ++alive; }
        ~Counted() { --alive; }
        int value;
    };
}

TEST_CASE ("SnapshotExchange hands over the newest data and frees the old on the UI thread", "[rt][snapshot]")
{
    {
        rt::SnapshotExchange<Counted> exchange (std::make_unique<Counted> (1), 4);
        CHECK (exchange.acquire().value == 1);

        exchange.publish (std::make_unique<Counted> (2));
        exchange.publish (std::make_unique<Counted> (3));   // replaces 2 before the audio thread saw it
        CHECK (Counted::alive == 2);                          // 1 (in use) and 3 (waiting)

        CHECK (exchange.acquire().value == 3);
        CHECK (Counted::alive == 2);                          // 1 is waiting to be collected
        exchange.collectGarbage();
        CHECK (Counted::alive == 1);
    }

    CHECK (Counted::alive == 0);
}

TEST_CASE ("SnapshotExchange keeps working if the UI thread is slow to collect", "[rt][snapshot]")
{
    {
        rt::SnapshotExchange<Counted> exchange (std::make_unique<Counted> (0), 2);

        for (int i = 1; i <= 10; ++i)
        {
            exchange.publish (std::make_unique<Counted> (i));
            exchange.acquire();   // after 2 swaps the garbage queue is full: it keeps the current one
        }

        CHECK (exchange.current().value == 2);
        exchange.collectGarbage();
        CHECK (exchange.acquire().value == 10);   // catches up once there is room
    }

    CHECK (Counted::alive == 0);
}

TEST_CASE ("SnapshotExchange is safe with the UI and audio threads running at once", "[rt][snapshot][threads]")
{
    {
        rt::SnapshotExchange<Counted> exchange (std::make_unique<Counted> (0));
        std::atomic<bool> done { false };
        int lastSeen = 0;
        bool alwaysIncreasing = true;

        std::thread audio ([&]
        {
            while (! done.load())
            {
                const auto v = exchange.acquire().value;
                alwaysIncreasing = alwaysIncreasing && v >= lastSeen;
                lastSeen = v;
            }
        });

        for (int i = 1; i <= 20000; ++i)
        {
            exchange.publish (std::make_unique<Counted> (i));
            if (i % 16 == 0)
                exchange.collectGarbage();
        }

        done = true;
        audio.join();
        CHECK (alwaysIncreasing);
    }

    CHECK (Counted::alive == 0);
}
