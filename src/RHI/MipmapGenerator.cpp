/**
 * @file MipmapGenerator.cpp
 * @brief Implementation of GPU-based mipmap generation using blit commands.
 */

#include "RHI/MipmapGenerator.h"

#include <algorithm>
#include <cmath>

namespace RHI
{
    void MipmapGenerator::Generate(
        VkCommandBuffer cmd,
        VkImage image,
        VkFormat format,
        uint32_t width,
        uint32_t height,
        uint32_t mipLevels,
        uint32_t arrayLayers)
    {
        if (mipLevels <= 1)
        {
            // No mipmaps to generate, just transition base level to shader read
            VkImageMemoryBarrier barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.image = image;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barrier.subresourceRange.aspectMask = GetAspectMask(format);
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = arrayLayers;

            vkCmdPipelineBarrier(
                cmd,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0,
                0, nullptr,
                0, nullptr,
                1, &barrier);
            return;
        }

        VkImageAspectFlags aspectMask = GetAspectMask(format);

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.image = image;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.subresourceRange.aspectMask = aspectMask;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = arrayLayers;
        barrier.subresourceRange.levelCount = 1;

        int32_t mipWidth = static_cast<int32_t>(width);
        int32_t mipHeight = static_cast<int32_t>(height);

        for (uint32_t i = 1; i < mipLevels; i++)
        {
            // Transition previous mip level from TRANSFER_DST to TRANSFER_SRC
            // This allows us to read from it for the blit operation
            barrier.subresourceRange.baseMipLevel = i - 1;
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

            vkCmdPipelineBarrier(
                cmd,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                0,
                0, nullptr,
                0, nullptr,
                1, &barrier);

            // Calculate destination mip level dimensions
            int32_t dstWidth = mipWidth > 1 ? mipWidth / 2 : 1;
            int32_t dstHeight = mipHeight > 1 ? mipHeight / 2 : 1;

            // Blit from previous level to current level with linear filtering
            VkImageBlit blit{};
            blit.srcOffsets[0] = {0, 0, 0};
            blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
            blit.srcSubresource.aspectMask = aspectMask;
            blit.srcSubresource.mipLevel = i - 1;
            blit.srcSubresource.baseArrayLayer = 0;
            blit.srcSubresource.layerCount = arrayLayers;
            blit.dstOffsets[0] = {0, 0, 0};
            blit.dstOffsets[1] = {dstWidth, dstHeight, 1};
            blit.dstSubresource.aspectMask = aspectMask;
            blit.dstSubresource.mipLevel = i;
            blit.dstSubresource.baseArrayLayer = 0;
            blit.dstSubresource.layerCount = arrayLayers;

            vkCmdBlitImage(
                cmd,
                image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1, &blit,
                VK_FILTER_LINEAR);

            // Transition the previous mip level to SHADER_READ_ONLY
            // It's done being used as a source
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            vkCmdPipelineBarrier(
                cmd,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0,
                0, nullptr,
                0, nullptr,
                1, &barrier);

            // Update dimensions for next iteration
            mipWidth = dstWidth;
            mipHeight = dstHeight;
        }

        // Transition the last mip level to SHADER_READ_ONLY
        barrier.subresourceRange.baseMipLevel = mipLevels - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(
            cmd,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier);
    }

    uint32_t MipmapGenerator::CalculateMipLevels(uint32_t width, uint32_t height)
    {
        return static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;
    }

    bool MipmapGenerator::SupportsLinearFiltering(
        VkPhysicalDevice physicalDevice,
        VkFormat format)
    {
        VkFormatProperties formatProps;
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &formatProps);

        return (formatProps.optimalTilingFeatures &
                VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) != 0;
    }

    VkImageAspectFlags MipmapGenerator::GetAspectMask(VkFormat format)
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
