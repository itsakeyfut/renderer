/**
 * @file SkyboxRenderer.h
 * @brief Skybox rendering using cubemap environment maps.
 *
 * Provides functionality to render a skybox using a cubemap texture,
 * typically generated from an HDR equirectangular environment map.
 */

#pragma once

#include "Core/Types.h"

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

namespace RHI
{
    class RHIDevice;
    class RHIImage;
    class RHISampler;
    class RHIPipeline;
    class RHIShader;
    class RHIDescriptorPool;
    class RHIDescriptorSet;
    class RHIDescriptorSetLayout;
    class RHICommandBuffer;
} // namespace RHI

namespace Resources
{
    class EnvironmentMap;
} // namespace Resources

namespace Renderer
{

/**
 * @brief Skybox renderer for environment map visualization
 *
 * Renders a fullscreen skybox using a cubemap texture. The skybox is rendered
 * at the far plane to appear behind all scene geometry.
 *
 * Usage:
 * @code
 * // Create renderer
 * auto skybox = SkyboxRenderer::Create(device, colorFormat, depthFormat);
 *
 * // Set environment map
 * skybox->SetEnvironmentMap(device, envMap);
 *
 * // During rendering (inside a render pass)
 * skybox->Render(commandBuffer, camera.GetViewMatrix(), camera.GetProjectionMatrix());
 * @endcode
 */
class SkyboxRenderer
{
public:
    /**
     * @brief Create a skybox renderer
     *
     * @param device The logical device
     * @param colorFormat Swapchain color format for pipeline compatibility
     * @param depthFormat Depth buffer format for pipeline compatibility
     * @return Shared pointer to the skybox renderer, or nullptr on failure
     */
    static Core::Ref<SkyboxRenderer> Create(
        const Core::Ref<RHI::RHIDevice>& device,
        VkFormat colorFormat,
        VkFormat depthFormat);

    /**
     * @brief Destructor
     */
    ~SkyboxRenderer();

    // Non-copyable
    SkyboxRenderer(const SkyboxRenderer&) = delete;
    SkyboxRenderer& operator=(const SkyboxRenderer&) = delete;

    // Non-movable
    SkyboxRenderer(SkyboxRenderer&&) = delete;
    SkyboxRenderer& operator=(SkyboxRenderer&&) = delete;

    /**
     * @brief Set the environment map to render as skybox
     *
     * @param device The logical device
     * @param environmentMap The environment map containing the cubemap
     * @return true on success, false on failure
     */
    bool SetEnvironmentMap(
        const Core::Ref<RHI::RHIDevice>& device,
        const Core::Ref<Resources::EnvironmentMap>& environmentMap);

    /**
     * @brief Check if an environment map is currently set
     * @return true if environment map is set, false otherwise
     */
    bool HasEnvironmentMap() const { return m_HasEnvironmentMap; }

    /**
     * @brief Render the skybox
     *
     * Should be called inside an active render pass. The skybox uses the depth
     * buffer with less-or-equal comparison to render behind all scene geometry.
     *
     * @param commandBuffer Command buffer for recording draw commands
     * @param viewMatrix Camera view matrix
     * @param projectionMatrix Camera projection matrix
     */
    void Render(
        const Core::Ref<RHI::RHICommandBuffer>& commandBuffer,
        const glm::mat4& viewMatrix,
        const glm::mat4& projectionMatrix);

private:
    /**
     * @brief Private constructor - use Create() factory method
     */
    SkyboxRenderer() = default;

    /**
     * @brief Initialize the skybox renderer
     *
     * @param device The logical device
     * @param colorFormat Swapchain color format
     * @param depthFormat Depth buffer format
     * @return true on success, false on failure
     */
    bool Initialize(
        const Core::Ref<RHI::RHIDevice>& device,
        VkFormat colorFormat,
        VkFormat depthFormat);

    /**
     * @brief Create the graphics pipeline
     *
     * @param device The logical device
     * @param colorFormat Swapchain color format
     * @param depthFormat Depth buffer format
     * @return true on success, false on failure
     */
    bool CreatePipeline(
        const Core::Ref<RHI::RHIDevice>& device,
        VkFormat colorFormat,
        VkFormat depthFormat);

    /**
     * @brief Create descriptor resources
     *
     * @param device The logical device
     * @return true on success, false on failure
     */
    bool CreateDescriptors(const Core::Ref<RHI::RHIDevice>& device);

    // Shaders
    Core::Ref<RHI::RHIShader> m_VertexShader;
    Core::Ref<RHI::RHIShader> m_PixelShader;

    // Pipeline
    Core::Ref<RHI::RHIPipeline> m_Pipeline;

    // Descriptors
    Core::Ref<RHI::RHIDescriptorSetLayout> m_DescriptorLayout;
    Core::Ref<RHI::RHIDescriptorPool> m_DescriptorPool;
    Core::Ref<RHI::RHIDescriptorSet> m_DescriptorSet;

    // Sampler for cubemap
    Core::Ref<RHI::RHISampler> m_Sampler;

    // State
    bool m_HasEnvironmentMap = false;
};

} // namespace Renderer
