/**
 * @file RHIDeletionQueue.cpp
 * @brief Implementation of deferred deletion queue.
 */

#include "RHI/RHIDeletionQueue.h"
#include "Core/Assert.h"
#include "Core/Log.h"

namespace RHI
{
    Core::Ref<RHIDeletionQueue> RHIDeletionQueue::Create(uint32_t frameDelay)
    {
        return Core::Ref<RHIDeletionQueue>(new RHIDeletionQueue(frameDelay));
    }

    RHIDeletionQueue::RHIDeletionQueue(uint32_t frameDelay)
        : m_FrameDelay(frameDelay)
    {
        ASSERT_MSG(frameDelay > 0, "Frame delay must be at least 1");
        LOG_DEBUG("DeletionQueue created with {} frame delay", frameDelay);
    }

    RHIDeletionQueue::~RHIDeletionQueue()
    {
        // Execute all remaining deletions on destruction
        FlushAll();
    }

    void RHIDeletionQueue::Push(Deletor&& deletor)
    {
        if (!deletor)
        {
            LOG_WARN("Attempted to push null deletor to DeletionQueue");
            return;
        }

        std::lock_guard<std::mutex> lock(m_Mutex);

        DeletionEntry entry;
        entry.Deletor = std::move(deletor);
        entry.QueuedFrame = m_CurrentFrame;

        uint32_t queuedFrame = entry.QueuedFrame;
        m_DeletionQueue.push_back(std::move(entry));

        LOG_TRACE("Queued deletion for frame {} (current: {})",
                  queuedFrame + m_FrameDelay, m_CurrentFrame);
    }

    void RHIDeletionQueue::Push(const Deletor& deletor)
    {
        if (!deletor)
        {
            LOG_WARN("Attempted to push null deletor to DeletionQueue");
            return;
        }

        std::lock_guard<std::mutex> lock(m_Mutex);

        DeletionEntry entry;
        entry.Deletor = deletor;
        entry.QueuedFrame = m_CurrentFrame;

        uint32_t queuedFrame = entry.QueuedFrame;
        m_DeletionQueue.push_back(std::move(entry));

        LOG_TRACE("Queued deletion for frame {} (current: {})",
                  queuedFrame + m_FrameDelay, m_CurrentFrame);
    }

    void RHIDeletionQueue::Flush(uint32_t currentFrame)
    {
        std::lock_guard<std::mutex> lock(m_Mutex);

        m_CurrentFrame = currentFrame;

        // Process all entries that have waited long enough
        size_t deletedCount = 0;

        while (!m_DeletionQueue.empty())
        {
            const auto& entry = m_DeletionQueue.front();

            // Check if this entry has waited long enough
            // Handle potential frame counter wrap-around by using signed arithmetic
            int32_t framesSinceQueued = static_cast<int32_t>(currentFrame) -
                                        static_cast<int32_t>(entry.QueuedFrame);

            // If frames since queued is negative (wrap-around), add UINT32_MAX
            // For practical purposes, we assume the frame counter won't actually wrap
            // during normal operation, but handle it gracefully
            if (framesSinceQueued < 0)
            {
                // This could happen if currentFrame wrapped around
                // In this case, assume enough time has passed
                framesSinceQueued = static_cast<int32_t>(m_FrameDelay);
            }

            if (static_cast<uint32_t>(framesSinceQueued) < m_FrameDelay)
            {
                // Entry hasn't waited long enough, and since queue is ordered,
                // no more entries need processing
                break;
            }

            // Execute the deletor
            if (entry.Deletor)
            {
                entry.Deletor();
                deletedCount++;
            }

            m_DeletionQueue.pop_front();
        }

        if (deletedCount > 0)
        {
            LOG_TRACE("DeletionQueue: Flushed {} deletions at frame {}",
                      deletedCount, currentFrame);
        }
    }

    void RHIDeletionQueue::FlushAll()
    {
        std::lock_guard<std::mutex> lock(m_Mutex);

        if (m_DeletionQueue.empty())
        {
            return;
        }

        size_t count = m_DeletionQueue.size();
        LOG_DEBUG("DeletionQueue: Flushing all {} pending deletions", count);

        // Execute all deletors regardless of frame
        while (!m_DeletionQueue.empty())
        {
            auto& entry = m_DeletionQueue.front();

            if (entry.Deletor)
            {
                entry.Deletor();
            }

            m_DeletionQueue.pop_front();
        }

        LOG_DEBUG("DeletionQueue: FlushAll completed, {} deletions executed", count);
    }

    size_t RHIDeletionQueue::GetPendingCount() const
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_DeletionQueue.size();
    }

    bool RHIDeletionQueue::IsEmpty() const
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_DeletionQueue.empty();
    }

} // namespace RHI
