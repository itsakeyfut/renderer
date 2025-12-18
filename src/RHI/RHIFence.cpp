/**
 * @file RHIFence.cpp
 * @brief Implementation of Vulkan Fence wrapper.
 */

#include "RHI/RHIFence.h"
#include "RHI/RHIDevice.h"
#include "Core/Assert.h"
#include "Core/Log.h"

namespace RHI
{
    Core::Ref<RHIFence> RHIFence::Create(
        const Core::Ref<RHIDevice>& device,
        bool signaled)
    {
        auto fence = Core::Ref<RHIFence>(new RHIFence());

        if (!fence->Initialize(device, signaled))
        {
            LOG_ERROR("Failed to initialize RHI Fence");
            return nullptr;
        }

        return fence;
    }

    RHIFence::~RHIFence()
    {
        if (m_Fence != VK_NULL_HANDLE && m_Device != VK_NULL_HANDLE)
        {
            vkDestroyFence(m_Device, m_Fence, nullptr);
            m_Fence = VK_NULL_HANDLE;
            LOG_DEBUG("Vulkan fence destroyed");
        }
    }

    bool RHIFence::Initialize(const Core::Ref<RHIDevice>& device, bool signaled)
    {
        ASSERT(device != nullptr);

        m_Device = device->GetHandle();

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

        // Create in signaled state if requested
        // This is useful for the first frame to avoid waiting on unsignaled fence
        if (signaled)
        {
            fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        }

        VkResult result = vkCreateFence(m_Device, &fenceInfo, nullptr, &m_Fence);
        if (result != VK_SUCCESS)
        {
            LOG_ERROR("Failed to create Vulkan fence, VkResult: {}", static_cast<int>(result));
            return false;
        }

        LOG_DEBUG("Vulkan fence created successfully (signaled: {})", signaled);
        return true;
    }

    bool RHIFence::Wait(uint64_t timeout)
    {
        ASSERT(m_Fence != VK_NULL_HANDLE);
        ASSERT(m_Device != VK_NULL_HANDLE);

        VkResult result = vkWaitForFences(m_Device, 1, &m_Fence, VK_TRUE, timeout);

        if (result == VK_SUCCESS)
        {
            return true;
        }
        else if (result == VK_TIMEOUT)
        {
            LOG_WARN("Fence wait timed out");
            return false;
        }
        else
        {
            LOG_ERROR("Failed to wait for fence, VkResult: {}", static_cast<int>(result));
            return false;
        }
    }

    void RHIFence::Reset()
    {
        ASSERT(m_Fence != VK_NULL_HANDLE);
        ASSERT(m_Device != VK_NULL_HANDLE);

        VkResult result = vkResetFences(m_Device, 1, &m_Fence);
        if (result != VK_SUCCESS)
        {
            LOG_ERROR("Failed to reset fence, VkResult: {}", static_cast<int>(result));
        }
    }

    bool RHIFence::IsSignaled() const
    {
        ASSERT(m_Fence != VK_NULL_HANDLE);
        ASSERT(m_Device != VK_NULL_HANDLE);

        VkResult result = vkGetFenceStatus(m_Device, m_Fence);
        return result == VK_SUCCESS;
    }

} // namespace RHI
