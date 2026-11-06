// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "RecordingRing.h"

#include <algorithm>
#include <cstring>

namespace aether
{

RecordingRing::RecordingRing (std::vector<int> deviceChannels, int capacityFrames)
    : channels (std::move (deviceChannels)), capacity (std::max (1, capacityFrames)),
      data ((size_t) capacity * std::max<size_t> (1, channels.size()), 0.0f)
{
}

void RecordingRing::push (const float* const* inputs, int numInputs, int numFrames) noexcept AETHER_NONBLOCKING
{
    if (numFrames <= 0)
        return;

    const auto writePosition = written.load (std::memory_order_relaxed);
    const auto readPosition = read.load (std::memory_order_acquire);

    if (writePosition - readPosition + numFrames > capacity)
    {
        dropped.fetch_add (numFrames, std::memory_order_relaxed);
        return;
    }

    const auto start = (int) (writePosition % capacity);
    const auto firstPart = std::min (numFrames, capacity - start);

    for (size_t c = 0; c < channels.size(); ++c)
    {
        auto* destination = data.data() + c * (size_t) capacity;
        const auto deviceChannel = channels[c];
        const float* source = (inputs != nullptr && deviceChannel >= 0 && deviceChannel < numInputs) ? inputs[deviceChannel] : nullptr;

        if (source != nullptr)
        {
            std::memcpy (destination + start, source, sizeof (float) * (size_t) firstPart);
            std::memcpy (destination, source + firstPart, sizeof (float) * (size_t) (numFrames - firstPart));
        }
        else
        {
            std::memset (destination + start, 0, sizeof (float) * (size_t) firstPart);
            std::memset (destination, 0, sizeof (float) * (size_t) (numFrames - firstPart));
        }
    }

    written.store (writePosition + numFrames, std::memory_order_release);
}

int RecordingRing::available() const noexcept
{
    return (int) (written.load (std::memory_order_acquire) - read.load (std::memory_order_relaxed));
}

int RecordingRing::pop (juce::AudioBuffer<float>& destination, int maxFrames)
{
    const auto readPosition = read.load (std::memory_order_relaxed);
    const auto count = std::min (maxFrames, (int) (written.load (std::memory_order_acquire) - readPosition));

    if (count <= 0)
        return 0;

    const auto start = (int) (readPosition % capacity);
    const auto firstPart = std::min (count, capacity - start);

    for (size_t c = 0; c < channels.size() && (int) c < destination.getNumChannels(); ++c)
    {
        const auto* source = data.data() + c * (size_t) capacity;
        destination.copyFrom ((int) c, 0, source + start, firstPart);

        if (count > firstPart)
            destination.copyFrom ((int) c, firstPart, source, count - firstPart);
    }

    read.store (readPosition + count, std::memory_order_release);
    return count;
}

} // namespace aether
