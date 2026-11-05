// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <atomic>
#include <cstddef>
#include <type_traits>
#include <vector>

#include "RealtimeAnnotations.h"

namespace aether::rt
{

/** A fixed-size, lock-free, single-producer / single-consumer queue.

    This is how Aether's threads talk to each other without locks: for example the audio
    thread pushes meter readings that the UI thread pops, and the UI thread pushes commands
    that the audio thread pops.

    Rules:
      - Exactly one thread may call tryPush(), and exactly one (other) thread may call tryPop().
      - All memory is allocated in the constructor; tryPush() and tryPop() never allocate,
        never lock and never wait. If the queue is full, tryPush() returns false.
      - T must be default-constructible and nothrow move-assignable (keep it small and simple).
*/
template <typename T>
class SpscQueue
{
public:
    static_assert (std::is_default_constructible_v<T>, "SpscQueue elements must be default-constructible");
    static_assert (std::is_nothrow_move_assignable_v<T>, "SpscQueue elements must be nothrow move-assignable");

    /** Creates a queue that can hold at least `minimumCapacity` items (rounded up to a power of 2). */
    explicit SpscQueue (std::size_t minimumCapacity)
        : slots (roundUpToPowerOfTwo (minimumCapacity)),
          mask (slots.size() - 1)
    {
    }

    SpscQueue (const SpscQueue&) = delete;
    SpscQueue& operator= (const SpscQueue&) = delete;

    /** Producer only. Returns false (and does nothing) if the queue is full. */
    bool tryPush (T item) noexcept AETHER_NONBLOCKING
    {
        const auto tail = writeIndex.load (std::memory_order_relaxed);
        const auto head = readIndex.load (std::memory_order_acquire);

        if (tail - head >= slots.size())
            return false;

        slots[tail & mask] = std::move (item);
        writeIndex.store (tail + 1, std::memory_order_release);
        return true;
    }

    /** Consumer only. Returns false if the queue is empty, otherwise moves the oldest item into `out`. */
    bool tryPop (T& out) noexcept AETHER_NONBLOCKING
    {
        const auto head = readIndex.load (std::memory_order_relaxed);
        const auto tail = writeIndex.load (std::memory_order_acquire);

        if (head == tail)
            return false;

        out = std::move (slots[head & mask]);
        readIndex.store (head + 1, std::memory_order_release);
        return true;
    }

    /** The maximum number of items the queue can hold. */
    std::size_t capacity() const noexcept { return slots.size(); }

    /** Number of items currently queued. Only approximate while other threads are active. */
    std::size_t sizeApprox() const noexcept
    {
        return writeIndex.load (std::memory_order_acquire) - readIndex.load (std::memory_order_acquire);
    }

private:
    static std::size_t roundUpToPowerOfTwo (std::size_t n) noexcept
    {
        std::size_t result = 1;
        while (result < n)
            result <<= 1;
        return result;
    }

    std::vector<T> slots;
    const std::size_t mask;

    // The two indices live on separate cache lines so the two threads don't slow each other down.
    alignas (64) std::atomic<std::size_t> writeIndex { 0 };   // only written by the producer
    alignas (64) std::atomic<std::size_t> readIndex  { 0 };   // only written by the consumer
};

} // namespace aether::rt
