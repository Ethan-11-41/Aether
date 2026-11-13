// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
//
// Sound effects and cinematic sounds (Milestone 16): how they sound, and adding them to a song
// at its tempo.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>

#include "dsp/DrumSounds.h"
#include "dsp/SoundEffects.h"
#include "io/AudioImport.h"
#include "model/ClipEdits.h"
#include "model/EditCommands.h"
#include "model/TimelineEdits.h"

using namespace aether;
using namespace aether::dsp;

namespace
{
    constexpr double rate = 48000.0;
    constexpr Tick q = ticksPerQuarter;

    double rms (const juce::AudioBuffer<float>& b, double from, double to)
    {
        const auto a = (int) (from * b.getNumSamples()), z = (int) (to * b.getNumSamples());
        double sum = 0.0;
        for (int c = 0; c < b.getNumChannels(); ++c)
            for (int i = a; i < z; ++i)
                sum += (double) b.getSample (c, i) * b.getSample (c, i);
        return std::sqrt (sum / std::max (1, (z - a) * b.getNumChannels()));
    }

    /** Zero crossings per second in part of the left channel: higher for brighter, higher sounds. */
    double crossings (const juce::AudioBuffer<float>& b, double from, double to)
    {
        const auto a = (int) (from * b.getNumSamples()), z = (int) (to * b.getNumSamples());
        int count = 0;
        for (int i = a + 1; i < z; ++i)
            if ((b.getSample (0, i - 1) < 0.0f) != (b.getSample (0, i) < 0.0f))
                ++count;
        return count / ((z - a) / rate);
    }

    struct TempFolder
    {
        TempFolder()   { folder.createDirectory(); }
        ~TempFolder()  { folder.deleteRecursively(); }
        juce::File folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("aether-tests")
                                .getNonexistentChildFile ("effects", {}, false);
    };
}

TEST_CASE ("Every sound effect is stereo, clean at both ends and always the same", "[soundfx]")
{
    REQUIRE (soundfx::all().size() >= 7);

    for (const auto& info : soundfx::all())
    {
        INFO (info.id);
        CHECK (soundfx::find (info.id) == &info);

        const auto sound = soundfx::render (info.id, 2.0, rate);
        REQUIRE (sound.getNumChannels() == 2);
        REQUIRE (sound.getNumSamples() > 4800);

        if (info.hasLength)
            CHECK (sound.getNumSamples() == 96000);   // made to the length asked for

        // About -2 dB at its loudest (a little less if that's in the very end, which is faded so
        // it doesn't click).
        const auto peak = sound.getMagnitude (0, sound.getNumSamples());
        CHECK (peak <= 0.801f);
        CHECK (peak > 0.7f);

        for (int c = 0; c < 2; ++c)
        {
            CHECK (std::abs (sound.getSample (c, 0)) < 0.01f);
            CHECK (std::abs (sound.getSample (c, sound.getNumSamples() - 1)) < 0.01f);
            const auto* d = sound.getReadPointer (c);
            CHECK (std::all_of (d, d + sound.getNumSamples(), [] (float x) { return std::isfinite (x); }));
        }

        const auto again = soundfx::render (info.id, 2.0, rate);
        CHECK (std::equal (sound.getReadPointer (0), sound.getReadPointer (0) + sound.getNumSamples(), again.getReadPointer (0)));
    }

    CHECK (soundfx::render ("no such effect", 2.0, rate).getNumSamples() == 0);
}

TEST_CASE ("Build-ups rise into their end; a downlifter falls away", "[soundfx]")
{
    for (const auto* id : { "riser-noise", "riser-synth", "reverse-swell" })
    {
        INFO (id);
        const auto sound = soundfx::render (id, 4.0, rate);
        CHECK (rms (sound, 0.85, 0.99) > 8.0 * rms (sound, 0.02, 0.15));   // much louder at the end
    }

    // The noise riser gets brighter and the synth riser higher as they go.
    const auto noise = soundfx::render ("riser-noise", 4.0, rate);
    CHECK (crossings (noise, 0.85, 0.99) > 4.0 * crossings (noise, 0.02, 0.15));
    const auto synth = soundfx::render ("riser-synth", 4.0, rate);
    CHECK (crossings (synth, 0.85, 0.99) > 1.5 * crossings (synth, 0.02, 0.15));

    const auto down = soundfx::render ("downlifter", 4.0, rate);
    CHECK (rms (down, 0.02, 0.15) > 8.0 * rms (down, 0.85, 0.99));
    CHECK (crossings (down, 0.02, 0.15) > 3.0 * crossings (down, 0.6, 0.75));
}

TEST_CASE ("The cinematic drums are big and low, and have their own kit", "[soundfx][drums]")
{
    const auto taiko = drums::render ("taiko-low", rate);
    int crossings = 0;
    for (int i = 1; i < 24000; ++i)
        if ((taiko.getSample (0, i - 1) < 0.0f) != (taiko.getSample (0, i) < 0.0f))
            ++crossings;
    CHECK (crossings / 0.5 < 400.0);            // a low drum (about 60 Hz, with a short knock)
    CHECK (taiko.getNumSamples() > 2 * 48000);  // with a long room behind it

    const auto* kit = drums::findKit ("Cinematic");
    REQUIRE (kit != nullptr);
    CHECK (juce::String (kit->pads[0].sound) == "taiko-low");

    for (const auto* id : { "taiko-high", "tom-big-low", "snare-big", "stomp", "impact", "boom", "metal-hit", "gong", "ticker",
                            "reverse-cymbal", "whoosh" })
    {
        INFO (id);
        REQUIRE (drums::findSound (id) != nullptr);
        CHECK (juce::String (drums::findSound (id)->group) == "Cinematic");
    }

    // The reverse cymbal swells up into its end.
    const auto swell = drums::render ("reverse-cymbal", rate);
    juce::AudioBuffer<float> stereo (1, swell.getNumSamples());
    stereo.copyFrom (0, 0, swell, 0, 0, swell.getNumSamples());
    CHECK (rms (stereo, 0.9, 0.99) > 5.0 * rms (stereo, 0.0, 0.2));
}

TEST_CASE ("A sound effect goes into the song at its tempo, on the bar", "[soundfx][import]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    TempFolder temp;
    const ProjectLocation location (temp.folder.getChildFile ("Song.aether"));
    Project project;   // 120 BPM, 4/4: a bar is 2 seconds
    const auto keys = edit::addTrack (project, TrackKind::midi, -1, true, "Keys").getId();
    const auto fx = edit::addTrack (project, TrackKind::audio, -1, true, "FX").getId();
    const auto bar = 4 * q;

    // A 2-bar riser into bar 5 (clicked a little before it): it starts at bar 3 and lasts 4 seconds.
    auto r = addSoundEffect (project, location, "riser-noise", 2, fx, 4 * bar - q / 3, rate);
    REQUIRE (r.problem.isEmpty());
    CHECK (r.trackId == fx);
    auto clip = clips::findClip (project, r.clipId);
    REQUIRE (clip.isValid());
    CHECK (clip.getStart() == 2 * bar);
    CHECK (clip.getDuration() == 192000);
    CHECK (clip.getEnd (timeline::buildTempoMap (project)) == 4 * bar);
    CHECK (clip.getName() == "Riser (noise)");
    CHECK (clip.getFile().startsWith ("Audio/Sound effects/"));
    CHECK (location.getAudioFolder().getChildFile ("Sound effects").findChildFiles (juce::File::findFiles, false).size() == 1);
    CHECK (project.getUndoManager().getUndoDescription() == "Add Riser (noise)");

    // The same again (elsewhere) uses the same file.
    r = addSoundEffect (project, location, "riser-noise", 2, fx, 8 * bar, rate);
    CHECK (location.getAudioFolder().getChildFile ("Sound effects").findChildFiles (juce::File::findFiles, false).size() == 1);

    // At another tempo it's made to fit: 2 bars at 60 BPM are 8 seconds.
    edit::setTempo (project, 0, 60.0);
    r = addSoundEffect (project, location, "riser-noise", 2, fx, 4 * bar, rate);
    CHECK (clips::findClip (project, r.clipId).getDuration() == 384000);

    // A hit starts where asked; on a MIDI track it goes on a new audio track just below it, in
    // the same undo step.
    const auto tracksBefore = project.getNumTracks();
    r = addSoundEffect (project, location, "impact", 1, keys, 3 * bar + q, rate);
    REQUIRE (r.problem.isEmpty());
    CHECK (project.getNumTracks() == tracksBefore + 1);
    CHECK (project.indexOfTrack (r.trackId) == project.indexOfTrack (keys) + 1);
    CHECK (project.findTrack (r.trackId).getName() == "Sound effects");
    CHECK (clips::findClip (project, r.clipId).getStart() == 3 * bar + q);

    project.getUndoManager().undo();
    CHECK (project.getNumTracks() == tracksBefore);

    // A downlifter starts on the nearest bar line and lasts its bars.
    r = addSoundEffect (project, location, "downlifter", 1, fx, 6 * bar + q, rate);
    CHECK (clips::findClip (project, r.clipId).getStart() == 6 * bar);
    CHECK (clips::findClip (project, r.clipId).getDuration() == 192000);   // 1 bar at 60 BPM: 4 seconds

    CHECK (addSoundEffect (project, location, "no such effect", 1, fx, 0, rate).problem.isNotEmpty());
}

TEST_CASE ("A sound effect in the song can be swapped, made longer or made again, in place", "[soundfx][import]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    TempFolder temp;
    const ProjectLocation location (temp.folder.getChildFile ("Song.aether"));
    Project project;   // 120 BPM, 4/4: a bar is 2 seconds
    const auto fx = edit::addTrack (project, TrackKind::audio, -1, true, "FX").getId();
    const auto bar = 4 * q;
    auto& undo = project.getUndoManager();

    // A 2-bar riser into bar 5, turned down and faded in.
    const auto r = addSoundEffect (project, location, "riser-noise", 2, fx, 4 * bar, rate);
    REQUIRE (r.problem.isEmpty());
    const auto id = r.clipId;
    REQUIRE (soundEffectOf (project, id) != nullptr);
    CHECK (juce::String (soundEffectOf (project, id)->id) == "riser-noise");
    CHECK (soundEffectBars (project, id) == 2);
    edit::setClipGain (project, id, -4.0);
    edit::setClipFades (project, id, 48000, 0);

    auto clip = [&] { return clips::findClip (project, id); };
    const auto end = [&] { return clip().getEnd (timeline::buildTempoMap (project)); };

    // 4 bars instead: it still rises into bar 5 (so it starts at bar 1), keeping its volume and fade.
    REQUIRE (changeSoundEffect (project, location, id, "riser-noise", 4, rate, "Make it 4 bars").isEmpty());
    CHECK (undo.getUndoDescription() == "Make it 4 bars");
    CHECK (clip().getStart() == 0);
    CHECK (end() == 4 * bar);
    CHECK (clip().getDuration() == 384000);
    CHECK (clip().getGainDb() == Catch::Approx (-4.0));
    CHECK (clip().getFadeIn() == 48000);
    CHECK (soundEffectBars (project, id) == 4);

    // Swapped for a reverse swell: the same bars, into the same bar line, on the same clip.
    REQUIRE (changeSoundEffect (project, location, id, "reverse-swell", 4, rate).isEmpty());
    CHECK (clip().getName() == "Reverse swell");
    CHECK (end() == 4 * bar);
    CHECK (project.getTrack (project.indexOfTrack (fx)).getState().getChildWithName (ids::Clips).getNumChildren() == 1);

    // Swapped for an impact: it goes on the bar the build-up rose into (a hit starts there).
    REQUIRE (changeSoundEffect (project, location, id, "impact", 1, rate).isEmpty());
    CHECK (clip().getStart() == 4 * bar);
    CHECK (clip().getName() == "Impact");
    CHECK (clip().getFadeIn() <= clip().getDuration());

    // And back to a riser: it rises into the bar the impact was on.
    REQUIRE (changeSoundEffect (project, location, id, "riser-synth", 2, rate).isEmpty());
    CHECK (clip().getStart() == 2 * bar);
    CHECK (end() == 4 * bar);

    // After a tempo change, made again at the new tempo: 2 bars at 60 BPM are 8 seconds, still
    // ending on bar 5.
    edit::setTempo (project, 0, 60.0);
    REQUIRE (changeSoundEffect (project, location, id, "riser-synth", 2, rate).isEmpty());
    CHECK (clip().getDuration() == 384000);
    CHECK (end() == 4 * bar);

    // Each change is one undo step.
    undo.undo();
    CHECK (clip().getDuration() == 192000);
    undo.undo();   // the tempo
    undo.undo();
    CHECK (clip().getName() == "Impact");

    // Only Aether's own sound effects can be changed like this.
    const auto plain = edit::addAudioClip (project, fx, { "Audio/Guitar.wav", "Guitar", rate, 48000 }, 0);
    CHECK (soundEffectOf (project, plain) == nullptr);
    CHECK (changeSoundEffect (project, location, plain, "impact", 1, rate).isNotEmpty());
    CHECK (changeSoundEffect (project, location, id, "no such effect", 1, rate).isNotEmpty());
}
