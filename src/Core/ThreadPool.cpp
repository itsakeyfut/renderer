/**
 * @file ThreadPool.cpp
 * @brief Implementation of the ThreadPool class.
 */

#include "Core/ThreadPool.h"
#include "Core/Log.h"

namespace Core {

Ref<ThreadPool> ThreadPool::Create(size_t numThreads)
{
    if (numThreads == 0) {
        numThreads = std::thread::hardware_concurrency();
        if (numThreads == 0) {
            numThreads = 2;  // Fallback if hardware_concurrency returns 0
        }
    }

    return Ref<ThreadPool>(new ThreadPool(numThreads));
}

ThreadPool::ThreadPool(size_t numThreads)
{
    LOG_DEBUG("Creating ThreadPool with {} worker threads", numThreads);

    m_Workers.reserve(numThreads);

    for (size_t i = 0; i < numThreads; ++i) {
        m_Workers.emplace_back(&ThreadPool::WorkerThread, this);
    }
}

ThreadPool::~ThreadPool()
{
    Shutdown();
}

void ThreadPool::Shutdown()
{
    {
        std::lock_guard<std::mutex> lock(m_QueueMutex);
        if (m_Stopping) {
            return;  // Already shutting down
        }
        m_Stopping = true;
    }

    // Wake up all workers
    m_Condition.notify_all();

    // Wait for all workers to finish
    for (auto& worker : m_Workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }

    LOG_DEBUG("ThreadPool shutdown complete");
}

size_t ThreadPool::GetPendingTaskCount() const
{
    std::lock_guard<std::mutex> lock(m_QueueMutex);
    return m_TaskQueue.size();
}

void ThreadPool::WaitForAll()
{
    std::unique_lock<std::mutex> lock(m_QueueMutex);

    m_FinishedCondition.wait(lock, [this]() {
        return m_TaskQueue.empty() && m_ActiveTasks == 0;
    });
}

void ThreadPool::WorkerThread()
{
    while (true) {
        Task task;

        {
            std::unique_lock<std::mutex> lock(m_QueueMutex);

            m_Condition.wait(lock, [this]() {
                return m_Stopping || !m_TaskQueue.empty();
            });

            if (m_Stopping && m_TaskQueue.empty()) {
                return;
            }

            task = std::move(const_cast<Task&>(m_TaskQueue.top()));
            m_TaskQueue.pop();
            ++m_ActiveTasks;
        }

        // Execute task outside the lock
        task.Func();

        {
            std::lock_guard<std::mutex> lock(m_QueueMutex);
            --m_ActiveTasks;
        }

        // Notify WaitForAll if queue is empty and no active tasks
        m_FinishedCondition.notify_all();
    }
}

} // namespace Core
