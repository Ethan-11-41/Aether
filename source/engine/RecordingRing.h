// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <atomic>
#include <cstdint>
#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>

#include "rt/RealtimeAnnotations.h"

namespace aether
{

/** Carries recorded sound from the audio thread to the thread that writes it to disk, without
    locks and without the audio thread allocating anything.

    One producer (the audio thread) and one consumer (the disk writer). It holds the chosen
    sound-card input channels, a few seconds' worth. If the disk ever falls so far behind that the
    ring is full, the audio thread drops the new sound and counts it, rather than waiting.
*/
class RecordingRing
{
public:
    /** `deviceChannels`: which sound-card inputs to record (0 = input 1). Allocates the memory. */
    RecordingRing (std::vector<int> deviceChannels, int capacityFrames);

    const std::vector<int>& getChannels() const noexcept   { return channels; }
    int getCapacity() const noexcept                       { return capacity; }

    /** Audio thread: appends `numFrames` of the chosen channels from the device's inputs
        (a missing channel records silence). */
    void push (const float* const* inputs, int numInputs, int numFrames) noexcept AETHER_NONBLOCKING;

    /** Writer: moves up to `maxFrames` into `destination` (one channel per recorded channel, at
        least `maxFrames` long). Returns how many frames it moved. */
    int pop (juce::AudioBuffer<float>& destination, int maxFrames);

    /** Frames waiting to be written. */
    int available() const noexcept;

    /** Frames the audio thread had to throw away because the ring was full. */
    std::int64_t getDroppedFrames() const noexcept   { return dropped.load (std::memory_order_relaxed); }

private:
    std::vector<int> channels;
    int capacity;
    std::vector<float> data;   // channel c, frame f at [c * capacity + f % capacity]
    std::atomic<std::int64_t> written { 0 }, read { 0 }, dropped { 0 };
};

} // namespace aether
