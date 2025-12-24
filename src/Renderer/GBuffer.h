/**
 * @file GBuffer.h
 * @brief G-Buffer management for deferred rendering.
 *
 * Manages multiple render targets for deferred shading including albedo,
 * normals, material properties, emissive, and depth.
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
    class RHIDeletionQueue;
    class RHISampler;
} // namespace RHI

namespace Renderer
{
    /**
     * @brief Configuration for G-Buffer creation
     */
    struct GBufferDesc
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
         * @brief Format for albedo render target (RT0)
         *
         * RGB = Albedo, A = unused
         */
        VkFormat AlbedoFormat = VK_FORMAT_R8G8B8A8_SRGB;

        /**
         * @brief Format for normal render target (RT1)
         *
         * RG = Normal (Octahedral encoded), BA = unused
         */
        VkFormat NormalFormat = VK_FORMAT_R16G16_SFLOAT;

        /**
         * @brief Format for material render target (RT2)
         *
         * R = Metallic, G = Roughness, B = AO, A = unused
         */
        VkFormat MaterialFormat = VK_FORMAT_R8G8B8A8_UNORM;

        /**
         * @brief Format for emissive render target (RT3)
         *
         * RGB = Emissive, A = unused
         */
        VkFormat EmissiveFormat = VK_FORMAT_R16G16B16A16_SFLOAT;

        /**
         * @brief Format for depth buffer
         */
        VkFormat DepthFormat = VK_FORMAT_D32_SFLOAT;

        /**
         * @brief Debug name for the G-Buffer
         */
        const char* DebugName = "GBuffer";
    };

    /**
     * @brief G-Buffer for deferred rendering
     *
     * Manages multiple render targets for deferred shading:
     * - RT0 (Albedo): R8G8B8A8_SRGB - Base color
     * - RT1 (Normal): R16G16_SFLOAT - Octahedral encoded normals
     * - RT2 (Material): R8G8B8A8_UNORM - Metallic/Roughness/AO
     * - RT3 (Emissive): R16G16B16A16_SFLOAT - Emissive color
     * - Depth: D32_SFLOAT - Depth buffer
     *
     * All color targets can be simultaneously written using MRT (Multiple Render Targets)
     * and sampled in the deferred lighting pass.
     *
     * Usage:
     * @code
     * // Create G-Buffer
     * GBufferDesc desc;
     * desc.Width = swapchain->GetExtent().width;
     * desc.Height = swapchain->GetExtent().height;
     * auto gBuffer = GBuffer::Create(device, deletionQueue, desc);
     *
     * // Geometry pass
     * gBuffer->Begin(cmdBuffer);
     * // ... draw scene geometry ...
     * gBuffer->End(cmdBuffer);
     *
     * // Lighting pass - sample G-Buffer textures
     * VkDescriptorImageInfo albedoInfo{};
     * albedoInfo.sampler = sampler->GetHandle();
     * albedoInfo.imageView = gBuffer->GetAlbedoView();
     * albedoInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
     * @endcode
     */
    class GBuffer
    {
    public:
        /**
         * @brief Number of color attachments in the G-Buffer
         */
        static constexpr uint32_t COLOR_ATTACHMENT_COUNT = 4;

        /**
         * @brief Factory method to create a G-Buffer
         * @param device The logical device
         * @param deletionQueue Deletion queue for deferred resource cleanup
         * @param desc G-Buffer description
         * @return Shared pointer to the created G-Buffer, or nullptr on failure
         */
        static Core::Ref<GBuffer> Create(
            const Core::Ref<RHI::RHIDevice>& device,
            const Core::Ref<RHI::RHIDeletionQueue>& deletionQueue,
            const GBufferDesc& desc);

        /**
         * @brief Destructor - queues resources for deferred deletion
         */
        ~GBuffer();

        // Non-copyable
        GBuffer(const GBuffer&) = delete;
        GBuffer& operator=(const GBuffer&) = delete;

        // Non-movable
        GBuffer(GBuffer&&) = delete;
        GBuffer& operator=(GBuffer&&) = delete;

        // ============================================================
        // Rendering Control
        // ============================================================

        /**
         * @brief Begin G-Buffer geometry pass
         *
         * Transitions all G-Buffer targets to attachment optimal layout and
         * begins dynamic rendering with MRT configuration. Sets viewport and
         * scissor to match G-Buffer dimensions.
         *
         * @param cmdBuffer Command buffer for recording commands
         */
        void Begin(const Core::Ref<RHI::RHICommandBuffer>& cmdBuffer);

        /**
         * @brief End G-Buffer geometry pass
         *
         * Ends the rendering pass and transitions all G-Buffer targets to
         * shader read-only optimal layout for sampling in the lighting pass.
         *
         * @param cmdBuffer Command buffer for recording commands
         */
        void End(const Core::Ref<RHI::RHICommandBuffer>& cmdBuffer);

        // ============================================================
        // Resource Management
        // ============================================================

        /**
         * @brief Resize the G-Buffer
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

        // ============================================================
        // Render Target Accessors
        // ============================================================

        /**
         * @brief Get the albedo (RT0) image
         * @return VkImage handle
         */
        VkImage GetAlbedoImage() const { return m_AlbedoImage; }

        /**
         * @brief Get the albedo (RT0) image view
         * @return VkImageView handle for binding to descriptors
         */
        VkImageView GetAlbedoView() const { return m_AlbedoView; }

        /**
         * @brief Get the normal (RT1) image
         * @return VkImage handle
         */
        VkImage GetNormalImage() const { return m_NormalImage; }

        /**
         * @brief Get the normal (RT1) image view
         * @return VkImageView handle for binding to descriptors
         */
        VkImageView GetNormalView() const { return m_NormalView; }

        /**
         * @brief Get the material (RT2) image
         * @return VkImage handle
         */
        VkImage GetMaterialImage() const { return m_MaterialImage; }

        /**
         * @brief Get the material (RT2) image view
         * @return VkImageView handle for binding to descriptors
         */
        VkImageView GetMaterialView() const { return m_MaterialView; }

        /**
         * @brief Get the emissive (RT3) image
         * @return VkImage handle
         */
        VkImage GetEmissiveImage() const { return m_EmissiveImage; }

        /**
         * @brief Get the emissive (RT3) image view
         * @return VkImageView handle for binding to descriptors
         */
        VkImageView GetEmissiveView() const { return m_EmissiveView; }

        /**
         * @brief Get the depth image
         * @return VkImage handle
         */
        VkImage GetDepthImage() const { return m_DepthImage; }

        /**
         * @brief Get the depth image view
         * @return VkImageView handle for binding to descriptors
         */
        VkImageView GetDepthView() const { return m_DepthView; }

        /**
         * @brief Get all color image views as a vector
         *
         * Returns views in order: [Albedo, Normal, Material, Emissive]
         *
         * @return Vector of VkImageView handles
         */
        std::vector<VkImageView> GetColorViews() const;

        /**
         * @brief Get all color formats as a vector
         *
         * Returns formats in order: [Albedo, Normal, Material, Emissive]
         *
         * @return Vector of VkFormat values
         */
        std::vector<VkFormat> GetColorFormats() const;

        // ============================================================
        // Property Accessors
        // ============================================================

        /**
         * @brief Get the G-Buffer width
         * @return Width in pixels
         */
        uint32_t GetWidth() const { return m_Width; }

        /**
         * @brief Get the G-Buffer height
         * @return Height in pixels
         */
        uint32_t GetHeight() const { return m_Height; }

        /**
         * @brief Get the albedo format
         * @return VkFormat of the albedo render target
         */
        VkFormat GetAlbedoFormat() const { return m_AlbedoFormat; }

        /**
         * @brief Get the normal format
         * @return VkFormat of the normal render target
         */
        VkFormat GetNormalFormat() const { return m_NormalFormat; }

        /**
         * @brief Get the material format
         * @return VkFormat of the material render target
         */
        VkFormat GetMaterialFormat() const { return m_MaterialFormat; }

        /**
         * @brief Get the emissive format
         * @return VkFormat of the emissive render target
         */
        VkFormat GetEmissiveFormat() const { return m_EmissiveFormat; }

        /**
         * @brief Get the depth format
         * @return VkFormat of the depth buffer
         */
        VkFormat GetDepthFormat() const { return m_DepthFormat; }

    private:
        /**
         * @brief Private constructor - use Create() factory method
         */
        GBuffer() = default;

        /**
         * @brief Initialize the G-Buffer
         * @param device The logical device
         * @param deletionQueue Deletion queue for deferred resource cleanup
         * @param desc G-Buffer description
         * @return true on success, false on failure
         */
        bool Initialize(
            const Core::Ref<RHI::RHIDevice>& device,
            const Core::Ref<RHI::RHIDeletionQueue>& deletionQueue,
            const GBufferDesc& desc);

        /**
         * @brief Create all render target images
         * @param device The logical device
         * @return true on success, false on failure
         */
        bool CreateImages(const Core::Ref<RHI::RHIDevice>& device);

        /**
         * @brief Create all render target image views
         * @param device The logical device
         * @return true on success, false on failure
         */
        bool CreateImageViews(const Core::Ref<RHI::RHIDevice>& device);

        /**
         * @brief Create a single color render target image
         * @param device The logical device
         * @param format Image format
         * @param outImage Output image handle
         * @param outAllocation Output allocation handle
         * @return true on success, false on failure
         */
        bool CreateColorImage(
            const Core::Ref<RHI::RHIDevice>& device,
            VkFormat format,
            VkImage& outImage,
            VmaAllocation& outAllocation);

        /**
         * @brief Create a single color render target image view
         * @param device The logical device
         * @param image Image to create view for
         * @param format Image format
         * @param outView Output image view handle
         * @return true on success, false on failure
         */
        bool CreateColorImageView(
            const Core::Ref<RHI::RHIDevice>& device,
            VkImage image,
            VkFormat format,
            VkImageView& outView);

        /**
         * @brief Create the depth image
         * @param device The logical device
         * @return true on success, false on failure
         */
        bool CreateDepthImage(const Core::Ref<RHI::RHIDevice>& device);

        /**
         * @brief Create the depth image view
         * @param device The logical device
         * @return true on success, false on failure
         */
        bool CreateDepthImageView(const Core::Ref<RHI::RHIDevice>& device);

        /**
         * @brief Destroy all resources
         * @param device The logical device
         */
        void Destroy(const Core::Ref<RHI::RHIDevice>& device);

        /**
         * @brief Transition a color image layout
         * @param cmdBuffer Command buffer for recording the transition
         * @param image Image to transition
         * @param oldLayout Current layout
         * @param newLayout Target layout
         */
        void TransitionColorLayout(
            const Core::Ref<RHI::RHICommandBuffer>& cmdBuffer,
            VkImage image,
            VkImageLayout oldLayout,
            VkImageLayout newLayout);

        /**
         * @brief Transition the depth image layout
         * @param cmdBuffer Command buffer for recording the transition
         * @param oldLayout Current layout
         * @param newLayout Target layout
         */
        void TransitionDepthLayout(
            const Core::Ref<RHI::RHICommandBuffer>& cmdBuffer,
            VkImageLayout oldLayout,
            VkImageLayout newLayout);

        // Vulkan device and allocator handles
        VkDevice m_Device = VK_NULL_HANDLE;
        VmaAllocator m_Allocator = nullptr;
        Core::Ref<RHI::RHIDeletionQueue> m_DeletionQueue;

        // Albedo (RT0) - RGB = Albedo, A = unused
        VkImage m_AlbedoImage = VK_NULL_HANDLE;
        VkImageView m_AlbedoView = VK_NULL_HANDLE;
        VmaAllocation m_AlbedoAllocation = nullptr;

        // Normal (RT1) - RG = Normal (Octahedral), BA = unused
        VkImage m_NormalImage = VK_NULL_HANDLE;
        VkImageView m_NormalView = VK_NULL_HANDLE;
        VmaAllocation m_NormalAllocation = nullptr;

        // Material (RT2) - R = Metallic, G = Roughness, B = AO, A = unused
        VkImage m_MaterialImage = VK_NULL_HANDLE;
        VkImageView m_MaterialView = VK_NULL_HANDLE;
        VmaAllocation m_MaterialAllocation = nullptr;

        // Emissive (RT3) - RGB = Emissive, A = unused
        VkImage m_EmissiveImage = VK_NULL_HANDLE;
        VkImageView m_EmissiveView = VK_NULL_HANDLE;
        VmaAllocation m_EmissiveAllocation = nullptr;

        // Depth buffer
        VkImage m_DepthImage = VK_NULL_HANDLE;
        VkImageView m_DepthView = VK_NULL_HANDLE;
        VmaAllocation m_DepthAllocation = nullptr;

        // Current layouts for each target
        VkImageLayout m_AlbedoLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkImageLayout m_NormalLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkImageLayout m_MaterialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkImageLayout m_EmissiveLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkImageLayout m_DepthLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        // Configuration
        uint32_t m_Width = 0;
        uint32_t m_Height = 0;
        VkFormat m_AlbedoFormat = VK_FORMAT_R8G8B8A8_SRGB;
        VkFormat m_NormalFormat = VK_FORMAT_R16G16_SFLOAT;
        VkFormat m_MaterialFormat = VK_FORMAT_R8G8B8A8_UNORM;
        VkFormat m_EmissiveFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
        VkFormat m_DepthFormat = VK_FORMAT_D32_SFLOAT;
    };

} // namespace Renderer
