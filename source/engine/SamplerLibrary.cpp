// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "SamplerLibrary.h"

#include <mutex>

#include "dsp/Sampler.h"
#include "model/MidiClips.h"

namespace aether::samplerlibrary
{

namespace
{
    std::mutex folderLock;
    juce::File folderOverride;

    struct CachedSfz
    {
        juce::Time modified;
        std::shared_ptr<const sfz::Instrument> instrument;
    };

    std::mutex sfzLock;

    // Made on first use (so it's destroyed before JUCE's leak checks run at exit).
    std::map<juce::String, CachedSfz>& sfzCache()
    {
        static std::map<juce::String, CachedSfz> cache;
        return cache;
    }
}

juce::File builtInFolder()
{
    {
        const std::scoped_lock lock (folderLock);
        if (folderOverride != juce::File())
            return folderOverride;
    }

    if (const auto fromEnvironment = juce::SystemStats::getEnvironmentVariable ("AETHER_SOUNDS", {}); fromEnvironment.isNotEmpty())
        return juce::File (fromEnvironment);

    return juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory().getChildFile ("Sounds");
}

void setBuiltInFolder (const juce::File& folder)
{
    const std::scoped_lock lock (folderLock);
    folderOverride = folder;
}

std::vector<Entry> list()
{
    std::vector<Entry> result;
    const auto folder = builtInFolder();
    const auto json = juce::JSON::parse (folder.getChildFile ("library.json"));

    if (const auto* items = json["instruments"].getArray())
    {
        for (const auto& item : *items)
        {
            Entry e;
            e.category = item["category"].toString();
            e.name = item["name"].toString();
            e.description = item["description"].toString();
            e.solo = (bool) item["solo"];
            const auto file = item["file"].toString();
            e.reference = samplersound::builtIn (file.upToLastOccurrenceOf (".sfz", false, true));

            if (e.name.isNotEmpty() && folder.getChildFile (file).existsAsFile())
                result.push_back (e);
        }
    }

    return result;
}

juce::File sfzFileOf (const samplersound::Reference& sound, const juce::File& projectFolder)
{
    switch (sound.kind)
    {
        case samplersound::Kind::builtIn:   return builtInFolder().getChildFile (sound.path + ".sfz");
        case samplersound::Kind::sfz:       return projectFolder.getChildFile (sound.path.replaceCharacter ('\\', '/'));
        case samplersound::Kind::recording:
        case samplersound::Kind::drum:
        case samplersound::Kind::none:      break;
    }

    return {};
}

std::shared_ptr<const sfz::Instrument> readSfz (const juce::File& file)
{
    if (! file.existsAsFile())
        return nullptr;

    const auto modified = file.getLastModificationTime();
    const std::scoped_lock lock (sfzLock);
    auto& entry = sfzCache()[file.getFullPathName()];

    if (entry.instrument == nullptr || entry.modified != modified)
    {
        entry.instrument = std::make_shared<const sfz::Instrument> (sfz::read (file));
        entry.modified = modified;
    }

    return entry.instrument;
}

std::vector<sfz::Region> regionsOf (const samplersound::Reference& sound, int rootKey, const juce::File& projectFolder)
{
    std::vector<sfz::Region> regions;

    if (sound.kind == samplersound::Kind::recording || sound.kind == samplersound::Kind::drum)
    {
        // One recording across the whole keyboard.
        sfz::Region r;
        r.sample = sound.kind == samplersound::Kind::drum ? "drum:" + sound.path : sound.path;
        r.rootKey = juce::jlimit (0, 127, rootKey);
        regions.push_back (r);
        return regions;
    }

    const auto file = sfzFileOf (sound, projectFolder);
    const auto instrument = readSfz (file);

    if (instrument == nullptr)
        return regions;

    regions = instrument->regions;
    for (auto& r : regions)
        r.sample = file.getParentDirectory().getChildFile (r.sample).getFullPathName();

    return regions;
}

std::map<juce::String, float> suggestedSettings (const samplersound::Reference& sound, const juce::File& projectFolder)
{
    // Everything back to the sampler's defaults first, so nothing is left over from the last sound.
    std::map<juce::String, float> settings;
    for (const auto& p : dsp::sampler::parameters())
        settings[p.id.toString()] = p.defaultValue;

    if (sound.kind == samplersound::Kind::builtIn || sound.kind == samplersound::Kind::sfz)
    {
        if (const auto instrument = readSfz (sfzFileOf (sound, projectFolder)))
        {
            if (instrument->attack)  settings["attack"] = *instrument->attack * 1000.0f;
            if (instrument->decay)   settings["decay"] = *instrument->decay * 1000.0f;
            if (instrument->sustain) settings["sustain"] = *instrument->sustain;
            if (instrument->release) settings["release"] = *instrument->release * 1000.0f;

            // amp_veltrack 0 (organ, harpsichord): playing harder doesn't make it louder.
            if (instrument->velocityTracking)
                settings["touch"] = std::clamp (*instrument->velocityTracking, 0.0f, 100.0f) * 0.7f;
        }

        // Solo instruments play one note at a time: an overlapping note takes over smoothly.
        if (sound.kind == samplersound::Kind::builtIn)
            for (const auto& entry : list())
                if (entry.reference == sound && entry.solo)
                    settings["voiceMode"] = (float) dsp::sampler::legato;

        return settings;
    }

    // Single recordings and drum sounds: short release; 808s and basses slide.
    settings["release"] = 120.0f;
    const auto name = sound.path.toLowerCase();

    if (sound.kind == samplersound::Kind::drum || name.contains ("808") || name.contains ("bass"))
        settings["voiceMode"] = (float) dsp::sampler::slide;

    return settings;
}

juce::Array<juce::File> filesUsed (const Project& project, const juce::File& projectFolder)
{
    juce::Array<juce::File> files;

    for (int t = 0; t < project.getNumTracks(); ++t)
    {
        const auto id = project.getTrack (t).getId();
        const auto sound = samplersound::of (project, id);

        for (const auto& r : regionsOf (sound, samplersound::rootKeyOf (project, id), projectFolder))
            if (! r.sample.startsWith ("drum:"))
                files.addIfNotAlreadyThere (projectFolder.getChildFile (r.sample));
    }

    return files;
}

std::pair<int, int> loadingProgress (const Project& project, const juce::String& trackId, const juce::File& projectFolder,
                                     AudioFileCache& cache)
{
    const auto sound = samplersound::of (project, trackId);
    const auto regions = regionsOf (sound, samplersound::rootKeyOf (project, trackId), projectFolder);
    int ready = 0, total = 0;

    for (const auto& r : regions)
    {
        if (r.sample.startsWith ("drum:"))
        {
            ++ready;
            ++total;
            continue;
        }

        const auto file = projectFolder.getChildFile (r.sample);
        if (! file.existsAsFile())
            continue;   // missing recordings aren't counted (so "0 of 0" means none were found)

        ++total;
        if (cache.getState (file) == AudioFileCache::State::ready)
            ++ready;
    }

    return { ready, total };
}

dsp::SampleMap buildMap (const std::vector<sfz::Region>& regions, const std::vector<std::shared_ptr<const AudioData>>& audio)
{
    dsp::SampleMap map;
    map.zones.reserve (regions.size());

    for (size_t i = 0; i < regions.size() && i < audio.size(); ++i)
    {
        const auto& data = audio[i];
        if (data == nullptr || data->samples.getNumSamples() < 4)
            continue;

        const auto& r = regions[i];
        const auto scale = data->sourceRate > 0.0 ? data->sampleRate / data->sourceRate : 1.0;
        auto toEngine = [scale] (std::int64_t sample) { return sample < 0 ? sample : (std::int64_t) std::llround ((double) sample * scale); };

        dsp::SampleZone z;
        z.sample = &data->samples;
        z.lowKey = r.lowKey;
        z.highKey = r.highKey;
        z.lowVelocity = r.lowVelocity;
        z.highVelocity = r.highVelocity;
        z.rootKey = r.rootKey;
        z.tune = r.tune;
        z.gain = (float) juce::Decibels::decibelsToGain ((double) r.volume, -100.0);
        z.pan = r.pan / 100.0f;
        z.sequenceLength = r.sequenceLength;
        z.sequencePosition = r.sequencePosition;
        z.start = std::clamp<std::int64_t> (toEngine (r.offset), 0, data->samples.getNumSamples() - 4);
        z.end = r.end > 0 ? toEngine (r.end) : -1;
        z.oneShot = r.oneShot;

        if (r.loop && r.loopStart >= 0 && r.loopEnd > r.loopStart)
        {
            // (Converting the rate can round the end a sample past the recording's end.)
            z.loopStart = toEngine (r.loopStart);
            z.loopEnd = std::min<std::int64_t> (toEngine (r.loopEnd), data->samples.getNumSamples());
        }

        map.zones.push_back (z);
    }

    return map;
}

} // namespace aether::samplerlibrary
