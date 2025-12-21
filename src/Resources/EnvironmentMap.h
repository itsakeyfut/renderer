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
 * }
 * @endcode
 *
 * Future extensions (not yet implemented):
 * - Pre-filtered environment map for specular IBL
 * - BRDF LUT generation
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

    // HDR data from file
    std::vector<float> m_HDRData;
    uint32_t m_HDRWidth = 0;
    uint32_t m_HDRHeight = 0;

    // Textures
    Core::Ref<RHI::RHIImage> m_Equirectangular;
    Core::Ref<RHI::RHIImage> m_Cubemap;
    Core::Ref<RHI::RHIImage> m_IrradianceMap;
    Core::Ref<RHI::RHISampler> m_Sampler;

    // Metadata
    std::string m_FilePath;
    uint32_t m_CubemapSize = 512;
    uint32_t m_IrradianceMapSize = 32;  // Low resolution is sufficient for diffuse IBL
};

} // namespace Resources
