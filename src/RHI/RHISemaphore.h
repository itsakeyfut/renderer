/**
 * @file RHISemaphore.h
 * @brief Vulkan Semaphore wrapper for the RHI layer.
 *
 * Provides GPU-GPU synchronization between queue operations.
 * Semaphores are used to coordinate operations across different queues
 * or within the same queue (e.g., Acquire -> Render -> Present).
 */

#pragma once

#include "Core/Types.h"

#include <vulkan/vulkan.h>

namespace RHI
{
    // Forward declarations
    class RHIDevice;

    /**
     * @brief Vulkan Semaphore wrapper class
     *
     * Semaphores are used for GPU-GPU synchronization, primarily between
     * queue operations. They are lightweight compared to fences since they
     * operate entirely on the GPU without CPU involvement.
     *
     * Common use cases:
     * - Image acquisition: Signal when swapchain image is available
     * - Render completion: Signal when rendering is finished
     * - Queue transfer: Synchronize operations between different queues
     *
     * Usage:
     * @code
     * auto imageAvailable = RHISemaphore::Create(device);
     * auto renderFinished = RHISemaphore::Create(device);
     *
     * // Acquire image, signaling imageAvailable
     * swapchain->AcquireNextImage(imageAvailable->GetHandle());
     *
     * // Submit commands, waiting on imageAvailable, signaling renderFinished
     * VkSubmitInfo submitInfo{};
     * submitInfo.waitSemaphoreCount = 1;
     * submitInfo.pWaitSemaphores = &imageAvailable->GetHandle();
     * submitInfo.signalSemaphoreCount = 1;
     * submitInfo.pSignalSemaphores = &renderFinished->GetHandle();
     *
     * // Present, waiting on renderFinished
     * swapchain->Present(queue, imageIndex, renderFinished->GetHandle());
     * @endcode
     */
    class RHISemaphore
    {
    public:
        /**
         * @brief Factory method to create a Semaphore
         * @param device The logical device
         * @return Shared pointer to the created semaphore, or nullptr on failure
         */
        static Core::Ref<RHISemaphore> Create(const Core::Ref<RHIDevice>& device);

        /**
         * @brief Destructor - destroys VkSemaphore
         */
        ~RHISemaphore();

        // Non-copyable
        RHISemaphore(const RHISemaphore&) = delete;
        RHISemaphore& operator=(const RHISemaphore&) = delete;

        // Non-movable
        RHISemaphore(RHISemaphore&&) = delete;
        RHISemaphore& operator=(RHISemaphore&&) = delete;

        /**
         * @brief Get the native VkSemaphore handle
         * @return VkSemaphore handle
         */
        VkSemaphore GetHandle() const { return m_Semaphore; }

    private:
        /**
         * @brief Private constructor - use Create() factory method
         */
        RHISemaphore() = default;

        /**
         * @brief Initialize the semaphore
         * @param device The logical device
         * @return true on success, false on failure
         */
        bool Initialize(const Core::Ref<RHIDevice>& device);

        VkDevice m_Device = VK_NULL_HANDLE;
        VkSemaphore m_Semaphore = VK_NULL_HANDLE;
    };

} // namespace RHI
