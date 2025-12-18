/**
 * @file RHISemaphore.cpp
 * @brief Implementation of Vulkan Semaphore wrapper.
 */

#include "RHI/RHISemaphore.h"
#include "RHI/RHIDevice.h"
#include "Core/Assert.h"
#include "Core/Log.h"

namespace RHI
{
    Core::Ref<RHISemaphore> RHISemaphore::Create(const Core::Ref<RHIDevice>& device)
    {
        auto semaphore = Core::Ref<RHISemaphore>(new RHISemaphore());

        if (!semaphore->Initialize(device))
        {
            LOG_ERROR("Failed to initialize RHI Semaphore");
            return nullptr;
        }

        return semaphore;
    }

    RHISemaphore::~RHISemaphore()
    {
        if (m_Semaphore != VK_NULL_HANDLE && m_Device != VK_NULL_HANDLE)
        {
            vkDestroySemaphore(m_Device, m_Semaphore, nullptr);
            m_Semaphore = VK_NULL_HANDLE;
            LOG_DEBUG("Vulkan semaphore destroyed");
        }
    }

    bool RHISemaphore::Initialize(const Core::Ref<RHIDevice>& device)
    {
        ASSERT(device != nullptr);

        m_Device = device->GetHandle();

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkResult result = vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr, &m_Semaphore);
        if (result != VK_SUCCESS)
        {
            LOG_ERROR("Failed to create Vulkan semaphore, VkResult: {}", static_cast<int>(result));
            return false;
        }

        LOG_DEBUG("Vulkan semaphore created successfully");
        return true;
    }

} // namespace RHI
