// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
//
// Reading SFZ files, laying out recordings by their names, and finding a recording's pitch.

#include <cmath>

#include <catch2/catch_test_macros.hpp>

#include "dsp/DrumSounds.h"
#include "dsp/PitchDetect.h"
#include "io/SampleMapping.h"
#include "io/SfzReader.h"

using namespace aether;

TEST_CASE ("SFZ note names and numbers", "[sfz]")
{
    CHECK (sfz::parseKey ("c4") == 60);
    CHECK (sfz::parseKey ("C4") == 60);
    CHECK (sfz::parseKey ("c#4") == 61);
    CHECK (sfz::parseKey ("db4") == 61);
    CHECK (sfz::parseKey ("a0") == 21);
    CHECK (sfz::parseKey ("c-1") == 0);
    CHECK (sfz::parseKey ("g9") == 127);
    CHECK (sfz::parseKey ("72") == 72);
    CHECK (sfz::parseKey ("h4") == -1);
    CHECK (sfz::parseKey ("") == -1);
    CHECK (sfz::parseKey ("200") == -1);
}

TEST_CASE ("An SFZ file's regions inherit from their group and global settings", "[sfz]")
{
    const auto text = R"(
        // A small piano
        #define $LAYER mf
        <control> default_path=Samples\
        <global> ampeg_release=0.6 ampeg_attack=0.002 volume=-3
        <group> lovel=1 hivel=80 seq_length=2
        <region> seq_position=1 sample=Piano C4 $LAYER take 1.wav lokey=58 hikey=c#4 pitch_keycenter=c4
        <region> seq_position=2 sample=Piano C4 $LAYER take 2.wav lokey=58 hikey=61 pitch_keycenter=60 tune=-25
        <group> lovel=81
        <region> key=e4 sample=Loud E4.wav pan=-30 loop_mode=loop_continuous loop_start=100 loop_end=9000
        <region> trigger=release sample=Release.wav key=60
        <region> sample=Fx.wav key=70 loop_mode=one_shot cutoff=500
    )";

    const auto instrument = sfz::parse (text);
    REQUIRE (instrument.regions.size() == 4);   // the release region is left out

    const auto& a = instrument.regions[0];
    CHECK (a.sample == "Samples/Piano C4 mf take 1.wav");
    CHECK (a.lowKey == 58);
    CHECK (a.highKey == 61);
    CHECK (a.rootKey == 60);
    CHECK (a.lowVelocity == 1);
    CHECK (a.highVelocity == 80);
    CHECK (a.sequenceLength == 2);
    CHECK (a.sequencePosition == 1);
    CHECK (a.volume == -3.0f);

    CHECK (instrument.regions[1].sequencePosition == 2);
    CHECK (std::abs (instrument.regions[1].tune + 0.25f) < 1.0e-6f);

    // A new group starts afresh: no seq_length, a different velocity range.
    const auto& e = instrument.regions[2];
    CHECK (e.lowKey == 64);
    CHECK (e.highKey == 64);
    CHECK (e.rootKey == 64);
    CHECK (e.lowVelocity == 81);
    CHECK (e.sequenceLength == 1);
    CHECK (e.pan == -30.0f);
    CHECK (e.loop);
    CHECK (e.loopStart == 100);
    CHECK (e.loopEnd == 9000);

    CHECK (instrument.regions[3].oneShot);

    REQUIRE (instrument.attack.has_value());
    CHECK (std::abs (*instrument.attack - 0.002f) < 1.0e-6f);
    CHECK (std::abs (*instrument.release - 0.6f) < 1.0e-6f);
    CHECK_FALSE (instrument.sustain.has_value());
    CHECK (instrument.ignored.contains ("cutoff"));

    // Written out and read back, the regions are the same.
    const auto again = sfz::parse (sfz::write (instrument));
    REQUIRE (again.regions.size() == instrument.regions.size());
    CHECK (again.regions[0].sample == a.sample);
    CHECK (again.regions[1].sequencePosition == 2);
    CHECK (std::abs (again.regions[1].tune + 0.25f) < 1.0e-6f);
    CHECK (again.regions[2].loopEnd == 9000);
    CHECK (again.regions[3].oneShot);
    CHECK (std::abs (*again.release - 0.6f) < 1.0e-3f);
}

TEST_CASE ("An SFZ file can include another", "[sfz]")
{
    juce::TemporaryFile folder;
    folder.getFile().createDirectory();
    folder.getFile().getChildFile ("regions.sfz").replaceWithText ("<region> sample=a.wav key=60\n<region> sample=b.wav key=62\n");
    const auto main = folder.getFile().getChildFile ("Instrument.sfz");
    main.replaceWithText ("<global> volume=-6\n#include \"regions.sfz\"\n");

    const auto instrument = sfz::read (main);
    REQUIRE (instrument.regions.size() == 2);
    CHECK (instrument.regions[1].sample == "b.wav");
    CHECK (instrument.regions[1].volume == -6.0f);
}

TEST_CASE ("Recordings are laid out over the keyboard by their names", "[sfz]")
{
    CHECK (samplemapping::noteInName ("Piano_C4_mf_rr2.wav") == 60);
    CHECK (samplemapping::noteInName ("UR1_G#3_pp_RR1.wav") == 56);
    CHECK (samplemapping::noteInName ("Bb Clarinet Eb5.flac") == 75);
    CHECK (samplemapping::noteInName ("KSHarp_Cs3_mf.wav") == 49);
    CHECK (samplemapping::noteInName ("My 808.wav") == -1);
    CHECK (samplemapping::loudnessInName ("Piano_C4_mf_rr2.wav") == 5);
    CHECK (samplemapping::loudnessInName ("Piano_C4_FF.wav") == 7);
    CHECK (samplemapping::loudnessInName ("Strings_A3_v3.wav") == 3);
    CHECK (samplemapping::takeInName ("Piano_C4_mf_rr2.wav") == 2);

    const juce::File folder ("/sounds/piano");
    juce::Array<juce::File> files;
    for (const auto* name : { "P_C4_pp_rr1.wav", "P_C4_pp_rr2.wav", "P_C4_f.wav", "P_E4_pp.wav", "P_E4_f.wav", "P_G4_f.wav" })
        files.add (folder.getChildFile (name));

    const auto instrument = samplemapping::mapByNames (files, folder);
    REQUIRE (instrument.regions.size() == 6);

    auto find = [&] (const char* name) -> sfz::Region
    {
        for (const auto& r : instrument.regions)
            if (r.sample == name)
                return r;
        FAIL_CHECK (name);   // (carries on, so the test reports every problem)
        return {};
    };

    // C4 covers the keys up to half way to E4; G4 stretches to the top.
    CHECK (find ("P_C4_f.wav").lowKey == 0);
    CHECK (find ("P_C4_f.wav").highKey == 62);
    CHECK (find ("P_E4_f.wav").lowKey == 63);
    CHECK (find ("P_E4_f.wav").highKey == 65);
    CHECK (find ("P_G4_f.wav").highKey == 127);

    // Soft and loud share out the velocities; the two soft takes alternate.
    CHECK (find ("P_C4_pp_rr1.wav").highVelocity == 63);
    CHECK (find ("P_C4_f.wav").lowVelocity == 64);
    CHECK (find ("P_C4_pp_rr2.wav").sequenceLength == 2);
    CHECK (find ("P_C4_pp_rr2.wav").sequencePosition == 2);
    CHECK (find ("P_G4_f.wav").lowVelocity == 1);   // the only layer there: every velocity

    // One recording without a note: the whole keyboard. Several: one per key from C2.
    const auto single = samplemapping::mapByNames ({ folder.getChildFile ("My 808.wav") }, folder);
    REQUIRE (single.regions.size() == 1);
    CHECK (single.regions[0].lowKey == 0);
    CHECK (single.regions[0].highKey == 127);

    const auto hits = samplemapping::mapByNames ({ folder.getChildFile ("kick.wav"), folder.getChildFile ("snare.wav") }, folder);
    REQUIRE (hits.regions.size() == 2);
    CHECK (hits.regions[0].sample == "kick.wav");
    CHECK (hits.regions[0].lowKey == 36);
    CHECK (hits.regions[1].rootKey == 37);
}

TEST_CASE ("A recording's pitch is found, an 808 included", "[sfz]")
{
    constexpr double rate = 48000.0;
    juce::AudioBuffer<float> a (1, 48000);
    for (int i = 0; i < a.getNumSamples(); ++i)
        a.setSample (0, i, 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * 440.0 * i / rate));

    const auto hz = dsp::pitch::detectFrequency (a, rate);
    REQUIRE (hz.has_value());
    CHECK (std::abs (*hz - 440.0) < 1.0);
    CHECK (dsp::pitch::detectKey (a, rate) == 69);

    // A low bass note with harmonics (like a bass guitar): 55 Hz, A1.
    juce::AudioBuffer<float> bass (1, 48000);
    for (int i = 0; i < bass.getNumSamples(); ++i)
    {
        const auto t = juce::MathConstants<double>::twoPi * 55.0 * i / rate;
        bass.setSample (0, i, (float) (0.5 * std::sin (t) + 0.3 * std::sin (2 * t) + 0.2 * std::sin (3 * t)));
    }
    CHECK (dsp::pitch::detectKey (bass, rate) == 33);

    // Aether's 808 kick settles at about 48 Hz: G1 (31).
    const auto kick = dsp::drums::render ("kick-808", rate);
    const auto key = dsp::pitch::detectKey (kick, rate);
    REQUIRE (key.has_value());
    CHECK ((*key == 31 || *key == 30));

    // Noise and silence have no pitch.
    juce::AudioBuffer<float> silence (1, 48000);
    silence.clear();
    CHECK_FALSE (dsp::pitch::detectFrequency (silence, rate).has_value());
}
