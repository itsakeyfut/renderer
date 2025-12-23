/**
 * @file CascadedShadowMap.h
 * @brief Cascaded Shadow Map (CSM) management for large scene shadow rendering.
 *
 * Implements Cascaded Shadow Maps to provide high-quality shadows across large
 * scenes by splitting the camera frustum into multiple depth ranges, each with
 * its own shadow map cascade.
 *
 * Reference:
 *   - "Parallel-Split Shadow Maps on Programmable GPUs" (Zhang et al.)
 *   - UE4: FShadowMapRenderTargets for CSM management
 */

#pragma once

#include "Core/Types.h"

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <array>

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

namespace Scene
{
    class Camera;
} // namespace Scene

namespace Renderer
{
    /**
     * @brief Number of shadow map cascades
     *
     * 4 cascades is the standard choice, balancing quality and performance.
     * Near cascades have higher resolution for detailed shadows, while far
     * cascades cover larger areas with lower effective resolution.
     */
    static constexpr uint32_t CASCADE_COUNT = 4;

    /**
     * @brief Configuration for cascaded shadow map creation
     */
    struct CascadedShadowMapDesc
    {
        /**
         * @brief Shadow map resolution per cascade (width and height are equal)
         *
         * Common values: 1024 (low), 2048 (high), 4096 (ultra)
         */
        uint32_t Resolution = 2048;

        /**
         * @brief Depth format (default D32_SFLOAT for highest precision)
         */
        VkFormat Format = VK_FORMAT_D32_SFLOAT;

        /**
         * @brief Lambda for logarithmic/uniform split blend
         *
         * 0.0 = purely uniform split
         * 1.0 = purely logarithmic split
         * 0.5 = balanced (commonly used)
         */
        float SplitLambda = 0.5f;

        /**
         * @brief Debug name for the shadow map array
         */
        const char* DebugName = "CascadedShadowMap";
    };

    /**
     * @brief Data for a single cascade
     *
     * GPU-compatible structure for shader uniform buffer usage.
     */
    struct CascadeData
    {
        /**
         * @brief Light-space view-projection matrix for this cascade
         */
        glm::mat4 ViewProjection;

        /**
         * @brief Split depth in view space (end of this cascade's range)
         */
        float SplitDepth;

        /**
         * @brief Padding for std140 alignment
         */
        float Padding[3];
    };

    static_assert(sizeof(CascadeData) == 80, "CascadeData size must be 80 bytes for std140 layout");

    /**
     * @brief GPU uniform buffer for CSM data
     *
     * Contains all cascade data for shader access.
     */
    struct CascadedShadowMapUBO
    {
        /**
         * @brief Data for each cascade
         */
        CascadeData Cascades[CASCADE_COUNT];

        /**
         * @brief Shadow bias for shadow acne prevention
         */
        float ShadowBias;

        /**
         * @brief Normal-based bias offset
         */
        float NormalBias;

        /**
         * @brief Shadow map resolution (width = height)
         */
        float ShadowMapSize;

        /**
         * @brief Padding for alignment
         */
        float Padding;
    };

    static_assert(sizeof(CascadedShadowMapUBO) == 336, "CascadedShadowMapUBO size must be 336 bytes");

    /**
     * @brief Cascaded Shadow Map class for high-quality shadow rendering
     *
     * Manages multiple shadow map cascades for rendering shadows across large
     * scenes. Each cascade covers a portion of the camera frustum:
     *   - Cascade 0: Near range (highest detail)
     *   - Cascade 1: Near-mid range
     *   - Cascade 2: Mid-far range
     *   - Cascade 3: Far range (largest coverage)
     *
     * Usage:
     * @code
     * // Create CSM
     * CascadedShadowMapDesc desc;
     * desc.Resolution = 2048;
     * auto csm = CascadedShadowMap::Create(device, deletionQueue, desc);
     *
     * // Update cascade matrices each frame
     * csm->Update(camera, lightDirection);
     *
     * // Render shadow pass for each cascade
     * for (uint32_t i = 0; i < CASCADE_COUNT; ++i) {
     *     csm->BeginCascade(cmdBuffer, i);
     *     // ... draw scene from light's perspective ...
     *     csm->EndCascade(cmdBuffer, i);
     * }
     *
     * // Transition to shader read for main pass
     * csm->TransitionToShaderRead(cmdBuffer);
     * @endcode
     */
    class CascadedShadowMap
    {
    public:
        /**
         * @brief Factory method to create a cascaded shadow map
         * @param device The logical device
         * @param deletionQueue Deletion queue for deferred resource cleanup
         * @param desc Cascaded shadow map description
         * @return Shared pointer to the created CSM, or nullptr on failure
         */
        static Core::Ref<CascadedShadowMap> Create(
            const Core::Ref<RHI::RHIDevice>& device,
            const Core::Ref<RHI::RHIDeletionQueue>& deletionQueue,
            const CascadedShadowMapDesc& desc = CascadedShadowMapDesc{});

        /**
         * @brief Destructor - queues resources for deferred deletion
         */
        ~CascadedShadowMap();

        // Non-copyable, non-movable
        CascadedShadowMap(const CascadedShadowMap&) = delete;
        CascadedShadowMap& operator=(const CascadedShadowMap&) = delete;
        CascadedShadowMap(CascadedShadowMap&&) = delete;
        CascadedShadowMap& operator=(CascadedShadowMap&&) = delete;

        // ============================================================
        // Update
        // ============================================================

        /**
         * @brief Update cascade split depths and light matrices
         *
         * Call this once per frame before rendering shadow passes.
         * Recalculates frustum splits and light-space matrices based on
         * the current camera and light direction.
         *
         * @param camera The main camera for frustum calculation
         * @param lightDirection Directional light direction (normalized)
         */
        void Update(const Scene::Camera& camera, const glm::vec3& lightDirection);

        // ============================================================
        // Rendering Control
        // ============================================================

        /**
         * @brief Begin rendering to a specific cascade
         *
         * Transitions the cascade layer to attachment optimal layout,
         * sets up viewport/scissor, and begins dynamic rendering.
         *
         * @param cmdBuffer Command buffer for recording commands
         * @param cascadeIndex Index of the cascade (0 to CASCADE_COUNT-1)
         */
        void BeginCascade(
            const Core::Ref<RHI::RHICommandBuffer>& cmdBuffer,
            uint32_t cascadeIndex);

        /**
         * @brief End rendering to the current cascade
         *
         * Ends the rendering pass for the current cascade.
         *
         * @param cmdBuffer Command buffer for recording commands
         * @param cascadeIndex Index of the cascade being ended
         */
        void EndCascade(
            const Core::Ref<RHI::RHICommandBuffer>& cmdBuffer,
            uint32_t cascadeIndex);

        /**
         * @brief Transition all cascades to shader read layout
         *
         * Call after all cascade rendering is complete, before the main
         * rendering pass samples from the shadow maps.
         *
         * @param cmdBuffer Command buffer for recording commands
         */
        void TransitionToShaderRead(const Core::Ref<RHI::RHICommandBuffer>& cmdBuffer);

        // ============================================================
        // Accessors
        // ============================================================

        /**
         * @brief Get the cascade data array
         * @return Const reference to cascade data array
         */
        const std::array<CascadeData, CASCADE_COUNT>& GetCascades() const { return m_Cascades; }

        /**
         * @brief Get the light-space matrix for a specific cascade
         * @param cascadeIndex Index of the cascade
         * @return Light-space view-projection matrix
         */
        const glm::mat4& GetLightSpaceMatrix(uint32_t cascadeIndex) const;

        /**
         * @brief Get the split depth for a specific cascade
         * @param cascadeIndex Index of the cascade
         * @return Split depth in clip space
         */
        float GetSplitDepth(uint32_t cascadeIndex) const;

        /**
         * @brief Get the shadow map array image
         * @return VkImage handle
         */
        VkImage GetImage() const { return m_Image; }

        /**
         * @brief Get the shadow map array image view (all layers)
         * @return VkImageView handle for shader binding
         */
        VkImageView GetImageView() const { return m_ArrayImageView; }

        /**
         * @brief Get the image view for a specific cascade layer
         * @param cascadeIndex Index of the cascade
         * @return VkImageView handle for the cascade layer
         */
        VkImageView GetCascadeImageView(uint32_t cascadeIndex) const;

        /**
         * @brief Get the shadow sampler
         * @return Shared pointer to the shadow sampler
         */
        Core::Ref<RHI::RHISampler> GetSampler() const { return m_Sampler; }

        /**
         * @brief Get the shadow map resolution per cascade
         * @return Resolution in pixels
         */
        uint32_t GetResolution() const { return m_Resolution; }

        /**
         * @brief Get the depth format
         * @return VkFormat of the depth texture
         */
        VkFormat GetFormat() const { return m_Format; }

        /**
         * @brief Get the UBO data for shader binding
         * @return CSM uniform buffer data
         */
        CascadedShadowMapUBO GetUBOData() const;

        /**
         * @brief Set the shadow bias
         * @param bias Depth bias for shadow acne prevention
         */
        void SetShadowBias(float bias) { m_ShadowBias = bias; }

        /**
         * @brief Set the normal bias
         * @param bias Normal-based offset for self-shadowing prevention
         */
        void SetNormalBias(float bias) { m_NormalBias = bias; }

    private:
        CascadedShadowMap() = default;

        bool Initialize(
            const Core::Ref<RHI::RHIDevice>& device,
            const Core::Ref<RHI::RHIDeletionQueue>& deletionQueue,
            const CascadedShadowMapDesc& desc);

        bool CreateImage(const Core::Ref<RHI::RHIDevice>& device);
        bool CreateImageViews(const Core::Ref<RHI::RHIDevice>& device);
        bool CreateSampler(const Core::Ref<RHI::RHIDevice>& device);

        /**
         * @brief Calculate cascade split depths using logarithmic/uniform blend
         * @param nearClip Camera near clip plane
         * @param farClip Camera far clip plane
         */
        void CalculateCascadeSplits(float nearClip, float farClip);

        /**
         * @brief Calculate light-space matrix for a cascade
         * @param camera Main camera for frustum calculation
         * @param nearSplit Near split depth in view space
         * @param farSplit Far split depth in view space
         * @param lightDir Normalized light direction
         * @return Light-space view-projection matrix
         */
        glm::mat4 CalculateLightSpaceMatrix(
            const Scene::Camera& camera,
            float nearSplit,
            float farSplit,
            const glm::vec3& lightDir);

        /**
         * @brief Transition a cascade layer layout
         */
        void TransitionCascadeLayout(
            const Core::Ref<RHI::RHICommandBuffer>& cmdBuffer,
            uint32_t cascadeIndex,
            VkImageLayout oldLayout,
            VkImageLayout newLayout);

        // Vulkan resources
        VkDevice m_Device = VK_NULL_HANDLE;
        VmaAllocator m_Allocator = nullptr;
        VkImage m_Image = VK_NULL_HANDLE;
        VmaAllocation m_Allocation = nullptr;
        VkImageView m_ArrayImageView = VK_NULL_HANDLE;
        std::array<VkImageView, CASCADE_COUNT> m_CascadeImageViews{};
        Core::Ref<RHI::RHISampler> m_Sampler;
        Core::Ref<RHI::RHIDeletionQueue> m_DeletionQueue;
        std::array<VkImageLayout, CASCADE_COUNT> m_CascadeLayouts{};

        // Configuration
        uint32_t m_Resolution = 2048;
        VkFormat m_Format = VK_FORMAT_D32_SFLOAT;
        float m_SplitLambda = 0.5f;
        float m_ShadowBias = 0.005f;
        float m_NormalBias = 0.02f;

        // Cascade data
        std::array<CascadeData, CASCADE_COUNT> m_Cascades;
        std::array<float, CASCADE_COUNT + 1> m_CascadeSplits{};
    };

} // namespace Renderer
