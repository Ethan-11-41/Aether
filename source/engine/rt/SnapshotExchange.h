// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <atomic>
#include <memory>

#include "RealtimeAnnotations.h"
#include "SpscQueue.h"

namespace aether::rt
{

/** Hands read-only data from the UI thread to the audio thread, without locks, and without
    the audio thread ever allocating or freeing memory.

    The UI thread builds a complete new object and publish()es it. At the start of each audio
    block the audio thread calls acquire(), which switches to the newest published object (one
    atomic pointer swap). The object it stops using is passed back through a lock-free queue,
    and the UI thread deletes it in collectGarbage().

    This is how edits reach the audio engine: e.g. a tempo change produces a new tempo map.
*/
template <typename T>
class SnapshotExchange
{
public:
    explicit SnapshotExchange (std::unique_ptr<T> initial, std::size_t garbageCapacity = 64)
        : active (initial.release()), garbage (garbageCapacity)
    {
    }

    /** Only destroy this when the audio thread has stopped using it. */
    ~SnapshotExchange()
    {
        delete pending.exchange (nullptr);
        delete active;
        collectGarbage();
    }

    SnapshotExchange (const SnapshotExchange&) = delete;
    SnapshotExchange& operator= (const SnapshotExchange&) = delete;

    /** UI thread: makes `next` the data the audio thread will use from its next block. */
    void publish (std::unique_ptr<T> next)
    {
        // If the audio thread hadn't picked up the previous one yet, it never will: delete it here.
        delete pending.exchange (next.release(), std::memory_order_acq_rel);
    }

    /** UI thread: deletes the objects the audio thread has finished with. Call regularly. */
    void collectGarbage()
    {
        T* old = nullptr;
        while (garbage.tryPop (old))
            delete old;
    }

    /** Audio thread: returns the newest published data. */
    const T& acquire() noexcept AETHER_NONBLOCKING
    {
        // Only switch if the old object can be handed back; otherwise keep using the current one
        // for now (the UI thread will empty the queue soon) rather than freeing it here.
        if (pending.load (std::memory_order_acquire) != nullptr && garbage.sizeApprox() < garbage.capacity())
        {
            if (auto* next = pending.exchange (nullptr, std::memory_order_acq_rel))
            {
                garbage.tryPush (active);
                active = next;
            }
        }

        return *active;
    }

    /** Audio thread (or when the audio thread isn't running): the data in use right now. */
    const T& current() const noexcept AETHER_NONBLOCKING   { return *active; }

private:
    std::atomic<T*> pending { nullptr };
    T* active;               // only touched by the audio thread once audio is running
    SpscQueue<T*> garbage;   // audio thread -> UI thread
};

} // namespace aether::rt
