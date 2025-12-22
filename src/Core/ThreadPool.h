/**
 * @file ThreadPool.h
 * @brief Thread pool implementation for async task execution.
 *
 * Provides a reusable thread pool for executing tasks asynchronously.
 * Used by the async resource loader and other systems requiring
 * background thread execution.
 */

#pragma once

#include "Core/Types.h"

#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace Core {

/**
 * @brief Thread pool for executing tasks asynchronously.
 *
 * Features:
 * - Configurable number of worker threads
 * - Task prioritization (higher priority = executed first)
 * - Graceful shutdown with task completion
 * - Support for std::future return values
 *
 * Usage:
 * @code
 * auto pool = ThreadPool::Create(4);  // 4 worker threads
 *
 * // Submit a task with a return value
 * auto future = pool->Submit([]() {
 *     return expensiveComputation();
 * });
 *
 * // Submit a task with priority
 * pool->Submit([]() { doWork(); }, Priority::High);
 *
 * // Wait for result
 * auto result = future.get();
 *
 * // Shutdown (waits for pending tasks)
 * pool->Shutdown();
 * @endcode
 */
class ThreadPool {
public:
    /**
     * @brief Task priority levels.
     *
     * Higher priority tasks are executed before lower priority ones.
     */
    enum class Priority {
        Low = 0,
        Normal = 1,
        High = 2
    };

    /**
     * @brief Factory method to create a thread pool.
     * @param numThreads Number of worker threads. If 0, uses hardware concurrency.
     * @return Shared pointer to the created thread pool.
     */
    static Ref<ThreadPool> Create(size_t numThreads = 0);

    /**
     * @brief Destructor - waits for all tasks to complete.
     */
    ~ThreadPool();

    // Non-copyable, non-movable
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    /**
     * @brief Submit a task for execution.
     *
     * @tparam F Callable type.
     * @tparam Args Argument types.
     * @param func The function to execute.
     * @param args Arguments to pass to the function.
     * @return std::future for the task's return value.
     */
    template<typename F, typename... Args>
    auto Submit(F&& func, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>>
    {
        return SubmitWithPriority(Priority::Normal, std::forward<F>(func), std::forward<Args>(args)...);
    }

    /**
     * @brief Submit a task with a specific priority.
     *
     * @tparam F Callable type.
     * @tparam Args Argument types.
     * @param priority Task priority.
     * @param func The function to execute.
     * @param args Arguments to pass to the function.
     * @return std::future for the task's return value.
     */
    template<typename F, typename... Args>
    auto SubmitWithPriority(Priority priority, F&& func, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>>
    {
        using ReturnType = std::invoke_result_t<F, Args...>;

        auto task = CreateRef<std::packaged_task<ReturnType()>>(
            std::bind(std::forward<F>(func), std::forward<Args>(args)...)
        );

        std::future<ReturnType> result = task->get_future();

        {
            std::lock_guard<std::mutex> lock(m_QueueMutex);

            if (m_Stopping) {
                throw std::runtime_error("ThreadPool: Cannot submit task to stopped pool");
            }

            m_TaskQueue.push(Task{
                static_cast<int>(priority),
                [task]() { (*task)(); }
            });
        }

        m_Condition.notify_one();
        return result;
    }

    /**
     * @brief Shut down the thread pool.
     *
     * Stops accepting new tasks and waits for all pending tasks to complete.
     * Safe to call multiple times.
     */
    void Shutdown();

    /**
     * @brief Check if the pool is running.
     * @return true if the pool is accepting tasks.
     */
    bool IsRunning() const { return !m_Stopping; }

    /**
     * @brief Get the number of worker threads.
     * @return Number of threads in the pool.
     */
    size_t GetThreadCount() const { return m_Workers.size(); }

    /**
     * @brief Get the number of pending tasks.
     * @return Number of tasks waiting to be executed.
     */
    size_t GetPendingTaskCount() const;

    /**
     * @brief Wait for all pending tasks to complete.
     *
     * Blocks until the task queue is empty and all workers are idle.
     */
    void WaitForAll();

private:
    /**
     * @brief Private constructor - use Create() factory method.
     * @param numThreads Number of worker threads to create.
     */
    explicit ThreadPool(size_t numThreads);

    /**
     * @brief Worker thread function.
     */
    void WorkerThread();

    /**
     * @brief Internal task structure with priority.
     */
    struct Task {
        int Priority;
        std::function<void()> Func;

        bool operator<(const Task& other) const {
            // Lower priority value means higher priority in priority_queue
            return Priority < other.Priority;
        }
    };

    std::vector<std::thread> m_Workers;
    std::priority_queue<Task> m_TaskQueue;
    mutable std::mutex m_QueueMutex;
    std::condition_variable m_Condition;
    std::condition_variable m_FinishedCondition;
    std::atomic<bool> m_Stopping{false};
    std::atomic<size_t> m_ActiveTasks{0};
};

} // namespace Core
