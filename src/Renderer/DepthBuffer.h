/**
 * @file DepthBuffer.h
 * @brief Depth buffer management for the Renderer layer.
 *
 * Manages VkImage and VkImageView for depth testing with automatic
 * resize support to match swapchain dimensions.
 */

#pragma once

#include "Core/Types.h"

#include <vulkan/vulkan.h>

// Forward declare VMA types
struct VmaAllocation_T;
typedef VmaAllocation_T* VmaAllocation;
struct VmaAllocator_T;
typedef VmaAllocator_T* VmaAllocator;

namespace RHI
{
    class RHIDevice;
    class RHICommandBuffer;
} // namespace RHI

namespace Renderer
{
    /**
     * @brief Configuration for depth buffer creation
     */
    struct DepthBufferDesc
    {
        /**
         * @brief Width in pixels
         */
        uint32_t Width = 0;

        /**
         * @brief Height in pixels
         */
        uint32_t Height = 0;

        /**
         * @brief Depth format (default D32_SFLOAT for highest precision)
         */
        VkFormat Format = VK_FORMAT_D32_SFLOAT;

        /**
         * @brief Debug name for the depth buffer
         */
        const char* DebugName = "DepthBuffer";
    };

    /**
     * @brief Depth buffer wrapper class
     *
     * Manages the depth buffer lifecycle including:
     * - VkImage creation with optimal tiling
     * - VkImageView for depth attachment binding
     * - Automatic memory allocation via VMA
     * - Resize support for window resize handling
     *
     * Usage:
     * @code
     * DepthBufferDesc desc;
     * desc.Width = swapchain->GetExtent().width;
     * desc.Height = swapchain->GetExtent().height;
     *
     * auto depthBuffer = DepthBuffer::Create(device, desc);
     * if (depthBuffer) {
     *     // Use in rendering
     *     renderConfig.Depth.ImageView = depthBuffer->GetImageView();
     *
     *     // On window resize
     *     depthBuffer->Resize(device, newWidth, newHeight);
     * }
     * @endcode
     */
    class DepthBuffer
    {
    public:
        /**
         * @brief Factory method to create a depth buffer
         * @param device The logical device
         * @param desc Depth buffer description
         * @return Shared pointer to the created depth buffer, or nullptr on failure
         */
        static Core::Ref<DepthBuffer> Create(
            const Core::Ref<RHI::RHIDevice>& device,
            const DepthBufferDesc& desc);

        /**
         * @brief Destructor - destroys image and image view
         */
        ~DepthBuffer();

        // Non-copyable
        DepthBuffer(const DepthBuffer&) = delete;
        DepthBuffer& operator=(const DepthBuffer&) = delete;

        // Non-movable
        DepthBuffer(DepthBuffer&&) = delete;
        DepthBuffer& operator=(DepthBuffer&&) = delete;

        /**
         * @brief Resize the depth buffer
         *
         * Destroys existing resources and creates new ones with updated dimensions.
         * Call this when the swapchain is resized.
         *
         * @param device The logical device
         * @param width New width in pixels
         * @param height New height in pixels
         * @return true on success, false on failure
         */
        bool Resize(
            const Core::Ref<RHI::RHIDevice>& device,
            uint32_t width,
            uint32_t height);

        /**
         * @brief Transition image layout
         *
         * Transitions the depth image between layouts. Used for initial
         * transition to DEPTH_STENCIL_ATTACHMENT_OPTIMAL.
         *
         * @param cmdBuffer Command buffer for recording the transition
         * @param oldLayout Current layout
         * @param newLayout Target layout
         */
        void TransitionLayout(
            const Core::Ref<RHI::RHICommandBuffer>& cmdBuffer,
            VkImageLayout oldLayout,
            VkImageLayout newLayout);

        /**
         * @brief Get the native VkImage handle
         * @return VkImage handle
         */
        VkImage GetImage() const { return m_Image; }

        /**
         * @brief Get the native VkImageView handle
         * @return VkImageView handle
         */
        VkImageView GetImageView() const { return m_ImageView; }

        /**
         * @brief Get the depth format
         * @return VkFormat of the depth buffer
         */
        VkFormat GetFormat() const { return m_Format; }

        /**
         * @brief Get the depth buffer width
         * @return Width in pixels
         */
        uint32_t GetWidth() const { return m_Width; }

        /**
         * @brief Get the depth buffer height
         * @return Height in pixels
         */
        uint32_t GetHeight() const { return m_Height; }

        /**
         * @brief Check if the format has a stencil component
         * @return true if the format includes stencil
         */
        bool HasStencil() const
        {
            return m_Format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
                   m_Format == VK_FORMAT_D24_UNORM_S8_UINT ||
                   m_Format == VK_FORMAT_D16_UNORM_S8_UINT;
        }

    private:
        /**
         * @brief Private constructor - use Create() factory method
         */
        DepthBuffer() = default;

        /**
         * @brief Initialize the depth buffer
         * @param device The logical device
         * @param desc Depth buffer description
         * @return true on success, false on failure
         */
        bool Initialize(
            const Core::Ref<RHI::RHIDevice>& device,
            const DepthBufferDesc& desc);

        /**
         * @brief Create the depth image and allocate memory
         * @param device The logical device
         * @return true on success, false on failure
         */
        bool CreateImage(const Core::Ref<RHI::RHIDevice>& device);

        /**
         * @brief Create the depth image view
         * @param device The logical device
         * @return true on success, false on failure
         */
        bool CreateImageView(const Core::Ref<RHI::RHIDevice>& device);

        /**
         * @brief Destroy all resources
         * @param device The logical device
         */
        void Destroy(const Core::Ref<RHI::RHIDevice>& device);

        VmaAllocator m_Allocator = nullptr;
        VkImage m_Image = VK_NULL_HANDLE;
        VkImageView m_ImageView = VK_NULL_HANDLE;
        VmaAllocation m_Allocation = nullptr;
        VkFormat m_Format = VK_FORMAT_D32_SFLOAT;
        uint32_t m_Width = 0;
        uint32_t m_Height = 0;
    };

} // namespace Renderer
