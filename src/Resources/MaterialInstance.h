/**
 * @file MaterialInstance.h
 * @brief Material instance with GPU descriptor set support.
 *
 * Provides a material instance class that manages GPU resources including
 * uniform buffers and descriptor sets for shader binding. Supports the
 * PBR texture workflow with base color, normal, metallic-roughness,
 * occlusion, and emissive textures.
 */

#pragma once

#include "Core/Types.h"
#include "Resources/ResourceHandle.h"

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

#include <array>
#include <string>
#include <unordered_map>

namespace RHI {
    class RHIDevice;
    class RHIBuffer;
    class RHITexture;
    class RHISampler;
    class RHIDescriptorSet;
    class RHIDescriptorSetLayout;
    class RHIDescriptorPool;
} // namespace RHI

namespace Resources {

/**
 * @brief Texture slot identifiers for PBR material textures.
 */
enum class TextureSlot : uint32_t {
    BaseColor = 0,          ///< Albedo/diffuse texture (sRGB)
    Normal = 1,             ///< Normal map (linear)
    MetallicRoughness = 2,  ///< Metallic (B) + Roughness (G) packed texture
    Occlusion = 3,          ///< Ambient occlusion texture
    Emissive = 4,           ///< Emissive texture (sRGB)
    Count = 5               ///< Total number of texture slots
};

/**
 * @brief GPU-aligned material data struct matching shader cbuffer layout.
 *
 * This struct must match the MaterialData cbuffer in shaders (register b3).
 * Memory layout is explicitly designed for GPU compatibility:
 * - vec4 alignment for vectors
 * - Padding to ensure 16-byte alignment boundaries
 */
struct MaterialGPUData {
    glm::vec4 BaseColor = glm::vec4(1.0f);      ///< Base color factor (RGBA)
    float Metallic = 0.0f;                       ///< Metallic factor (0-1)
    float Roughness = 0.5f;                      ///< Roughness factor (0-1)
    float AmbientOcclusion = 1.0f;               ///< AO strength (0-1)
    float NormalScale = 1.0f;                    ///< Normal map intensity
    glm::vec3 EmissiveFactor = glm::vec3(0.0f);  ///< Emissive color factor
    float AlphaCutoff = 0.5f;                    ///< Alpha test cutoff threshold

    // Texture presence flags (GPU-side detection)
    int32_t HasBaseColorTexture = 0;             ///< 1 if base color texture bound
    int32_t HasNormalTexture = 0;                ///< 1 if normal texture bound
    int32_t HasMetallicRoughnessTexture = 0;     ///< 1 if metallic-roughness bound
    int32_t HasOcclusionTexture = 0;             ///< 1 if occlusion texture bound
    int32_t HasEmissiveTexture = 0;              ///< 1 if emissive texture bound
    int32_t Padding[3] = {0, 0, 0};              ///< Padding to 16-byte alignment
};

// Ensure struct size is multiple of 16 bytes for UBO alignment
static_assert(sizeof(MaterialGPUData) % 16 == 0,
    "MaterialGPUData must be 16-byte aligned for UBO");

/**
 * @brief Material instance with GPU resource management.
 *
 * MaterialInstance represents a unique set of material parameters and textures
 * that can be bound to shaders. Each instance maintains:
 * - A uniform buffer containing material parameters
 * - A descriptor set binding textures and samplers
 *
 * The class follows the base Material / Instance pattern where:
 * - Base Material defines the shader/pipeline (not implemented here)
 * - MaterialInstance provides per-object parameter variation
 *
 * Usage:
 * @code
 * // Create material instance
 * auto instance = MaterialInstance::Create(device, pool, layout);
 *
 * // Set textures
 * instance->SetTexture(TextureSlot::BaseColor, albedoTexture);
 * instance->SetTexture(TextureSlot::Normal, normalTexture);
 *
 * // Set parameters
 * instance->SetBaseColor(glm::vec4(1.0f, 0.8f, 0.6f, 1.0f));
 * instance->SetMetallic(0.2f);
 * instance->SetRoughness(0.7f);
 *
 * // Bind for rendering
 * commandBuffer->BindDescriptorSets(..., instance->GetDescriptorSet()->GetHandle());
 * @endcode
 */
class MaterialInstance {
public:
    /**
     * @brief Factory method to create a material instance.
     *
     * Creates a new material instance with its own uniform buffer and
     * descriptor set. Requires a pre-created descriptor pool and layout.
     *
     * @param device The RHI device for resource creation.
     * @param pool The descriptor pool to allocate from.
     * @param layout The descriptor set layout defining binding points.
     * @param sampler The sampler to use for texture sampling.
     * @param defaultTexture Default texture for unbound slots (white 1x1).
     * @return Shared pointer to the created instance, or nullptr on failure.
     */
    static Core::Ref<MaterialInstance> Create(
        const Core::Ref<RHI::RHIDevice>& device,
        const Core::Ref<RHI::RHIDescriptorPool>& pool,
        const Core::Ref<RHI::RHIDescriptorSetLayout>& layout,
        const Core::Ref<RHI::RHISampler>& sampler,
        const Core::Ref<RHI::RHITexture>& defaultTexture);

    /**
     * @brief Create the standard descriptor set layout for materials.
     *
     * Creates a layout with the following bindings:
     * - Binding 0: Material UBO (uniform buffer, fragment stage)
     * - Binding 1: Base color texture (combined image sampler)
     * - Binding 2: Normal texture (combined image sampler)
     * - Binding 3: Metallic-roughness texture (combined image sampler)
     * - Binding 4: Occlusion texture (combined image sampler)
     * - Binding 5: Emissive texture (combined image sampler)
     *
     * @param device The RHI device.
     * @return Shared pointer to the created layout.
     */
    static Core::Ref<RHI::RHIDescriptorSetLayout> CreateDescriptorSetLayout(
        const Core::Ref<RHI::RHIDevice>& device);

    /**
     * @brief Destructor.
     */
    ~MaterialInstance() = default;

    // Non-copyable
    MaterialInstance(const MaterialInstance&) = delete;
    MaterialInstance& operator=(const MaterialInstance&) = delete;

    // Non-movable (contains GPU resources)
    MaterialInstance(MaterialInstance&&) = delete;
    MaterialInstance& operator=(MaterialInstance&&) = delete;

    // =========================================================================
    // Texture Management
    // =========================================================================

    /**
     * @brief Sets a texture for the specified slot.
     *
     * Updates the descriptor set immediately. If texture is nullptr,
     * the default texture (white 1x1) is bound instead.
     *
     * @param slot The texture slot to bind to.
     * @param texture The texture to bind.
     */
    void SetTexture(TextureSlot slot, const Core::Ref<RHI::RHITexture>& texture);

    /**
     * @brief Sets a texture by name.
     *
     * Convenience method for string-based texture binding.
     * Valid names: "baseColor", "normal", "metallicRoughness", "occlusion", "emissive"
     *
     * @param name Texture slot name.
     * @param texture The texture to bind.
     * @return true if the name was valid.
     */
    bool SetTexture(const std::string& name, const Core::Ref<RHI::RHITexture>& texture);

    /**
     * @brief Gets the texture bound to a slot.
     * @param slot The texture slot.
     * @return The bound texture, or nullptr if using default.
     */
    Core::Ref<RHI::RHITexture> GetTexture(TextureSlot slot) const;

    /**
     * @brief Checks if a custom texture is bound to a slot.
     * @param slot The texture slot.
     * @return true if a non-default texture is bound.
     */
    bool HasTexture(TextureSlot slot) const;

    // =========================================================================
    // Parameter Setters
    // =========================================================================

    /**
     * @brief Sets the base color factor.
     * @param color RGBA color multiplier.
     */
    void SetBaseColor(const glm::vec4& color);

    /**
     * @brief Sets the metallic factor.
     * @param metallic Metallic value (0 = dielectric, 1 = metal).
     */
    void SetMetallic(float metallic);

    /**
     * @brief Sets the roughness factor.
     * @param roughness Roughness value (0 = smooth, 1 = rough).
     */
    void SetRoughness(float roughness);

    /**
     * @brief Sets the ambient occlusion strength.
     * @param ao AO multiplier (0 = fully occluded, 1 = no occlusion).
     */
    void SetAmbientOcclusion(float ao);

    /**
     * @brief Sets the normal map intensity.
     * @param scale Normal scale (1 = full intensity, 0 = no effect).
     */
    void SetNormalScale(float scale);

    /**
     * @brief Sets the emissive color factor.
     * @param emissive RGB emissive color.
     */
    void SetEmissiveFactor(const glm::vec3& emissive);

    /**
     * @brief Sets the alpha cutoff threshold.
     * @param cutoff Alpha value below which pixels are discarded.
     */
    void SetAlphaCutoff(float cutoff);

    /**
     * @brief Sets a float parameter by name.
     * @param name Parameter name.
     * @param value The value to set.
     * @return true if the name was valid.
     */
    bool SetParameter(const std::string& name, float value);

    /**
     * @brief Sets a vec4 parameter by name.
     * @param name Parameter name.
     * @param value The value to set.
     * @return true if the name was valid.
     */
    bool SetParameter(const std::string& name, const glm::vec4& value);

    // =========================================================================
    // Parameter Getters
    // =========================================================================

    /**
     * @brief Gets the base color factor.
     */
    const glm::vec4& GetBaseColor() const { return m_MaterialData.BaseColor; }

    /**
     * @brief Gets the metallic factor.
     */
    float GetMetallic() const { return m_MaterialData.Metallic; }

    /**
     * @brief Gets the roughness factor.
     */
    float GetRoughness() const { return m_MaterialData.Roughness; }

    /**
     * @brief Gets the ambient occlusion strength.
     */
    float GetAmbientOcclusion() const { return m_MaterialData.AmbientOcclusion; }

    /**
     * @brief Gets the normal scale.
     */
    float GetNormalScale() const { return m_MaterialData.NormalScale; }

    /**
     * @brief Gets the emissive factor.
     */
    const glm::vec3& GetEmissiveFactor() const { return m_MaterialData.EmissiveFactor; }

    /**
     * @brief Gets the alpha cutoff.
     */
    float GetAlphaCutoff() const { return m_MaterialData.AlphaCutoff; }

    /**
     * @brief Gets the full material data struct.
     * @return Const reference to GPU data.
     */
    const MaterialGPUData& GetMaterialData() const { return m_MaterialData; }

    // =========================================================================
    // GPU Resources
    // =========================================================================

    /**
     * @brief Gets the descriptor set for shader binding.
     * @return The descriptor set containing material resources.
     */
    const Core::Ref<RHI::RHIDescriptorSet>& GetDescriptorSet() const
    {
        return m_DescriptorSet;
    }

    /**
     * @brief Gets the uniform buffer containing material parameters.
     * @return The uniform buffer.
     */
    const Core::Ref<RHI::RHIBuffer>& GetUniformBuffer() const
    {
        return m_UniformBuffer;
    }

    /**
     * @brief Uploads pending material data changes to the GPU.
     *
     * Called automatically when parameters change. Can be called
     * manually to batch updates.
     */
    void UploadToGPU();

    /**
     * @brief Checks if there are pending GPU updates.
     * @return true if UploadToGPU needs to be called.
     */
    bool IsDirty() const { return m_Dirty; }

private:
    /**
     * @brief Private constructor - use Create() factory method.
     */
    MaterialInstance() = default;

    /**
     * @brief Initializes the material instance.
     */
    bool Initialize(
        const Core::Ref<RHI::RHIDevice>& device,
        const Core::Ref<RHI::RHIDescriptorPool>& pool,
        const Core::Ref<RHI::RHIDescriptorSetLayout>& layout,
        const Core::Ref<RHI::RHISampler>& sampler,
        const Core::Ref<RHI::RHITexture>& defaultTexture);

    /**
     * @brief Updates the descriptor set with current textures.
     */
    void UpdateDescriptorSet();

    /**
     * @brief Marks the material as needing GPU upload.
     */
    void MarkDirty() { m_Dirty = true; }

    /**
     * @brief Converts texture slot name to enum.
     */
    static TextureSlot NameToSlot(const std::string& name);

    // GPU resources
    Core::Ref<RHI::RHIDevice> m_Device;
    Core::Ref<RHI::RHIBuffer> m_UniformBuffer;
    Core::Ref<RHI::RHIDescriptorSet> m_DescriptorSet;
    Core::Ref<RHI::RHISampler> m_Sampler;

    // Textures (indexed by TextureSlot)
    std::array<Core::Ref<RHI::RHITexture>, static_cast<size_t>(TextureSlot::Count)> m_Textures;
    Core::Ref<RHI::RHITexture> m_DefaultTexture;

    // CPU-side material data
    MaterialGPUData m_MaterialData;
    bool m_Dirty = true;
};

} // namespace Resources
