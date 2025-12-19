/**
 * @file RHIImage.h
 * @brief Vulkan Image wrapper for the RHI layer.
 *
 * Manages VkImage and VkImageView for textures and render targets
 * with VMA memory allocation support.
 */

#pragma once

#include "Core/Types.h"
#include "RHI/RHIDevice.h"

#include <vulkan/vulkan.h>

namespace RHI
{
    // Forward declarations
    class RHICommandBuffer;

    /**
     * @brief Image usage types for simplified image creation
     */
    enum class ImageUsage
    {
        Texture,        ///< Sampled texture (shader read)
        RenderTarget,   ///< Color attachment for rendering
        DepthStencil,   ///< Depth/stencil attachment
        Storage,        ///< Storage image for compute
        TransferSrc,    ///< Source for transfer operations
        TransferDst     ///< Destination for transfer operations
    };

    /**
     * @brief Configuration for image creation
     */
    struct ImageDesc
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
         * @brief Depth for 3D textures (1 for 2D)
         */
        uint32_t Depth = 1;

        /**
         * @brief Number of mip levels (1 for no mipmaps)
         */
        uint32_t MipLevels = 1;

        /**
         * @brief Number of array layers (1 for non-array)
         */
        uint32_t ArrayLayers = 1;

        /**
         * @brief Image format
         */
        VkFormat Format = VK_FORMAT_R8G8B8A8_SRGB;

        /**
         * @brief Primary usage type
         */
        ImageUsage Usage = ImageUsage::Texture;

        /**
         * @brief Sample count (for multisampling)
         */
        VkSampleCountFlagBits Samples = VK_SAMPLE_COUNT_1_BIT;

        /**
         * @brief Generate mipmaps during upload
         */
        bool GenerateMipmaps = false;

        /**
         * @brief Debug name for the image
         */
        const char* DebugName = nullptr;
    };

    /**
     * @brief Vulkan Image wrapper class
     *
     * Manages VkImage lifecycle with VMA memory allocation.
     * Supports textures, render targets, and depth buffers.
     *
     * Usage:
     * @code
     * ImageDesc desc;
     * desc.Width = textureWidth;
     * desc.Height = textureHeight;
     * desc.Format = VK_FORMAT_R8G8B8A8_SRGB;
     * desc.Usage = ImageUsage::Texture;
     *
     * auto image = RHIImage::Create(device, desc);
     * image->UploadData(device, pixels, width * height * 4);
     *
     * // Bind in shader via sampler
     * @endcode
     */
    class RHIImage
    {
    public:
        /**
         * @brief Factory method to create an image
         * @param device The logical device
         * @param desc Image description
         * @return Shared pointer to the created image, or nullptr on failure
         */
        static Core::Ref<RHIImage> Create(
            const Core::Ref<RHIDevice>& device,
            const ImageDesc& desc);

        /**
         * @brief Factory method to create an image with initial data
         *
         * Creates an image and uploads data using a staging buffer.
         * Blocks until the transfer is complete.
         *
         * @param device The logical device
         * @param desc Image description
         * @param data Pointer to pixel data
         * @param dataSize Size of data in bytes
         * @return Shared pointer to the created image, or nullptr on failure
         */
        static Core::Ref<RHIImage> CreateWithData(
            const Core::Ref<RHIDevice>& device,
            const ImageDesc& desc,
            const void* data,
            VkDeviceSize dataSize);

        /**
         * @brief Destructor - destroys image and image view
         */
        ~RHIImage();

        // Non-copyable
        RHIImage(const RHIImage&) = delete;
        RHIImage& operator=(const RHIImage&) = delete;

        // Non-movable
        RHIImage(RHIImage&&) = delete;
        RHIImage& operator=(RHIImage&&) = delete;

        /**
         * @brief Upload data to the image
         *
         * Uses a staging buffer to upload data to GPU memory.
         * Transitions layout to SHADER_READ_ONLY_OPTIMAL for textures.
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
         * @brief Transition image layout
         * @param cmdBuffer Command buffer for recording
         * @param oldLayout Current layout
         * @param newLayout Target layout
         */
        void TransitionLayout(
            const Core::Ref<RHICommandBuffer>& cmdBuffer,
            VkImageLayout oldLayout,
            VkImageLayout newLayout);

        /**
         * @brief Generate mipmaps
         *
         * Generates mipmaps using linear filtering blits.
         * Image must support linear filtering.
         *
         * @param device The logical device
         * @return true on success, false on failure
         */
        bool GenerateMipmaps(const Core::Ref<RHIDevice>& device);

        /**
         * @brief Get the native VkImage handle
         * @return VkImage handle
         */
        VkImage GetHandle() const { return m_Image; }

        /**
         * @brief Get the native VkImageView handle
         * @return VkImageView handle
         */
        VkImageView GetImageView() const { return m_ImageView; }

        /**
         * @brief Get the image format
         * @return VkFormat
         */
        VkFormat GetFormat() const { return m_Format; }

        /**
         * @brief Get the image width
         * @return Width in pixels
         */
        uint32_t GetWidth() const { return m_Width; }

        /**
         * @brief Get the image height
         * @return Height in pixels
         */
        uint32_t GetHeight() const { return m_Height; }

        /**
         * @brief Get the number of mip levels
         * @return Mip level count
         */
        uint32_t GetMipLevels() const { return m_MipLevels; }

        /**
         * @brief Get the current image layout
         * @return VkImageLayout
         */
        VkImageLayout GetLayout() const { return m_CurrentLayout; }

        /**
         * @brief Set the current image layout (for external barrier recording)
         *
         * Use this to update the tracked layout after recording a pipeline
         * barrier externally. This does NOT record any Vulkan commands.
         *
         * @param layout The new layout to track
         */
        void SetLayout(VkImageLayout layout) { m_CurrentLayout = layout; }

    private:
        /**
         * @brief Private constructor - use Create() factory method
         */
        RHIImage() = default;

        /**
         * @brief Initialize the image
         * @param device The logical device
         * @param desc Image description
         * @return true on success, false on failure
         */
        bool Initialize(
            const Core::Ref<RHIDevice>& device,
            const ImageDesc& desc);

        /**
         * @brief Create the VkImage
         * @param device The logical device
         * @return true on success, false on failure
         */
        bool CreateImage(const Core::Ref<RHIDevice>& device);

        /**
         * @brief Create the VkImageView
         * @param device The logical device
         * @return true on success, false on failure
         */
        bool CreateImageView(const Core::Ref<RHIDevice>& device);

        /**
         * @brief Convert ImageUsage to VkImageUsageFlags
         * @param usage ImageUsage enum value
         * @return Corresponding VkImageUsageFlags
         */
        static VkImageUsageFlags ToVkImageUsage(ImageUsage usage);

        /**
         * @brief Get aspect mask for format
         * @param format VkFormat
         * @return VkImageAspectFlags
         */
        static VkImageAspectFlags GetAspectMask(VkFormat format);

        VmaAllocator m_Allocator = VK_NULL_HANDLE;
        VkImage m_Image = VK_NULL_HANDLE;
        VkImageView m_ImageView = VK_NULL_HANDLE;
        VmaAllocation m_Allocation = VK_NULL_HANDLE;

        VkFormat m_Format = VK_FORMAT_R8G8B8A8_SRGB;
        uint32_t m_Width = 1;
        uint32_t m_Height = 1;
        uint32_t m_Depth = 1;
        uint32_t m_MipLevels = 1;
        uint32_t m_ArrayLayers = 1;
        VkSampleCountFlagBits m_Samples = VK_SAMPLE_COUNT_1_BIT;
        ImageUsage m_Usage = ImageUsage::Texture;
        VkImageLayout m_CurrentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    };

} // namespace RHI
