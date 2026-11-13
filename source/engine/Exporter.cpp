// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "Exporter.h"

#include <cmath>
#include <random>
#include <set>

#include "AudioEngine.h"
#include "SamplerLibrary.h"
#include "TimelineSync.h"
#include "dsp/DrumKit.h"
#include "io/AudioImport.h"
#include "io/Mp3Encoder.h"
#include "model/ClipEdits.h"
#include "model/MidiClips.h"
#include "model/TimelineEdits.h"

namespace aether
{

juce::String ExportOptions::extension() const
{
    switch (format)
    {
        case ExportFormat::wav:  return ".wav";
        case ExportFormat::flac: return ".flac";
        case ExportFormat::mp3:  return ".mp3";
    }

    return ".wav";
}

ExportRange exportRange (const Project& project, bool loopRegionOnly)
{
    if (loopRegionOnly)
    {
        const auto loop = timeline::getLoop (project);
        return { loop.start, loop.end };
    }

    const auto tempo = timeline::buildTempoMap (project);
    ExportRange range;

    for (const auto& clip : clips::allClips (project))
        range.end = std::max (range.end, clip.getEnd (tempo));

    return range;
}

//==============================================================================
juce::Result renderProject (const Project& project, const ProjectLocation& location, double sampleRate,
                            ExportRange range, bool includeTail, juce::AudioBuffer<float>& output,
                            const std::function<bool (float)>& progress)
{
    if (range.end <= range.start)
        return juce::Result::fail ("There's nothing to export: the song has no clips (or the loop region is empty).");

    // Load every clip's audio at the export's sample rate.
    AudioFileCache cache;
    cache.setTargetSampleRate (sampleRate);

    for (const auto& clip : clips::allClips (project))
        if (! clip.isMidi())
            cache.getAudio (resolveClipFile (location, clip.getFile()));

    // ...the samplers' recordings...
    for (const auto& file : samplerlibrary::filesUsed (project, location.getFolder()))
        cache.getAudio (file);

    // ...and the drum pads' sounds that come from files.
    for (int t = 0; t < project.getNumTracks(); ++t)
    {
        const auto instrument = midi::instrumentOf (project, project.getTrack (t).getId());

        if (instrument[ids::type].toString() == dsp::drumkit::typeId)
            for (int pad = 0; pad < dsp::drums::numPads; ++pad)
                if (const auto sound = instrument[dsp::drumkit::soundProperty (pad)].toString();
                    sound.isNotEmpty() && ! dsp::drumkit::isBuiltIn (sound))
                    cache.getAudio (resolveClipFile (location, sound));
    }

    cache.waitUntilIdle();

    // A private engine, playing the song from the start of the range.
    MixerResources resources;
    auto snapshot = buildTimelineSnapshot (project, sampleRate, [&cache, &location] (const juce::String& file)
    {
        return cache.getAudio (resolveClipFile (location, file));
    }, &resources, {}, {}, location.getFolder(), [] (const StretchJob& job)
    {
        return job.render();   // clips that follow the tempo, stretched now (export waits for them)
    });

    snapshot->loopEnabled = false;   // play straight through

    const auto& tempo = snapshot->tempo;
    const auto startSample = (std::int64_t) std::llround (tempo.ticksToSeconds (range.start) * sampleRate);
    const auto endSample = (std::int64_t) std::llround (tempo.ticksToSeconds (range.end) * sampleRate);
    const auto length = (int) (endSample - startSample);

    constexpr int blockSize = 512;
    AudioEngine engine;
    engine.prepare (sampleRate, blockSize);
    engine.setTimeline (std::move (snapshot));
    engine.locate (range.start);
    engine.play();

    output.setSize (2, length, false, true);
    output.clear();

    for (int done = 0; done < length; done += blockSize)
    {
        const auto n = std::min (blockSize, length - done);
        float* outputs[] = { output.getWritePointer (0, done), output.getWritePointer (1, done) };
        engine.process (nullptr, 0, outputs, 2, n);

        if ((done / blockSize) % 64 == 0)
        {
            engine.collectGarbage();

            if (progress && ! progress ((float) done / (float) length))
                return juce::Result::fail ("Cancelled.");
        }
    }

    // The tail: what rings on after the end (reverb, echoes, released notes), until it's silent.
    if (includeTail)
    {
        engine.stop();

        const auto maxTail = (int) (sampleRate * 10.0);
        const auto quietNeeded = (int) (sampleRate * 0.5);
        juce::AudioBuffer<float> tail (2, maxTail);
        tail.clear();
        int tailLength = 0, quiet = 0, lastSound = 0;

        while (tailLength < maxTail && quiet < quietNeeded)
        {
            const auto n = std::min (blockSize, maxTail - tailLength);
            float* outputs[] = { tail.getWritePointer (0, tailLength), tail.getWritePointer (1, tailLength) };
            engine.process (nullptr, 0, outputs, 2, n);

            const auto peak = std::max (tail.getMagnitude (0, tailLength, n), tail.getMagnitude (1, tailLength, n));
            tailLength += n;

            if (peak > 1.0e-5f)   // -100 dB
            {
                quiet = 0;
                lastSound = tailLength;
            }
            else
            {
                quiet += n;
            }
        }

        if (lastSound > 0)
        {
            output.setSize (2, length + lastSound, true, true);

            for (int ch = 0; ch < 2; ++ch)
                output.copyFrom (ch, length, tail, ch, 0, lastSound);
        }
    }

    engine.collectGarbage();

    if (progress)
        progress (1.0f);

    return juce::Result::ok();
}

//==============================================================================
juce::Result writeAudioFile (const juce::AudioBuffer<float>& audio, const ExportOptions& options, const juce::File& file)
{
    file.getParentDirectory().createDirectory();
    constexpr int chunk = 8192;

    if (options.format == ExportFormat::mp3)
    {
        Mp3Encoder encoder;

        if (auto result = encoder.open (file, options.sampleRate, 2, options.mp3Kbps); result.failed())
            return result;

        for (int done = 0; done < audio.getNumSamples(); done += chunk)
        {
            const auto n = std::min (chunk, audio.getNumSamples() - done);

            if (auto result = encoder.write (audio.getReadPointer (0, done), audio.getReadPointer (1, done), n); result.failed())
                return result;
        }

        return encoder.close();
    }

    // WAV and FLAC, through JUCE.
    std::unique_ptr<juce::AudioFormat> format;
    if (options.format == ExportFormat::flac)
        format = std::make_unique<juce::FlacAudioFormat>();
    else
        format = std::make_unique<juce::WavAudioFormat>();

    const auto floatingPoint = options.format == ExportFormat::wav && options.bitDepth == 32;
    auto writerOptions = juce::AudioFormatWriterOptions{}.withSampleRate (options.sampleRate)
                                                         .withNumChannels (2)
                                                         .withBitsPerSample (options.bitDepth);
    if (floatingPoint)
        writerOptions = writerOptions.withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint);

    file.deleteFile();
    std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (file);

    if (static_cast<juce::FileOutputStream&> (*stream).failedToOpen())
        return juce::Result::fail ("Couldn't create " + file.getFullPathName() + ".");

    auto writer = format->createWriterFor (stream, writerOptions);

    if (writer == nullptr)
        return juce::Result::fail ("Couldn't write a " + juce::String (options.bitDepth) + "-bit "
                                   + format->getFormatName() + " file at " + juce::String (options.sampleRate / 1000.0, 1) + " kHz.");

    // 16-bit files get a whisper of noise (TPDF dither), which hides the rounding that would
    // otherwise make quiet sounds grainy.
    const auto dither = options.bitDepth == 16;
    std::mt19937 random (12345);
    std::uniform_real_distribution<float> noise (-0.5f / 32768.0f, 0.5f / 32768.0f);
    juce::AudioBuffer<float> block (2, chunk);

    for (int done = 0; done < audio.getNumSamples(); done += chunk)
    {
        const auto n = std::min (chunk, audio.getNumSamples() - done);

        for (int ch = 0; ch < 2; ++ch)
        {
            block.copyFrom (ch, 0, audio, ch, done, n);

            if (dither)
            {
                auto* data = block.getWritePointer (ch);
                for (int i = 0; i < n; ++i)
                    data[i] += noise (random) + noise (random);
            }
        }

        if (! writer->writeFromAudioSampleBuffer (block, 0, n))
            return juce::Result::fail ("Couldn't write " + file.getFullPathName() + " (is the disk full?).");
    }

    writer = nullptr;   // finishes the file
    return juce::Result::ok();
}

//==============================================================================
juce::StringArray ExportJob::stemTracks (const Project& project)
{
    juce::StringArray ids;

    for (int i = 0; i < project.getNumTracks(); ++i)
    {
        const auto track = project.getTrack (i);

        if (! track.isBus() && ! track.isMuted() && ! clips::clipsOnTrack (track.getState()).isEmpty())
            ids.add (track.getId());
    }

    return ids;
}

ExportJob::ExportJob (const Project& project, const ProjectLocation& projectLocation, const ExportOptions& exportOptions)
    : options (exportOptions), location (projectLocation), range (exportRange (project, exportOptions.loopRegionOnly))
{
    if (options.name.trim().isEmpty())
        options.name = project.getName();

    std::set<juce::String> planned;
    auto plan = [this, &planned] (const juce::File& folder, const juce::String& baseName)
    {
        auto name = juce::File::createLegalFileName (baseName).trim();
        auto file = folder.getChildFile (name + options.extension());

        for (int n = 2; file.exists() || planned.count (file.getFullPathName()) > 0; ++n)
            file = folder.getChildFile (name + " (" + juce::String (n) + ")" + options.extension());

        planned.insert (file.getFullPathName());
        return file;
    };

    // The mix: the project as you hear it now (mutes and solos included).
    if (options.mix)
        renders.push_back ({ "Mix", project.getState().createCopy(), plan (options.folder, options.name) });

    // Stems: each track on its own (soloed), through its buses, effects and the master.
    if (options.stems)
    {
        const auto stemFolder = options.folder.getChildFile (juce::File::createLegalFileName (options.name + " stems"));

        for (const auto& id : stemTracks (project))
        {
            auto copy = project.getState().createCopy();

            for (auto track : copy.getChildWithName (ids::Tracks))
                track.setProperty (ids::solo, track[ids::id].toString() == id, nullptr);

            const auto name = project.findTrack (id).getName();
            renders.push_back ({ "Stem: " + name, copy, plan (stemFolder, options.name + " - " + name) });
        }
    }
}

juce::String ExportJob::getCurrentStep() const
{
    const auto index = currentRender.load();

    if (! juce::isPositiveAndBelow (index, (int) renders.size()))
        return {};

    return renders[(size_t) index].label + " (" + juce::String (index + 1) + " of " + juce::String ((int) renders.size()) + ")";
}

juce::Result ExportJob::run()
{
    if (renders.empty())
        return juce::Result::fail ("Nothing to export: tick the mix or stems (stems need tracks with clips that aren't muted).");

    if (range.end <= range.start)
        return juce::Result::fail ("There's nothing to export: the song has no clips (or the loop region is empty).");

    if (! options.folder.createDirectory())
        return juce::Result::fail ("Couldn't create the folder " + options.folder.getFullPathName() + ".");

    const auto count = (float) renders.size();

    for (size_t i = 0; i < renders.size(); ++i)
    {
        currentRender = (int) i;
        auto project = Project::fromState (renders[i].state);
        juce::AudioBuffer<float> audio;

        const auto result = renderProject (*project, location, options.sampleRate, range, options.includeTail, audio,
                                           [this, i, count] (float done)
        {
            progress = ((float) i + done * 0.9f) / count;
            return ! cancelled.load();
        });

        if (result.failed())
            return result;

        const auto peak = std::max (audio.getMagnitude (0, 0, audio.getNumSamples()), audio.getMagnitude (1, 0, audio.getNumSamples()));
        const auto isMix = renders[i].label == "Mix";

        if (isMix && options.normalize && peak > 0.0f)
        {
            audio.applyGain (juce::Decibels::decibelsToGain (-1.0f) / peak);
        }
        else if (peak > 1.0f && ! (options.format == ExportFormat::wav && options.bitDepth == 32))
        {
            const auto overBy = juce::Decibels::gainToDecibels (peak);
            warnings.add (renders[i].label + " goes " + juce::String (overBy, 1) + " dB over the top, so it's clipped "
                          "(distorted) in the file. Turn the master fader down by at least that much"
                          + juce::String (isMix ? ", or tick Normalize." : "."));
        }

        if (const auto written = writeAudioFile (audio, options, renders[i].file); written.failed())
            return written;

        files.add (renders[i].file);
        progress = (float) (i + 1) / count;
    }

    return juce::Result::ok();
}

} // namespace aether
