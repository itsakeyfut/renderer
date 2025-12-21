/**
 * @file EnvironmentMap.h
 * @brief HDR Environment Map loading and cubemap conversion.
 *
 * Provides functionality to load HDR equirectangular environment maps
 * and convert them to cubemaps for Image-Based Lighting (IBL).
 */

#pragma once

#include "Core/Types.h"

#include <vulkan/vulkan.h>
#include <string>
#include <vector>

namespace RHI
{
    class RHIDevice;
    class RHIImage;
    class RHISampler;
    class RHIDescriptorPool;
    class RHIDescriptorSet;
    class RHIDescriptorSetLayout;
    class RHIPipeline;
    class RHIShader;
} // namespace RHI

namespace Resources
{

/**
 * @brief HDR Environment Map resource
 *
 * Loads HDR equirectangular environment maps and converts them to cubemaps.
 * Supports IBL workflow with cubemap for environment reflections and
 * irradiance map for diffuse IBL.
 *
 * Usage:
 * @code
 * auto envMap = EnvironmentMap::LoadHDR(device, "textures/environment.hdr");
 * if (envMap) {
 *     // Use cubemap in shader for IBL
 *     auto cubemap = envMap->GetCubemap();
 *     // Use irradiance map for diffuse IBL
 *     auto irradiance = envMap->GetIrradianceMap();
 *     // Use prefiltered map for specular IBL
 *     auto prefiltered = envMap->GetPrefilteredMap();
 *     // Use BRDF LUT for split-sum approximation
 *     auto brdfLut = envMap->GetBRDFLut();
 * }
 * @endcode
 */
class EnvironmentMap
{
public:
    /**
     * @brief Load an HDR environment map from file
     *
     * Loads an equirectangular HDR image and converts it to a cubemap.
     * Supports .hdr (Radiance RGBE) format.
     *
     * @param device The logical device
     * @param filepath Path to the HDR image file
     * @param cubemapSize Size of each cubemap face (default: 512)
     * @return Shared pointer to the environment map, or nullptr on failure
     */
    static Core::Ref<EnvironmentMap> LoadHDR(
        const Core::Ref<RHI::RHIDevice>& device,
        const std::string& filepath,
        uint32_t cubemapSize = 512);

    /**
     * @brief Destructor
     */
    ~EnvironmentMap();

    // Non-copyable
    EnvironmentMap(const EnvironmentMap&) = delete;
    EnvironmentMap& operator=(const EnvironmentMap&) = delete;

    // Non-movable
    EnvironmentMap(EnvironmentMap&&) = delete;
    EnvironmentMap& operator=(EnvironmentMap&&) = delete;

    /**
     * @brief Get the cubemap texture
     * @return Shared pointer to the cubemap RHIImage
     */
    const Core::Ref<RHI::RHIImage>& GetCubemap() const { return m_Cubemap; }

    /**
     * @brief Get the cubemap image view
     * @return VkImageView handle for the cubemap
     */
    VkImageView GetCubemapView() const;

    /**
     * @brief Get the source equirectangular texture
     * @return Shared pointer to the equirectangular RHIImage
     */
    const Core::Ref<RHI::RHIImage>& GetEquirectangular() const { return m_Equirectangular; }

    /**
     * @brief Get the cubemap face size
     * @return Size in pixels of each cubemap face
     */
    uint32_t GetCubemapSize() const { return m_CubemapSize; }

    /**
     * @brief Get the source file path
     * @return Path to the loaded HDR file
     */
    const std::string& GetFilePath() const { return m_FilePath; }

    /**
     * @brief Get the irradiance map for diffuse IBL
     * @return Shared pointer to the irradiance cubemap RHIImage, or nullptr if not generated
     */
    const Core::Ref<RHI::RHIImage>& GetIrradianceMap() const { return m_IrradianceMap; }

    /**
     * @brief Get the irradiance map image view
     * @return VkImageView handle for the irradiance cubemap, or VK_NULL_HANDLE if not generated
     */
    VkImageView GetIrradianceMapView() const;

    /**
     * @brief Get the irradiance map face size
     * @return Size in pixels of each irradiance cubemap face
     */
    uint32_t GetIrradianceMapSize() const { return m_IrradianceMapSize; }

    /**
     * @brief Check if irradiance map has been generated
     * @return true if irradiance map is available
     */
    bool HasIrradianceMap() const { return m_IrradianceMap != nullptr; }

    /**
     * @brief Get the prefiltered environment map for specular IBL
     * @return Shared pointer to the prefiltered cubemap RHIImage, or nullptr if not generated
     *
     * The prefiltered map stores convolved environment data at different roughness levels.
     * Mip level 0 = roughness 0 (mirror reflection)
     * Higher mip levels = increasing roughness (more diffuse)
     */
    const Core::Ref<RHI::RHIImage>& GetPrefilteredMap() const { return m_PrefilteredMap; }

    /**
     * @brief Get the prefiltered map image view
     * @return VkImageView handle for the prefiltered cubemap, or VK_NULL_HANDLE if not generated
     */
    VkImageView GetPrefilteredMapView() const;

    /**
     * @brief Get the prefiltered map base size
     * @return Size in pixels of the largest mip level (mip 0)
     */
    uint32_t GetPrefilteredMapSize() const { return m_PrefilteredMapSize; }

    /**
     * @brief Get the number of mip levels in the prefiltered map
     * @return Number of mip levels (each corresponds to different roughness)
     */
    uint32_t GetPrefilteredMapMipLevels() const { return m_PrefilteredMipLevels; }

    /**
     * @brief Check if prefiltered map has been generated
     * @return true if prefiltered map is available
     */
    bool HasPrefilteredMap() const { return m_PrefilteredMap != nullptr; }

    /**
     * @brief Get the BRDF LUT for split-sum approximation
     * @return Shared pointer to the BRDF LUT RHIImage, or nullptr if not generated
     *
     * The BRDF LUT is a 2D texture storing pre-integrated BRDF values:
     * - U axis: NdotV (view angle, 0.0 to 1.0)
     * - V axis: roughness (0.0 to 1.0)
     * - R channel: F0 scale factor
     * - G channel: F0 bias factor
     *
     * Used in the split-sum approximation: Specular = Prefiltered * (F0 * scale + bias)
     */
    const Core::Ref<RHI::RHIImage>& GetBRDFLut() const { return m_BRDFLut; }

    /**
     * @brief Get the BRDF LUT image view
     * @return VkImageView handle for the BRDF LUT, or VK_NULL_HANDLE if not generated
     */
    VkImageView GetBRDFLutView() const;

    /**
     * @brief Get the BRDF LUT size
     * @return Size in pixels (square texture)
     */
    uint32_t GetBRDFLutSize() const { return m_BRDFLutSize; }

    /**
     * @brief Check if BRDF LUT has been generated
     * @return true if BRDF LUT is available
     */
    bool HasBRDFLut() const { return m_BRDFLut != nullptr; }

private:
    /**
     * @brief Private constructor - use LoadHDR() factory method
     */
    EnvironmentMap() = default;

    /**
     * @brief Initialize from HDR file
     * @param device The logical device
     * @param filepath Path to HDR file
     * @param cubemapSize Size of cubemap faces
     * @return true on success, false on failure
     */
    bool Initialize(
        const Core::Ref<RHI::RHIDevice>& device,
        const std::string& filepath,
        uint32_t cubemapSize);

    /**
     * @brief Load HDR image data from file
     * @param filepath Path to HDR file
     * @return true on success, false on failure
     */
    bool LoadHDRData(const std::string& filepath);

    /**
     * @brief Create the equirectangular texture from loaded data
     * @param device The logical device
     * @return true on success, false on failure
     */
    bool CreateEquirectangularTexture(const Core::Ref<RHI::RHIDevice>& device);

    /**
     * @brief Create the cubemap texture
     * @param device The logical device
     * @return true on success, false on failure
     */
    bool CreateCubemapTexture(const Core::Ref<RHI::RHIDevice>& device);

    /**
     * @brief Convert equirectangular to cubemap using compute shader
     * @param device The logical device
     * @return true on success, false on failure
     */
    bool ConvertToCubemap(const Core::Ref<RHI::RHIDevice>& device);

    /**
     * @brief Create the irradiance cubemap texture
     * @param device The logical device
     * @return true on success, false on failure
     */
    bool CreateIrradianceMapTexture(const Core::Ref<RHI::RHIDevice>& device);

    /**
     * @brief Generate irradiance map from cubemap using compute shader
     * @param device The logical device
     * @return true on success, false on failure
     */
    bool GenerateIrradianceMap(const Core::Ref<RHI::RHIDevice>& device);

    /**
     * @brief Create the prefiltered cubemap texture with mip levels
     * @param device The logical device
     * @return true on success, false on failure
     */
    bool CreatePrefilteredMapTexture(const Core::Ref<RHI::RHIDevice>& device);

    /**
     * @brief Generate prefiltered map from cubemap using compute shader
     *
     * Processes each mip level with corresponding roughness value.
     * Uses importance sampling with GGX distribution.
     *
     * @param device The logical device
     * @return true on success, false on failure
     */
    bool GeneratePrefilteredMap(const Core::Ref<RHI::RHIDevice>& device);

    /**
     * @brief Create the BRDF LUT texture
     * @param device The logical device
     * @return true on success, false on failure
     */
    bool CreateBRDFLutTexture(const Core::Ref<RHI::RHIDevice>& device);

    /**
     * @brief Generate BRDF LUT using compute shader
     *
     * Pre-integrates the BRDF for the split-sum approximation.
     * Stores F0 scale and bias factors for different NdotV and roughness values.
     *
     * @param device The logical device
     * @return true on success, false on failure
     */
    bool GenerateBRDFLut(const Core::Ref<RHI::RHIDevice>& device);

    // HDR data from file
    std::vector<float> m_HDRData;
    uint32_t m_HDRWidth = 0;
    uint32_t m_HDRHeight = 0;

    // Textures
    Core::Ref<RHI::RHIImage> m_Equirectangular;
    Core::Ref<RHI::RHIImage> m_Cubemap;
    Core::Ref<RHI::RHIImage> m_IrradianceMap;
    Core::Ref<RHI::RHIImage> m_PrefilteredMap;
    Core::Ref<RHI::RHIImage> m_BRDFLut;
    Core::Ref<RHI::RHISampler> m_Sampler;

    // Metadata
    std::string m_FilePath;
    uint32_t m_CubemapSize = 512;
    uint32_t m_IrradianceMapSize = 32;   // Low resolution is sufficient for diffuse IBL
    uint32_t m_PrefilteredMapSize = 128; // Base size for prefiltered map
    uint32_t m_PrefilteredMipLevels = 5; // Mip levels for roughness sampling (0.0 to 1.0)
    uint32_t m_BRDFLutSize = 512;        // 512x512 is sufficient for BRDF LUT
};

} // namespace Resources
