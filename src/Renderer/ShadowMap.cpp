/**
 * @file ShadowMap.cpp
 * @brief Shadow map implementation.
 */

#include "Renderer/ShadowMap.h"
#include "RHI/RHICommandBuffer.h"
#include "RHI/RHIDeletionQueue.h"
#include "RHI/RHIDevice.h"
#include "RHI/RHISampler.h"
#include "Core/Log.h"

#include <vk_mem_alloc.h>

namespace Renderer
{
    Core::Ref<ShadowMap> ShadowMap::Create(
        const Core::Ref<RHI::RHIDevice>& device,
        const Core::Ref<RHI::RHIDeletionQueue>& deletionQueue,
        const ShadowMapDesc& desc)
    {
        auto shadowMap = Core::Ref<ShadowMap>(new ShadowMap());
        if (!shadowMap->Initialize(device, deletionQueue, desc))
        {
            return nullptr;
        }
        return shadowMap;
    }

    ShadowMap::~ShadowMap()
    {
        // Queue resources for deferred deletion to avoid GPU resource conflicts
        if (m_DeletionQueue && m_ImageView != VK_NULL_HANDLE)
        {
            VkDevice device = m_Device;
            VkImageView imageView = m_ImageView;
            m_DeletionQueue->Push([device, imageView]() {
                vkDestroyImageView(device, imageView, nullptr);
                LOG_DEBUG("Destroyed shadow map image view (deferred)");
            });
            m_ImageView = VK_NULL_HANDLE;
        }

        if (m_DeletionQueue && m_Image != VK_NULL_HANDLE && m_Allocator != nullptr)
        {
            VmaAllocator allocator = m_Allocator;
            VkImage image = m_Image;
            VmaAllocation allocation = m_Allocation;
            m_DeletionQueue->Push([allocator, image, allocation]() {
                vmaDestroyImage(allocator, image, allocation);
                LOG_DEBUG("Destroyed shadow map image (deferred)");
            });
            m_Image = VK_NULL_HANDLE;
            m_Allocation = nullptr;
        }

        // Sampler is released automatically via Ref<>
        m_Sampler.reset();
    }

    bool ShadowMap::Initialize(
        const Core::Ref<RHI::RHIDevice>& device,
        const Core::Ref<RHI::RHIDeletionQueue>& deletionQueue,
        const ShadowMapDesc& desc)
    {
        m_Device = device->GetHandle();
        m_Allocator = device->GetAllocator();
        m_DeletionQueue = deletionQueue;
        m_Resolution = desc.Resolution;
        m_Format = desc.Format;

        if (m_Resolution == 0)
        {
            LOG_ERROR("Invalid shadow map resolution: {}", m_Resolution);
            return false;
        }

        if (!CreateImage(device))
        {
            return false;
        }

        if (!CreateImageView(device))
        {
            return false;
        }

        if (!CreateSampler(device))
        {
            return false;
        }

        LOG_DEBUG("Created shadow map: {}x{}, format={}",
                  m_Resolution, m_Resolution, static_cast<int>(m_Format));
        return true;
    }

    bool ShadowMap::CreateImage(const Core::Ref<RHI::RHIDevice>& device)
    {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = m_Resolution;
        imageInfo.extent.height = m_Resolution;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = m_Format;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        // Key difference from DepthBuffer: we need SAMPLED_BIT for shader access
        imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                          VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        VkResult result = vmaCreateImage(
            device->GetAllocator(),
            &imageInfo,
            &allocInfo,
            &m_Image,
            &m_Allocation,
            nullptr);

        if (result != VK_SUCCESS)
        {
            LOG_ERROR("Failed to create shadow map image: {}", static_cast<int>(result));
            return false;
        }

        m_CurrentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        return true;
    }

    bool ShadowMap::CreateImageView(const Core::Ref<RHI::RHIDevice>& device)
    {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_Image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = m_Format;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        VkResult result = vkCreateImageView(
            device->GetHandle(),
            &viewInfo,
            nullptr,
            &m_ImageView);

        if (result != VK_SUCCESS)
        {
            LOG_ERROR("Failed to create shadow map image view: {}", static_cast<int>(result));
            return false;
        }

        return true;
    }

    bool ShadowMap::CreateSampler(const Core::Ref<RHI::RHIDevice>& device)
    {
        // Use the pre-configured shadow sampler with PCF comparison
        m_Sampler = RHI::RHISampler::CreateShadow(device);
        if (!m_Sampler)
        {
            LOG_ERROR("Failed to create shadow map sampler");
            return false;
        }

        return true;
    }

    void ShadowMap::Begin(const Core::Ref<RHI::RHICommandBuffer>& cmdBuffer)
    {
        // Transition to depth attachment optimal if needed
        if (m_CurrentLayout != VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
        {
            TransitionLayout(
                cmdBuffer,
                m_CurrentLayout,
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
        }

        // Configure rendering for shadow pass (depth-only, no color attachments)
        RHI::RenderingConfig config;
        config.RenderArea = {{0, 0}, {m_Resolution, m_Resolution}};
        config.LayerCount = 1;

        // No color attachments for shadow pass
        config.ColorAttachments.clear();

        // Depth attachment configuration
        config.Depth.ImageView = m_ImageView;
        config.Depth.Layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        config.Depth.LoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        config.Depth.StoreOp = VK_ATTACHMENT_STORE_OP_STORE;  // Must store for sampling
        config.Depth.ClearValue = {1.0f, 0};  // Clear to max depth

        // Begin dynamic rendering
        cmdBuffer->BeginRendering(config);

        // Set viewport and scissor for shadow map resolution
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(m_Resolution);
        viewport.height = static_cast<float>(m_Resolution);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        cmdBuffer->SetViewport(viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = {m_Resolution, m_Resolution};
        cmdBuffer->SetScissor(scissor);
    }

    void ShadowMap::End(const Core::Ref<RHI::RHICommandBuffer>& cmdBuffer)
    {
        // End dynamic rendering
        cmdBuffer->EndRendering();

        // Transition to shader read-only for sampling in main pass
        TransitionLayout(
            cmdBuffer,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    void ShadowMap::TransitionLayout(
        const Core::Ref<RHI::RHICommandBuffer>& cmdBuffer,
        VkImageLayout oldLayout,
        VkImageLayout newLayout)
    {
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = m_Image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;

        // Determine access masks and pipeline stages based on transition
        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
            newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
        {
            // Initial transition for rendering
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            dstStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL &&
                 newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
        {
            // Transition from sampling to rendering (next frame)
            barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            srcStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            dstStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL &&
                 newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            // Transition from rendering to sampling
            barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            srcStage = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        else
        {
            LOG_WARN("Unsupported shadow map layout transition: {} -> {}",
                     static_cast<int>(oldLayout), static_cast<int>(newLayout));
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        }

        cmdBuffer->PipelineBarrier(srcStage, dstStage, {}, {}, {barrier});
        m_CurrentLayout = newLayout;
    }

    void ShadowMap::SetLightMatrices(const glm::mat4& view, const glm::mat4& projection)
    {
        m_LightView = view;
        m_LightProjection = projection;
    }

    glm::mat4 ShadowMap::GetLightSpaceMatrix() const
    {
        return m_LightProjection * m_LightView;
    }

} // namespace Renderer
