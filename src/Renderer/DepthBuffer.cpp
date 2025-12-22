/**
 * @file DepthBuffer.cpp
 * @brief Depth buffer implementation.
 */

#include "Renderer/DepthBuffer.h"
#include "RHI/RHICommandBuffer.h"
#include "RHI/RHIDeletionQueue.h"
#include "RHI/RHIDevice.h"
#include "Core/Log.h"

#include <vk_mem_alloc.h>

namespace Renderer
{
    Core::Ref<DepthBuffer> DepthBuffer::Create(
        const Core::Ref<RHI::RHIDevice>& device,
        const Core::Ref<RHI::RHIDeletionQueue>& deletionQueue,
        const DepthBufferDesc& desc)
    {
        auto depthBuffer = Core::Ref<DepthBuffer>(new DepthBuffer());
        if (!depthBuffer->Initialize(device, deletionQueue, desc))
        {
            return nullptr;
        }
        return depthBuffer;
    }

    DepthBuffer::~DepthBuffer()
    {
        // Queue resources for deferred deletion to avoid GPU resource conflicts
        if (m_DeletionQueue && m_ImageView != VK_NULL_HANDLE)
        {
            VkDevice device = m_Device;
            VkImageView imageView = m_ImageView;
            m_DeletionQueue->Push([device, imageView]() {
                vkDestroyImageView(device, imageView, nullptr);
                LOG_DEBUG("Destroyed depth buffer image view (deferred)");
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
                LOG_DEBUG("Destroyed depth buffer image (deferred)");
            });
            m_Image = VK_NULL_HANDLE;
            m_Allocation = nullptr;
        }
    }

    bool DepthBuffer::Initialize(
        const Core::Ref<RHI::RHIDevice>& device,
        const Core::Ref<RHI::RHIDeletionQueue>& deletionQueue,
        const DepthBufferDesc& desc)
    {
        m_Device = device->GetHandle();
        m_Allocator = device->GetAllocator();
        m_DeletionQueue = deletionQueue;
        m_Width = desc.Width;
        m_Height = desc.Height;
        m_Format = desc.Format;

        if (m_Width == 0 || m_Height == 0)
        {
            LOG_ERROR("Invalid depth buffer dimensions: {}x{}", m_Width, m_Height);
            return false;
        }

        if (!CreateImage(device))
        {
            return false;
        }

        if (!CreateImageView(device))
        {
            Destroy(device);
            return false;
        }

        LOG_DEBUG("Created depth buffer: {}x{}, format={}", m_Width, m_Height, static_cast<int>(m_Format));
        return true;
    }

    bool DepthBuffer::CreateImage(const Core::Ref<RHI::RHIDevice>& device)
    {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = m_Width;
        imageInfo.extent.height = m_Height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = m_Format;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
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
            LOG_ERROR("Failed to create depth image: {}", static_cast<int>(result));
            return false;
        }

        return true;
    }

    bool DepthBuffer::CreateImageView(const Core::Ref<RHI::RHIDevice>& device)
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

        // Include stencil aspect if format has stencil
        if (HasStencil())
        {
            viewInfo.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }

        VkResult result = vkCreateImageView(
            device->GetHandle(),
            &viewInfo,
            nullptr,
            &m_ImageView);

        if (result != VK_SUCCESS)
        {
            LOG_ERROR("Failed to create depth image view: {}", static_cast<int>(result));
            return false;
        }

        return true;
    }

    void DepthBuffer::Destroy(const Core::Ref<RHI::RHIDevice>& device)
    {
        if (m_ImageView != VK_NULL_HANDLE)
        {
            vkDestroyImageView(device->GetHandle(), m_ImageView, nullptr);
            m_ImageView = VK_NULL_HANDLE;
        }

        if (m_Image != VK_NULL_HANDLE)
        {
            vmaDestroyImage(device->GetAllocator(), m_Image, m_Allocation);
            m_Image = VK_NULL_HANDLE;
            m_Allocation = nullptr;
        }
    }

    bool DepthBuffer::Resize(
        const Core::Ref<RHI::RHIDevice>& device,
        uint32_t width,
        uint32_t height)
    {
        if (width == 0 || height == 0)
        {
            LOG_ERROR("Invalid resize dimensions: {}x{}", width, height);
            return false;
        }

        // No need to resize if dimensions haven't changed
        if (width == m_Width && height == m_Height)
        {
            return true;
        }

        // Destroy existing resources
        Destroy(device);

        // Update dimensions
        m_Width = width;
        m_Height = height;

        // Recreate resources
        if (!CreateImage(device))
        {
            return false;
        }

        if (!CreateImageView(device))
        {
            Destroy(device);
            return false;
        }

        LOG_DEBUG("Resized depth buffer: {}x{}", m_Width, m_Height);
        return true;
    }

    void DepthBuffer::TransitionLayout(
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
        if (HasStencil())
        {
            barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;

        // Determine access masks based on layouts
        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
            newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
        {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL &&
                 newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            srcStage = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        else
        {
            LOG_WARN("Unsupported depth buffer layout transition: {} -> {}",
                     static_cast<int>(oldLayout), static_cast<int>(newLayout));
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        }

        cmdBuffer->PipelineBarrier(srcStage, dstStage, {}, {}, {barrier});
    }

} // namespace Renderer
