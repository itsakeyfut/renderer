/**
 * @file TestRHIDeletionQueue.cpp
 * @brief Test file for RHI/RHIDeletionQueue.h using GoogleTest.
 *
 * This file tests that the RHIDeletionQueue class correctly defers
 * deletions and executes them after the configured frame delay.
 */

#include <gtest/gtest.h>
#include "RHI/RHIDeletionQueue.h"
#include "Core/Log.h"

#include <atomic>
#include <thread>
#include <vector>

// =============================================================================
// Test Fixture for DeletionQueue Tests
// =============================================================================

class RHIDeletionQueueTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!Core::Log::IsInitialized()) {
            Core::Log::Init();
        }
    }

    void TearDown() override {
        // Nothing to clean up
    }
};

// =============================================================================
// Creation Tests
// =============================================================================

TEST_F(RHIDeletionQueueTest, CreatesQueue) {
    auto queue = RHI::RHIDeletionQueue::Create();

    ASSERT_NE(queue, nullptr);
    EXPECT_TRUE(queue->IsEmpty());
    EXPECT_EQ(queue->GetPendingCount(), 0u);
}

TEST_F(RHIDeletionQueueTest, CreatesQueueWithDefaultDelay) {
    auto queue = RHI::RHIDeletionQueue::Create();

    ASSERT_NE(queue, nullptr);
    EXPECT_EQ(queue->GetFrameDelay(), RHI::DEFAULT_FRAME_DELAY);
}

TEST_F(RHIDeletionQueueTest, CreatesQueueWithCustomDelay) {
    auto queue = RHI::RHIDeletionQueue::Create(5);

    ASSERT_NE(queue, nullptr);
    EXPECT_EQ(queue->GetFrameDelay(), 5u);
}

// =============================================================================
// Push Tests
// =============================================================================

TEST_F(RHIDeletionQueueTest, PushIncreasesCount) {
    auto queue = RHI::RHIDeletionQueue::Create();

    queue->Push([]() {});
    EXPECT_EQ(queue->GetPendingCount(), 1u);

    queue->Push([]() {});
    EXPECT_EQ(queue->GetPendingCount(), 2u);

    queue->Push([]() {});
    EXPECT_EQ(queue->GetPendingCount(), 3u);
}

TEST_F(RHIDeletionQueueTest, PushNullDeletorIgnored) {
    auto queue = RHI::RHIDeletionQueue::Create();

    RHI::RHIDeletionQueue::Deletor nullDeletor;
    queue->Push(std::move(nullDeletor));

    EXPECT_TRUE(queue->IsEmpty());
    EXPECT_EQ(queue->GetPendingCount(), 0u);
}

TEST_F(RHIDeletionQueueTest, PushConstRefWorks) {
    auto queue = RHI::RHIDeletionQueue::Create();
    bool called = false;

    RHI::RHIDeletionQueue::Deletor deletor = [&called]() { called = true; };
    queue->Push(deletor);

    EXPECT_EQ(queue->GetPendingCount(), 1u);

    queue->FlushAll();
    EXPECT_TRUE(called);
}

TEST_F(RHIDeletionQueueTest, PushTemplateWorks) {
    auto queue = RHI::RHIDeletionQueue::Create();
    int* ptr = new int(42);
    bool deleted = false;

    std::function<void(int*)> deletor = [&deleted](int* p) {
        delete p;
        deleted = true;
    };
    queue->Push(ptr, deletor);

    EXPECT_EQ(queue->GetPendingCount(), 1u);

    queue->FlushAll();
    EXPECT_TRUE(deleted);
}

TEST_F(RHIDeletionQueueTest, PushTemplateNullPointerIgnored) {
    auto queue = RHI::RHIDeletionQueue::Create();

    int* nullPtr = nullptr;
    std::function<void(int*)> deletor = [](int* p) { delete p; };
    queue->Push(nullPtr, deletor);

    EXPECT_TRUE(queue->IsEmpty());
}

// =============================================================================
// Flush Tests
// =============================================================================

TEST_F(RHIDeletionQueueTest, FlushDoesNotDeleteBeforeDelay) {
    auto queue = RHI::RHIDeletionQueue::Create(2);  // 2 frame delay
    bool deleted = false;

    queue->Push([&deleted]() { deleted = true; });

    // Frame 0 - should not delete yet (queued at frame 0, need 2 frames)
    queue->Flush(0);
    EXPECT_FALSE(deleted);
    EXPECT_EQ(queue->GetPendingCount(), 1u);

    // Frame 1 - still should not delete (only 1 frame has passed)
    queue->Flush(1);
    EXPECT_FALSE(deleted);
    EXPECT_EQ(queue->GetPendingCount(), 1u);
}

TEST_F(RHIDeletionQueueTest, FlushDeletesAfterDelay) {
    auto queue = RHI::RHIDeletionQueue::Create(2);  // 2 frame delay
    bool deleted = false;

    queue->Push([&deleted]() { deleted = true; });

    // Frame 0 - queued
    queue->Flush(0);
    EXPECT_FALSE(deleted);

    // Frame 1 - one frame passed
    queue->Flush(1);
    EXPECT_FALSE(deleted);

    // Frame 2 - two frames passed, should delete
    queue->Flush(2);
    EXPECT_TRUE(deleted);
    EXPECT_TRUE(queue->IsEmpty());
}

TEST_F(RHIDeletionQueueTest, FlushDeletesMultiple) {
    auto queue = RHI::RHIDeletionQueue::Create(1);  // 1 frame delay
    int deleteCount = 0;

    // Queue 3 deletions at frame 0
    queue->Push([&deleteCount]() { deleteCount++; });
    queue->Push([&deleteCount]() { deleteCount++; });
    queue->Push([&deleteCount]() { deleteCount++; });

    // Frame 0 - queue
    queue->Flush(0);
    EXPECT_EQ(deleteCount, 0);
    EXPECT_EQ(queue->GetPendingCount(), 3u);

    // Frame 1 - should delete all
    queue->Flush(1);
    EXPECT_EQ(deleteCount, 3);
    EXPECT_TRUE(queue->IsEmpty());
}

TEST_F(RHIDeletionQueueTest, FlushDeletesInOrder) {
    auto queue = RHI::RHIDeletionQueue::Create(1);
    std::vector<int> order;

    queue->Push([&order]() { order.push_back(1); });
    queue->Push([&order]() { order.push_back(2); });
    queue->Push([&order]() { order.push_back(3); });

    queue->Flush(0);  // Queue
    queue->Flush(1);  // Execute

    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0], 1);
    EXPECT_EQ(order[1], 2);
    EXPECT_EQ(order[2], 3);
}

TEST_F(RHIDeletionQueueTest, FlushHandlesStaggeredDeletions) {
    auto queue = RHI::RHIDeletionQueue::Create(2);
    int count1 = 0, count2 = 0;

    // Frame 0 - queue first deletion
    // Note: Flush() must be called BEFORE Push() to set the current frame
    queue->Flush(0);
    queue->Push([&count1]() { count1++; });

    // Frame 1 - queue second deletion
    queue->Flush(1);
    queue->Push([&count2]() { count2++; });

    EXPECT_EQ(count1, 0);
    EXPECT_EQ(count2, 0);

    // Frame 2 - first deletion should execute (2 frames since frame 0)
    queue->Flush(2);
    EXPECT_EQ(count1, 1);
    EXPECT_EQ(count2, 0);

    // Frame 3 - second deletion should execute (2 frames since frame 1)
    queue->Flush(3);
    EXPECT_EQ(count1, 1);
    EXPECT_EQ(count2, 1);
}

// =============================================================================
// FlushAll Tests
// =============================================================================

TEST_F(RHIDeletionQueueTest, FlushAllDeletesEverything) {
    auto queue = RHI::RHIDeletionQueue::Create(100);  // Very long delay
    int deleteCount = 0;

    queue->Push([&deleteCount]() { deleteCount++; });
    queue->Push([&deleteCount]() { deleteCount++; });
    queue->Push([&deleteCount]() { deleteCount++; });

    // Should delete all regardless of delay
    queue->FlushAll();

    EXPECT_EQ(deleteCount, 3);
    EXPECT_TRUE(queue->IsEmpty());
}

TEST_F(RHIDeletionQueueTest, FlushAllOnEmptyQueueIsSafe) {
    auto queue = RHI::RHIDeletionQueue::Create();

    // Should not crash
    EXPECT_NO_THROW(queue->FlushAll());
    EXPECT_TRUE(queue->IsEmpty());
}

TEST_F(RHIDeletionQueueTest, FlushAllDeletesInOrder) {
    auto queue = RHI::RHIDeletionQueue::Create(100);
    std::vector<int> order;

    queue->Push([&order]() { order.push_back(1); });
    queue->Push([&order]() { order.push_back(2); });
    queue->Push([&order]() { order.push_back(3); });

    queue->FlushAll();

    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0], 1);
    EXPECT_EQ(order[1], 2);
    EXPECT_EQ(order[2], 3);
}

// =============================================================================
// Destructor Tests
// =============================================================================

TEST_F(RHIDeletionQueueTest, DestructorFlushesAll) {
    int deleteCount = 0;

    {
        auto queue = RHI::RHIDeletionQueue::Create(100);

        queue->Push([&deleteCount]() { deleteCount++; });
        queue->Push([&deleteCount]() { deleteCount++; });
        queue->Push([&deleteCount]() { deleteCount++; });

        // Queue goes out of scope here
    }

    // Destructor should have called FlushAll
    EXPECT_EQ(deleteCount, 3);
}

// =============================================================================
// Thread Safety Tests
// =============================================================================

TEST_F(RHIDeletionQueueTest, ThreadSafePush) {
    auto queue = RHI::RHIDeletionQueue::Create();
    std::atomic<int> counter{0};
    const int numThreads = 4;
    const int pushesPerThread = 100;

    std::vector<std::thread> threads;

    // Spawn threads that each push multiple deletors
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&queue, &counter, pushesPerThread]() {
            for (int j = 0; j < pushesPerThread; ++j) {
                queue->Push([&counter]() { counter++; });
            }
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(queue->GetPendingCount(), static_cast<size_t>(numThreads * pushesPerThread));

    // Flush all and verify all deletors were executed
    queue->FlushAll();
    EXPECT_EQ(counter.load(), numThreads * pushesPerThread);
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_F(RHIDeletionQueueTest, SingleFrameDelay) {
    auto queue = RHI::RHIDeletionQueue::Create(1);
    bool deleted = false;

    queue->Push([&deleted]() { deleted = true; });
    queue->Flush(0);  // Queue

    EXPECT_FALSE(deleted);

    queue->Flush(1);  // Should execute
    EXPECT_TRUE(deleted);
}

TEST_F(RHIDeletionQueueTest, LargeFrameDelay) {
    auto queue = RHI::RHIDeletionQueue::Create(10);
    bool deleted = false;

    queue->Push([&deleted]() { deleted = true; });

    // Flush multiple times, should not delete until frame 10
    for (uint32_t i = 0; i < 10; ++i) {
        queue->Flush(i);
        EXPECT_FALSE(deleted) << "Deleted too early at frame " << i;
    }

    // Frame 10 - should delete
    queue->Flush(10);
    EXPECT_TRUE(deleted);
}

TEST_F(RHIDeletionQueueTest, ContinuousFrameLoop) {
    auto queue = RHI::RHIDeletionQueue::Create(2);
    int deleteCount = 0;

    // Simulate a typical frame loop
    // Note: Flush() is called at the START of each frame to:
    // 1. Set the current frame for subsequent Push() calls
    // 2. Process deletions from previous frames
    for (uint32_t frame = 0; frame < 10; ++frame) {
        // Flush at start of frame
        queue->Flush(frame);

        // Queue a deletion every frame
        queue->Push([&deleteCount]() { deleteCount++; });
    }

    // After 10 frames with 2 frame delay:
    // - Items queued at frames 0-7 will be executed when Flush(2-9) is called
    // - But since we stop at frame 9, items from frames 8-9 are still pending
    // Let's trace it:
    // - Flush(0): set frame to 0, no deletions
    // - Push at frame 0
    // - Flush(1): set frame to 1, no deletions (1-0 < 2)
    // - Push at frame 1
    // - Flush(2): set frame to 2, delete frame 0 (2-0 >= 2), count = 1
    // - Push at frame 2
    // - ...
    // - Flush(9): delete frame 7 (9-7 >= 2), count = 8
    // - Push at frame 9
    // Items from frames 8 and 9 are still pending
    EXPECT_EQ(deleteCount, 8);
    EXPECT_EQ(queue->GetPendingCount(), 2u);

    // Continue for 2 more frames to flush remaining
    queue->Flush(10);  // Deletes frame 8 item
    queue->Flush(11);  // Deletes frame 9 item

    EXPECT_EQ(deleteCount, 10);
    EXPECT_TRUE(queue->IsEmpty());
}

TEST_F(RHIDeletionQueueTest, IsEmptyAndGetPendingCountConsistent) {
    auto queue = RHI::RHIDeletionQueue::Create();

    EXPECT_TRUE(queue->IsEmpty());
    EXPECT_EQ(queue->GetPendingCount(), 0u);

    queue->Push([]() {});

    EXPECT_FALSE(queue->IsEmpty());
    EXPECT_EQ(queue->GetPendingCount(), 1u);

    queue->FlushAll();

    EXPECT_TRUE(queue->IsEmpty());
    EXPECT_EQ(queue->GetPendingCount(), 0u);
}
