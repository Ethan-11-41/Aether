// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
//
// Exporting: the song rendered offline must be exactly what the engine plays, in every format,
// with stems that add up to the mix.

#include <cmath>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "engine/Exporter.h"
#include "io/Mp3Encoder.h"
#include "model/ClipEdits.h"
#include "model/EditCommands.h"
#include "model/MidiClips.h"
#include "model/Mixer.h"
#include "model/TimelineEdits.h"

using namespace aether;
using Catch::Matchers::WithinAbs;

namespace
{
    constexpr double rate = 48000.0;
    constexpr Tick q = ticksPerQuarter;   // 24000 samples at 120 BPM

    float tone (int i)   { return 0.25f * std::sin (2.0f * juce::MathConstants<float>::pi * 440.0f * (float) i / (float) rate); }

    struct Fixture
    {
        Fixture()
        {
            folder.createDirectory();
            location = ProjectLocation (folder.getChildFile ("Song.aether"));
            Mp3Encoder::setLibraryFile (juce::File (AETHER_TEST_MP3LAME));

            // A one-second 440 Hz tone, as a 32-bit float WAV in the project's Audio folder.
            const auto file = folder.getChildFile ("Audio").getChildFile ("tone.wav");
            file.getParentDirectory().createDirectory();
            juce::AudioBuffer<float> audio (1, 48000);
            for (int i = 0; i < 48000; ++i)
                audio.setSample (0, i, tone (i));

            juce::WavAudioFormat wav;
            std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (file);
            auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate (rate).withNumChannels (1)
                                                           .withBitsPerSample (32)
                                                           .withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint));
            writer->writeFromAudioSampleBuffer (audio, 0, 48000);
            writer = nullptr;

            a = edit::addTrack (project, TrackKind::audio, -1, true, "Tone").getId();
            b = edit::addTrack (project, TrackKind::audio, -1, true, "Tone two").getId();
            edit::addAudioClip (project, a, { "Audio/tone.wav", "tone", rate, 48000 }, q);       // from 0.5 s
            edit::addAudioClip (project, b, { "Audio/tone.wav", "tone", rate, 48000 }, 3 * q);   // from 1.5 s
        }

        ~Fixture()   { folder.deleteRecursively(); }

        ExportOptions options (ExportFormat format, int bits = 24)
        {
            ExportOptions o;
            o.format = format;
            o.bitDepth = bits;
            o.sampleRate = rate;
            o.folder = folder.getChildFile ("Exports");
            o.name = "Song";
            o.includeTail = false;
            return o;
        }

        static juce::AudioBuffer<float> read (const juce::File& file)
        {
            juce::AudioFormatManager formats;
            formats.registerBasicFormats();
            std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
            REQUIRE (reader != nullptr);
            juce::AudioBuffer<float> audio ((int) reader->numChannels, (int) reader->lengthInSamples);
            reader->read (&audio, 0, (int) reader->lengthInSamples, 0, true, true);
            return audio;
        }

        juce::ScopedJuceInitialiser_GUI juce;
        juce::File folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("aether-export-" + juce::Uuid().toString());
        ProjectLocation location;
        Project project;
        juce::String a, b;
    };
}

TEST_CASE ("The export covers the song, or the loop region", "[export]")
{
    Fixture f;
    const auto song = exportRange (f.project, false);
    CHECK (song.start == 0);
    CHECK (song.end == 3 * q + 2 * q);   // the last clip ends at 2.5 s = beat 5

    edit::setLoopRange (f.project, q, 2 * q);
    CHECK (exportRange (f.project, true).start == q);
    CHECK (exportRange (f.project, true).end == 2 * q);

    Project empty;
    CHECK (exportRange (empty, false).end == 0);
    juce::AudioBuffer<float> out;
    CHECK (renderProject (empty, f.location, rate, exportRange (empty, false), false, out).getErrorMessage().contains ("nothing to export"));
}

TEST_CASE ("Rendering gives exactly what the engine plays", "[export]")
{
    Fixture f;
    juce::AudioBuffer<float> out;
    REQUIRE (renderProject (f.project, f.location, rate, exportRange (f.project, false), false, out).wasOk());

    REQUIRE (out.getNumSamples() == 5 * 24000);
    CHECK (out.getSample (0, 23999) == 0.0f);
    CHECK (out.getSample (0, 24000 + 100) == tone (100));                 // track A, sample for sample
    CHECK (out.getSample (1, 24000 + 30000) == tone (30000));
    CHECK (out.getSample (0, 72000 - 1) == tone (47999));                 // A's last sample...
    CHECK (out.getSample (0, 72000 + 10) == tone (10));                    // ...then B
    CHECK (out.getSample (0, 5 * 24000 - 1) == tone (47999));              // B's last sample
}

TEST_CASE ("Effect tails ring out after the end, then the file stops", "[export]")
{
    Fixture f;
    const auto echo = edit::addInsert (f.project, mixer::masterId, "delay");
    edit::setInsertParameter (f.project, echo, "time", 8.0f);   // a quarter note
    juce::AudioBuffer<float> withTail, without;

    REQUIRE (renderProject (f.project, f.location, rate, exportRange (f.project, false), false, without).wasOk());
    REQUIRE (renderProject (f.project, f.location, rate, exportRange (f.project, false), true, withTail).wasOk());

    CHECK (without.getNumSamples() == 5 * 24000);
    CHECK (withTail.getNumSamples() > without.getNumSamples() + 24000);          // the echoes
    CHECK (withTail.getNumSamples() < without.getNumSamples() + (int) rate * 10);  // but not forever
    CHECK (withTail.getMagnitude (0, without.getNumSamples(), 24000) > 0.01f);
}

TEST_CASE ("Exporting at another sample rate converts everything", "[export]")
{
    Fixture f;
    juce::AudioBuffer<float> out;
    REQUIRE (renderProject (f.project, f.location, 44100.0, exportRange (f.project, false), false, out).wasOk());
    CHECK (out.getNumSamples() == 5 * 22050);
    CHECK_THAT (out.getMagnitude (0, 22050, 44100), WithinAbs (0.25, 0.01));
}

TEST_CASE ("WAV and FLAC files hold the sound, to their precision", "[export][formats]")
{
    Fixture f;
    juce::AudioBuffer<float> out;
    REQUIRE (renderProject (f.project, f.location, rate, exportRange (f.project, false), false, out).wasOk());

    struct Case { ExportFormat format; int bits; float tolerance; };

    for (const auto& c : { Case { ExportFormat::wav, 32, 0.0f }, Case { ExportFormat::wav, 24, 1.0e-6f },
                           Case { ExportFormat::wav, 16, 1.0e-4f }, Case { ExportFormat::flac, 24, 1.0e-6f },
                           Case { ExportFormat::flac, 16, 1.0e-4f } })
    {
        const auto options = f.options (c.format, c.bits);
        const auto file = options.folder.getChildFile ("test" + juce::String (c.bits) + options.extension());
        INFO (file.getFileName());
        REQUIRE (writeAudioFile (out, options, file).wasOk());

        const auto back = Fixture::read (file);
        REQUIRE (back.getNumChannels() == 2);
        REQUIRE (back.getNumSamples() == out.getNumSamples());

        for (int i = 0; i < out.getNumSamples(); i += 37)
            REQUIRE_THAT (back.getSample (0, i), WithinAbs (out.getSample (0, i), c.tolerance));
    }
}

TEST_CASE ("MP3 files sound like the mix", "[export][formats][mp3]")
{
    Fixture f;
    REQUIRE (Mp3Encoder::isAvailable());
    CHECK (Mp3Encoder::getLameVersion().startsWith ("3.100"));

    juce::AudioBuffer<float> out;
    REQUIRE (renderProject (f.project, f.location, rate, exportRange (f.project, false), false, out).wasOk());

    for (const int kbps : { 128, 320 })
    {
        auto options = f.options (ExportFormat::mp3);
        options.mp3Kbps = kbps;
        const auto file = options.folder.getChildFile ("test" + juce::String (kbps) + ".mp3");
        REQUIRE (writeAudioFile (out, options, file).wasOk());
        CHECK (file.getSize() > (juce::int64) (kbps * 1000 / 8 * 2));   // about 2.5 s at that bit rate

        // Decoded, it's as loud as the mix (MP3 adds a little silence at the start).
        const auto back = Fixture::read (file);
        CHECK (back.getNumSamples() >= out.getNumSamples());
        CHECK_THAT (back.getRMSLevel (0, 0, back.getNumSamples()), WithinAbs (out.getRMSLevel (0, 0, out.getNumSamples()), 0.01));
    }

    // Without the library, MP3 export says why instead of failing silently.
    Mp3Encoder::setLibraryFile (f.folder.getChildFile ("nowhere.dll"));
    CHECK_FALSE (Mp3Encoder::isAvailable());
    auto options = f.options (ExportFormat::mp3);
    CHECK (writeAudioFile (out, options, options.folder.getChildFile ("x.mp3")).getErrorMessage().contains ("LAME"));
}

TEST_CASE ("Stems add up to the mix", "[export][stems]")
{
    Fixture f;
    edit::setVolume (f.project, f.b, -6.0);
    const auto bus = edit::addTrack (f.project, TrackKind::bus, -1, true, "Reverb").getId();
    edit::addInsert (f.project, bus, "reverb");
    edit::addSend (f.project, f.a, bus);
    edit::setMuted (f.project, { edit::addTrack (f.project, TrackKind::audio).getId() }, true);   // empty and muted: no stem

    auto options = f.options (ExportFormat::wav, 32);
    options.stems = true;
    ExportJob job (f.project, f.location, options);
    REQUIRE (job.run().wasOk());

    REQUIRE (job.getFiles().size() == 3);   // the mix and two stems
    CHECK (job.getFiles()[0].getFileName() == "Song.wav");
    CHECK (job.getFiles()[1].getFileName() == "Song - Tone.wav");
    CHECK (job.getFiles()[2].getParentDirectory().getFileName() == "Song stems");
    CHECK (job.getProgress() == 1.0f);

    const auto mix = Fixture::read (job.getFiles()[0]);
    const auto stemA = Fixture::read (job.getFiles()[1]);   // with its reverb send
    const auto stemB = Fixture::read (job.getFiles()[2]);

    for (int i = 0; i < mix.getNumSamples(); i += 101)
        REQUIRE_THAT (stemA.getSample (0, i) + stemB.getSample (0, i), WithinAbs (mix.getSample (0, i), 1.0e-5));

    // Exporting again doesn't overwrite: new names.
    ExportJob again (f.project, f.location, options);
    REQUIRE (again.run().wasOk());
    CHECK (again.getFiles()[0].getFileName() == "Song (2).wav");
}

TEST_CASE ("A mix that's too loud is reported, or normalized", "[export]")
{
    Fixture f;
    edit::setVolume (f.project, mixer::masterId, 6.0);
    edit::setVolume (f.project, f.a, 6.0);
    edit::setClipGain (f.project, clips::clipsOnTrack (f.project.findTrack (f.a).getState())[0].getId(), 6.0);   // 0.25 x 8: clipping

    auto options = f.options (ExportFormat::wav, 24);
    ExportJob loud (f.project, f.location, options);
    REQUIRE (loud.run().wasOk());
    REQUIRE (loud.getWarnings().size() == 1);
    CHECK (loud.getWarnings()[0].contains ("clipped"));

    options.normalize = true;
    ExportJob normalized (f.project, f.location, options);
    REQUIRE (normalized.run().wasOk());
    CHECK (normalized.getWarnings().isEmpty());
    const auto audio = Fixture::read (normalized.getFiles()[0]);
    CHECK_THAT (juce::Decibels::gainToDecibels (audio.getMagnitude (0, 0, audio.getNumSamples())), WithinAbs (-1.0, 0.01));
}

TEST_CASE ("MIDI tracks are exported too, and an export can be cancelled", "[export]")
{
    Fixture f;
    const auto keys = edit::addTrack (f.project, TrackKind::midi).getId();
    edit::addMidiClip (f.project, keys, "x", 0, 8 * q, { { 60, 100, 0, 8 * q } });

    juce::AudioBuffer<float> out;
    REQUIRE (renderProject (f.project, f.location, rate, exportRange (f.project, false), false, out).wasOk());
    CHECK (out.getMagnitude (0, 0, 20000) > 0.01f);   // the synth, before the audio clips start

    const auto cancelled = renderProject (f.project, f.location, rate, exportRange (f.project, false), false, out,
                                          [] (float) { return false; });
    CHECK (cancelled.getErrorMessage() == "Cancelled.");
}
