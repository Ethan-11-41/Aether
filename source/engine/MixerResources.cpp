// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "MixerResources.h"

namespace aether
{

void MixerResources::beginBuild()
{
    for (auto& [id, entry] : channels) entry.used = false;
    for (auto& [id, entry] : sends)    entry.used = false;
    for (auto& [id, entry] : effects)  entry.used = false;
    for (auto& [id, entry] : instruments) entry.used = false;
}

template <typename T>
void MixerResources::dropUnused (std::map<juce::String, Entry<T>>& entries)
{
    for (auto it = entries.begin(); it != entries.end();)
        it = it->second.used ? std::next (it) : entries.erase (it);
}

void MixerResources::endBuild()
{
    dropUnused (channels);
    dropUnused (sends);
    dropUnused (effects);
    dropUnused (instruments);
}

std::shared_ptr<ChannelState> MixerResources::channel (const juce::String& channelId)
{
    auto& entry = channels[channelId];

    if (entry.object == nullptr)
        entry.object = std::make_shared<ChannelState>();

    entry.used = true;
    return entry.object;
}

std::shared_ptr<SendState> MixerResources::send (const juce::String& sendId)
{
    auto& entry = sends[sendId];

    if (entry.object == nullptr)
        entry.object = std::make_shared<SendState>();

    entry.used = true;
    return entry.object;
}

std::shared_ptr<dsp::Effect> MixerResources::effect (const juce::String& insertId, const juce::String& type, double sampleRate)
{
    auto& entry = effects[insertId];

    if (entry.object == nullptr || entry.type != type || entry.sampleRate != sampleRate)
    {
        std::shared_ptr<dsp::Effect> made = dsp::createEffect (type);

        if (made != nullptr)
            made->prepare (sampleRate, mixChunkSize);

        entry.object = std::move (made);
        entry.type = type;
        entry.sampleRate = sampleRate;
    }

    entry.used = true;
    return entry.object;
}

std::shared_ptr<dsp::Instrument> MixerResources::instrument (const juce::String& instrumentId, const juce::String& type, double sampleRate)
{
    auto& entry = instruments[instrumentId];

    if (entry.object == nullptr || entry.type != type || entry.sampleRate != sampleRate)
    {
        std::shared_ptr<dsp::Instrument> made = dsp::createInstrument (type);

        if (made != nullptr)
            made->prepare (sampleRate, mixChunkSize);

        entry.object = std::move (made);
        entry.type = type;
        entry.sampleRate = sampleRate;
    }

    entry.used = true;
    return entry.object;
}

std::pair<float, float> MixerResources::takePeaks (const juce::String& channelId)
{
    const auto it = channels.find (channelId);

    if (it == channels.end() || it->second.object == nullptr)
        return { 0.0f, 0.0f };

    return { it->second.object->takePeak (0), it->second.object->takePeak (1) };
}

} // namespace aether
