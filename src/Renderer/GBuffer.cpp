/**
 * @file GBuffer.cpp
 * @brief G-Buffer implementation for deferred rendering.
 */

#include "Renderer/GBuffer.h"
#include "RHI/RHICommandBuffer.h"
#include "RHI/RHIDeletionQueue.h"
#include "RHI/RHIDevice.h"
#include "Core/Log.h"

#include <vk_mem_alloc.h>

namespace Renderer
{
    Core::Ref<GBuffer> GBuffer::Create(
        const Core::Ref<RHI::RHIDevice>& device,
        const Core::Ref<RHI::RHIDeletionQueue>& deletionQueue,
        const GBufferDesc& desc)
    {
        auto gBuffer = Core::Ref<GBuffer>(new GBuffer());
        if (!gBuffer->Initialize(device, deletionQueue, desc))
        {
            return nullptr;
        }
        return gBuffer;
    }

    GBuffer::~GBuffer()
    {
        // Queue all resources for deferred deletion to avoid GPU resource conflicts

        // Albedo
        if (m_DeletionQueue && m_AlbedoView != VK_NULL_HANDLE)
        {
            VkDevice device = m_Device;
            VkImageView view = m_AlbedoView;
            m_DeletionQueue->Push([device, view]() {
                vkDestroyImageView(device, view, nullptr);
                LOG_DEBUG("Destroyed G-Buffer albedo image view (deferred)");
            });
            m_AlbedoView = VK_NULL_HANDLE;
        }
        if (m_DeletionQueue && m_AlbedoImage != VK_NULL_HANDLE && m_Allocator != nullptr)
        {
            VmaAllocator allocator = m_Allocator;
            VkImage image = m_AlbedoImage;
            VmaAllocation allocation = m_AlbedoAllocation;
            m_DeletionQueue->Push([allocator, image, allocation]() {
                vmaDestroyImage(allocator, image, allocation);
                LOG_DEBUG("Destroyed G-Buffer albedo image (deferred)");
            });
            m_AlbedoImage = VK_NULL_HANDLE;
            m_AlbedoAllocation = nullptr;
        }

        // Normal
        if (m_DeletionQueue && m_NormalView != VK_NULL_HANDLE)
        {
            VkDevice device = m_Device;
            VkImageView view = m_NormalView;
            m_DeletionQueue->Push([device, view]() {
                vkDestroyImageView(device, view, nullptr);
                LOG_DEBUG("Destroyed G-Buffer normal image view (deferred)");
            });
            m_NormalView = VK_NULL_HANDLE;
        }
        if (m_DeletionQueue && m_NormalImage != VK_NULL_HANDLE && m_Allocator != nullptr)
        {
            VmaAllocator allocator = m_Allocator;
            VkImage image = m_NormalImage;
            VmaAllocation allocation = m_NormalAllocation;
            m_DeletionQueue->Push([allocator, image, allocation]() {
                vmaDestroyImage(allocator, image, allocation);
                LOG_DEBUG("Destroyed G-Buffer normal image (deferred)");
            });
            m_NormalImage = VK_NULL_HANDLE;
            m_NormalAllocation = nullptr;
        }

        // Material
        if (m_DeletionQueue && m_MaterialView != VK_NULL_HANDLE)
        {
            VkDevice device = m_Device;
            VkImageView view = m_MaterialView;
            m_DeletionQueue->Push([device, view]() {
                vkDestroyImageView(device, view, nullptr);
                LOG_DEBUG("Destroyed G-Buffer material image view (deferred)");
            });
            m_MaterialView = VK_NULL_HANDLE;
        }
        if (m_DeletionQueue && m_MaterialImage != VK_NULL_HANDLE && m_Allocator != nullptr)
        {
            VmaAllocator allocator = m_Allocator;
            VkImage image = m_MaterialImage;
            VmaAllocation allocation = m_MaterialAllocation;
            m_DeletionQueue->Push([allocator, image, allocation]() {
                vmaDestroyImage(allocator, image, allocation);
                LOG_DEBUG("Destroyed G-Buffer material image (deferred)");
            });
            m_MaterialImage = VK_NULL_HANDLE;
            m_MaterialAllocation = nullptr;
        }

        // Emissive
        if (m_DeletionQueue && m_EmissiveView != VK_NULL_HANDLE)
        {
            VkDevice device = m_Device;
            VkImageView view = m_EmissiveView;
            m_DeletionQueue->Push([device, view]() {
                vkDestroyImageView(device, view, nullptr);
                LOG_DEBUG("Destroyed G-Buffer emissive image view (deferred)");
            });
            m_EmissiveView = VK_NULL_HANDLE;
        }
        if (m_DeletionQueue && m_EmissiveImage != VK_NULL_HANDLE && m_Allocator != nullptr)
        {
            VmaAllocator allocator = m_Allocator;
            VkImage image = m_EmissiveImage;
            VmaAllocation allocation = m_EmissiveAllocation;
            m_DeletionQueue->Push([allocator, image, allocation]() {
                vmaDestroyImage(allocator, image, allocation);
                LOG_DEBUG("Destroyed G-Buffer emissive image (deferred)");
            });
            m_EmissiveImage = VK_NULL_HANDLE;
            m_EmissiveAllocation = nullptr;
        }

        // Depth
        if (m_DeletionQueue && m_DepthView != VK_NULL_HANDLE)
        {
            VkDevice device = m_Device;
            VkImageView view = m_DepthView;
            m_DeletionQueue->Push([device, view]() {
                vkDestroyImageView(device, view, nullptr);
                LOG_DEBUG("Destroyed G-Buffer depth image view (deferred)");
            });
            m_DepthView = VK_NULL_HANDLE;
        }
        if (m_DeletionQueue && m_DepthImage != VK_NULL_HANDLE && m_Allocator != nullptr)
        {
            VmaAllocator allocator = m_Allocator;
            VkImage image = m_DepthImage;
            VmaAllocation allocation = m_DepthAllocation;
            m_DeletionQueue->Push([allocator, image, allocation]() {
                vmaDestroyImage(allocator, image, allocation);
                LOG_DEBUG("Destroyed G-Buffer depth image (deferred)");
            });
            m_DepthImage = VK_NULL_HANDLE;
            m_DepthAllocation = nullptr;
        }
    }

    bool GBuffer::Initialize(
        const Core::Ref<RHI::RHIDevice>& device,
        const Core::Ref<RHI::RHIDeletionQueue>& deletionQueue,
        const GBufferDesc& desc)
    {
        if (!device)
        {
            LOG_ERROR("Device cannot be null");
            return false;
        }

        if (!deletionQueue)
        {
            LOG_ERROR("Deletion queue cannot be null");
            return false;
        }

        m_Device = device->GetHandle();
        m_Allocator = device->GetAllocator();
        m_DeletionQueue = deletionQueue;
        m_Width = desc.Width;
        m_Height = desc.Height;
        m_AlbedoFormat = desc.AlbedoFormat;
        m_NormalFormat = desc.NormalFormat;
        m_MaterialFormat = desc.MaterialFormat;
        m_EmissiveFormat = desc.EmissiveFormat;
        m_DepthFormat = desc.DepthFormat;

        if (m_Width == 0 || m_Height == 0)
        {
            LOG_ERROR("Invalid G-Buffer dimensions: {}x{}", m_Width, m_Height);
            return false;
        }

        if (!CreateImages(device))
        {
            return false;
        }

        if (!CreateImageViews(device))
        {
            Destroy(device);
            return false;
        }

        LOG_INFO("Created G-Buffer: {}x{} with {} color attachments",
                 m_Width, m_Height, COLOR_ATTACHMENT_COUNT);
        return true;
    }

    bool GBuffer::CreateImages(const Core::Ref<RHI::RHIDevice>& device)
    {
        // Create color render targets
        if (!CreateColorImage(device, m_AlbedoFormat, m_AlbedoImage, m_AlbedoAllocation))
        {
            LOG_ERROR("Failed to create G-Buffer albedo image");
            return false;
        }

        if (!CreateColorImage(device, m_NormalFormat, m_NormalImage, m_NormalAllocation))
        {
            LOG_ERROR("Failed to create G-Buffer normal image");
            return false;
        }

        if (!CreateColorImage(device, m_MaterialFormat, m_MaterialImage, m_MaterialAllocation))
        {
            LOG_ERROR("Failed to create G-Buffer material image");
            return false;
        }

        if (!CreateColorImage(device, m_EmissiveFormat, m_EmissiveImage, m_EmissiveAllocation))
        {
            LOG_ERROR("Failed to create G-Buffer emissive image");
            return false;
        }

        // Create depth buffer
        if (!CreateDepthImage(device))
        {
            LOG_ERROR("Failed to create G-Buffer depth image");
            return false;
        }

        return true;
    }

    bool GBuffer::CreateColorImage(
        const Core::Ref<RHI::RHIDevice>& device,
        VkFormat format,
        VkImage& outImage,
        VmaAllocation& outAllocation)
    {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = m_Width;
        imageInfo.extent.height = m_Height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = format;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        // Color attachment for rendering + sampled for deferred lighting pass
        imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        VkResult result = vmaCreateImage(
            device->GetAllocator(),
            &imageInfo,
            &allocInfo,
            &outImage,
            &outAllocation,
            nullptr);

        if (result != VK_SUCCESS)
        {
            LOG_ERROR("Failed to create G-Buffer color image: {}", static_cast<int>(result));
            return false;
        }

        return true;
    }

    bool GBuffer::CreateDepthImage(const Core::Ref<RHI::RHIDevice>& device)
    {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = m_Width;
        imageInfo.extent.height = m_Height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = m_DepthFormat;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        // Depth attachment for rendering + sampled for deferred lighting pass
        imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        VkResult result = vmaCreateImage(
            device->GetAllocator(),
            &imageInfo,
            &allocInfo,
            &m_DepthImage,
            &m_DepthAllocation,
            nullptr);

        if (result != VK_SUCCESS)
        {
            LOG_ERROR("Failed to create G-Buffer depth image: {}", static_cast<int>(result));
            return false;
        }

        return true;
    }

    bool GBuffer::CreateImageViews(const Core::Ref<RHI::RHIDevice>& device)
    {
        // Create color image views
        if (!CreateColorImageView(device, m_AlbedoImage, m_AlbedoFormat, m_AlbedoView))
        {
            LOG_ERROR("Failed to create G-Buffer albedo image view");
            return false;
        }

        if (!CreateColorImageView(device, m_NormalImage, m_NormalFormat, m_NormalView))
        {
            LOG_ERROR("Failed to create G-Buffer normal image view");
            return false;
        }

        if (!CreateColorImageView(device, m_MaterialImage, m_MaterialFormat, m_MaterialView))
        {
            LOG_ERROR("Failed to create G-Buffer material image view");
            return false;
        }

        if (!CreateColorImageView(device, m_EmissiveImage, m_EmissiveFormat, m_EmissiveView))
        {
            LOG_ERROR("Failed to create G-Buffer emissive image view");
            return false;
        }

        // Create depth image view
        if (!CreateDepthImageView(device))
        {
            LOG_ERROR("Failed to create G-Buffer depth image view");
            return false;
        }

        return true;
    }

    bool GBuffer::CreateColorImageView(
        const Core::Ref<RHI::RHIDevice>& device,
        VkImage image,
        VkFormat format,
        VkImageView& outView)
    {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        VkResult result = vkCreateImageView(
            device->GetHandle(),
            &viewInfo,
            nullptr,
            &outView);

        if (result != VK_SUCCESS)
        {
            LOG_ERROR("Failed to create G-Buffer color image view: {}", static_cast<int>(result));
            return false;
        }

        return true;
    }

    bool GBuffer::CreateDepthImageView(const Core::Ref<RHI::RHIDevice>& device)
    {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_DepthImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = m_DepthFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        VkResult result = vkCreateImageView(
            device->GetHandle(),
            &viewInfo,
            nullptr,
            &m_DepthView);

        if (result != VK_SUCCESS)
        {
            LOG_ERROR("Failed to create G-Buffer depth image view: {}", static_cast<int>(result));
            return false;
        }

        return true;
    }

    void GBuffer::Destroy(const Core::Ref<RHI::RHIDevice>& device)
    {
        // Queue all resources for deferred deletion

        // Albedo
        if (m_DeletionQueue && m_AlbedoView != VK_NULL_HANDLE)
        {
            VkDevice vkDevice = device->GetHandle();
            VkImageView view = m_AlbedoView;
            m_DeletionQueue->Push([vkDevice, view]() {
                vkDestroyImageView(vkDevice, view, nullptr);
                LOG_DEBUG("Destroyed G-Buffer albedo image view (deferred via Destroy)");
            });
            m_AlbedoView = VK_NULL_HANDLE;
        }
        if (m_DeletionQueue && m_AlbedoImage != VK_NULL_HANDLE)
        {
            VmaAllocator allocator = device->GetAllocator();
            VkImage image = m_AlbedoImage;
            VmaAllocation allocation = m_AlbedoAllocation;
            m_DeletionQueue->Push([allocator, image, allocation]() {
                vmaDestroyImage(allocator, image, allocation);
                LOG_DEBUG("Destroyed G-Buffer albedo image (deferred via Destroy)");
            });
            m_AlbedoImage = VK_NULL_HANDLE;
            m_AlbedoAllocation = nullptr;
        }

        // Normal
        if (m_DeletionQueue && m_NormalView != VK_NULL_HANDLE)
        {
            VkDevice vkDevice = device->GetHandle();
            VkImageView view = m_NormalView;
            m_DeletionQueue->Push([vkDevice, view]() {
                vkDestroyImageView(vkDevice, view, nullptr);
                LOG_DEBUG("Destroyed G-Buffer normal image view (deferred via Destroy)");
            });
            m_NormalView = VK_NULL_HANDLE;
        }
        if (m_DeletionQueue && m_NormalImage != VK_NULL_HANDLE)
        {
            VmaAllocator allocator = device->GetAllocator();
            VkImage image = m_NormalImage;
            VmaAllocation allocation = m_NormalAllocation;
            m_DeletionQueue->Push([allocator, image, allocation]() {
                vmaDestroyImage(allocator, image, allocation);
                LOG_DEBUG("Destroyed G-Buffer normal image (deferred via Destroy)");
            });
            m_NormalImage = VK_NULL_HANDLE;
            m_NormalAllocation = nullptr;
        }

        // Material
        if (m_DeletionQueue && m_MaterialView != VK_NULL_HANDLE)
        {
            VkDevice vkDevice = device->GetHandle();
            VkImageView view = m_MaterialView;
            m_DeletionQueue->Push([vkDevice, view]() {
                vkDestroyImageView(vkDevice, view, nullptr);
                LOG_DEBUG("Destroyed G-Buffer material image view (deferred via Destroy)");
            });
            m_MaterialView = VK_NULL_HANDLE;
        }
        if (m_DeletionQueue && m_MaterialImage != VK_NULL_HANDLE)
        {
            VmaAllocator allocator = device->GetAllocator();
            VkImage image = m_MaterialImage;
            VmaAllocation allocation = m_MaterialAllocation;
            m_DeletionQueue->Push([allocator, image, allocation]() {
                vmaDestroyImage(allocator, image, allocation);
                LOG_DEBUG("Destroyed G-Buffer material image (deferred via Destroy)");
            });
            m_MaterialImage = VK_NULL_HANDLE;
            m_MaterialAllocation = nullptr;
        }

        // Emissive
        if (m_DeletionQueue && m_EmissiveView != VK_NULL_HANDLE)
        {
            VkDevice vkDevice = device->GetHandle();
            VkImageView view = m_EmissiveView;
            m_DeletionQueue->Push([vkDevice, view]() {
                vkDestroyImageView(vkDevice, view, nullptr);
                LOG_DEBUG("Destroyed G-Buffer emissive image view (deferred via Destroy)");
            });
            m_EmissiveView = VK_NULL_HANDLE;
        }
        if (m_DeletionQueue && m_EmissiveImage != VK_NULL_HANDLE)
        {
            VmaAllocator allocator = device->GetAllocator();
            VkImage image = m_EmissiveImage;
            VmaAllocation allocation = m_EmissiveAllocation;
            m_DeletionQueue->Push([allocator, image, allocation]() {
                vmaDestroyImage(allocator, image, allocation);
                LOG_DEBUG("Destroyed G-Buffer emissive image (deferred via Destroy)");
            });
            m_EmissiveImage = VK_NULL_HANDLE;
            m_EmissiveAllocation = nullptr;
        }

        // Depth
        if (m_DeletionQueue && m_DepthView != VK_NULL_HANDLE)
        {
            VkDevice vkDevice = device->GetHandle();
            VkImageView view = m_DepthView;
            m_DeletionQueue->Push([vkDevice, view]() {
                vkDestroyImageView(vkDevice, view, nullptr);
                LOG_DEBUG("Destroyed G-Buffer depth image view (deferred via Destroy)");
            });
            m_DepthView = VK_NULL_HANDLE;
        }
        if (m_DeletionQueue && m_DepthImage != VK_NULL_HANDLE)
        {
            VmaAllocator allocator = device->GetAllocator();
            VkImage image = m_DepthImage;
            VmaAllocation allocation = m_DepthAllocation;
            m_DeletionQueue->Push([allocator, image, allocation]() {
                vmaDestroyImage(allocator, image, allocation);
                LOG_DEBUG("Destroyed G-Buffer depth image (deferred via Destroy)");
            });
            m_DepthImage = VK_NULL_HANDLE;
            m_DepthAllocation = nullptr;
        }

        // Reset layouts
        m_AlbedoLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        m_NormalLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        m_MaterialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        m_EmissiveLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        m_DepthLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    }

    bool GBuffer::Resize(
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
        if (!CreateImages(device))
        {
            return false;
        }

        if (!CreateImageViews(device))
        {
            Destroy(device);
            return false;
        }

        LOG_DEBUG("Resized G-Buffer: {}x{}", m_Width, m_Height);
        return true;
    }

    void GBuffer::TransitionColorLayout(
        const Core::Ref<RHI::RHICommandBuffer>& cmdBuffer,
        VkImage image,
        VkImageLayout oldLayout,
        VkImageLayout newLayout)
    {
        if (oldLayout == newLayout)
        {
            return;
        }

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        // Determine access masks based on layouts
        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
            newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
        {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL &&
                 newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            srcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL &&
                 newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
        {
            barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            srcStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            dstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        }
        else
        {
            LOG_WARN("Unsupported G-Buffer color layout transition: {} -> {}",
                     static_cast<int>(oldLayout), static_cast<int>(newLayout));
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        }

        cmdBuffer->PipelineBarrier(srcStage, dstStage, {}, {}, {barrier});
    }

    void GBuffer::TransitionDepthLayout(
        const Core::Ref<RHI::RHICommandBuffer>& cmdBuffer,
        VkImageLayout oldLayout,
        VkImageLayout newLayout)
    {
        if (oldLayout == newLayout)
        {
            return;
        }

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = m_DepthImage;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
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
        else if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL &&
                 newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
        {
            barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            srcStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            dstStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        }
        else
        {
            LOG_WARN("Unsupported G-Buffer depth layout transition: {} -> {}",
                     static_cast<int>(oldLayout), static_cast<int>(newLayout));
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        }

        cmdBuffer->PipelineBarrier(srcStage, dstStage, {}, {}, {barrier});
    }

    void GBuffer::Begin(const Core::Ref<RHI::RHICommandBuffer>& cmdBuffer)
    {
        // Transition all color targets to attachment optimal
        TransitionColorLayout(cmdBuffer, m_AlbedoImage, m_AlbedoLayout,
                              VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        TransitionColorLayout(cmdBuffer, m_NormalImage, m_NormalLayout,
                              VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        TransitionColorLayout(cmdBuffer, m_MaterialImage, m_MaterialLayout,
                              VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        TransitionColorLayout(cmdBuffer, m_EmissiveImage, m_EmissiveLayout,
                              VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        // Transition depth to attachment optimal
        TransitionDepthLayout(cmdBuffer, m_DepthLayout,
                              VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

        // Update current layouts
        m_AlbedoLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        m_NormalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        m_MaterialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        m_EmissiveLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        m_DepthLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        // Configure MRT rendering
        RHI::RenderingConfig config;
        config.RenderArea = {{0, 0}, {m_Width, m_Height}};

        // RT0: Albedo
        RHI::ColorAttachment albedoAttachment;
        albedoAttachment.ImageView = m_AlbedoView;
        albedoAttachment.LoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        albedoAttachment.StoreOp = VK_ATTACHMENT_STORE_OP_STORE;
        albedoAttachment.ClearValue = {{0.0f, 0.0f, 0.0f, 1.0f}};
        config.ColorAttachments.push_back(albedoAttachment);

        // RT1: Normal
        RHI::ColorAttachment normalAttachment;
        normalAttachment.ImageView = m_NormalView;
        normalAttachment.LoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        normalAttachment.StoreOp = VK_ATTACHMENT_STORE_OP_STORE;
        normalAttachment.ClearValue = {{0.0f, 0.0f, 0.0f, 0.0f}};
        config.ColorAttachments.push_back(normalAttachment);

        // RT2: Material (Metallic/Roughness/AO)
        RHI::ColorAttachment materialAttachment;
        materialAttachment.ImageView = m_MaterialView;
        materialAttachment.LoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        materialAttachment.StoreOp = VK_ATTACHMENT_STORE_OP_STORE;
        materialAttachment.ClearValue = {{0.0f, 0.5f, 1.0f, 0.0f}}; // Default: no metallic, 0.5 roughness, full AO
        config.ColorAttachments.push_back(materialAttachment);

        // RT3: Emissive
        RHI::ColorAttachment emissiveAttachment;
        emissiveAttachment.ImageView = m_EmissiveView;
        emissiveAttachment.LoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        emissiveAttachment.StoreOp = VK_ATTACHMENT_STORE_OP_STORE;
        emissiveAttachment.ClearValue = {{0.0f, 0.0f, 0.0f, 0.0f}};
        config.ColorAttachments.push_back(emissiveAttachment);

        // Depth
        config.Depth.ImageView = m_DepthView;
        config.Depth.LoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        config.Depth.StoreOp = VK_ATTACHMENT_STORE_OP_STORE;
        config.Depth.ClearValue = {1.0f, 0};

        cmdBuffer->BeginRendering(config);

        // Set viewport and scissor
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(m_Width);
        viewport.height = static_cast<float>(m_Height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        cmdBuffer->SetViewport(viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = {m_Width, m_Height};
        cmdBuffer->SetScissor(scissor);
    }

    void GBuffer::End(const Core::Ref<RHI::RHICommandBuffer>& cmdBuffer)
    {
        cmdBuffer->EndRendering();

        // Transition all color targets to shader read-only optimal for lighting pass
        TransitionColorLayout(cmdBuffer, m_AlbedoImage, m_AlbedoLayout,
                              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        TransitionColorLayout(cmdBuffer, m_NormalImage, m_NormalLayout,
                              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        TransitionColorLayout(cmdBuffer, m_MaterialImage, m_MaterialLayout,
                              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        TransitionColorLayout(cmdBuffer, m_EmissiveImage, m_EmissiveLayout,
                              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        // Transition depth to shader read-only optimal
        TransitionDepthLayout(cmdBuffer, m_DepthLayout,
                              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        // Update current layouts
        m_AlbedoLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        m_NormalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        m_MaterialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        m_EmissiveLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        m_DepthLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    std::vector<VkImageView> GBuffer::GetColorViews() const
    {
        return {m_AlbedoView, m_NormalView, m_MaterialView, m_EmissiveView};
    }

    std::vector<VkFormat> GBuffer::GetColorFormats() const
    {
        return {m_AlbedoFormat, m_NormalFormat, m_MaterialFormat, m_EmissiveFormat};
    }

} // namespace Renderer
