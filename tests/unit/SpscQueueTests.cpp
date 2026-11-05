// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include <cstdint>
#include <thread>

#include <catch2/catch_test_macros.hpp>

#include "engine/rt/SpscQueue.h"

using aether::rt::SpscQueue;

TEST_CASE ("Queue capacity is rounded up to a power of two", "[rt][queue]")
{
    CHECK (SpscQueue<int> (1).capacity() == 1);
    CHECK (SpscQueue<int> (5).capacity() == 8);
    CHECK (SpscQueue<int> (64).capacity() == 64);
    CHECK (SpscQueue<int> (65).capacity() == 128);
}

TEST_CASE ("Items come out in the order they went in", "[rt][queue]")
{
    SpscQueue<int> queue (8);
    int value = 0;

    CHECK_FALSE (queue.tryPop (value));

    for (int i = 0; i < 8; ++i)
        REQUIRE (queue.tryPush (i));

    CHECK (queue.sizeApprox() == 8);

    for (int i = 0; i < 8; ++i)
    {
        REQUIRE (queue.tryPop (value));
        CHECK (value == i);
    }

    CHECK_FALSE (queue.tryPop (value));
    CHECK (queue.sizeApprox() == 0);
}

TEST_CASE ("A full queue refuses new items instead of overwriting or allocating", "[rt][queue]")
{
    SpscQueue<int> queue (4);

    for (int i = 0; i < 4; ++i)
        REQUIRE (queue.tryPush (i));

    CHECK_FALSE (queue.tryPush (100));

    int value = -1;
    REQUIRE (queue.tryPop (value));
    CHECK (value == 0);

    // One slot is free again.
    CHECK (queue.tryPush (4));
    CHECK_FALSE (queue.tryPush (5));
}

TEST_CASE ("The queue keeps working after wrapping around many times", "[rt][queue]")
{
    SpscQueue<int> queue (4);
    int value = 0;

    for (int i = 0; i < 10000; ++i)
    {
        REQUIRE (queue.tryPush (i));
        REQUIRE (queue.tryPush (i + 1));
        REQUIRE (queue.tryPop (value));
        REQUIRE (value == i);
        REQUIRE (queue.tryPop (value));
        REQUIRE (value == i + 1);
    }
}

TEST_CASE ("Two threads can use the queue at the same time without losing or reordering items", "[rt][queue][threads]")
{
    constexpr std::uint64_t numItems = 500000;
    SpscQueue<std::uint64_t> queue (256);

    std::thread producer ([&]
    {
        for (std::uint64_t i = 0; i < numItems; ++i)
            while (! queue.tryPush (i))
                std::this_thread::yield();
    });

    std::uint64_t expected = 0;
    bool inOrder = true;

    while (expected < numItems)
    {
        std::uint64_t value = 0;

        if (queue.tryPop (value))
        {
            inOrder = inOrder && (value == expected);
            ++expected;
        }
        else
        {
            std::this_thread::yield();
        }
    }

    producer.join();

    CHECK (inOrder);
    CHECK (expected == numItems);
}
