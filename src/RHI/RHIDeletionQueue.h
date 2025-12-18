/**
 * @file RHIDeletionQueue.h
 * @brief Deferred deletion queue for safe GPU resource cleanup.
 *
 * Provides a mechanism to safely delete GPU resources that may still be
 * in use by the GPU. Resources are queued for deletion and only destroyed
 * after a configurable number of frames have passed, ensuring the GPU has
 * finished using them.
 *
 * This is essential for Vulkan where deleting resources that are still
 * referenced by in-flight command buffers causes undefined behavior.
 */

#pragma once

#include "Core/Types.h"

#include <array>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>

namespace RHI
{
    /**
     * @brief Default number of frames to wait before deleting resources
     *
     * This should match or exceed MAX_FRAMES_IN_FLIGHT to ensure resources
     * are not deleted while still in use by queued commands.
     */
    static constexpr uint32_t DEFAULT_FRAME_DELAY = 2;

    /**
     * @brief Deferred deletion queue for GPU resources
     *
     * The DeletionQueue manages deferred destruction of GPU resources to prevent
     * destroying resources that are still being used by in-flight frames.
     *
     * In Vulkan, resources (buffers, images, pipelines, etc.) cannot be safely
     * deleted while command buffers referencing them are still executing on the GPU.
     * This class queues deletion operations and executes them only after a
     * configurable number of frames have passed.
     *
     * Thread Safety:
     * - Push() is thread-safe and can be called from multiple threads
     * - Flush() and FlushAll() should be called from the main render thread
     *
     * Usage:
     * @code
     * // Create deletion queue
     * auto deletionQueue = RHIDeletionQueue::Create();
     *
     * // Queue a buffer for deletion
     * deletionQueue->Push([=]() {
     *     vmaDestroyBuffer(allocator, buffer, allocation);
     * });
     *
     * // At end of each frame, flush pending deletions
     * deletionQueue->Flush(currentFrameIndex);
     *
     * // On shutdown, delete everything immediately
     * deletionQueue->FlushAll();
     * @endcode
     */
    class RHIDeletionQueue
    {
    public:
        /**
         * @brief Deletor function type
         *
         * A callable that performs the actual resource deletion.
         * The function is called with no arguments when it's time to delete.
         */
        using Deletor = std::function<void()>;

        /**
         * @brief Factory method to create a DeletionQueue
         * @param frameDelay Number of frames to wait before executing deletions.
         *                   Should be >= MAX_FRAMES_IN_FLIGHT. Default is 2.
         * @return Shared pointer to the created deletion queue
         */
        static Core::Ref<RHIDeletionQueue> Create(uint32_t frameDelay = DEFAULT_FRAME_DELAY);

        /**
         * @brief Destructor - executes all pending deletions
         */
        ~RHIDeletionQueue();

        // Non-copyable
        RHIDeletionQueue(const RHIDeletionQueue&) = delete;
        RHIDeletionQueue& operator=(const RHIDeletionQueue&) = delete;

        // Non-movable
        RHIDeletionQueue(RHIDeletionQueue&&) = delete;
        RHIDeletionQueue& operator=(RHIDeletionQueue&&) = delete;

        /**
         * @brief Queue a deletor for deferred execution
         *
         * The deletor will be executed after m_FrameDelay frames have passed.
         * This method is thread-safe.
         *
         * @param deletor The function to call when deleting the resource
         */
        void Push(Deletor&& deletor);

        /**
         * @brief Queue a deletor for deferred execution (const reference version)
         *
         * @param deletor The function to call when deleting the resource
         */
        void Push(const Deletor& deletor);

        /**
         * @brief Queue a resource for deletion with a custom deletor
         *
         * Convenience template for queueing typed resource deletion.
         *
         * @tparam T Resource type
         * @param resource Pointer to the resource to delete
         * @param deletor Function that takes the resource and deletes it
         */
        template<typename T>
        void Push(T* resource, std::function<void(T*)> deletor)
        {
            if (resource != nullptr && deletor)
            {
                Push([resource, deletor = std::move(deletor)]() {
                    deletor(resource);
                });
            }
        }

        /**
         * @brief Process pending deletions for the current frame
         *
         * Should be called at the end of each frame. Executes deletors that
         * have been queued for at least m_FrameDelay frames.
         *
         * @param currentFrame The current frame index (wraps around)
         */
        void Flush(uint32_t currentFrame);

        /**
         * @brief Execute all pending deletions immediately
         *
         * Should be called during shutdown to clean up all remaining resources.
         * After calling this, the queue is empty.
         *
         * @warning Only call after ensuring the GPU is idle (e.g., after
         *          vkDeviceWaitIdle) to avoid deleting resources in use.
         */
        void FlushAll();

        /**
         * @brief Get the number of pending deletions
         * @return Total count of queued deletors across all frame slots
         */
        size_t GetPendingCount() const;

        /**
         * @brief Check if the queue is empty
         * @return true if no deletions are pending
         */
        bool IsEmpty() const;

        /**
         * @brief Get the configured frame delay
         * @return Number of frames deletions are delayed
         */
        uint32_t GetFrameDelay() const { return m_FrameDelay; }

    private:
        /**
         * @brief Private constructor - use Create() factory method
         * @param frameDelay Number of frames to delay deletions
         */
        explicit RHIDeletionQueue(uint32_t frameDelay);

        /**
         * @brief Entry in the deletion queue
         */
        struct DeletionEntry
        {
            Deletor Deletor;         ///< The deletion function
            uint32_t QueuedFrame;    ///< Frame when this entry was queued
        };

        /**
         * @brief Number of frames to wait before executing deletions
         */
        uint32_t m_FrameDelay;

        /**
         * @brief Current frame counter (monotonically increasing)
         */
        uint32_t m_CurrentFrame = 0;

        /**
         * @brief Queue of pending deletions
         *
         * Uses a deque for efficient front removal and back insertion.
         * Entries are sorted by queued frame (oldest at front).
         */
        std::deque<DeletionEntry> m_DeletionQueue;

        /**
         * @brief Mutex for thread-safe access to the deletion queue
         */
        mutable std::mutex m_Mutex;
    };

} // namespace RHI
