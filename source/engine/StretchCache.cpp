// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "StretchCache.h"

#include <algorithm>
#include <cmath>

namespace aether
{

namespace
{
    /** A short fingerprint (FNV-1a, 64 bits) of numbers. */
    struct Fingerprint
    {
        std::uint64_t value = 1469598103934665603ULL;

        void add (std::int64_t x)
        {
            for (int i = 0; i < 8; ++i)
            {
                value ^= (std::uint64_t) ((x >> (i * 8)) & 0xff);
                value *= 1099511628211ULL;
            }
        }

        void add (double x)   { add ((std::int64_t) std::llround (x * 1000.0)); }
    };
}

StretchJob StretchJob::make (const Clip& clip, const TempoMap& tempo, std::shared_ptr<const AudioData> audioToUse)
{
    StretchJob job;
    job.clipId = clip.getId();
    job.audio = std::move (audioToUse);

    const auto engineRate = job.audio != nullptr ? job.audio->sampleRate : clip.getSourceRate();
    const auto ratio = engineRate / clip.getSourceRate();
    const auto toEngine = [ratio] (juce::int64 samples) { return (std::int64_t) std::llround ((double) samples * ratio); };

    job.offset = toEngine (clip.getOffset());
    job.contentLength = std::max<std::int64_t> (1, toEngine (clip.getContentLength()));
    job.phase = clip.isLooping() ? toEngine (clip.getPhase()) % job.contentLength : 0;
    job.duration = std::max<std::int64_t> (1, toEngine (clip.getDuration()));
    job.loop = clip.isLooping();
    job.semitones = warp::transposeOf (clip);

    for (const auto& point : warp::timeMap (clip, tempo))
        job.map.push_back ({ point.seconds * engineRate, point.played * ratio });

    job.outputLength = job.map.empty() ? 0 : (std::int64_t) std::llround (job.map.back().output);

    Fingerprint print;
    for (const auto x : { job.offset, job.contentLength, job.phase, job.duration, job.outputLength, (std::int64_t) job.loop })
        print.add (x);
    print.add (job.semitones);
    print.add (engineRate);
    for (const auto& p : job.map)
    {
        print.add (p.output);
        print.add (p.input);
    }

    job.key = clip.getFile() + "#" + juce::String::toHexString ((juce::int64) print.value);
    return job;
}

std::shared_ptr<const AudioData> StretchJob::render (const std::function<bool()>& shouldStop) const
{
    if (audio == nullptr || audio->samples.getNumSamples() == 0 || outputLength <= 0)
        return {};

    // The clip's audio in the order it plays (a loop repeats; outside a clip that doesn't loop,
    // the rest of its file, so the stretcher hears what's around the clip's edges).
    const auto& samples = audio->samples;
    const auto fileLength = (std::int64_t) samples.getNumSamples();

    dsp::timestretch::Source source;
    source.numChannels = std::min (2, samples.getNumChannels());
    source.length = duration;
    source.read = [this, &samples, fileLength] (juce::int64 from, int count, juce::AudioBuffer<float>& out)
    {
        for (int c = 0; c < out.getNumChannels(); ++c)
        {
            auto* dest = out.getWritePointer (c);
            const auto* src = samples.getReadPointer (std::min (c, samples.getNumChannels() - 1));

            for (int i = 0; i < count; ++i)
            {
                const auto played = from + i;
                const auto at = loop ? offset + ((phase + played) % contentLength + contentLength) % contentLength
                                     : offset + played;
                dest[i] = at >= 0 && at < fileLength ? src[at] : 0.0f;
            }
        }
    };

    dsp::timestretch::Options options;
    options.sampleRate = audio->sampleRate;
    options.semitones = semitones;
    options.shouldStop = shouldStop;

    auto stretched = dsp::timestretch::render (source, map, outputLength, options);

    if (stretched.getNumSamples() == 0)
        return {};

    auto result = std::make_shared<AudioData>();
    result->samples = std::move (stretched);
    result->sampleRate = audio->sampleRate;
    result->sourceRate = audio->sampleRate;
    return result;
}

//==============================================================================
StretchCache::StretchCache()
    : workers (juce::ThreadPoolOptions{}.withThreadName ("Aether stretching").withNumberOfThreads (2))
{
}

StretchCache::~StretchCache()
{
    {
        std::lock_guard<std::mutex> guard (lock);
        for (auto& [key, entry] : entries)
            entry.stop->store (true);
    }

    workers.removeAllJobs (true, 10000);
}

StretchCache::Result StretchCache::get (const StretchJob& job)
{
    std::lock_guard<std::mutex> guard (lock);

    const auto existing = entries.find (job.key);

    if (existing != entries.end() && existing->second.ready)
        return { existing->second.audio, true };

    // Not asked for yet (or stopped before it was made): make it.
    if (existing == entries.end() || existing->second.stop->load())
    {
        // A newer job for the clip: older ones still being made aren't needed.
        for (auto& [key, entry] : entries)
            if (entry.clipId == job.clipId && ! entry.ready)
                entry.stop->store (true);

        Entry entry;
        entry.clipId = job.clipId;
        entry.order = ++counter;
        auto stop = entry.stop;
        entries.insert_or_assign (job.key, std::move (entry));

        // (The cache waits for its workers when it goes, so they can use it.)
        workers.addJob ([this, job, stop]
        {
            if (stop->load())
                return;

            auto audio = job.render ([stop] { return stop->load(); });

            {
                std::lock_guard<std::mutex> g (lock);
                auto found = entries.find (job.key);

                if (found == entries.end() || found->second.stop != stop)
                    return;

                if (audio == nullptr)
                {
                    entries.erase (found);   // stopped (or nothing to do): asked again, it starts again
                    return;
                }

                found->second.audio = std::move (audio);
                found->second.ready = true;
            }

            sendChangeMessage();
        });
    }

    // Meanwhile: the clip's most recent result.
    const Entry* latest = nullptr;
    for (const auto& [key, entry] : entries)
        if (entry.clipId == job.clipId && entry.ready && (latest == nullptr || entry.order > latest->order))
            latest = &entry;

    return { latest != nullptr ? latest->audio : nullptr, false };
}

bool StretchCache::isBusy() const
{
    std::lock_guard<std::mutex> guard (lock);
    return std::any_of (entries.begin(), entries.end(), [] (const auto& e) { return ! e.second.ready && ! e.second.stop->load(); });
}

bool StretchCache::isPreparing (const juce::String& clipId) const
{
    std::lock_guard<std::mutex> guard (lock);
    return std::any_of (entries.begin(), entries.end(), [&clipId] (const auto& e)
    {
        return e.second.clipId == clipId && ! e.second.ready && ! e.second.stop->load();
    });
}

void StretchCache::keepOnly (const std::set<juce::String>& inUse)
{
    std::lock_guard<std::mutex> guard (lock);

    for (auto it = entries.begin(); it != entries.end();)
    {
        if (inUse.count (it->first) == 0)
        {
            it->second.stop->store (true);
            it = entries.erase (it);
        }
        else
        {
            ++it;
        }
    }
}

void StretchCache::clear()
{
    keepOnly ({});
}

void StretchCache::waitUntilIdle()
{
    while (workers.getNumJobs() > 0)
        juce::Thread::sleep (5);
}

int StretchCache::getNumEntries() const
{
    std::lock_guard<std::mutex> guard (lock);
    return (int) entries.size();
}

} // namespace aether
