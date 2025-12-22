/**
 * @file ShadowMap.h
 * @brief Shadow map management for shadow rendering.
 *
 * Provides a depth-only render target for shadow mapping with configurable
 * resolution and light matrix management.
 */

#pragma once

#include "Core/Types.h"

#include <glm/glm.hpp>
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
    class RHISampler;
} // namespace RHI

namespace Renderer
{
    /**
     * @brief Configuration for shadow map creation
     */
    struct ShadowMapDesc
    {
        /**
         * @brief Shadow map resolution (width and height are equal)
         *
         * Common values: 512 (low), 1024 (medium), 2048 (high), 4096 (ultra)
         */
        uint32_t Resolution = 2048;

        /**
         * @brief Depth format (default D32_SFLOAT for highest precision)
         */
        VkFormat Format = VK_FORMAT_D32_SFLOAT;

        /**
         * @brief Debug name for the shadow map
         */
        const char* DebugName = "ShadowMap";
    };

    /**
     * @brief Shadow map class for depth-only shadow rendering
     *
     * Manages a depth texture used for shadow mapping with:
     * - VkImage with depth format and sampled usage
     * - VkImageView for rendering and sampling
     * - Shadow sampler with PCF comparison
     * - Light view and projection matrices
     *
     * Usage:
     * @code
     * // Create shadow map
     * ShadowMapDesc desc;
     * desc.Resolution = 2048;
     * auto shadowMap = ShadowMap::Create(device, desc);
     *
     * // Set light matrices
     * shadowMap->SetLightMatrices(lightView, lightProjection);
     *
     * // Shadow pass rendering
     * shadowMap->Begin(cmdBuffer);
     * // ... draw scene from light's perspective ...
     * shadowMap->End(cmdBuffer);
     *
     * // Use in main pass
     * VkDescriptorImageInfo shadowInfo{};
     * shadowInfo.sampler = shadowMap->GetSampler()->GetHandle();
     * shadowInfo.imageView = shadowMap->GetImageView();
     * shadowInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
     * @endcode
     */
    class ShadowMap
    {
    public:
        /**
         * @brief Factory method to create a shadow map
         * @param device The logical device
         * @param desc Shadow map description
         * @return Shared pointer to the created shadow map, or nullptr on failure
         */
        static Core::Ref<ShadowMap> Create(
            const Core::Ref<RHI::RHIDevice>& device,
            const ShadowMapDesc& desc = ShadowMapDesc{});

        /**
         * @brief Destructor - destroys image, image view, and sampler
         */
        ~ShadowMap();

        // Non-copyable
        ShadowMap(const ShadowMap&) = delete;
        ShadowMap& operator=(const ShadowMap&) = delete;

        // Non-movable
        ShadowMap(ShadowMap&&) = delete;
        ShadowMap& operator=(ShadowMap&&) = delete;

        // ============================================================
        // Rendering Control
        // ============================================================

        /**
         * @brief Begin shadow map rendering pass
         *
         * Transitions the depth image to attachment optimal layout and begins
         * dynamic rendering with depth-only configuration. Sets viewport and
         * scissor to match shadow map resolution.
         *
         * @param cmdBuffer Command buffer for recording commands
         */
        void Begin(const Core::Ref<RHI::RHICommandBuffer>& cmdBuffer);

        /**
         * @brief End shadow map rendering pass
         *
         * Ends the rendering pass and transitions the depth image to
         * shader read-only optimal layout for sampling in the main pass.
         *
         * @param cmdBuffer Command buffer for recording commands
         */
        void End(const Core::Ref<RHI::RHICommandBuffer>& cmdBuffer);

        // ============================================================
        // Light Matrix Management
        // ============================================================

        /**
         * @brief Set the light view and projection matrices
         * @param view Light view matrix (world to light space)
         * @param projection Light projection matrix (orthographic for directional)
         */
        void SetLightMatrices(const glm::mat4& view, const glm::mat4& projection);

        /**
         * @brief Get the combined light-space matrix
         *
         * Returns projection * view matrix for transforming world positions
         * to light clip space (used in shadow pass vertex shader).
         *
         * @return Combined light-space matrix
         */
        glm::mat4 GetLightSpaceMatrix() const;

        /**
         * @brief Get the light view matrix
         * @return Light view matrix
         */
        const glm::mat4& GetLightView() const { return m_LightView; }

        /**
         * @brief Get the light projection matrix
         * @return Light projection matrix
         */
        const glm::mat4& GetLightProjection() const { return m_LightProjection; }

        // ============================================================
        // Accessors
        // ============================================================

        /**
         * @brief Get the native VkImage handle
         * @return VkImage handle
         */
        VkImage GetImage() const { return m_Image; }

        /**
         * @brief Get the native VkImageView handle
         * @return VkImageView handle for binding to descriptors
         */
        VkImageView GetImageView() const { return m_ImageView; }

        /**
         * @brief Get the shadow sampler
         *
         * Returns a sampler configured for shadow mapping with:
         * - Linear filtering for PCF soft shadows
         * - Clamp-to-border addressing with white border
         * - Depth comparison enabled (LESS_OR_EQUAL)
         *
         * @return Shared pointer to the shadow sampler
         */
        Core::Ref<RHI::RHISampler> GetSampler() const { return m_Sampler; }

        /**
         * @brief Get the shadow map resolution
         * @return Resolution in pixels (width = height)
         */
        uint32_t GetResolution() const { return m_Resolution; }

        /**
         * @brief Get the depth format
         * @return VkFormat of the depth texture
         */
        VkFormat GetFormat() const { return m_Format; }

        /**
         * @brief Get the current image layout
         * @return Current VkImageLayout
         */
        VkImageLayout GetCurrentLayout() const { return m_CurrentLayout; }

    private:
        /**
         * @brief Private constructor - use Create() factory method
         */
        ShadowMap() = default;

        /**
         * @brief Initialize the shadow map
         * @param device The logical device
         * @param desc Shadow map description
         * @return true on success, false on failure
         */
        bool Initialize(
            const Core::Ref<RHI::RHIDevice>& device,
            const ShadowMapDesc& desc);

        /**
         * @brief Create the depth image with sampled usage
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
         * @brief Create the shadow sampler
         * @param device The logical device
         * @return true on success, false on failure
         */
        bool CreateSampler(const Core::Ref<RHI::RHIDevice>& device);

        /**
         * @brief Transition image layout
         * @param cmdBuffer Command buffer for recording the transition
         * @param oldLayout Current layout
         * @param newLayout Target layout
         */
        void TransitionLayout(
            const Core::Ref<RHI::RHICommandBuffer>& cmdBuffer,
            VkImageLayout oldLayout,
            VkImageLayout newLayout);

        // Vulkan resources
        VmaAllocator m_Allocator = nullptr;
        VkImage m_Image = VK_NULL_HANDLE;
        VkImageView m_ImageView = VK_NULL_HANDLE;
        VmaAllocation m_Allocation = nullptr;
        Core::Ref<RHI::RHISampler> m_Sampler;
        VkImageLayout m_CurrentLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        // Configuration
        uint32_t m_Resolution = 2048;
        VkFormat m_Format = VK_FORMAT_D32_SFLOAT;

        // Light matrices
        glm::mat4 m_LightView = glm::mat4(1.0f);
        glm::mat4 m_LightProjection = glm::mat4(1.0f);
    };

} // namespace Renderer
