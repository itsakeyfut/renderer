/**
 * @file LightVolumeRenderer.h
 * @brief Light volume rendering for deferred shading optimization.
 *
 * Renders bounding volumes for point lights (spheres) and spot lights (cones)
 * to limit lighting calculations to affected pixels only. Uses stencil
 * optimization to skip pixels outside the light's influence.
 */

#pragma once

#include "Core/Types.h"
#include "Renderer/FrameManager.h"
#include "Scene/Camera.h"
#include "Scene/Light.h"

#include <glm/glm.hpp>
#include <vector>
#include <vulkan/vulkan.h>

namespace RHI
{
    class RHIDevice;
    class RHIBuffer;
    class RHICommandBuffer;
    class RHIDeletionQueue;
    class RHIPipeline;
    class RHIShader;
    class RHIDescriptorSetLayout;
    class RHIDescriptorPool;
    class RHIDescriptorSet;
} // namespace RHI

namespace Renderer
{
    class GBuffer;
    class LightManager;

    /**
     * @brief Configuration for light volume renderer creation
     */
    struct LightVolumeRendererDesc
    {
        /**
         * @brief Number of segments for sphere generation (higher = smoother)
         */
        uint32_t SphereSegments = 16;

        /**
         * @brief Number of segments for cone base (higher = smoother)
         */
        uint32_t ConeSegments = 16;

        /**
         * @brief Output color format for light accumulation
         */
        VkFormat ColorFormat = VK_FORMAT_R16G16B16A16_SFLOAT;

        /**
         * @brief Combined depth-stencil format for light volume rendering
         *
         * Must be a combined depth-stencil format (e.g., D32_SFLOAT_S8_UINT)
         * since stencil operations are required for the optimization.
         */
        VkFormat DepthStencilFormat = VK_FORMAT_D32_SFLOAT_S8_UINT;

        /**
         * @brief Debug name for the renderer
         */
        const char* DebugName = "LightVolumeRenderer";
    };

    /**
     * @brief Push constants for light volume rendering
     */
    struct LightVolumePushConstants
    {
        glm::mat4 ModelViewProjection;  // Model * View * Projection
        glm::mat4 Model;                // Model matrix for world position
        uint32_t LightIndex;            // Index into light storage buffer
        uint32_t Padding[3];            // Alignment padding
    };

    static_assert(sizeof(LightVolumePushConstants) == 144,
        "LightVolumePushConstants size must be 144 bytes for push constant alignment");

    /**
     * @brief Light volume renderer for deferred shading
     *
     * Implements light volume rendering to optimize deferred lighting by only
     * calculating lighting for pixels within a light's influence. Uses:
     *
     * - Sphere meshes for point lights (scaled to light radius)
     * - Cone meshes for spot lights (scaled to cone angle and range)
     * - Stencil optimization to handle camera inside/outside volume cases
     *
     * Stencil Optimization Algorithm:
     * 1. Render back-faces: increment stencil where depth test fails
     *    (marks pixels where geometry is in front of back face)
     * 2. Render front-faces: shade only where stencil != 0 and depth fails
     *    (pixels inside volume but behind geometry)
     *
     * Usage:
     * @code
     * auto renderer = LightVolumeRenderer::Create(device, deletionQueue, desc);
     *
     * // During lighting pass
     * renderer->Begin(cmdBuffer, gBuffer, camera, frameIndex);
     * renderer->RenderPointLights(cmdBuffer, pointLights, camera);
     * renderer->RenderSpotLights(cmdBuffer, spotLights, camera);
     * renderer->End(cmdBuffer);
     * @endcode
     */
    class LightVolumeRenderer
    {
    public:
        /**
         * @brief Factory method to create a light volume renderer
         * @param device The logical device
         * @param deletionQueue Deletion queue for deferred resource cleanup
         * @param desc Renderer configuration
         * @return Shared pointer to the created renderer, or nullptr on failure
         */
        static Core::Ref<LightVolumeRenderer> Create(
            const Core::Ref<RHI::RHIDevice>& device,
            const Core::Ref<RHI::RHIDeletionQueue>& deletionQueue,
            const LightVolumeRendererDesc& desc = LightVolumeRendererDesc{});

        /**
         * @brief Destructor
         */
        ~LightVolumeRenderer();

        // Non-copyable
        LightVolumeRenderer(const LightVolumeRenderer&) = delete;
        LightVolumeRenderer& operator=(const LightVolumeRenderer&) = delete;

        // Non-movable
        LightVolumeRenderer(LightVolumeRenderer&&) = delete;
        LightVolumeRenderer& operator=(LightVolumeRenderer&&) = delete;

        // ============================================================
        // Initialization
        // ============================================================

        /**
         * @brief Initialize pipelines with descriptor set layouts
         *
         * Must be called after creation with the required layouts from other
         * systems (GBuffer, LightManager, IBL).
         *
         * @param gBufferLayout Descriptor set layout for G-Buffer textures (Set 0)
         * @param lightLayout Descriptor set layout for light data (Set 1)
         * @return true on success, false on failure
         */
        bool InitializePipelines(
            VkDescriptorSetLayout gBufferLayout,
            VkDescriptorSetLayout lightLayout);

        // ============================================================
        // Rendering
        // ============================================================

        /**
         * @brief Render point lights using sphere volumes
         *
         * For each point light, renders a sphere scaled to the light's radius.
         * Uses stencil optimization to skip pixels outside the volume.
         *
         * @param cmdBuffer Command buffer for recording commands
         * @param lights Vector of point lights to render
         * @param camera Camera for view/projection matrices
         * @param gBufferSet Descriptor set for G-Buffer textures
         * @param lightSet Descriptor set for light data
         */
        void RenderPointLights(
            const Core::Ref<RHI::RHICommandBuffer>& cmdBuffer,
            const std::vector<Scene::PointLight>& lights,
            const Scene::Camera& camera,
            VkDescriptorSet gBufferSet,
            VkDescriptorSet lightSet);

        /**
         * @brief Render spot lights using cone volumes
         *
         * For each spot light, renders a cone based on the outer cone angle
         * and effective range. Uses stencil optimization.
         *
         * @param cmdBuffer Command buffer for recording commands
         * @param lights Vector of spot lights to render
         * @param camera Camera for view/projection matrices
         * @param gBufferSet Descriptor set for G-Buffer textures
         * @param lightSet Descriptor set for light data
         */
        void RenderSpotLights(
            const Core::Ref<RHI::RHICommandBuffer>& cmdBuffer,
            const std::vector<Scene::SpotLight>& lights,
            const Scene::Camera& camera,
            VkDescriptorSet gBufferSet,
            VkDescriptorSet lightSet);

        // ============================================================
        // Accessors
        // ============================================================

        /**
         * @brief Get the number of sphere vertices
         * @return Vertex count for sphere mesh
         */
        uint32_t GetSphereVertexCount() const { return m_SphereVertexCount; }

        /**
         * @brief Get the number of sphere indices
         * @return Index count for sphere mesh
         */
        uint32_t GetSphereIndexCount() const { return m_SphereIndexCount; }

        /**
         * @brief Get the number of cone vertices
         * @return Vertex count for cone mesh
         */
        uint32_t GetConeVertexCount() const { return m_ConeVertexCount; }

        /**
         * @brief Get the number of cone indices
         * @return Index count for cone mesh
         */
        uint32_t GetConeIndexCount() const { return m_ConeIndexCount; }

    private:
        /**
         * @brief Private constructor - use Create() factory method
         */
        LightVolumeRenderer() = default;

        /**
         * @brief Initialize the renderer
         * @param device The logical device
         * @param deletionQueue Deletion queue for deferred resource cleanup
         * @param desc Renderer configuration
         * @return true on success, false on failure
         */
        bool Initialize(
            const Core::Ref<RHI::RHIDevice>& device,
            const Core::Ref<RHI::RHIDeletionQueue>& deletionQueue,
            const LightVolumeRendererDesc& desc);

        /**
         * @brief Generate sphere mesh for point light volumes
         * @param device The logical device
         * @param segments Number of segments (latitude and longitude)
         * @return true on success, false on failure
         */
        bool GenerateSphereMesh(
            const Core::Ref<RHI::RHIDevice>& device,
            uint32_t segments);

        /**
         * @brief Generate cone mesh for spot light volumes
         * @param device The logical device
         * @param segments Number of segments around the base
         * @return true on success, false on failure
         */
        bool GenerateConeMesh(
            const Core::Ref<RHI::RHIDevice>& device,
            uint32_t segments);

        /**
         * @brief Create shaders for light volume rendering
         * @param device The logical device
         * @return true on success, false on failure
         */
        bool CreateShaders(const Core::Ref<RHI::RHIDevice>& device);

        /**
         * @brief Create the stencil pass pipeline
         *
         * Pipeline for marking stencil buffer with back-face rendering.
         *
         * @param device The logical device
         * @param gBufferLayout G-Buffer descriptor set layout
         * @param lightLayout Light descriptor set layout
         * @return true on success, false on failure
         */
        bool CreateStencilPipeline(
            const Core::Ref<RHI::RHIDevice>& device,
            VkDescriptorSetLayout gBufferLayout,
            VkDescriptorSetLayout lightLayout);

        /**
         * @brief Create the point light pipeline
         * @param device The logical device
         * @param gBufferLayout G-Buffer descriptor set layout
         * @param lightLayout Light descriptor set layout
         * @return true on success, false on failure
         */
        bool CreatePointLightPipeline(
            const Core::Ref<RHI::RHIDevice>& device,
            VkDescriptorSetLayout gBufferLayout,
            VkDescriptorSetLayout lightLayout);

        /**
         * @brief Create the spot light pipeline
         * @param device The logical device
         * @param gBufferLayout G-Buffer descriptor set layout
         * @param lightLayout Light descriptor set layout
         * @return true on success, false on failure
         */
        bool CreateSpotLightPipeline(
            const Core::Ref<RHI::RHIDevice>& device,
            VkDescriptorSetLayout gBufferLayout,
            VkDescriptorSetLayout lightLayout);

        /**
         * @brief Calculate model matrix for a point light sphere
         * @param light Point light data
         * @return 4x4 model matrix (translation + scale)
         */
        glm::mat4 CalculatePointLightMatrix(const Scene::PointLight& light) const;

        /**
         * @brief Calculate model matrix for a spot light cone
         * @param light Spot light data
         * @return 4x4 model matrix (translation + rotation + scale)
         */
        glm::mat4 CalculateSpotLightMatrix(const Scene::SpotLight& light) const;

        // Device reference
        Core::Ref<RHI::RHIDevice> m_Device;
        Core::Ref<RHI::RHIDeletionQueue> m_DeletionQueue;

        // Mesh resources
        Core::Ref<RHI::RHIBuffer> m_SphereVertexBuffer;
        Core::Ref<RHI::RHIBuffer> m_SphereIndexBuffer;
        Core::Ref<RHI::RHIBuffer> m_ConeVertexBuffer;
        Core::Ref<RHI::RHIBuffer> m_ConeIndexBuffer;

        uint32_t m_SphereVertexCount = 0;
        uint32_t m_SphereIndexCount = 0;
        uint32_t m_ConeVertexCount = 0;
        uint32_t m_ConeIndexCount = 0;

        // Shaders
        Core::Ref<RHI::RHIShader> m_VolumeVertexShader;
        Core::Ref<RHI::RHIShader> m_PointLightPixelShader;
        Core::Ref<RHI::RHIShader> m_SpotLightPixelShader;

        // Pipelines
        Core::Ref<RHI::RHIPipeline> m_StencilPipeline;
        Core::Ref<RHI::RHIPipeline> m_PointLightPipeline;
        Core::Ref<RHI::RHIPipeline> m_SpotLightPipeline;

        // Configuration
        VkFormat m_ColorFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
        VkFormat m_DepthStencilFormat = VK_FORMAT_D32_SFLOAT_S8_UINT;
        bool m_PipelinesInitialized = false;
    };

} // namespace Renderer
