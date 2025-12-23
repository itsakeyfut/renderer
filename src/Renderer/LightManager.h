/**
 * @file LightManager.h
 * @brief Light management system for the renderer.
 *
 * Manages scene lights (directional, point, spot) and their GPU resources
 * including uniform buffers and storage buffers for shader access.
 */

#pragma once

#include "Core/Assert.h"
#include "Core/Types.h"
#include "RHI/RHIBuffer.h"
#include "RHI/RHIDescriptorSetLayout.h"
#include "RHI/RHIDescriptorPool.h"
#include "RHI/RHIDescriptorSet.h"
#include "RHI/RHIDevice.h"
#include "RHI/RHISampler.h"
#include "RHI/RHITexture.h"
#include "Scene/Light.h"
#include "FrameManager.h"
#include "ShadowMap.h"

#include <glm/glm.hpp>
#include <vector>
#include <array>

namespace Renderer {

/**
 * @brief Maximum number of point lights supported in a single frame.
 */
constexpr uint32_t MAX_POINT_LIGHTS = 64;

/**
 * @brief Maximum number of spot lights supported in a single frame.
 */
constexpr uint32_t MAX_SPOT_LIGHTS = 32;

/**
 * @brief Configuration for LightManager creation.
 */
struct LightManagerConfig {
    uint32_t MaxPointLights = MAX_POINT_LIGHTS;
    uint32_t MaxSpotLights = MAX_SPOT_LIGHTS;
};

/**
 * @brief Manages scene lighting data and GPU resources.
 *
 * The LightManager provides:
 * - Storage for scene lights (1 directional, multiple point/spot)
 * - GPU buffer management (UBO for directional, storage buffers for arrays)
 * - Shadow map integration for directional light shadows
 * - Descriptor set layout and sets for shader binding
 * - Per-frame update functionality for double/triple buffering
 *
 * Shader binding layout (Set 2):
 *   - Binding 0: LightUBO (uniform buffer)
 *   - Binding 1: PointLight storage buffer
 *   - Binding 2: SpotLight storage buffer
 *   - Binding 3: Shadow map (combined image sampler with comparison)
 *   - Binding 4: ShadowData UBO (light-space matrix, bias)
 */
class LightManager {
public:
    /**
     * @brief Creates a new LightManager instance.
     * @param device RHI device for resource creation.
     * @param config Configuration options.
     * @return Shared pointer to the created LightManager, or nullptr on failure.
     */
    static Core::Ref<LightManager> Create(
        const Core::Ref<RHI::RHIDevice>& device,
        const LightManagerConfig& config = {});

    ~LightManager() = default;

    // Prevent copying
    LightManager(const LightManager&) = delete;
    LightManager& operator=(const LightManager&) = delete;

    /**
     * @brief Sets the directional light for the scene.
     * @param light Directional light data.
     */
    void SetDirectionalLight(const Scene::DirectionalLight& light);

    /**
     * @brief Gets the current directional light.
     * @return Reference to the directional light.
     */
    const Scene::DirectionalLight& GetDirectionalLight() const { return m_DirectionalLight; }

    /**
     * @brief Clears all point lights.
     */
    void ClearPointLights();

    /**
     * @brief Adds a point light to the scene.
     * @param light Point light data.
     * @return Index of the added light, or -1 if max capacity reached.
     */
    int32_t AddPointLight(const Scene::PointLight& light);

    /**
     * @brief Updates a point light at the specified index.
     * @param index Index of the light to update.
     * @param light New light data.
     * @return true if successful, false if index is out of range.
     */
    bool UpdatePointLight(size_t index, const Scene::PointLight& light);

    /**
     * @brief Gets the number of active point lights.
     * @return Number of point lights.
     */
    size_t GetPointLightCount() const { return m_PointLights.size(); }

    /**
     * @brief Gets a point light at the specified index.
     * @param index Index of the light.
     * @return Pointer to the light, or nullptr if index is out of range.
     */
    const Scene::PointLight* GetPointLight(size_t index) const;

    /**
     * @brief Clears all spot lights.
     */
    void ClearSpotLights();

    /**
     * @brief Adds a spot light to the scene.
     * @param light Spot light data.
     * @return Index of the added light, or -1 if max capacity reached.
     */
    int32_t AddSpotLight(const Scene::SpotLight& light);

    /**
     * @brief Updates a spot light at the specified index.
     * @param index Index of the light to update.
     * @param light New light data.
     * @return true if successful, false if index is out of range.
     */
    bool UpdateSpotLight(size_t index, const Scene::SpotLight& light);

    /**
     * @brief Gets the number of active spot lights.
     * @return Number of spot lights.
     */
    size_t GetSpotLightCount() const { return m_SpotLights.size(); }

    /**
     * @brief Gets a spot light at the specified index.
     * @param index Index of the light.
     * @return Pointer to the light, or nullptr if index is out of range.
     */
    const Scene::SpotLight* GetSpotLight(size_t index) const;

    // =========================================================================
    // Shadow Map Integration
    // =========================================================================

    /**
     * @brief Sets the shadow map for directional light shadows.
     *
     * This binds the shadow map texture and sampler to descriptor set binding 3.
     * The shadow map should be rendered before the main pass and transitioned
     * to shader read layout.
     *
     * @param shadowMap The shadow map to bind.
     */
    void SetShadowMap(const Core::Ref<ShadowMap>& shadowMap);

    /**
     * @brief Gets the current shadow map.
     * @return The bound shadow map, or nullptr if none.
     */
    const Core::Ref<ShadowMap>& GetShadowMap() const { return m_ShadowMap; }

    /**
     * @brief Sets the shadow bias for shadow acne prevention.
     * @param bias Depth bias value.
     */
    void SetShadowBias(float bias) { m_ShadowBias = bias; MarkDirty(); }

    /**
     * @brief Gets the current shadow bias.
     * @return Shadow bias value.
     */
    float GetShadowBias() const { return m_ShadowBias; }

    /**
     * @brief Sets the normal bias for self-shadowing prevention.
     * @param bias Normal-based offset value.
     */
    void SetNormalBias(float bias) { m_NormalBias = bias; MarkDirty(); }

    /**
     * @brief Gets the current normal bias.
     * @return Normal bias value.
     */
    float GetNormalBias() const { return m_NormalBias; }

    /**
     * @brief Sets the shadow strength (darkness) multiplier.
     * @param strength Shadow darkness (0.0 = no shadow, 1.0 = full shadow).
     */
    void SetShadowStrength(float strength) { m_ShadowStrength = glm::clamp(strength, 0.0f, 1.0f); MarkDirty(); }

    /**
     * @brief Gets the current shadow strength.
     * @return Shadow strength value (0.0 to 1.0).
     */
    float GetShadowStrength() const { return m_ShadowStrength; }

    /**
     * @brief Checks if shadow mapping is enabled.
     * @return true if a shadow map is bound.
     */
    bool IsShadowEnabled() const { return m_ShadowMap != nullptr; }

    /**
     * @brief Updates GPU buffers for the specified frame index.
     *
     * This should be called once per frame before rendering to ensure
     * light data is uploaded to GPU memory.
     *
     * @param frameIndex Current frame index (0 to MAX_FRAMES_IN_FLIGHT-1).
     */
    void UpdateGPUBuffers(uint32_t frameIndex);

    /**
     * @brief Gets the descriptor set layout for light data.
     *
     * This layout should be used when creating the graphics pipeline.
     * It defines bindings for:
     *   - Binding 0: LightUBO (uniform buffer, fragment stage)
     *   - Binding 1: PointLight array (storage buffer, fragment stage)
     *   - Binding 2: SpotLight array (storage buffer, fragment stage)
     *
     * @return Shared pointer to the descriptor set layout.
     */
    const Core::Ref<RHI::RHIDescriptorSetLayout>& GetDescriptorSetLayout() const {
        return m_DescriptorSetLayout;
    }

    /**
     * @brief Gets the descriptor set for the specified frame index.
     * @param frameIndex Current frame index (0 to MAX_FRAMES_IN_FLIGHT-1).
     * @return Shared pointer to the descriptor set.
     */
    const Core::Ref<RHI::RHIDescriptorSet>& GetDescriptorSet(uint32_t frameIndex) const {
        ASSERT(frameIndex < MAX_FRAMES_IN_FLIGHT);
        return m_DescriptorSets[frameIndex];
    }

    /**
     * @brief Marks light data as dirty, requiring GPU update for all frames.
     *
     * With per-frame buffering, all in-flight frames need to be updated
     * when light data changes.
     */
    void MarkDirty() { m_DirtyFrameCount = MAX_FRAMES_IN_FLIGHT; }

    /**
     * @brief Checks if any frames still need GPU update.
     * @return true if at least one frame needs updating.
     */
    bool IsDirty() const { return m_DirtyFrameCount > 0; }

private:
    LightManager() = default;

    /**
     * @brief Initializes GPU resources.
     * @param device RHI device for resource creation.
     * @param config Configuration options.
     * @return true on success, false on failure.
     */
    bool Initialize(
        const Core::Ref<RHI::RHIDevice>& device,
        const LightManagerConfig& config);

    // Light data
    Scene::DirectionalLight m_DirectionalLight;
    std::vector<Scene::PointLight> m_PointLights;
    std::vector<Scene::SpotLight> m_SpotLights;

    // Configuration
    uint32_t m_MaxPointLights = MAX_POINT_LIGHTS;
    uint32_t m_MaxSpotLights = MAX_SPOT_LIGHTS;

    // GPU resources (per-frame for double buffering)
    std::array<Core::Ref<RHI::RHIBuffer>, MAX_FRAMES_IN_FLIGHT> m_LightUBOs;
    std::array<Core::Ref<RHI::RHIBuffer>, MAX_FRAMES_IN_FLIGHT> m_PointLightBuffers;
    std::array<Core::Ref<RHI::RHIBuffer>, MAX_FRAMES_IN_FLIGHT> m_SpotLightBuffers;

    // Shadow resources
    Core::Ref<ShadowMap> m_ShadowMap;
    Core::Ref<RHI::RHITexture> m_FallbackShadowMap;
    Core::Ref<RHI::RHISampler> m_FallbackShadowSampler;
    std::array<Core::Ref<RHI::RHIBuffer>, MAX_FRAMES_IN_FLIGHT> m_ShadowUBOs;
    float m_ShadowBias = 0.005f;
    float m_NormalBias = 0.02f;
    float m_ShadowStrength = 1.0f;

    // Descriptor resources
    Core::Ref<RHI::RHIDescriptorSetLayout> m_DescriptorSetLayout;
    Core::Ref<RHI::RHIDescriptorPool> m_DescriptorPool;
    std::array<Core::Ref<RHI::RHIDescriptorSet>, MAX_FRAMES_IN_FLIGHT> m_DescriptorSets;

    // State tracking - counts frames needing update for per-frame buffering
    uint32_t m_DirtyFrameCount = MAX_FRAMES_IN_FLIGHT;

    // Device reference for buffer updates
    Core::Ref<RHI::RHIDevice> m_Device;
};

} // namespace Renderer
