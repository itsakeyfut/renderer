/**
 * @file RHIFence.h
 * @brief Vulkan Fence wrapper for the RHI layer.
 *
 * Provides CPU-GPU synchronization for frame management.
 * Fences allow the CPU to wait for GPU operations to complete,
 * essential for managing frame-in-flight resources.
 */

#pragma once

#include "Core/Types.h"

#include <vulkan/vulkan.h>

#include <cstdint>

namespace RHI
{
    // Forward declarations
    class RHIDevice;

    /**
     * @brief Vulkan Fence wrapper class
     *
     * Fences are used for CPU-GPU synchronization, allowing the CPU to wait
     * for GPU operations to complete. They are heavier than semaphores since
     * they involve CPU blocking, but are necessary for frame synchronization.
     *
     * Common use cases:
     * - Frame-in-flight management: Wait for previous frame's GPU work
     * - Resource readback: Ensure GPU writes are complete before CPU reads
     * - Command buffer reuse: Wait before resetting/reusing command buffers
     *
     * Usage:
     * @code
     * // Create fence in signaled state for first frame
     * auto inFlightFence = RHIFence::Create(device, true);
     *
     * // Frame loop
     * while (running) {
     *     // Wait for previous frame to complete
     *     inFlightFence->Wait();
     *     inFlightFence->Reset();
     *
     *     // Record and submit commands, signaling fence on completion
     *     VkSubmitInfo submitInfo{};
     *     // ... setup submitInfo ...
     *     vkQueueSubmit(queue, 1, &submitInfo, inFlightFence->GetHandle());
     * }
     * @endcode
     */
    class RHIFence
    {
    public:
        /**
         * @brief Factory method to create a Fence
         * @param device The logical device
         * @param signaled If true, create the fence in signaled state.
         *                 Useful for the first frame to avoid deadlock.
         * @return Shared pointer to the created fence, or nullptr on failure
         */
        static Core::Ref<RHIFence> Create(
            const Core::Ref<RHIDevice>& device,
            bool signaled = false);

        /**
         * @brief Destructor - destroys VkFence
         */
        ~RHIFence();

        // Non-copyable
        RHIFence(const RHIFence&) = delete;
        RHIFence& operator=(const RHIFence&) = delete;

        // Non-movable
        RHIFence(RHIFence&&) = delete;
        RHIFence& operator=(RHIFence&&) = delete;

        /**
         * @brief Get the native VkFence handle
         * @return VkFence handle
         */
        VkFence GetHandle() const { return m_Fence; }

        /**
         * @brief Wait for the fence to become signaled
         *
         * Blocks the calling thread until the fence is signaled by the GPU
         * or the timeout expires. This is a heavyweight operation that blocks
         * the CPU, so use sparingly and prefer semaphores for GPU-GPU sync.
         *
         * @param timeout Maximum time to wait in nanoseconds (default: UINT64_MAX = infinite)
         * @return true if the fence was signaled, false on timeout
         */
        bool Wait(uint64_t timeout = UINT64_MAX);

        /**
         * @brief Reset the fence to unsignaled state
         *
         * Must be called before the fence can be used for another wait operation.
         * The fence must be in the signaled state when Reset() is called.
         */
        void Reset();

        /**
         * @brief Check if the fence is currently signaled
         *
         * Non-blocking query of the fence state.
         *
         * @return true if signaled, false if unsignaled
         */
        bool IsSignaled() const;

    private:
        /**
         * @brief Private constructor - use Create() factory method
         */
        RHIFence() = default;

        /**
         * @brief Initialize the fence
         * @param device The logical device
         * @param signaled Initial signaled state
         * @return true on success, false on failure
         */
        bool Initialize(const Core::Ref<RHIDevice>& device, bool signaled);

        VkDevice m_Device = VK_NULL_HANDLE;
        VkFence m_Fence = VK_NULL_HANDLE;
    };

} // namespace RHI
