// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
//
// Clips that follow the tempo or are transposed, played by the engine (Milestone 17): they're
// stretched in the background (StretchCache) and played from the stretched copy.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include "engine/AudioEngine.h"
#include "engine/StretchCache.h"
#include "engine/TimelineSync.h"
#include "model/ClipEdits.h"
#include "model/EditCommands.h"
#include "model/TimelineEdits.h"
#include "model/Warp.h"

using namespace aether;

namespace
{
    constexpr double rate = 48000.0;
    constexpr Tick q = ticksPerQuarter;

    /** 4 seconds: a short hit on every beat at 120 BPM (every half second), silence between. */
    std::shared_ptr<AudioData> beatHits()
    {
        auto data = std::make_shared<AudioData>();
        data->sampleRate = data->sourceRate = rate;
        data->samples.setSize (2, 192000);
        data->samples.clear();
        juce::Random random (3);

        for (int beat = 0; beat < 8; ++beat)
            for (int i = 0; i < 2400; ++i)
                for (int c = 0; c < 2; ++c)
                    data->samples.setSample (c, beat * 24000 + i, (random.nextFloat() * 2.0f - 1.0f) * 0.8f * std::exp ((float) -i / 400.0f));

        return data;
    }

    std::shared_ptr<AudioData> sine (double hz, int length)
    {
        auto data = std::make_shared<AudioData>();
        data->sampleRate = data->sourceRate = rate;
        data->samples.setSize (2, length);
        for (int i = 0; i < length; ++i)
            for (int c = 0; c < 2; ++c)
                data->samples.setSample (c, i, 0.4f * (float) std::sin (juce::MathConstants<double>::twoPi * hz * i / rate));
        return data;
    }

    std::vector<float> play (std::unique_ptr<TimelineSnapshot> timeline, int total)
    {
        AudioEngine engine;
        engine.prepare (rate, 512);
        engine.setTimeline (std::move (timeline));
        engine.play();

        std::vector<float> out ((size_t) total), l (512), r (512);
        for (int done = 0; done < total; done += 512)
        {
            const auto n = std::min (512, total - done);
            float* outputs[] = { l.data(), r.data() };
            engine.process (nullptr, 0, outputs, 2, n);
            std::copy (l.begin(), l.begin() + n, out.begin() + done);
        }

        return out;
    }

    /** Where each hit starts (as in the stretcher's tests): loud after a quiet stretch. */
    std::vector<double> onsets (const std::vector<float>& x)
    {
        constexpr int window = 96;
        std::vector<float> levels;
        for (size_t i = 0; i + window < x.size(); i += window)
        {
            double sum = 0.0;
            for (size_t j = i; j < i + window; ++j)
                sum += (double) x[j] * x[j];
            levels.push_back ((float) std::sqrt (sum / window));
        }

        std::vector<double> result;
        for (size_t k = 0; k < levels.size(); ++k)
        {
            bool quietBefore = k >= 15;
            for (size_t j = k >= 65 ? k - 65 : 0; j + 15 < k; ++j)
                quietBefore = quietBefore && levels[j] < 0.02f;

            if (levels[k] > 0.05f && (quietBefore || result.empty()))
            {
                float loudest = 0.0f;
                for (size_t j = k; j < std::min (levels.size(), k + 50); ++j)
                    loudest = std::max (loudest, levels[j]);

                auto at = k;
                while (levels[at] < 0.5f * loudest)
                    ++at;

                result.push_back ((double) at * window / rate);
                k = at + 50;
            }
        }

        return result;
    }

    double frequency (const std::vector<float>& x, int from, int to)
    {
        int crossings = 0;
        for (int i = from + 1; i < to; ++i)
            if ((x[(size_t) i - 1] < 0.0f) != (x[(size_t) i] < 0.0f))
                ++crossings;
        return crossings / 2.0 / ((to - from) / rate);
    }

    struct Fixture
    {
        Fixture()
        {
            track = edit::addTrack (project, TrackKind::audio).getId();
            clipId = edit::addAudioClip (project, track, { "Audio/beats.wav", "beats", rate, 192000 }, 4 * q);   // at bar 2
        }

        std::unique_ptr<TimelineSnapshot> snapshot (bool stretch = true)
        {
            return buildTimelineSnapshot (project, rate, [this] (const juce::String&) { return audio; }, nullptr, {}, {}, {},
                                          stretch ? StretchResolver ([] (const StretchJob& job) { return job.render(); }) : StretchResolver());
        }

        Project project;
        juce::String track, clipId;
        std::shared_ptr<AudioData> audio = beatHits();
    };
}

TEST_CASE ("A clip that follows the tempo plays on the song's beats at another tempo", "[stretch][engine]")
{
    Fixture f;
    REQUIRE (edit::setFollowTempo (f.project, { f.clipId }, true));
    edit::setTempo (f.project, 0, 90.0);   // a beat is now 2/3 s; the clip starts at 4 beats: 2.667 s

    auto timeline = f.snapshot();
    REQUIRE (timeline->tracks.size() == 1);
    REQUIRE (timeline->tracks[0].clips.size() == 1);
    CHECK (timeline->tracks[0].clips[0].duration == Catch::Approx (8.0 * 2.0 / 3.0 * rate).margin (2.0));

    const auto out = play (std::move (timeline), (int) (11.0 * rate));
    const auto hits = onsets (out);
    REQUIRE (hits.size() == 8);

    for (size_t beat = 0; beat < hits.size(); ++beat)
    {
        INFO ("beat " << beat);
        CHECK (hits[beat] == Catch::Approx ((4.0 + (double) beat) * 2.0 / 3.0).margin (0.008));
    }

    // Without the stretched copy (not ready yet), it plays as it is, from the same place.
    const auto plain = onsets (play (f.snapshot (false), (int) (8.0 * rate)));
    REQUIRE (plain.size() == 8);
    CHECK (plain[1] - plain[0] == Catch::Approx (0.5).margin (0.004));
}

TEST_CASE ("A clip at its own tempo isn't stretched at all", "[stretch][engine]")
{
    Fixture f;
    REQUIRE (edit::setFollowTempo (f.project, { f.clipId }, true));

    int asked = 0;
    auto timeline = buildTimelineSnapshot (f.project, rate, [&f] (const juce::String&) { return f.audio; }, nullptr, {}, {}, {},
                                           [&asked] (const StretchJob& job) { ++asked; return job.render(); });
    CHECK (asked == 0);
    REQUIRE (timeline->tracks[0].clips.size() == 1);
    CHECK (timeline->tracks[0].clips[0].audio == f.audio.get());
}

TEST_CASE ("A transposed clip plays higher, at the same speed", "[stretch][engine]")
{
    Project project;
    const auto track = edit::addTrack (project, TrackKind::audio).getId();
    const auto clip = edit::addAudioClip (project, track, { "Audio/a440.wav", "a440", rate, 96000 }, 0);
    REQUIRE (edit::setClipTranspose (project, { clip }, 12.0));
    const auto a440 = sine (440.0, 96000);

    auto timeline = buildTimelineSnapshot (project, rate, [&a440] (const juce::String&) { return a440; }, nullptr, {}, {}, {},
                                           [] (const StretchJob& job) { return job.render(); });
    REQUIRE (timeline->tracks[0].clips.size() == 1);
    CHECK (timeline->tracks[0].clips[0].duration == 96000);

    const auto out = play (std::move (timeline), 96000);
    CHECK (frequency (out, 30000, 70000) == Catch::Approx (880.0).margin (8.0));
}

TEST_CASE ("Stretched clips are made in the background, and the newest one wins", "[stretch][cache]")
{
    Fixture f;
    REQUIRE (edit::setFollowTempo (f.project, { f.clipId }, true));
    edit::setTempo (f.project, 0, 100.0);

    StretchCache cache;
    const auto clip = clips::findClip (f.project, f.clipId);
    const auto job = StretchJob::make (clip, timeline::buildTempoMap (f.project), f.audio);

    // Asked the first time: nothing yet, it's being made.
    auto result = cache.get (job);
    CHECK (result.audio == nullptr);
    CHECK (cache.isPreparing (f.clipId));
    cache.waitUntilIdle();

    result = cache.get (job);
    REQUIRE (result.audio != nullptr);
    CHECK (result.exact);
    CHECK (result.audio->samples.getNumSamples() == (int) job.outputLength);
    CHECK_FALSE (cache.isBusy());

    // The same job again (another clip at the same tempo, or after an undo): the same result.
    CHECK (StretchJob::make (clip, timeline::buildTempoMap (f.project), f.audio).key == job.key);

    // Another tempo: meanwhile, the clip's previous result.
    edit::setTempo (f.project, 0, 80.0);
    const auto slower = StretchJob::make (clip, timeline::buildTempoMap (f.project), f.audio);
    CHECK (slower.key != job.key);
    result = cache.get (slower);
    CHECK (result.audio != nullptr);
    CHECK_FALSE (result.exact);
    cache.waitUntilIdle();
    CHECK (cache.get (slower).exact);

    // Forgetting what isn't used any more.
    cache.keepOnly ({ slower.key });
    CHECK (cache.getNumEntries() == 1);
}
