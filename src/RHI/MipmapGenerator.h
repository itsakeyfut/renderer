/**
 * @file MipmapGenerator.h
 * @brief Utility class for generating mipmaps using GPU blit commands.
 *
 * Provides static methods for mipmap generation and mip level calculation.
 * Uses vkCmdBlitImage with linear filtering for high-quality downsampling.
 */

#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>

namespace RHI
{
    /**
     * @brief Utility class for GPU-based mipmap generation.
     *
     * Generates mipmaps using vkCmdBlitImage with linear filtering.
     * Records commands to an existing command buffer without submission.
     *
     * Image layout requirements:
     * - Input: Image must be in VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
     * - Output: All mip levels will be in VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
     *
     * Usage:
     * @code
     * uint32_t mipLevels = MipmapGenerator::CalculateMipLevels(width, height);
     *
     * // Begin command buffer recording...
     * MipmapGenerator::Generate(
     *     cmdBuffer,
     *     image,
     *     VK_FORMAT_R8G8B8A8_SRGB,
     *     width,
     *     height,
     *     mipLevels
     * );
     * // End command buffer recording and submit...
     * @endcode
     */
    class MipmapGenerator
    {
    public:
        /**
         * @brief Generate mipmaps for an image using blit commands.
         *
         * Records blit commands and barriers to generate all mip levels.
         * The image must be in TRANSFER_DST_OPTIMAL layout before calling.
         * After completion, all mip levels will be in SHADER_READ_ONLY_OPTIMAL.
         *
         * @param cmd Command buffer to record into (must be in recording state)
         * @param image VkImage to generate mipmaps for
         * @param format Image format (for aspect mask determination)
         * @param width Base level width in pixels
         * @param height Base level height in pixels
         * @param mipLevels Total number of mip levels including base
         * @param arrayLayers Number of array layers (default 1)
         *
         * @note The image format must support linear filtering
         *       (VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)
         */
        static void Generate(
            VkCommandBuffer cmd,
            VkImage image,
            VkFormat format,
            uint32_t width,
            uint32_t height,
            uint32_t mipLevels,
            uint32_t arrayLayers = 1);

        /**
         * @brief Calculate the number of mip levels for given dimensions.
         *
         * Computes floor(log2(max(width, height))) + 1, which gives the
         * complete mipchain down to 1x1 pixels.
         *
         * @param width Image width in pixels
         * @param height Image height in pixels
         * @return Number of mip levels for a complete mipchain
         */
        static uint32_t CalculateMipLevels(uint32_t width, uint32_t height);

        /**
         * @brief Check if a format supports linear filtering for mipmap generation.
         *
         * @param physicalDevice Physical device to check features on
         * @param format Image format to check
         * @return true if format supports linear filtering, false otherwise
         */
        static bool SupportsLinearFiltering(
            VkPhysicalDevice physicalDevice,
            VkFormat format);

    private:
        /**
         * @brief Get the aspect mask for a given format.
         * @param format VkFormat to get aspect mask for
         * @return VkImageAspectFlags for the format
         */
        static VkImageAspectFlags GetAspectMask(VkFormat format);

        // Static utility class - no instantiation
        MipmapGenerator() = delete;
        ~MipmapGenerator() = delete;
        MipmapGenerator(const MipmapGenerator&) = delete;
        MipmapGenerator& operator=(const MipmapGenerator&) = delete;
    };

} // namespace RHI
