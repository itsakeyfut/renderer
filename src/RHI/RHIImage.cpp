/**
 * @file RHIImage.cpp
 * @brief Implementation of the Vulkan Image wrapper.
 */

#include "RHI/RHIImage.h"
#include "RHI/RHIBuffer.h"
#include "RHI/RHICommandPool.h"
#include "RHI/RHICommandBuffer.h"
#include "Core/Assert.h"
#include "Core/Log.h"

#include <cmath>

namespace RHI
{
    Core::Ref<RHIImage> RHIImage::Create(
        const Core::Ref<RHIDevice>& device,
        const ImageDesc& desc)
    {
        ASSERT(device != nullptr);
        ASSERT(desc.Width > 0 && desc.Height > 0);

        auto image = Core::Ref<RHIImage>(new RHIImage());
        if (!image->Initialize(device, desc))
        {
            LOG_ERROR("Failed to create RHIImage");
            return nullptr;
        }

        if (desc.DebugName)
        {
            LOG_DEBUG("Created image '{}': {}x{}, format={}",
                      desc.DebugName, desc.Width, desc.Height, static_cast<int>(desc.Format));
        }

        return image;
    }

    Core::Ref<RHIImage> RHIImage::CreateWithData(
        const Core::Ref<RHIDevice>& device,
        const ImageDesc& desc,
        const void* data,
        VkDeviceSize dataSize)
    {
        ASSERT(device != nullptr);
        ASSERT(data != nullptr);
        ASSERT(dataSize > 0);

        auto image = Create(device, desc);
        if (!image)
        {
            return nullptr;
        }

        if (!image->UploadData(device, data, dataSize))
        {
            LOG_ERROR("Failed to upload data to image");
            return nullptr;
        }

        // Generate mipmaps if requested and image has multiple mip levels
        if (desc.GenerateMipmaps && image->GetMipLevels() > 1)
        {
            if (!image->GenerateMipmaps(device))
            {
                LOG_WARN("Failed to generate mipmaps for image");
                // Ensure image is still in a usable layout for sampling
                // GenerateMipmaps leaves it in TRANSFER_DST if it fails early
            }
        }

        // Ensure texture is in shader read layout after all operations
        if (desc.Usage == ImageUsage::Texture &&
            image->GetLayout() != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            // Create a one-time command buffer to transition the image
            RHICommandPoolConfig poolConfig;
            poolConfig.QueueType = CommandPoolQueueType::Graphics;
            poolConfig.Transient = true;
            auto commandPool = RHICommandPool::Create(device, poolConfig);
            if (commandPool)
            {
                auto cmdBuffer = RHICommandBuffer::Create(device, commandPool);
                if (cmdBuffer)
                {
                    cmdBuffer->Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
                    image->TransitionLayout(cmdBuffer, image->GetLayout(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                    cmdBuffer->End();

                    VkSubmitInfo submitInfo{};
                    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                    submitInfo.commandBufferCount = 1;
                    VkCommandBuffer cmdHandle = cmdBuffer->GetHandle();
                    submitInfo.pCommandBuffers = &cmdHandle;

                    VkResult result = vkQueueSubmit(device->GetGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
                    if (result != VK_SUCCESS)
                    {
                        LOG_ERROR("Failed to submit final layout transition command: VkResult {}", static_cast<int>(result));
                        return nullptr;
                    }
                    vkQueueWaitIdle(device->GetGraphicsQueue());
                }
            }
        }

        return image;
    }

    RHIImage::~RHIImage()
    {
        if (m_Allocator != VK_NULL_HANDLE)
        {
            VmaAllocatorInfo allocInfo;
            vmaGetAllocatorInfo(m_Allocator, &allocInfo);

            if (m_StorageImageView != VK_NULL_HANDLE)
            {
                vkDestroyImageView(allocInfo.device, m_StorageImageView, nullptr);
                m_StorageImageView = VK_NULL_HANDLE;
            }

            if (m_ImageView != VK_NULL_HANDLE)
            {
                vkDestroyImageView(allocInfo.device, m_ImageView, nullptr);
                m_ImageView = VK_NULL_HANDLE;
            }
        }

        if (m_Image != VK_NULL_HANDLE && m_Allocator != VK_NULL_HANDLE)
        {
            vmaDestroyImage(m_Allocator, m_Image, m_Allocation);
            m_Image = VK_NULL_HANDLE;
            m_Allocation = VK_NULL_HANDLE;
        }
    }

    bool RHIImage::Initialize(
        const Core::Ref<RHIDevice>& device,
        const ImageDesc& desc)
    {
        m_Allocator = device->GetAllocator();
        m_Width = desc.Width;
        m_Height = desc.Height;
        m_Depth = desc.Depth;
        m_MipLevels = desc.MipLevels;
        m_ArrayLayers = desc.ArrayLayers;
        m_Format = desc.Format;
        m_Samples = desc.Samples;
        m_Usage = desc.Usage;
        m_IsCubemap = desc.IsCubemap;

        // Validate cubemap requirements
        if (m_IsCubemap)
        {
            if (m_ArrayLayers != 6)
            {
                LOG_ERROR("Cubemap images must have exactly 6 array layers, got {}", m_ArrayLayers);
                return false;
            }
            if (m_Width != m_Height)
            {
                LOG_WARN("Cubemap images should have equal width and height for best results");
            }
        }

        // Calculate mip levels if generating mipmaps
        if (desc.GenerateMipmaps)
        {
            m_MipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(m_Width, m_Height)))) + 1;
        }

        if (!CreateImage(device))
        {
            return false;
        }

        if (!CreateImageView(device))
        {
            vmaDestroyImage(m_Allocator, m_Image, m_Allocation);
            m_Image = VK_NULL_HANDLE;
            return false;
        }

        return true;
    }

    bool RHIImage::CreateImage(const Core::Ref<RHIDevice>& device)
    {
        (void)device;  // VMA allocator already cached in m_Allocator
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = m_Depth > 1 ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = m_Width;
        imageInfo.extent.height = m_Height;
        imageInfo.extent.depth = m_Depth;
        imageInfo.mipLevels = m_MipLevels;
        imageInfo.arrayLayers = m_ArrayLayers;
        imageInfo.format = m_Format;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = ToVkImageUsage(m_Usage);
        imageInfo.samples = m_Samples;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        // Add cubemap compatible flag for cubemap images
        if (m_IsCubemap)
        {
            imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        }

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        VkResult result = vmaCreateImage(
            m_Allocator,
            &imageInfo,
            &allocInfo,
            &m_Image,
            &m_Allocation,
            nullptr);

        if (result != VK_SUCCESS)
        {
            LOG_ERROR("Failed to create image: VkResult {}", static_cast<int>(result));
            return false;
        }

        return true;
    }

    bool RHIImage::CreateImageView(const Core::Ref<RHIDevice>& device)
    {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_Image;

        // Determine image view type based on image configuration
        if (m_IsCubemap)
        {
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
        }
        else if (m_Depth > 1)
        {
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;
        }
        else if (m_ArrayLayers > 1)
        {
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        }
        else
        {
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        }

        viewInfo.format = m_Format;
        viewInfo.subresourceRange.aspectMask = GetAspectMask(m_Format);
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = m_MipLevels;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = m_ArrayLayers;

        VkResult result = vkCreateImageView(
            device->GetHandle(),
            &viewInfo,
            nullptr,
            &m_ImageView);

        if (result != VK_SUCCESS)
        {
            LOG_ERROR("Failed to create image view: VkResult {}", static_cast<int>(result));
            return false;
        }

        // For cubemaps with Storage usage, create an additional 2D array view
        // This is required because compute shaders can't use VK_IMAGE_VIEW_TYPE_CUBE
        // for storage images - they need VK_IMAGE_VIEW_TYPE_2D_ARRAY
        if (m_IsCubemap && m_Usage == ImageUsage::Storage)
        {
            VkImageViewCreateInfo storageViewInfo = viewInfo;
            storageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;

            result = vkCreateImageView(
                device->GetHandle(),
                &storageViewInfo,
                nullptr,
                &m_StorageImageView);

            if (result != VK_SUCCESS)
            {
                LOG_ERROR("Failed to create storage image view: VkResult {}", static_cast<int>(result));
                return false;
            }
        }

        return true;
    }

    bool RHIImage::UploadData(
        const Core::Ref<RHIDevice>& device,
        const void* data,
        VkDeviceSize dataSize)
    {
        ASSERT(data != nullptr);
        ASSERT(dataSize > 0);

        // Create staging buffer
        BufferDesc stagingDesc;
        stagingDesc.Size = dataSize;
        stagingDesc.Usage = BufferUsage::Staging;
        stagingDesc.Memory = MemoryUsage::CpuOnly;

        auto stagingBuffer = RHIBuffer::Create(device, stagingDesc);
        if (!stagingBuffer)
        {
            LOG_ERROR("Failed to create staging buffer for image upload");
            return false;
        }

        if (!stagingBuffer->SetData(data, dataSize))
        {
            LOG_ERROR("Failed to copy data to staging buffer");
            return false;
        }

        // Create command pool and buffer for transfer
        RHICommandPoolConfig poolConfig;
        poolConfig.QueueType = CommandPoolQueueType::Graphics;
        poolConfig.Transient = true;
        auto commandPool = RHICommandPool::Create(device, poolConfig);
        if (!commandPool)
        {
            LOG_ERROR("Failed to create command pool for image upload");
            return false;
        }

        auto commandBuffer = RHICommandBuffer::Create(device, commandPool);
        if (!commandBuffer)
        {
            LOG_ERROR("Failed to create command buffer for image upload");
            return false;
        }

        commandBuffer->Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

        // Transition to transfer destination
        TransitionLayout(commandBuffer, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        // Copy buffer to image
        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;  // Tightly packed
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = GetAspectMask(m_Format);
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = m_ArrayLayers;
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {m_Width, m_Height, m_Depth};

        vkCmdCopyBufferToImage(
            commandBuffer->GetHandle(),
            stagingBuffer->GetHandle(),
            m_Image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &region);

        // Transition to shader read optimal (for textures without mipmaps)
        // If mipmaps will be generated, leave in TRANSFER_DST_OPTIMAL for GenerateMipmaps
        if (m_Usage == ImageUsage::Texture && m_MipLevels == 1)
        {
            TransitionLayout(commandBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }

        commandBuffer->End();

        // Submit and wait
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        VkCommandBuffer cmdHandle = commandBuffer->GetHandle();
        submitInfo.pCommandBuffers = &cmdHandle;

        VkResult result = vkQueueSubmit(device->GetGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
        if (result != VK_SUCCESS)
        {
            LOG_ERROR("Failed to submit image upload command: VkResult {}", static_cast<int>(result));
            return false;
        }
        vkQueueWaitIdle(device->GetGraphicsQueue());

        return true;
    }

    void RHIImage::TransitionLayout(
        const Core::Ref<RHICommandBuffer>& cmdBuffer,
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
        barrier.subresourceRange.aspectMask = GetAspectMask(m_Format);
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = m_MipLevels;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = m_ArrayLayers;

        VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
            newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
                 newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
                 newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL &&
                 newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        {
            barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            srcStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        }
        else
        {
            LOG_WARN("Unsupported image layout transition: {} -> {}",
                     static_cast<int>(oldLayout), static_cast<int>(newLayout));
            // Use conservative fallback for unknown transitions
            barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
            srcStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
            dstStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        }

        cmdBuffer->PipelineBarrier(srcStage, dstStage, {}, {}, {barrier});
        m_CurrentLayout = newLayout;
    }

    bool RHIImage::GenerateMipmaps(const Core::Ref<RHIDevice>& device)
    {
        // Check if format supports linear filtering
        VkFormatProperties formatProps;
        vkGetPhysicalDeviceFormatProperties(device->GetPhysicalDevice(), m_Format, &formatProps);

        if (!(formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
        {
            LOG_WARN("Image format does not support linear filtering for mipmaps");
            return false;
        }

        // Create command pool and buffer
        RHICommandPoolConfig poolConfig;
        poolConfig.QueueType = CommandPoolQueueType::Graphics;
        poolConfig.Transient = true;
        auto commandPool = RHICommandPool::Create(device, poolConfig);
        if (!commandPool)
        {
            return false;
        }

        auto commandBuffer = RHICommandBuffer::Create(device, commandPool);
        if (!commandBuffer)
        {
            return false;
        }

        commandBuffer->Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.image = m_Image;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = m_ArrayLayers;
        barrier.subresourceRange.levelCount = 1;

        int32_t mipWidth = static_cast<int32_t>(m_Width);
        int32_t mipHeight = static_cast<int32_t>(m_Height);

        for (uint32_t i = 1; i < m_MipLevels; i++)
        {
            // Transition previous mip level to transfer source
            barrier.subresourceRange.baseMipLevel = i - 1;
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

            commandBuffer->PipelineBarrier(
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                {}, {}, {barrier});

            // Blit to current mip level
            VkImageBlit blit{};
            blit.srcOffsets[0] = {0, 0, 0};
            blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
            blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.srcSubresource.mipLevel = i - 1;
            blit.srcSubresource.baseArrayLayer = 0;
            blit.srcSubresource.layerCount = m_ArrayLayers;
            blit.dstOffsets[0] = {0, 0, 0};
            blit.dstOffsets[1] = {
                mipWidth > 1 ? mipWidth / 2 : 1,
                mipHeight > 1 ? mipHeight / 2 : 1,
                1
            };
            blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.dstSubresource.mipLevel = i;
            blit.dstSubresource.baseArrayLayer = 0;
            blit.dstSubresource.layerCount = m_ArrayLayers;

            vkCmdBlitImage(
                commandBuffer->GetHandle(),
                m_Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                m_Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1, &blit,
                VK_FILTER_LINEAR);

            // Transition previous mip to shader read
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            commandBuffer->PipelineBarrier(
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                {}, {}, {barrier});

            if (mipWidth > 1) mipWidth /= 2;
            if (mipHeight > 1) mipHeight /= 2;
        }

        // Transition last mip level to shader read
        barrier.subresourceRange.baseMipLevel = m_MipLevels - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        commandBuffer->PipelineBarrier(
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            {}, {}, {barrier});

        commandBuffer->End();

        // Submit and wait
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        VkCommandBuffer cmdHandle = commandBuffer->GetHandle();
        submitInfo.pCommandBuffers = &cmdHandle;

        VkResult result = vkQueueSubmit(device->GetGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
        if (result != VK_SUCCESS)
        {
            LOG_ERROR("Failed to submit mipmap generation command: VkResult {}", static_cast<int>(result));
            return false;
        }
        vkQueueWaitIdle(device->GetGraphicsQueue());

        m_CurrentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        return true;
    }

    VkImageUsageFlags RHIImage::ToVkImageUsage(ImageUsage usage)
    {
        switch (usage)
        {
        case ImageUsage::Texture:
            return VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

        case ImageUsage::RenderTarget:
            return VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

        case ImageUsage::DepthStencil:
            return VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

        case ImageUsage::Storage:
            return VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

        case ImageUsage::TransferSrc:
            return VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

        case ImageUsage::TransferDst:
            return VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        default:
            return VK_IMAGE_USAGE_SAMPLED_BIT;
        }
    }

    VkImageAspectFlags RHIImage::GetAspectMask(VkFormat format)
    {
        switch (format)
        {
        case VK_FORMAT_D16_UNORM:
        case VK_FORMAT_D32_SFLOAT:
            return VK_IMAGE_ASPECT_DEPTH_BIT;

        case VK_FORMAT_D16_UNORM_S8_UINT:
        case VK_FORMAT_D24_UNORM_S8_UINT:
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
            return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;

        default:
            return VK_IMAGE_ASPECT_COLOR_BIT;
        }
    }

} // namespace RHI
