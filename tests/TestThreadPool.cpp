/**
 * @file TestThreadPool.cpp
 * @brief Unit tests for Core/ThreadPool.
 *
 * Tests the ThreadPool class for correct task execution,
 * prioritization, and thread safety.
 */

#include <gtest/gtest.h>
#include "Core/ThreadPool.h"
#include "Core/Log.h"

#include <atomic>
#include <chrono>
#include <set>
#include <thread>
#include <vector>

// =============================================================================
// Test Fixture
// =============================================================================

class ThreadPoolTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!Core::Log::IsInitialized()) {
            Core::Log::Init();
        }
    }
};

// =============================================================================
// Creation Tests
// =============================================================================

TEST_F(ThreadPoolTest, CreatesPoolWithSpecifiedThreads) {
    auto pool = Core::ThreadPool::Create(4);

    ASSERT_NE(pool, nullptr);
    EXPECT_EQ(pool->GetThreadCount(), 4u);
    EXPECT_TRUE(pool->IsRunning());
}

TEST_F(ThreadPoolTest, CreatesPoolWithAutoThreads) {
    auto pool = Core::ThreadPool::Create(0);

    ASSERT_NE(pool, nullptr);
    EXPECT_GE(pool->GetThreadCount(), 1u);
    EXPECT_TRUE(pool->IsRunning());
}

TEST_F(ThreadPoolTest, InitiallyEmpty) {
    auto pool = Core::ThreadPool::Create(2);

    EXPECT_EQ(pool->GetPendingTaskCount(), 0u);
}

// =============================================================================
// Submit Tests
// =============================================================================

TEST_F(ThreadPoolTest, SubmitExecutesTask) {
    auto pool = Core::ThreadPool::Create(2);
    std::atomic<bool> executed{false};

    auto future = pool->Submit([&executed]() {
        executed = true;
    });

    future.wait();
    EXPECT_TRUE(executed);
}

TEST_F(ThreadPoolTest, SubmitReturnsValue) {
    auto pool = Core::ThreadPool::Create(2);

    auto future = pool->Submit([]() {
        return 42;
    });

    EXPECT_EQ(future.get(), 42);
}

TEST_F(ThreadPoolTest, SubmitWithArguments) {
    auto pool = Core::ThreadPool::Create(2);

    auto future = pool->Submit([](int a, int b) {
        return a + b;
    }, 10, 20);

    EXPECT_EQ(future.get(), 30);
}

TEST_F(ThreadPoolTest, SubmitMultipleTasks) {
    auto pool = Core::ThreadPool::Create(4);
    std::atomic<int> counter{0};
    const int numTasks = 100;

    std::vector<std::future<void>> futures;
    for (int i = 0; i < numTasks; ++i) {
        futures.push_back(pool->Submit([&counter]() {
            counter++;
        }));
    }

    for (auto& future : futures) {
        future.wait();
    }

    EXPECT_EQ(counter.load(), numTasks);
}

TEST_F(ThreadPoolTest, SubmitToStoppedPoolThrows) {
    auto pool = Core::ThreadPool::Create(2);
    pool->Shutdown();

    EXPECT_THROW(pool->Submit([]() {}), std::runtime_error);
}

// =============================================================================
// Priority Tests
// =============================================================================

TEST_F(ThreadPoolTest, HighPriorityExecutesFirst) {
    // Create a single-threaded pool to ensure ordering
    auto pool = Core::ThreadPool::Create(1);

    // Block the single worker thread
    std::atomic<bool> blocking{true};
    auto blockFuture = pool->Submit([&blocking]() {
        while (blocking) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });

    // Wait a bit for the blocking task to start
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Queue tasks with different priorities
    std::vector<int> executionOrder;
    std::mutex orderMutex;

    auto lowFuture = pool->SubmitWithPriority(
        Core::ThreadPool::Priority::Low,
        [&executionOrder, &orderMutex]() {
            std::lock_guard<std::mutex> lock(orderMutex);
            executionOrder.push_back(0);  // Low = 0
        }
    );

    auto normalFuture = pool->SubmitWithPriority(
        Core::ThreadPool::Priority::Normal,
        [&executionOrder, &orderMutex]() {
            std::lock_guard<std::mutex> lock(orderMutex);
            executionOrder.push_back(1);  // Normal = 1
        }
    );

    auto highFuture = pool->SubmitWithPriority(
        Core::ThreadPool::Priority::High,
        [&executionOrder, &orderMutex]() {
            std::lock_guard<std::mutex> lock(orderMutex);
            executionOrder.push_back(2);  // High = 2
        }
    );

    // Release the blocking task
    blocking = false;
    blockFuture.wait();

    // Wait for all tasks to complete
    lowFuture.wait();
    normalFuture.wait();
    highFuture.wait();

    // Verify high priority executed first, then normal, then low
    ASSERT_EQ(executionOrder.size(), 3u);
    EXPECT_EQ(executionOrder[0], 2);  // High
    EXPECT_EQ(executionOrder[1], 1);  // Normal
    EXPECT_EQ(executionOrder[2], 0);  // Low
}

// =============================================================================
// Shutdown Tests
// =============================================================================

TEST_F(ThreadPoolTest, ShutdownWaitsForTasks) {
    auto pool = Core::ThreadPool::Create(2);
    std::atomic<int> completed{0};

    for (int i = 0; i < 10; ++i) {
        pool->Submit([&completed]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            completed++;
        });
    }

    pool->Shutdown();

    // All tasks should have completed
    EXPECT_EQ(completed.load(), 10);
}

TEST_F(ThreadPoolTest, ShutdownMultipleTimes) {
    auto pool = Core::ThreadPool::Create(2);

    // Should not crash
    pool->Shutdown();
    pool->Shutdown();
    pool->Shutdown();

    EXPECT_FALSE(pool->IsRunning());
}

TEST_F(ThreadPoolTest, DestructorWaitsForTasks) {
    std::atomic<int> completed{0};

    {
        auto pool = Core::ThreadPool::Create(2);

        for (int i = 0; i < 10; ++i) {
            pool->Submit([&completed]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                completed++;
            });
        }

        // Pool destroyed here
    }

    // All tasks should have completed
    EXPECT_EQ(completed.load(), 10);
}

// =============================================================================
// WaitForAll Tests
// =============================================================================

TEST_F(ThreadPoolTest, WaitForAllBlocksUntilComplete) {
    auto pool = Core::ThreadPool::Create(4);
    std::atomic<int> counter{0};

    for (int i = 0; i < 100; ++i) {
        pool->Submit([&counter]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            counter++;
        });
    }

    pool->WaitForAll();

    EXPECT_EQ(counter.load(), 100);
    EXPECT_EQ(pool->GetPendingTaskCount(), 0u);
}

TEST_F(ThreadPoolTest, WaitForAllOnEmptyPool) {
    auto pool = Core::ThreadPool::Create(2);

    // Should not block
    pool->WaitForAll();

    EXPECT_TRUE(pool->IsRunning());
}

// =============================================================================
// Thread Safety Tests
// =============================================================================

TEST_F(ThreadPoolTest, ConcurrentSubmit) {
    auto pool = Core::ThreadPool::Create(4);
    std::atomic<int> counter{0};
    const int numThreads = 4;
    const int tasksPerThread = 100;

    std::vector<std::thread> submitters;
    for (int t = 0; t < numThreads; ++t) {
        submitters.emplace_back([&pool, &counter, tasksPerThread]() {
            for (int i = 0; i < tasksPerThread; ++i) {
                pool->Submit([&counter]() {
                    counter++;
                });
            }
        });
    }

    for (auto& thread : submitters) {
        thread.join();
    }

    pool->WaitForAll();

    EXPECT_EQ(counter.load(), numThreads * tasksPerThread);
}

TEST_F(ThreadPoolTest, TasksRunOnDifferentThreads) {
    auto pool = Core::ThreadPool::Create(4);
    std::set<std::thread::id> threadIds;
    std::mutex idMutex;

    std::vector<std::future<void>> futures;
    for (int i = 0; i < 100; ++i) {
        futures.push_back(pool->Submit([&threadIds, &idMutex]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            std::lock_guard<std::mutex> lock(idMutex);
            threadIds.insert(std::this_thread::get_id());
        }));
    }

    for (auto& future : futures) {
        future.wait();
    }

    // With 4 threads, we should see multiple thread IDs
    EXPECT_GT(threadIds.size(), 1u);
}

// =============================================================================
// GetPendingTaskCount Tests
// =============================================================================

TEST_F(ThreadPoolTest, PendingTaskCountUpdates) {
    auto pool = Core::ThreadPool::Create(1);

    // Block the single worker
    std::atomic<bool> blocking{true};
    pool->Submit([&blocking]() {
        while (blocking) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });

    // Wait for worker to start blocking
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Queue more tasks
    for (int i = 0; i < 5; ++i) {
        pool->Submit([]() {});
    }

    EXPECT_EQ(pool->GetPendingTaskCount(), 5u);

    // Release and wait
    blocking = false;
    pool->WaitForAll();

    EXPECT_EQ(pool->GetPendingTaskCount(), 0u);
}

// =============================================================================
// Exception Handling Tests
// =============================================================================

TEST_F(ThreadPoolTest, ExceptionInTaskPropagatesToFuture) {
    auto pool = Core::ThreadPool::Create(2);

    auto future = pool->Submit([]() -> int {
        throw std::runtime_error("Test exception");
    });

    EXPECT_THROW(future.get(), std::runtime_error);
}

TEST_F(ThreadPoolTest, ExceptionDoesNotStopPool) {
    auto pool = Core::ThreadPool::Create(2);

    // Submit task that throws
    auto badFuture = pool->Submit([]() {
        throw std::runtime_error("Test exception");
    });

    // Submit normal task
    auto goodFuture = pool->Submit([]() {
        return 42;
    });

    // Ignore exception
    try {
        badFuture.get();
    } catch (...) {
    }

    // Good task should still work
    EXPECT_EQ(goodFuture.get(), 42);
    EXPECT_TRUE(pool->IsRunning());
}
