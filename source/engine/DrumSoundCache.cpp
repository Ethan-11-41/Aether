// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "DrumSoundCache.h"

#include <map>
#include <mutex>

#include "dsp/DrumSounds.h"

namespace aether
{

std::shared_ptr<const AudioData> builtInDrumSound (const juce::String& soundId, double sampleRate)
{
    static std::mutex lock;
    static std::map<std::pair<juce::String, int>, std::shared_ptr<const AudioData>> cache;

    const auto rate = sampleRate > 0.0 ? sampleRate : 48000.0;
    const std::lock_guard<std::mutex> guard (lock);
    auto& entry = cache[{ soundId, juce::roundToInt (rate) }];

    if (entry == nullptr)
    {
        auto samples = dsp::drums::render (soundId, rate);
        if (samples.getNumSamples() == 0)
            return nullptr;

        auto data = std::make_shared<AudioData>();
        data->samples = std::move (samples);
        data->sampleRate = data->sourceRate = rate;
        entry = std::move (data);
    }

    return entry;
}

} // namespace aether
