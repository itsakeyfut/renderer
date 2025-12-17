/**
 * @file RHICommandPool.h
 * @brief Vulkan Command Pool wrapper for the RHI layer.
 *
 * Manages VkCommandPool creation and command buffer allocation.
 * Command pools are used to allocate command buffers for specific queue families.
 */

#pragma once

#include "Core/Types.h"

#include <vulkan/vulkan.h>

namespace RHI
{
    // Forward declarations
    class RHIDevice;

    /**
     * @brief Queue type for command pool creation
     */
    enum class CommandPoolQueueType
    {
        Graphics,   ///< Graphics queue family
        Compute,    ///< Compute queue family
        Transfer    ///< Transfer queue family
    };

    /**
     * @brief Configuration for command pool creation
     */
    struct RHICommandPoolConfig
    {
        /**
         * @brief The queue type this command pool will be associated with
         */
        CommandPoolQueueType QueueType = CommandPoolQueueType::Graphics;

        /**
         * @brief Allow individual command buffer reset
         * If true, command buffers can be reset individually.
         * If false, all command buffers must be reset together via pool reset.
         */
        bool AllowReset = true;

        /**
         * @brief Hint that command buffers will be short-lived
         * Enables VK_COMMAND_POOL_CREATE_TRANSIENT_BIT for optimization.
         */
        bool Transient = false;
    };

    /**
     * @brief Vulkan Command Pool wrapper class
     *
     * Manages the command pool lifecycle and provides command buffer allocation.
     * Each command pool is associated with a specific queue family.
     *
     * Usage:
     * @code
     * auto pool = RHICommandPool::Create(device);
     * auto cmdBuffer = RHICommandBuffer::Create(device, pool->GetHandle());
     * @endcode
     */
    class RHICommandPool
    {
    public:
        /**
         * @brief Factory method to create a Command Pool
         * @param device The logical device
         * @param config Command pool configuration
         * @return Shared pointer to the created command pool, or nullptr on failure
         */
        static Core::Ref<RHICommandPool> Create(
            const Core::Ref<RHIDevice>& device,
            const RHICommandPoolConfig& config = {});

        /**
         * @brief Destructor - destroys VkCommandPool
         */
        ~RHICommandPool();

        // Non-copyable
        RHICommandPool(const RHICommandPool&) = delete;
        RHICommandPool& operator=(const RHICommandPool&) = delete;

        // Non-movable
        RHICommandPool(RHICommandPool&&) = delete;
        RHICommandPool& operator=(RHICommandPool&&) = delete;

        /**
         * @brief Get the native VkCommandPool handle
         * @return VkCommandPool handle
         */
        VkCommandPool GetHandle() const { return m_CommandPool; }

        /**
         * @brief Get the queue family index this pool is associated with
         * @return Queue family index
         */
        uint32_t GetQueueFamilyIndex() const { return m_QueueFamilyIndex; }

        /**
         * @brief Get the queue type this pool is associated with
         * @return Queue type
         */
        CommandPoolQueueType GetQueueType() const { return m_QueueType; }

        /**
         * @brief Reset all command buffers in the pool
         *
         * Resets all command buffers allocated from this pool to their initial state.
         * This is more efficient than resetting individual command buffers.
         *
         * @param releaseResources If true, resources are returned to the system
         */
        void Reset(bool releaseResources = false);

    private:
        /**
         * @brief Private constructor - use Create() factory method
         */
        RHICommandPool() = default;

        /**
         * @brief Initialize the command pool
         * @param device The logical device
         * @param config Command pool configuration
         * @return true on success, false on failure
         */
        bool Initialize(
            const Core::Ref<RHIDevice>& device,
            const RHICommandPoolConfig& config);

        VkDevice m_Device = VK_NULL_HANDLE;
        VkCommandPool m_CommandPool = VK_NULL_HANDLE;
        uint32_t m_QueueFamilyIndex = 0;
        CommandPoolQueueType m_QueueType = CommandPoolQueueType::Graphics;
    };

} // namespace RHI
