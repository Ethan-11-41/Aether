// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
//
// Clip playback in the audio engine: every sample of a clip must come out at exactly the right
// time and level, whatever the block size, looping, fades or edits.

#include <cmath>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "engine/AudioEngine.h"
#include "engine/TimelineSync.h"
#include "model/ClipEdits.h"
#include "model/EditCommands.h"
#include "model/TimelineEdits.h"

using namespace aether;
using Catch::Matchers::WithinAbs;

namespace
{
    constexpr double rate = 48000.0;
    constexpr Tick q = ticksPerQuarter;   // 24000 samples at 120 BPM

    /** Audio whose sample i is (i + 1) / 100000 on the left and the negative on the right, so
        every sample is different and it's easy to tell which one came out. */
    std::shared_ptr<AudioData> makeRamp (int length, int channels = 2)
    {
        auto data = std::make_shared<AudioData>();
        data->sampleRate = rate;
        data->sourceRate = rate;
        data->samples.setSize (channels, length);
        for (int i = 0; i < length; ++i)
            for (int c = 0; c < channels; ++c)
                data->samples.setSample (c, i, (c == 0 ? 1.0f : -1.0f) * (float) (i + 1) / 100000.0f);
        return data;
    }

    struct Rendered
    {
        std::vector<float> left, right;
    };

    Rendered render (AudioEngine& engine, int total, int blockSize)
    {
        Rendered r { std::vector<float> ((size_t) total), std::vector<float> ((size_t) total) };
        std::vector<float> l ((size_t) blockSize), rr ((size_t) blockSize);

        for (int start = 0; start < total; start += blockSize)
        {
            const auto n = std::min (blockSize, total - start);
            float* out[] = { l.data(), rr.data() };
            engine.process (nullptr, 0, out, 2, n);
            std::copy (l.begin(), l.begin() + n, r.left.begin() + start);
            std::copy (rr.begin(), rr.begin() + n, r.right.begin() + start);
        }

        return r;
    }

    /** Plays a single-clip timeline from the start and renders it. */
    Rendered play (const ClipPlayback& clip, std::shared_ptr<AudioData> audio, int total, int blockSize = 512)
    {
        AudioEngine engine;
        engine.prepare (rate, blockSize);

        auto timeline = std::make_unique<TimelineSnapshot>();
        timeline->sampleRate = rate;
        TrackPlayback track;
        track.clips.push_back (clip);
        track.clips.back().audio = audio.get();
        timeline->tracks.push_back (track);
        timeline->keepAlive.push_back (audio);
        engine.setTimeline (std::move (timeline));
        engine.play();
        return render (engine, total, blockSize);
    }

    ClipPlayback basicClip (std::int64_t start, std::int64_t length)
    {
        ClipPlayback c;
        c.start = start;
        c.duration = length;
        c.contentLength = length;
        return c;
    }

    float rampValue (std::int64_t index)   { return (float) (index + 1) / 100000.0f; }
}

TEST_CASE ("A clip's samples come out at exactly the right time, at any block size", "[engine][clips]")
{
    const auto audio = makeRamp (5000);

    for (const int blockSize : { 1, 64, 333, 480, 2048 })
    {
        const auto out = play (basicClip (1000, 5000), audio, 8000, blockSize);
        INFO ("block size " << blockSize);

        CHECK (out.left[999] == 0.0f);               // silence before the clip
        CHECK (out.left[1000] == rampValue (0));      // its first sample
        CHECK (out.left[3500] == rampValue (2500));
        CHECK (out.left[5999] == rampValue (4999));   // its last sample
        CHECK (out.left[6000] == 0.0f);               // silence after
        CHECK (out.right[3500] == -rampValue (2500)); // right channel to right
    }
}

TEST_CASE ("A clip can start part-way into its file", "[engine][clips]")
{
    const auto audio = makeRamp (5000);
    auto clip = basicClip (0, 1000);
    clip.offset = 2000;

    const auto out = play (clip, audio, 2000);
    CHECK (out.left[0] == rampValue (2000));
    CHECK (out.left[999] == rampValue (2999));
    CHECK (out.left[1000] == 0.0f);
}

TEST_CASE ("A looping clip repeats its content", "[engine][clips][loop]")
{
    const auto audio = makeRamp (5000);
    auto clip = basicClip (0, 3500);
    clip.loop = true;
    clip.offset = 100;
    clip.contentLength = 1000;
    clip.phase = 250;

    const auto out = play (clip, audio, 4000);
    CHECK (out.left[0] == rampValue (100 + 250));
    CHECK (out.left[749] == rampValue (100 + 999));   // end of the content...
    CHECK (out.left[750] == rampValue (100));          // ...and round again
    CHECK (out.left[1750] == rampValue (100));
    CHECK (out.left[3499] == rampValue (100 + (250 + 3499) % 1000));
    CHECK (out.left[3500] == 0.0f);
}

TEST_CASE ("Clip volume and fades shape the level sample by sample", "[engine][clips][fades]")
{
    auto audio = std::make_shared<AudioData>();
    audio->sampleRate = rate;
    audio->samples.setSize (1, 4000);
    audio->samples.clear();
    for (int i = 0; i < 4000; ++i)
        audio->samples.setSample (0, i, 0.5f);

    auto clip = basicClip (0, 4000);
    clip.gain = 0.5f;
    clip.fadeIn = 1000;
    clip.fadeOut = 1000;
    clip.fadeInCurve = FadeCurve::linear;
    clip.fadeOutCurve = FadeCurve::linear;

    const auto out = play (clip, audio, 4000);
    CHECK (out.left[0] == 0.0f);                                          // fade-in starts silent
    CHECK_THAT (out.left[500], WithinAbs (0.25 * 0.5, 1e-6));             // half way through the fade
    CHECK_THAT (out.left[2000], WithinAbs (0.25, 1e-6));                  // full: 0.5 x gain 0.5
    CHECK_THAT (out.left[3499], WithinAbs (0.25 * 500.0 / 1000.0, 1e-3)); // half way through the fade-out
    CHECK (out.left[3999] == 0.0f);                                       // ends silent
    CHECK (out.right[2000] == out.left[2000]);                            // a mono file plays on both sides
}

TEST_CASE ("Overlapping clips are mixed together", "[engine][clips]")
{
    const auto audio = makeRamp (2000);

    AudioEngine engine;
    engine.prepare (rate, 256);
    auto timeline = std::make_unique<TimelineSnapshot>();
    timeline->sampleRate = rate;

    TrackPlayback trackA, trackB;
    trackA.clips.push_back (basicClip (0, 2000));
    trackA.clips.push_back (basicClip (1000, 2000));    // overlaps the first on the same track
    trackB.clips.push_back (basicClip (500, 2000));     // and one on another track
    for (auto* t : { &trackA, &trackB })
        for (auto& c : t->clips)
            c.audio = audio.get();

    timeline->tracks = { trackA, trackB };
    timeline->keepAlive.push_back (audio);
    engine.setTimeline (std::move (timeline));
    engine.play();

    const auto out = render (engine, 4000, 256);
    CHECK_THAT (out.left[1500], WithinAbs (rampValue (1500) + rampValue (500) + rampValue (1000), 1e-6));
}

TEST_CASE ("Clips play correctly through the transport loop", "[engine][clips][loop]")
{
    const auto audio = makeRamp (100000);

    AudioEngine engine;
    engine.prepare (rate, 700);
    auto timeline = std::make_unique<TimelineSnapshot>();
    timeline->sampleRate = rate;
    timeline->loopEnabled = true;
    timeline->loopStart = q;        // samples 24000..48000
    timeline->loopEnd = 2 * q;
    TrackPlayback track;
    track.clips.push_back (basicClip (0, 100000));
    track.clips.back().audio = audio.get();
    timeline->tracks.push_back (track);
    timeline->keepAlive.push_back (audio);
    engine.setTimeline (std::move (timeline));
    engine.locate (q);
    engine.play();

    const auto out = render (engine, 30000, 700);
    CHECK (out.left[0] == rampValue (24000));
    CHECK (out.left[23999] == rampValue (47999));
    CHECK (out.left[24000] == rampValue (24000));   // jumped back exactly
}

TEST_CASE ("A snapshot made for another sample rate stays silent", "[engine][clips]")
{
    const auto audio = makeRamp (1000);

    AudioEngine engine;
    engine.prepare (44100.0, 256);   // the device is running at 44.1 kHz...
    auto timeline = std::make_unique<TimelineSnapshot>();
    timeline->sampleRate = rate;     // ...but the timeline was worked out for 48 kHz
    TrackPlayback track;
    track.clips.push_back (basicClip (0, 1000));
    track.clips.back().audio = audio.get();
    timeline->tracks.push_back (track);
    timeline->keepAlive.push_back (audio);
    engine.setTimeline (std::move (timeline));
    engine.play();

    const auto out = render (engine, 1000, 256);
    CHECK (std::all_of (out.left.begin(), out.left.end(), [] (float v) { return v == 0.0f; }));
}

//==============================================================================
namespace
{
    /** A project with one 2-second clip, and the engine playing it. */
    struct ProjectFixture
    {
        ProjectFixture()
        {
            trackId = edit::addTrack (project, TrackKind::audio).getId();
            clipId = edit::addAudioClip (project, trackId, { "Audio/ramp.wav", "ramp", rate, 96000 }, q);
            edit::setClipFades (project, clipId, 2000, 3000);
        }

        Rendered renderProject (int total, int blockSize = 512)
        {
            AudioEngine engine;
            engine.prepare (rate, blockSize);
            engine.setTimeline (buildTimelineSnapshot (project, rate, [this] (const juce::String&) { return audio; }));
            engine.play();
            return render (engine, total, blockSize);
        }

        Project project;
        juce::String trackId, clipId;
        std::shared_ptr<AudioData> audio = makeRamp (96000);
    };
}

TEST_CASE ("The engine plays a project's clips where the project says", "[engine][clips][project]")
{
    ProjectFixture f;
    const auto out = f.renderProject (24000 + 96000 + 100);

    CHECK (out.left[23999] == 0.0f);
    CHECK (out.left[24000] == 0.0f);                       // the fade-in starts at silence
    CHECK (out.left[24000 + 10000] == rampValue (10000));  // after the fade: the file, unchanged
    CHECK (out.left[24000 + 96000] == 0.0f);
}

TEST_CASE ("Splitting a clip doesn't change what you hear", "[engine][clips][project]")
{
    ProjectFixture f;
    const auto before = f.renderProject (130000, 480);

    REQUIRE (edit::splitClip (f.project, f.clipId, 3 * q + q / 3).isNotEmpty());
    const auto after = f.renderProject (130000, 480);

    CHECK (after.left == before.left);
    CHECK (after.right == before.right);
}

TEST_CASE ("Moving a clip moves its sound by exactly that much", "[engine][clips][project]")
{
    ProjectFixture f;
    const auto before = f.renderProject (200000);

    edit::setClipPosition (f.project, f.clipId, 3 * q, f.trackId);   // two beats = 48000 samples later
    const auto after = f.renderProject (200000);

    for (int i = 0; i < 120000; ++i)
        REQUIRE (after.left[(size_t) (i + 48000)] == before.left[(size_t) i]);
}

TEST_CASE ("Clips follow the tempo: a slower tempo starts them later in time", "[engine][clips][project]")
{
    ProjectFixture f;
    edit::setTempo (f.project, 0, 60.0);   // one beat is now 1 s, so the clip at beat 2 starts at 48000

    const auto out = f.renderProject (60000);
    CHECK (out.left[47999] == 0.0f);
    CHECK (out.left[48000 + 5000] == rampValue (5000));
}
