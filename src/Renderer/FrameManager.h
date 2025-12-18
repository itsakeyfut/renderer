/**
 * @file FrameManager.h
 * @brief Frame resource management for the Renderer layer.
 *
 * Manages frame-in-flight resources including command buffers, semaphores,
 * and fences for efficient CPU/GPU parallelism. Implements double/triple
 * buffering to minimize latency while avoiding resource conflicts.
 */

#pragma once

#include "Core/Types.h"

#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>
#include <vector>

namespace RHI
{
    // Forward declarations
    class RHIDevice;
    class RHICommandPool;
    class RHICommandBuffer;
    class RHISemaphore;
    class RHIFence;
} // namespace RHI

namespace Renderer
{
    /**
     * @brief Maximum number of frames that can be processed concurrently
     *
     * Using 2 frames allows CPU to record commands for frame N+1 while
     * GPU is rendering frame N. This provides good parallelism with
     * minimal latency. 3 frames would increase throughput slightly
     * but also increase input latency.
     */
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

    /**
     * @brief Per-frame resources for frame-in-flight management
     *
     * Each frame has its own set of resources to avoid conflicts
     * when the CPU is preparing the next frame while the GPU is
     * still processing the current frame.
     */
    struct FrameData
    {
        /**
         * @brief Command buffer for recording GPU commands
         */
        Core::Ref<RHI::RHICommandBuffer> CommandBuffer;

        /**
         * @brief Semaphore signaled when swapchain image is available
         *
         * Used by AcquireNextImage to signal that the image is ready
         * for rendering. Queue submission waits on this semaphore.
         */
        Core::Ref<RHI::RHISemaphore> ImageAvailableSemaphore;

        /**
         * @brief Fence for CPU-GPU synchronization
         *
         * Signaled when GPU completes the frame's commands.
         * CPU waits on this fence before reusing the frame's resources.
         * Created in signaled state to allow first frame to proceed.
         */
        Core::Ref<RHI::RHIFence> InFlightFence;
    };

    /**
     * @brief Manages frame-in-flight resources for efficient rendering
     *
     * The FrameManager implements a double-buffering scheme where multiple
     * frames can be in various stages of processing simultaneously:
     * - Frame N: GPU rendering (previous frame)
     * - Frame N+1: CPU recording (current frame)
     *
     * This parallelism is achieved by maintaining separate resources
     * (command buffers, semaphores, fences) for each frame slot.
     *
     * Frame Loop Flow:
     * @code
     * while (running) {
     *     // 1. Wait for previous frame's GPU work to complete
     *     frameManager->WaitForFrame();
     *
     *     // 2. Acquire swapchain image
     *     uint32_t imageIndex = swapchain->AcquireNextImage(
     *         frameManager->GetImageAvailableSemaphore()
     *     );
     *
     *     // 3. Record and submit commands
     *     auto& cmdBuffer = frameManager->GetCommandBuffer();
     *     cmdBuffer->Begin();
     *     // ... rendering commands ...
     *     cmdBuffer->End();
     *
     *     // 4. Submit with synchronization
     *     VkSubmitInfo submitInfo{};
     *     VkSemaphore waitSemaphores[] = {frameManager->GetImageAvailableSemaphore()};
     *     VkSemaphore signalSemaphores[] = {frameManager->GetRenderFinishedSemaphore()};
     *     // ... configure submitInfo ...
     *     vkQueueSubmit(queue, 1, &submitInfo, frameManager->GetInFlightFence());
     *
     *     // 5. Present
     *     swapchain->Present(queue, imageIndex, frameManager->GetRenderFinishedSemaphore());
     *
     *     // 6. Advance to next frame
     *     frameManager->NextFrame();
     * }
     * @endcode
     *
     * @note Fences are created in signaled state to prevent deadlock on first frame.
     */
    class FrameManager
    {
    public:
        /**
         * @brief Factory method to create a FrameManager
         * @param device The logical device for resource creation
         * @return Shared pointer to the created FrameManager, or nullptr on failure
         */
        static Core::Ref<FrameManager> Create(const Core::Ref<RHI::RHIDevice>& device);

        /**
         * @brief Destructor - releases all frame resources
         */
        ~FrameManager();

        // Non-copyable
        FrameManager(const FrameManager&) = delete;
        FrameManager& operator=(const FrameManager&) = delete;

        // Non-movable
        FrameManager(FrameManager&&) = delete;
        FrameManager& operator=(FrameManager&&) = delete;

        // ============================================================
        // Frame Access
        // ============================================================

        /**
         * @brief Get the current frame's data
         * @return Reference to the current FrameData
         */
        FrameData& GetCurrentFrame() { return m_Frames[m_CurrentFrame]; }

        /**
         * @brief Get the current frame's data (const version)
         * @return Const reference to the current FrameData
         */
        const FrameData& GetCurrentFrame() const { return m_Frames[m_CurrentFrame]; }

        /**
         * @brief Get the current frame index
         * @return Frame index (0 to MAX_FRAMES_IN_FLIGHT - 1)
         */
        uint32_t GetCurrentFrameIndex() const { return m_CurrentFrame; }

        // ============================================================
        // Frame Synchronization
        // ============================================================

        /**
         * @brief Wait for the current frame's GPU work to complete
         *
         * Blocks until the fence for the current frame is signaled,
         * then resets the fence for the next use. Must be called at
         * the start of each frame before recording new commands.
         *
         * @param timeout Maximum wait time in nanoseconds (default: infinite)
         * @return true if the fence was signaled, false on timeout
         */
        bool WaitForFrame(uint64_t timeout = UINT64_MAX);

        /**
         * @brief Advance to the next frame
         *
         * Updates the current frame index to the next slot using
         * modular arithmetic: (current + 1) % MAX_FRAMES_IN_FLIGHT
         */
        void NextFrame();

        /**
         * @brief Reset the current frame's fence without waiting
         *
         * Used after swapchain recreation when the fence might be signaled
         * but we want to skip the wait. Resets the fence to unsignaled state.
         */
        void ResetCurrentFence();

        /**
         * @brief Reset all synchronization primitives for swapchain recreation
         *
         * Used after device->WaitIdle() to ensure all fences and semaphores
         * are in a clean state. This prevents deadlocks and validation errors
         * from primitives that were signaled but never consumed due to
         * swapchain recreation.
         *
         * @param device The logical device for recreation
         * @return true on success, false on failure
         */
        bool ResetSyncPrimitives(const Core::Ref<RHI::RHIDevice>& device);

        // ============================================================
        // Convenience Accessors
        // ============================================================

        /**
         * @brief Get the current frame's command buffer
         * @return Reference to the command buffer
         */
        Core::Ref<RHI::RHICommandBuffer>& GetCommandBuffer()
        {
            return m_Frames[m_CurrentFrame].CommandBuffer;
        }

        /**
         * @brief Get the current frame's image available semaphore handle
         * @return VkSemaphore handle
         */
        VkSemaphore GetImageAvailableSemaphore() const;

        /**
         * @brief Get the render finished semaphore for a specific swapchain image
         *
         * Render finished semaphores are indexed by swapchain image index (not frame index)
         * to avoid semaphore reuse conflicts when the acquired image index differs from
         * the frame-in-flight index.
         *
         * @param imageIndex Swapchain image index from AcquireNextImage
         * @return VkSemaphore handle
         */
        VkSemaphore GetRenderFinishedSemaphore(uint32_t imageIndex) const;

        /**
         * @brief Get the current frame's in-flight fence handle
         * @return VkFence handle
         */
        VkFence GetInFlightFence() const;

        // ============================================================
        // Frame Data Access
        // ============================================================

        /**
         * @brief Get frame data by index
         * @param index Frame index (0 to MAX_FRAMES_IN_FLIGHT - 1)
         * @return Reference to the specified FrameData
         */
        FrameData& GetFrame(uint32_t index);

        /**
         * @brief Get frame data by index (const version)
         * @param index Frame index (0 to MAX_FRAMES_IN_FLIGHT - 1)
         * @return Const reference to the specified FrameData
         */
        const FrameData& GetFrame(uint32_t index) const;

        /**
         * @brief Get the total number of frame slots
         * @return MAX_FRAMES_IN_FLIGHT
         */
        static constexpr uint32_t GetFrameCount() { return MAX_FRAMES_IN_FLIGHT; }

        // ============================================================
        // Swapchain Image Semaphore Management
        // ============================================================

        /**
         * @brief Create render finished semaphores for swapchain images
         *
         * Must be called after swapchain creation/recreation to allocate
         * the correct number of semaphores matching the swapchain image count.
         *
         * @param device The logical device
         * @param imageCount Number of swapchain images
         * @return true on success, false on failure
         */
        bool CreateRenderFinishedSemaphores(
            const Core::Ref<RHI::RHIDevice>& device,
            uint32_t imageCount);

        /**
         * @brief Get the number of render finished semaphores
         * @return Current count of render finished semaphores
         */
        uint32_t GetRenderFinishedSemaphoreCount() const
        {
            return static_cast<uint32_t>(m_RenderFinishedSemaphores.size());
        }

    private:
        /**
         * @brief Private constructor - use Create() factory method
         */
        FrameManager() = default;

        /**
         * @brief Initialize the frame manager
         * @param device The logical device
         * @return true on success, false on failure
         */
        bool Initialize(const Core::Ref<RHI::RHIDevice>& device);

        /**
         * @brief Create frame resources (command buffer, semaphores, fence)
         * @param device The logical device
         * @param frameData FrameData to initialize
         * @return true on success, false on failure
         */
        bool CreateFrameResources(
            const Core::Ref<RHI::RHIDevice>& device,
            FrameData& frameData);

        Core::Ref<RHI::RHIDevice> m_Device;  ///< Device reference for semaphore creation
        Core::Ref<RHI::RHICommandPool> m_CommandPool;  ///< Shared command pool for all frames
        std::array<FrameData, MAX_FRAMES_IN_FLIGHT> m_Frames;
        uint32_t m_CurrentFrame = 0;

        /**
         * @brief Render finished semaphores indexed by swapchain image
         *
         * These semaphores are separate from per-frame resources because
         * the swapchain can return images in any order. Using image-indexed
         * semaphores prevents reuse conflicts when presenting.
         */
        std::vector<Core::Ref<RHI::RHISemaphore>> m_RenderFinishedSemaphores;
    };

} // namespace Renderer
