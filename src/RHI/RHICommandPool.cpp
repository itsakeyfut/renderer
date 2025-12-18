/**
 * @file RHICommandPool.cpp
 * @brief Implementation of Vulkan Command Pool wrapper.
 */

#include "RHI/RHICommandPool.h"
#include "RHI/RHIDevice.h"
#include "Core/Assert.h"
#include "Core/Log.h"

namespace RHI
{
    Core::Ref<RHICommandPool> RHICommandPool::Create(
        const Core::Ref<RHIDevice>& device,
        const RHICommandPoolConfig& config)
    {
        auto pool = Core::Ref<RHICommandPool>(new RHICommandPool());

        if (!pool->Initialize(device, config))
        {
            LOG_ERROR("Failed to initialize RHI Command Pool");
            return nullptr;
        }

        return pool;
    }

    RHICommandPool::~RHICommandPool()
    {
        if (m_CommandPool != VK_NULL_HANDLE && m_Device != VK_NULL_HANDLE)
        {
            vkDestroyCommandPool(m_Device, m_CommandPool, nullptr);
            m_CommandPool = VK_NULL_HANDLE;
            LOG_DEBUG("Vulkan command pool destroyed");
        }
    }

    bool RHICommandPool::Initialize(
        const Core::Ref<RHIDevice>& device,
        const RHICommandPoolConfig& config)
    {
        ASSERT(device != nullptr);

        m_Device = device->GetHandle();
        m_QueueType = config.QueueType;

        // Get the appropriate queue family index
        const auto& queueFamilies = device->GetQueueFamilies();

        switch (config.QueueType)
        {
            case CommandPoolQueueType::Graphics:
                if (!queueFamilies.GraphicsFamily.has_value())
                {
                    LOG_ERROR("Graphics queue family not available");
                    return false;
                }
                m_QueueFamilyIndex = queueFamilies.GraphicsFamily.value();
                break;

            case CommandPoolQueueType::Compute:
                if (!queueFamilies.ComputeFamily.has_value())
                {
                    LOG_ERROR("Compute queue family not available");
                    return false;
                }
                m_QueueFamilyIndex = queueFamilies.ComputeFamily.value();
                break;

            case CommandPoolQueueType::Transfer:
                if (!queueFamilies.TransferFamily.has_value())
                {
                    // Fall back to graphics queue if no dedicated transfer queue
                    if (!queueFamilies.GraphicsFamily.has_value())
                    {
                        LOG_ERROR("No transfer or graphics queue family available");
                        return false;
                    }
                    m_QueueFamilyIndex = queueFamilies.GraphicsFamily.value();
                    LOG_DEBUG("Using graphics queue family for transfer operations");
                }
                else
                {
                    m_QueueFamilyIndex = queueFamilies.TransferFamily.value();
                }
                break;
        }

        // Build create flags
        VkCommandPoolCreateFlags flags = 0;

        if (config.AllowReset)
        {
            flags |= VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        }

        if (config.Transient)
        {
            flags |= VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        }

        // Create command pool
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = m_QueueFamilyIndex;
        poolInfo.flags = flags;

        VkResult result = vkCreateCommandPool(m_Device, &poolInfo, nullptr, &m_CommandPool);
        if (result != VK_SUCCESS)
        {
            LOG_ERROR("Failed to create Vulkan command pool, VkResult: {}", static_cast<int>(result));
            return false;
        }

        const char* queueTypeName = "Unknown";
        switch (config.QueueType)
        {
            case CommandPoolQueueType::Graphics: queueTypeName = "Graphics"; break;
            case CommandPoolQueueType::Compute: queueTypeName = "Compute"; break;
            case CommandPoolQueueType::Transfer: queueTypeName = "Transfer"; break;
        }

        LOG_INFO("Vulkan command pool created successfully");
        LOG_INFO("  Queue Type: {}", queueTypeName);
        LOG_INFO("  Queue Family Index: {}", m_QueueFamilyIndex);
        LOG_DEBUG("  Allow Reset: {}", config.AllowReset);
        LOG_DEBUG("  Transient: {}", config.Transient);

        return true;
    }

    void RHICommandPool::Reset(bool releaseResources)
    {
        ASSERT(m_CommandPool != VK_NULL_HANDLE);
        ASSERT(m_Device != VK_NULL_HANDLE);

        VkCommandPoolResetFlags flags = 0;
        if (releaseResources)
        {
            flags |= VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT;
        }

        VkResult result = vkResetCommandPool(m_Device, m_CommandPool, flags);
        if (result != VK_SUCCESS)
        {
            LOG_ERROR("Failed to reset command pool, VkResult: {}", static_cast<int>(result));
        }
    }

} // namespace RHI
