// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
//
// The mixer in the audio engine: volume, pan, mute, solo, buses, sends, the master, effects and
// meters, checked on the actual sound that comes out.

#include <cmath>
#include <numeric>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "dsp/Effect.h"
#include "engine/AudioEngine.h"
#include "engine/MixerRouting.h"
#include "engine/TimelineSync.h"
#include "model/ClipEdits.h"
#include "model/EditCommands.h"
#include "model/Mixer.h"

using namespace aether;
using Catch::Matchers::WithinAbs;

//==============================================================================
TEST_CASE ("Buses are mixed after everything that plays into them", "[engine][mixer][routing]")
{
    // 0: track -> bus 2;  1: track -> bus 3;  2: bus -> bus 3;  3: bus -> master (4)
    const auto r = routing::compute (4, { { 0, 2 }, { 1, 3 }, { 2, 3 }, { 3, 4 } }, {});
    REQUIRE (r.order.size() == 4);

    auto position = [&] (int channel) { return std::find (r.order.begin(), r.order.end(), channel) - r.order.begin(); };
    CHECK (position (0) < position (2));
    CHECK (position (2) < position (3));
    CHECK (position (1) < position (3));
    CHECK (std::all_of (r.audible.begin(), r.audible.end(), [] (bool a) { return a; }));
}

TEST_CASE ("A loop in a hand-edited file is broken instead of hanging", "[engine][mixer][routing]")
{
    const auto r = routing::compute (2, { { 0, 1 }, { 1, 0 } }, {});
    CHECK (r.order.size() == 2);
    CHECK (std::count (r.kept.begin(), r.kept.end(), false) == 1);
}

TEST_CASE ("Solo lets you hear the soloed tracks, their buses, and what plays into them", "[engine][mixer][solo]")
{
    // Channels: 0 drums, 1 bass, 2 guitar, 3 group bus, 4 reverb bus; master = 5.
    const std::vector<routing::Connection> connections {
        { 0, 3 },   // 0: drums -> group
        { 1, 3 },   // 1: bass -> group
        { 2, 5 },   // 2: guitar -> master
        { 0, 4 },   // 3: drums send -> reverb
        { 2, 4 },   // 4: guitar send -> reverb
        { 3, 5 },   // 5: group -> master
        { 4, 5 },   // 6: reverb -> master
    };

    auto audible = [&] (std::vector<bool> soloed) { return routing::compute (5, connections, soloed).audible; };

    SECTION ("Soloing the drums: the drums through the group, and the drums' reverb")
    {
        const auto a = audible ({ true, false, false, false, false });
        CHECK (a == std::vector<bool> { true, false, false, true, false, true, true });
    }

    SECTION ("Soloing the group: everything in the group, but not their reverb")
    {
        const auto a = audible ({ false, false, false, true, false });
        CHECK (a == std::vector<bool> { true, true, false, false, false, true, false });
    }

    SECTION ("Soloing the reverb: just the reverb (the tracks still feed it)")
    {
        const auto a = audible ({ false, false, false, false, true });
        CHECK (a == std::vector<bool> { false, false, false, true, true, false, true });
    }

    SECTION ("Soloing the guitar and the bass")
    {
        const auto a = audible ({ false, true, true, false, false });
        CHECK (a == std::vector<bool> { false, true, true, false, true, true, true });
    }
}

//==============================================================================
namespace
{
    constexpr double rate = 48000.0;   // a quarter note is 24000 samples at 120 BPM

    std::shared_ptr<AudioData> makeAudio (int length, std::function<float (int)> sample)
    {
        auto data = std::make_shared<AudioData>();
        data->sampleRate = data->sourceRate = rate;
        data->samples.setSize (1, length);

        for (int i = 0; i < length; ++i)
            data->samples.setSample (0, i, sample (i));

        return data;
    }

    struct Output
    {
        std::vector<float> left, right;

        float maxAbs (size_t from, size_t to, bool useLeft = true) const
        {
            const auto& v = useLeft ? left : right;
            float m = 0.0f;
            for (auto i = from; i < std::min (to, v.size()); ++i)
                m = std::max (m, std::abs (v[i]));
            return m;
        }

        float sum (size_t from, size_t to) const
        {
            return std::accumulate (left.begin() + (long) from, left.begin() + (long) std::min (to, left.size()), 0.0f);
        }
    };

    /** A project whose tracks play a constant 0.5 (or another sound) from the very start, and an
        engine playing it. */
    struct Fixture
    {
        Fixture()
        {
            engine.prepare (rate, 256);
        }

        juce::String addTrackWithSound (const juce::String& name)
        {
            const auto id = edit::addTrack (project, TrackKind::audio, -1, true, name).getId();
            edit::addAudioClip (project, id, { "Audio/" + name + ".wav", name, rate, 480000 }, 0);
            files.set ("Audio/" + name + ".wav", constant);
            return id;
        }

        void update()
        {
            engine.setTimeline (buildTimelineSnapshot (project, rate, [this] (const juce::String& file)
            {
                return files.contains (file) ? files[file] : nullptr;
            }, &resources));
        }

        Output render (int total, int blockSize = 256)
        {
            Output out { std::vector<float> ((size_t) total), std::vector<float> ((size_t) total) };
            std::vector<float> l ((size_t) blockSize), r ((size_t) blockSize);

            for (int start = 0; start < total; start += blockSize)
            {
                const auto n = std::min (blockSize, total - start);
                float* outputs[] = { l.data(), r.data() };
                engine.process (nullptr, 0, outputs, 2, n);
                std::copy (l.begin(), l.begin() + n, out.left.begin() + start);
                std::copy (r.begin(), r.begin() + n, out.right.begin() + start);
            }

            engine.collectGarbage();
            return out;
        }

        Output play (int total)
        {
            update();
            engine.play();
            return render (total);
        }

        Project project;
        AudioEngine engine;
        MixerResources resources;
        std::shared_ptr<AudioData> constant = makeAudio (480000, [] (int) { return 0.5f; });
        juce::HashMap<juce::String, std::shared_ptr<AudioData>> files;
    };
}

TEST_CASE ("Volume and pan change the level of each side", "[engine][mixer]")
{
    Fixture f;
    const auto track = f.addTrackWithSound ("a");

    SECTION ("0 dB, centre: unchanged")
    {
        const auto out = f.play (1000);
        CHECK (out.left[500] == 0.5f);
        CHECK (out.right[500] == 0.5f);
    }

    SECTION ("-6 dB: half the level")
    {
        edit::setVolume (f.project, track, -6.0206);
        const auto out = f.play (1000);
        CHECK_THAT (out.left[500], WithinAbs (0.25, 1.0e-4));
    }

    SECTION ("Silent")
    {
        edit::setVolume (f.project, track, mixer::silentDb);
        CHECK (f.play (1000).maxAbs (600, 1000) == 0.0f);   // after the 10 ms fade
    }

    SECTION ("Panned fully right: the left side is silent, the right unchanged")
    {
        edit::setPan (f.project, track, 1.0);
        const auto out = f.play (1000);
        CHECK (out.left[500] == 0.0f);
        CHECK (out.right[500] == 0.5f);
    }

    SECTION ("Panned half left: the right side is turned down by 3 dB")
    {
        edit::setPan (f.project, track, -0.5);
        const auto out = f.play (1000);
        CHECK (out.left[500] == 0.5f);
        CHECK_THAT (out.right[500], WithinAbs (0.5 * std::sqrt (0.5), 1.0e-5));
    }

    SECTION ("The master volume")
    {
        edit::setVolume (f.project, mixer::masterId, -6.0206);
        CHECK_THAT (f.play (1000).left[500], WithinAbs (0.25, 1.0e-4));
    }
}

TEST_CASE ("Volume and mute changes fade over 10 ms instead of clicking", "[engine][mixer]")
{
    Fixture f;
    const auto track = f.addTrackWithSound ("a");
    f.play (1000);

    edit::setMuted (f.project, { track }, true);
    f.update();
    const auto out = f.render (1000);

    CHECK (out.left[0] > 0.49f);           // still sounding at first...
    CHECK (out.left[600] == 0.0f);         // ...silent after 10 ms (480 samples)

    // Each sample is a little quieter than the one before: no jump.
    for (size_t i = 1; i < 480; ++i)
    {
        REQUIRE (out.left[i] <= out.left[i - 1]);
        REQUIRE (out.left[i - 1] - out.left[i] < 0.002f);
    }
}

TEST_CASE ("Mute and solo choose which tracks are heard", "[engine][mixer][solo]")
{
    Fixture f;
    const auto a = f.addTrackWithSound ("a");
    const auto b = f.addTrackWithSound ("b");
    f.files.set ("Audio/b.wav", makeAudio (480000, [] (int) { return 0.25f; }));

    CHECK_THAT (f.play (1000).left[500], WithinAbs (0.75, 1.0e-6));

    edit::setMuted (f.project, { a }, true);
    CHECK_THAT (f.play (1000).left[500], WithinAbs (0.25, 1.0e-6));

    edit::setMuted (f.project, { a }, false);
    edit::setSoloed (f.project, { a }, true);
    CHECK_THAT (f.play (1000).left[500], WithinAbs (0.5, 1.0e-6));

    edit::setSoloed (f.project, { b }, true);   // both soloed: both heard
    CHECK_THAT (f.play (1000).left[500], WithinAbs (0.75, 1.0e-6));
}

TEST_CASE ("Tracks play through buses, with the bus's own volume and mute", "[engine][mixer][routing]")
{
    Fixture f;
    const auto a = f.addTrackWithSound ("a");
    const auto b = f.addTrackWithSound ("b");
    const auto bus = edit::addTrack (f.project, TrackKind::bus).getId();
    edit::setOutput (f.project, { a, b }, bus);

    CHECK_THAT (f.play (1000).left[500], WithinAbs (1.0, 1.0e-6));

    edit::setVolume (f.project, bus, -6.0206);
    CHECK_THAT (f.play (1000).left[500], WithinAbs (0.5, 1.0e-4));

    edit::setMuted (f.project, { bus }, true);
    CHECK (f.play (1000).maxAbs (600, 1000) == 0.0f);   // after the 10 ms fade

    // A bus can play into another bus.
    edit::setMuted (f.project, { bus }, false);
    const auto bus2 = edit::addTrack (f.project, TrackKind::bus).getId();
    edit::moveTrack (f.project, bus2, 0);   // listed first, but still mixed after bus 1
    edit::setOutput (f.project, { bus }, bus2);
    edit::setVolume (f.project, bus2, -6.0206);
    CHECK_THAT (f.play (1000).left[500], WithinAbs (0.25, 1.0e-4));
}

TEST_CASE ("Sends copy a track's sound to a bus, before or after its fader", "[engine][mixer][routing]")
{
    Fixture f;
    const auto a = f.addTrackWithSound ("a");
    const auto bus = edit::addTrack (f.project, TrackKind::bus).getId();
    const auto send = edit::addSend (f.project, a, bus);
    edit::setSendLevel (f.project, send, 0.0);
    edit::setVolume (f.project, a, -6.0206);

    // After the fader: half directly + half through the bus.
    CHECK_THAT (f.play (1000).left[500], WithinAbs (0.5, 1.0e-4));

    // Before the fader: the fader doesn't change what's sent.
    edit::setSendPreFader (f.project, send, true);
    edit::setVolume (f.project, a, mixer::silentDb);
    CHECK_THAT (f.play (1000).left[500], WithinAbs (0.5, 1.0e-6));

    // Muting the track mutes its sends too.
    edit::setMuted (f.project, { a }, true);
    CHECK (f.play (1000).maxAbs (600, 1000) == 0.0f);   // after the 10 ms fade
}

TEST_CASE ("Channel meters show each channel's level", "[engine][mixer][meters]")
{
    Fixture f;
    const auto a = f.addTrackWithSound ("a");
    edit::setVolume (f.project, a, -6.0206);
    edit::setPan (f.project, a, 1.0);
    f.play (1000);

    const auto [left, right] = f.resources.takePeaks (a);
    CHECK (left == 0.0f);
    CHECK_THAT (right, WithinAbs (0.25, 1.0e-4));

    const auto [masterLeft, masterRight] = f.resources.takePeaks (mixer::masterId);
    CHECK_THAT (masterRight, WithinAbs (0.25, 1.0e-4));
    CHECK (masterLeft == 0.0f);

    // Reading a meter resets it.
    CHECK (f.resources.takePeaks (a).second == 0.0f);
}

TEST_CASE ("The delay echoes in time with the tempo", "[engine][mixer][effects]")
{
    Fixture f;
    const auto a = f.addTrackWithSound ("a");
    f.files.set ("Audio/a.wav", makeAudio (480000, [] (int i) { return i == 1000 ? 1.0f : 0.0f; }));

    const auto echo = edit::addInsert (f.project, a, "delay");
    edit::setInsertParameter (f.project, echo, "time", 8.0f);        // 1/4 note: 24000 samples at 120 BPM
    edit::setInsertParameter (f.project, echo, "mix", 50.0f);        // dry and echo both at full level
    edit::setInsertParameter (f.project, echo, "feedback", 50.0f);

    const auto out = f.play (80000);
    CHECK (out.left[1000] == 1.0f);                                  // the original
    CHECK (out.maxAbs (1001, 25000) == 0.0f);                        // nothing until the echo...
    CHECK_THAT (out.left[25000], WithinAbs (1.0, 1.0e-6));          // ...exactly a beat later
    CHECK_THAT (out.sum (48990, 49500), WithinAbs (0.5, 0.02));     // the second echo at half the level

    SECTION ("Switched off, it does nothing")
    {
        edit::setInsertBypassed (f.project, echo, true);
        f.engine.locate (0);
        const auto bypassed = f.play (30000);
        CHECK (bypassed.left[1000] == 1.0f);
        CHECK (bypassed.maxAbs (1001, 30000) == 0.0f);
    }
}

TEST_CASE ("An echo keeps going while you change other things", "[engine][mixer][effects]")
{
    Fixture f;
    const auto a = f.addTrackWithSound ("a");
    f.files.set ("Audio/a.wav", makeAudio (480000, [] (int i) { return i == 1000 ? 1.0f : 0.0f; }));
    const auto echo = edit::addInsert (f.project, a, "delay");
    edit::setInsertParameter (f.project, echo, "time", 8.0f);
    edit::setInsertParameter (f.project, echo, "mix", 50.0f);

    f.play (10000);

    // An unrelated edit rebuilds the engine's snapshot half-way through the echo's delay.
    edit::addTrack (f.project, TrackKind::audio);
    f.update();
    const auto out = f.render (20000);
    CHECK_THAT (out.left[25000 - 10000], WithinAbs (1.0, 1.0e-6));
}

TEST_CASE ("The reverb adds a tail, and the master can have effects", "[engine][mixer][effects]")
{
    Fixture f;
    const auto a = f.addTrackWithSound ("a");
    f.files.set ("Audio/a.wav", makeAudio (480000, [] (int i) { return i < 100 ? 0.5f : 0.0f; }));

    const auto dry = f.play (24000);
    CHECK (dry.maxAbs (200, 24000) == 0.0f);

    f.engine.stop();
    f.engine.locate (0);
    edit::addInsert (f.project, mixer::masterId, "reverb");
    const auto wet = f.play (24000);
    CHECK (wet.maxAbs (2000, 24000) > 0.001f);   // still ringing long after the sound stopped
    CHECK (wet.maxAbs (2000, 24000, false) > 0.001f);
}

TEST_CASE ("Switching an effect on or off, or adding one while playing, fades instead of clicking", "[engine][mixer][effects]")
{
    Fixture f;
    const auto track = f.addTrackWithSound ("a");   // a steady 0.5
    const auto eq = edit::addInsert (f.project, track, "eq");
    edit::setInsertParameter (f.project, eq, "output", 12.0f);   // about 4 times louder

    auto largestStep = [] (const std::vector<float>& s)
    {
        float most = 0.0f;
        for (size_t i = 1; i < s.size(); ++i)
            most = std::max (most, std::abs (s[i] - s[i - 1]));
        return most;
    };

    // Opened with the effect on: on straight away (no fade at the start of playback or an export).
    const auto on = f.play (4800);
    CHECK_THAT (on.left[100], WithinAbs (2.0, 0.05));

    // Off: down to the dry sound over 20 ms (960 samples at 48 kHz), in small steps.
    edit::setInsertBypassed (f.project, eq, true);
    f.update();
    const auto off = f.render (4800);
    CHECK (off.left[0] > 1.9f);
    CHECK (largestStep (off.left) < 0.01f);
    CHECK_THAT (off.left[2000], WithinAbs (0.5, 0.001));

    // On again.
    edit::setInsertBypassed (f.project, eq, false);
    f.update();
    const auto back = f.render (4800);
    CHECK (largestStep (back.left) < 0.01f);
    CHECK_THAT (back.left[2000], WithinAbs (2.0, 0.05));

    // A new effect added while playing fades in too.
    const auto second = edit::addInsert (f.project, track, "eq");
    edit::setInsertParameter (f.project, second, "output", -12.0f);
    f.update();
    const auto added = f.render (4800);
    CHECK (largestStep (added.left) < 0.01f);
    CHECK_THAT (added.left[2000], WithinAbs (0.5, 0.02));
}

TEST_CASE ("Changing the delay's time or the reverb's size doesn't bend the pitch", "[engine][effects]")
{
    // A steady sine through the effect, fully wet; the time changes half-way. Gliding the echo time
    // would make the sound's zero crossings speed up or slow down (a "laser" sound); crossfading
    // keeps them evenly spaced.
    for (const auto* typeId : { "delay", "reverb" })
    {
        INFO (typeId);
        const auto* type = dsp::findEffectType (typeId);
        REQUIRE (type != nullptr);
        auto effect = dsp::createEffect (typeId);
        effect->prepare (48000.0, 512);

        dsp::EffectParameterValues v {};
        for (size_t i = 0; i < type->parameters.size(); ++i)
            v[i] = type->parameters[i].defaultValue;

        const auto mixIndex = (size_t) (std::find_if (type->parameters.begin(), type->parameters.end(),
                                                      [] (const dsp::EffectParameter& p) { return p.id.toString() == "mix"; })
                                        - type->parameters.begin());
        v[mixIndex] = 100.0f;

        dsp::EffectContext context;
        context.sampleRate = 48000.0;
        constexpr double hz = 1000.0;
        std::vector<float> out;
        std::vector<float> l (512), r (512);
        int n = 0;

        for (int block = 0; block < 400; ++block)
        {
            if (block == 200)
            {
                if (juce::String (typeId) == "delay")
                    v[0] = 0.0f;       // time: 1/8 to 1/32
                else
                    v[0] = 100.0f;     // size: 50 % to 100 %
            }

            for (size_t i = 0; i < l.size(); ++i, ++n)
                l[i] = r[i] = 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * hz * n / 48000.0);

            effect->process (l.data(), r.data(), (int) l.size(), v, context);
            out.insert (out.end(), l.begin(), l.end());
        }

        // Upward zero crossings after the change: a bent pitch would make some periods much
        // shorter or longer than 48 samples.
        std::vector<double> crossings;
        for (size_t i = 200 * 512; i + 1 < 200 * 512 + 9600; ++i)
            if (out[i] <= 0.0f && out[i + 1] > 0.0f)
                crossings.push_back ((double) i + out[i] / (out[i] - out[i + 1]));

        // (While crossfading, the old and new echoes of a pure tone can cancel for a moment;
        // periods where it's nearly silent are skipped.)
        REQUIRE (crossings.size() > 100);
        for (size_t i = 1; i < crossings.size(); ++i)
        {
            float peak = 0.0f;
            for (auto k = (size_t) crossings[i - 1]; k < (size_t) crossings[i]; ++k)
                peak = std::max (peak, std::abs (out[k]));

            if (peak > 0.05f)
                CHECK (std::abs (crossings[i] - crossings[i - 1] - 48.0) < 3.0);
        }
    }
}
