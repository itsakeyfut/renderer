/**
 * @file FrameManager.cpp
 * @brief Implementation of the FrameManager class.
 */

#include "Renderer/FrameManager.h"

#include "Core/Assert.h"
#include "Core/Log.h"
#include "RHI/RHICommandBuffer.h"
#include "RHI/RHICommandPool.h"
#include "RHI/RHIDevice.h"
#include "RHI/RHIFence.h"
#include "RHI/RHISemaphore.h"

namespace Renderer
{
    Core::Ref<FrameManager> FrameManager::Create(const Core::Ref<RHI::RHIDevice>& device)
    {
        auto frameManager = Core::Ref<FrameManager>(new FrameManager());

        if (!frameManager->Initialize(device))
        {
            LOG_ERROR("Failed to initialize FrameManager");
            return nullptr;
        }

        return frameManager;
    }

    FrameManager::~FrameManager()
    {
        // Frame resources are cleaned up automatically via shared_ptr destructors
        // The order of destruction is handled by the member declaration order
        LOG_DEBUG("FrameManager destroyed");
    }

    bool FrameManager::Initialize(const Core::Ref<RHI::RHIDevice>& device)
    {
        ASSERT(device != nullptr);

        // Create a shared command pool for all frame command buffers
        // Using AllowReset=true so each command buffer can be reset individually
        RHI::RHICommandPoolConfig poolConfig{};
        poolConfig.QueueType = RHI::CommandPoolQueueType::Graphics;
        poolConfig.AllowReset = true;
        poolConfig.Transient = false;

        m_CommandPool = RHI::RHICommandPool::Create(device, poolConfig);
        if (!m_CommandPool)
        {
            LOG_ERROR("Failed to create command pool for FrameManager");
            return false;
        }

        // Create resources for each frame
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            if (!CreateFrameResources(device, m_Frames[i]))
            {
                LOG_ERROR("Failed to create resources for frame {}", i);
                return false;
            }
        }

        LOG_INFO("FrameManager initialized with {} frames in flight", MAX_FRAMES_IN_FLIGHT);
        return true;
    }

    bool FrameManager::CreateFrameResources(
        const Core::Ref<RHI::RHIDevice>& device,
        FrameData& frameData)
    {
        // Create command buffer
        frameData.CommandBuffer = RHI::RHICommandBuffer::Create(
            device,
            m_CommandPool,
            VK_COMMAND_BUFFER_LEVEL_PRIMARY);
        if (!frameData.CommandBuffer)
        {
            LOG_ERROR("Failed to create command buffer for frame");
            return false;
        }

        // Create image available semaphore
        frameData.ImageAvailableSemaphore = RHI::RHISemaphore::Create(device);
        if (!frameData.ImageAvailableSemaphore)
        {
            LOG_ERROR("Failed to create image available semaphore for frame");
            return false;
        }

        // Create render finished semaphore
        frameData.RenderFinishedSemaphore = RHI::RHISemaphore::Create(device);
        if (!frameData.RenderFinishedSemaphore)
        {
            LOG_ERROR("Failed to create render finished semaphore for frame");
            return false;
        }

        // Create in-flight fence (signaled state to prevent deadlock on first frame)
        frameData.InFlightFence = RHI::RHIFence::Create(device, true);
        if (!frameData.InFlightFence)
        {
            LOG_ERROR("Failed to create in-flight fence for frame");
            return false;
        }

        return true;
    }

    bool FrameManager::WaitForFrame(uint64_t timeout)
    {
        auto& frame = m_Frames[m_CurrentFrame];

        // Wait for the fence to be signaled (GPU finished with this frame's resources)
        if (!frame.InFlightFence->Wait(timeout))
        {
            LOG_WARN("Frame {} fence wait timed out", m_CurrentFrame);
            return false;
        }

        // Reset the fence for the next frame submission
        frame.InFlightFence->Reset();

        return true;
    }

    void FrameManager::NextFrame()
    {
        m_CurrentFrame = (m_CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    VkSemaphore FrameManager::GetImageAvailableSemaphore() const
    {
        return m_Frames[m_CurrentFrame].ImageAvailableSemaphore->GetHandle();
    }

    VkSemaphore FrameManager::GetRenderFinishedSemaphore() const
    {
        return m_Frames[m_CurrentFrame].RenderFinishedSemaphore->GetHandle();
    }

    VkFence FrameManager::GetInFlightFence() const
    {
        return m_Frames[m_CurrentFrame].InFlightFence->GetHandle();
    }

    FrameData& FrameManager::GetFrame(uint32_t index)
    {
        ASSERT(index < MAX_FRAMES_IN_FLIGHT);
        return m_Frames[index];
    }

    const FrameData& FrameManager::GetFrame(uint32_t index) const
    {
        ASSERT(index < MAX_FRAMES_IN_FLIGHT);
        return m_Frames[index];
    }

} // namespace Renderer
