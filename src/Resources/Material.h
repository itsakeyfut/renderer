/**
 * @file Material.h
 * @brief Material resource for surface rendering properties.
 *
 * This is a stub implementation. Full material support with descriptor sets
 * will be added in a separate issue when the rendering pipeline is expanded.
 */

#pragma once

#include "Core/Types.h"
#include "Resources/ResourceHandle.h"
#include <string>
#include <cstdint>

namespace Resources {

// Forward declaration
class Texture;

/**
 * @brief Material blend mode.
 */
enum class BlendMode : uint8_t {
    Opaque,
    AlphaTest,
    AlphaBlend,
    Additive,
};

/**
 * @brief Material cull mode.
 */
enum class CullMode : uint8_t {
    None,
    Front,
    Back,
};

/**
 * @brief PBR material parameters.
 */
struct PBRParameters {
    float BaseColor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    float EmissiveColor[3] = {0.0f, 0.0f, 0.0f};
    float Metallic = 0.0f;
    float Roughness = 0.5f;
    float AmbientOcclusion = 1.0f;
    float AlphaCutoff = 0.5f;
    float NormalScale = 1.0f;
};

/**
 * @brief Material resource class.
 *
 * Represents a material with textures and rendering parameters.
 * Uses a PBR workflow with support for common texture types.
 */
class Material {
public:
    /**
     * @brief Constructs a material with the given name.
     * @param name Material name for identification.
     */
    explicit Material(const std::string& name)
        : m_Name(name)
    {
    }

    /**
     * @brief Gets the material name.
     * @return Material name string.
     */
    const std::string& GetName() const { return m_Name; }

    /**
     * @brief Sets the material name.
     * @param name New name for the material.
     */
    void SetName(const std::string& name) { m_Name = name; }

    // =========================================================================
    // Texture Handles
    // =========================================================================

    /**
     * @brief Gets the diffuse/albedo texture handle.
     * @return TextureHandle to the diffuse texture.
     */
    TextureHandle GetDiffuseTexture() const { return m_DiffuseTexture; }

    /**
     * @brief Sets the diffuse/albedo texture.
     * @param texture Handle to the texture.
     */
    void SetDiffuseTexture(TextureHandle texture) { m_DiffuseTexture = texture; }

    /**
     * @brief Gets the normal map texture handle.
     * @return TextureHandle to the normal texture.
     */
    TextureHandle GetNormalTexture() const { return m_NormalTexture; }

    /**
     * @brief Sets the normal map texture.
     * @param texture Handle to the texture.
     */
    void SetNormalTexture(TextureHandle texture) { m_NormalTexture = texture; }

    /**
     * @brief Gets the metallic-roughness texture handle.
     * @return TextureHandle to the metallic-roughness texture.
     */
    TextureHandle GetMetallicRoughnessTexture() const { return m_MetallicRoughnessTexture; }

    /**
     * @brief Sets the metallic-roughness texture.
     * @param texture Handle to the texture.
     */
    void SetMetallicRoughnessTexture(TextureHandle texture) { m_MetallicRoughnessTexture = texture; }

    /**
     * @brief Gets the ambient occlusion texture handle.
     * @return TextureHandle to the AO texture.
     */
    TextureHandle GetAOTexture() const { return m_AOTexture; }

    /**
     * @brief Sets the ambient occlusion texture.
     * @param texture Handle to the texture.
     */
    void SetAOTexture(TextureHandle texture) { m_AOTexture = texture; }

    /**
     * @brief Gets the emissive texture handle.
     * @return TextureHandle to the emissive texture.
     */
    TextureHandle GetEmissiveTexture() const { return m_EmissiveTexture; }

    /**
     * @brief Sets the emissive texture.
     * @param texture Handle to the texture.
     */
    void SetEmissiveTexture(TextureHandle texture) { m_EmissiveTexture = texture; }

    // =========================================================================
    // PBR Parameters
    // =========================================================================

    /**
     * @brief Gets the PBR parameters.
     * @return Reference to PBR parameters.
     */
    const PBRParameters& GetParameters() const { return m_Parameters; }

    /**
     * @brief Gets mutable PBR parameters.
     * @return Mutable reference to PBR parameters.
     */
    PBRParameters& GetParameters() { return m_Parameters; }

    /**
     * @brief Sets the PBR parameters.
     * @param params New PBR parameters.
     */
    void SetParameters(const PBRParameters& params) { m_Parameters = params; }

    /**
     * @brief Sets the base color.
     * @param r Red component (0-1).
     * @param g Green component (0-1).
     * @param b Blue component (0-1).
     * @param a Alpha component (0-1).
     */
    void SetBaseColor(float r, float g, float b, float a = 1.0f)
    {
        m_Parameters.BaseColor[0] = r;
        m_Parameters.BaseColor[1] = g;
        m_Parameters.BaseColor[2] = b;
        m_Parameters.BaseColor[3] = a;
    }

    /**
     * @brief Sets the metallic value.
     * @param metallic Metallic factor (0-1).
     */
    void SetMetallic(float metallic) { m_Parameters.Metallic = metallic; }

    /**
     * @brief Sets the roughness value.
     * @param roughness Roughness factor (0-1).
     */
    void SetRoughness(float roughness) { m_Parameters.Roughness = roughness; }

    // =========================================================================
    // Render State
    // =========================================================================

    /**
     * @brief Gets the blend mode.
     * @return Current blend mode.
     */
    BlendMode GetBlendMode() const { return m_BlendMode; }

    /**
     * @brief Sets the blend mode.
     * @param mode New blend mode.
     */
    void SetBlendMode(BlendMode mode) { m_BlendMode = mode; }

    /**
     * @brief Gets the cull mode.
     * @return Current cull mode.
     */
    CullMode GetCullMode() const { return m_CullMode; }

    /**
     * @brief Sets the cull mode.
     * @param mode New cull mode.
     */
    void SetCullMode(CullMode mode) { m_CullMode = mode; }

    /**
     * @brief Checks if the material is double-sided.
     * @return true if double-sided (no culling).
     */
    bool IsDoubleSided() const { return m_CullMode == CullMode::None; }

    /**
     * @brief Sets whether the material is double-sided.
     * @param doubleSided true for no culling.
     */
    void SetDoubleSided(bool doubleSided)
    {
        m_CullMode = doubleSided ? CullMode::None : CullMode::Back;
    }

    /**
     * @brief Checks if depth writing is enabled.
     * @return true if depth write is enabled.
     */
    bool IsDepthWriteEnabled() const { return m_DepthWrite; }

    /**
     * @brief Sets depth write state.
     * @param enabled true to enable depth writing.
     */
    void SetDepthWrite(bool enabled) { m_DepthWrite = enabled; }

    /**
     * @brief Estimates memory usage of this material.
     * @return Approximate memory usage in bytes (excluding textures).
     */
    size_t GetMemorySize() const
    {
        return sizeof(Material);
    }

private:
    std::string m_Name;

    // Texture handles
    TextureHandle m_DiffuseTexture;
    TextureHandle m_NormalTexture;
    TextureHandle m_MetallicRoughnessTexture;
    TextureHandle m_AOTexture;
    TextureHandle m_EmissiveTexture;

    // Parameters
    PBRParameters m_Parameters;

    // Render state
    BlendMode m_BlendMode = BlendMode::Opaque;
    CullMode m_CullMode = CullMode::Back;
    bool m_DepthWrite = true;
};

} // namespace Resources
