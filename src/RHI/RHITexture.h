/**
 * @file RHITexture.h
 * @brief High-level texture wrapper for the RHI layer.
 *
 * Provides a texture-focused API for loading and managing GPU textures.
 * Uses RHIImage internally for low-level Vulkan operations.
 */

#pragma once

#include "Core/Types.h"
#include "RHI/RHIDevice.h"
#include "RHI/RHIImage.h"

#include <vulkan/vulkan.h>
#include <string>

namespace RHI
{
    // Forward declarations
    class RHICommandBuffer;

    /**
     * @brief Configuration for texture creation
     *
     * Provides texture-specific settings. For loading from files,
     * use RHITexture::LoadFromFile() instead which auto-configures
     * dimensions from the image file.
     */
    struct TextureDesc
    {
        /**
         * @brief Width in pixels
         */
        uint32_t Width = 1;

        /**
         * @brief Height in pixels
         */
        uint32_t Height = 1;

        /**
         * @brief Number of mip levels (1 = no mipmaps)
         *
         * Set to 0 for auto-calculation based on dimensions.
         */
        uint32_t MipLevels = 1;

        /**
         * @brief Texture format
         */
        VkFormat Format = VK_FORMAT_R8G8B8A8_SRGB;

        /**
         * @brief Image usage flags
         *
         * Default includes sampling and transfer destination for uploads.
         */
        VkImageUsageFlags Usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        /**
         * @brief Whether to generate mipmaps during upload
         */
        bool GenerateMips = false;

        /**
         * @brief Debug name for the texture
         */
        const char* DebugName = nullptr;
    };

    /**
     * @brief High-level texture wrapper class
     *
     * Provides a user-friendly interface for texture operations including
     * file loading with stb_image, automatic mipmap generation, and
     * layout transitions.
     *
     * Usage (from file):
     * @code
     * auto texture = RHITexture::LoadFromFile(device, "textures/albedo.png");
     * if (texture) {
     *     // Ready for shader sampling
     *     VkImageView view = texture->GetImageView();
     * }
     * @endcode
     *
     * Usage (programmatic):
     * @code
     * TextureDesc desc;
     * desc.Width = 256;
     * desc.Height = 256;
     * desc.Format = VK_FORMAT_R8G8B8A8_SRGB;
     *
     * auto texture = RHITexture::Create(device, desc);
     * texture->UploadData(device, pixels, pixelSize);
     * @endcode
     */
    class RHITexture
    {
    public:
        /**
         * @brief Factory method to create an empty texture
         *
         * Creates a texture with the specified dimensions and format.
         * Use UploadData() to fill with pixel data.
         *
         * @param device The logical device
         * @param desc Texture description
         * @return Shared pointer to the created texture, or nullptr on failure
         */
        static Core::Ref<RHITexture> Create(
            const Core::Ref<RHIDevice>& device,
            const TextureDesc& desc);

        /**
         * @brief Factory method to load a texture from file
         *
         * Loads an image file using stb_image and creates a GPU texture.
         * Supports PNG, JPG, BMP, TGA, HDR, and other common formats.
         *
         * @param device The logical device
         * @param filepath Path to the image file
         * @param sRGB Use sRGB format (true for color textures, false for data textures like normal maps)
         * @param generateMips Generate mipmaps automatically
         * @return Shared pointer to the loaded texture, or nullptr on failure
         */
        static Core::Ref<RHITexture> LoadFromFile(
            const Core::Ref<RHIDevice>& device,
            const std::string& filepath,
            bool sRGB = true,
            bool generateMips = true);

        /**
         * @brief Destructor
         */
        ~RHITexture();

        // Non-copyable
        RHITexture(const RHITexture&) = delete;
        RHITexture& operator=(const RHITexture&) = delete;

        // Non-movable
        RHITexture(RHITexture&&) = delete;
        RHITexture& operator=(RHITexture&&) = delete;

        /**
         * @brief Upload pixel data to the texture
         *
         * Uses a staging buffer to upload data to GPU memory.
         * Automatically transitions layout to SHADER_READ_ONLY_OPTIMAL.
         *
         * @param device The logical device
         * @param data Pointer to pixel data
         * @param dataSize Size of data in bytes
         * @return true on success, false on failure
         */
        bool UploadData(
            const Core::Ref<RHIDevice>& device,
            const void* data,
            VkDeviceSize dataSize);

        /**
         * @brief Transition image layout with explicit pipeline stages
         *
         * Records a pipeline barrier to transition the image layout.
         * Use this for fine-grained control over synchronization.
         *
         * @param cmd Command buffer handle for recording
         * @param oldLayout Current layout (use VK_IMAGE_LAYOUT_UNDEFINED if unknown)
         * @param newLayout Target layout
         * @param srcStage Source pipeline stage mask
         * @param dstStage Destination pipeline stage mask
         */
        void TransitionLayout(
            VkCommandBuffer cmd,
            VkImageLayout oldLayout,
            VkImageLayout newLayout,
            VkPipelineStageFlags srcStage,
            VkPipelineStageFlags dstStage);

        /**
         * @brief Generate mipmaps for the texture
         *
         * Generates mipmap chain using linear filtering.
         * The texture format must support linear filtering.
         *
         * @param device The logical device
         * @return true on success, false on failure
         */
        bool GenerateMipmaps(const Core::Ref<RHIDevice>& device);

        /**
         * @brief Get the native VkImage handle
         * @return VkImage handle
         */
        VkImage GetImage() const;

        /**
         * @brief Get the native VkImageView handle
         * @return VkImageView handle
         */
        VkImageView GetImageView() const;

        /**
         * @brief Get the texture format
         * @return VkFormat
         */
        VkFormat GetFormat() const;

        /**
         * @brief Get the texture width
         * @return Width in pixels
         */
        uint32_t GetWidth() const;

        /**
         * @brief Get the texture height
         * @return Height in pixels
         */
        uint32_t GetHeight() const;

        /**
         * @brief Get the number of mip levels
         * @return Mip level count
         */
        uint32_t GetMipLevels() const;

        /**
         * @brief Get the current image layout
         * @return VkImageLayout
         */
        VkImageLayout GetLayout() const;

        /**
         * @brief Get the underlying RHIImage
         *
         * Provides access to the low-level image for advanced operations.
         *
         * @return Shared pointer to the underlying RHIImage
         */
        const Core::Ref<RHIImage>& GetRHIImage() const { return m_Image; }

        /**
         * @brief Get the file path (if loaded from file)
         * @return File path, or empty string if created programmatically
         */
        const std::string& GetFilePath() const { return m_FilePath; }

    private:
        /**
         * @brief Private constructor - use factory methods
         */
        RHITexture() = default;

        /**
         * @brief Initialize from description
         * @param device The logical device
         * @param desc Texture description
         * @return true on success, false on failure
         */
        bool Initialize(
            const Core::Ref<RHIDevice>& device,
            const TextureDesc& desc);

        /**
         * @brief Get access mask for layout transition
         * @param layout Image layout
         * @return Access mask appropriate for the layout
         */
        static VkAccessFlags GetAccessMask(VkImageLayout layout);

        Core::Ref<RHIImage> m_Image;
        std::string m_FilePath;
        TextureDesc m_Desc;
    };

} // namespace RHI
