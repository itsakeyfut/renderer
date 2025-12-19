/**
 * @file TextureLoader.h
 * @brief Texture loading interface.
 *
 * Provides functionality to load textures from image files and create
 * GPU-resident RHIImage resources.
 */

#pragma once

#include "Core/Types.h"
#include "RHI/RHIImage.h"

#include <string>
#include <unordered_map>

namespace RHI
{
    class RHIDevice;
    class RHISampler;
} // namespace RHI

namespace Resources
{

/**
 * @brief Configuration for texture loading.
 */
struct TextureLoadOptions
{
    /**
     * @brief Generate mipmaps for the texture.
     */
    bool GenerateMipmaps = true;

    /**
     * @brief Force sRGB format for color textures.
     * Set to false for non-color data (normal maps, roughness).
     */
    bool SRGB = true;

    /**
     * @brief Flip Y coordinate when loading.
     * Some formats have origin at bottom-left.
     */
    bool FlipY = false;
};

/**
 * @brief Loaded texture resource.
 */
struct Texture
{
    Core::Ref<RHI::RHIImage> Image;
    uint32_t Width = 0;
    uint32_t Height = 0;
    uint32_t Channels = 0;
    std::string Path;
};

/**
 * @brief Static class for loading textures from various formats.
 *
 * Supports common image formats: PNG, JPG, BMP, TGA, HDR.
 *
 * Usage:
 * @code
 * TextureLoadOptions options;
 * options.GenerateMipmaps = true;
 *
 * auto texture = TextureLoader::Load(device, "textures/albedo.png", options);
 * if (texture) {
 *     // Use texture->Image in descriptor set
 * }
 * @endcode
 */
class TextureLoader
{
public:
    TextureLoader() = delete;
    ~TextureLoader() = delete;

    /**
     * @brief Load a texture from file.
     *
     * @param device The logical device.
     * @param filepath Path to the image file.
     * @param options Loading options.
     * @return Shared pointer to the loaded texture, or nullptr on failure.
     */
    static Core::Ref<Texture> Load(
        const Core::Ref<RHI::RHIDevice>& device,
        const std::string& filepath,
        const TextureLoadOptions& options = {});

    /**
     * @brief Create a 1x1 solid color texture.
     *
     * Useful for default/fallback textures.
     *
     * @param device The logical device.
     * @param r Red component (0-255).
     * @param g Green component (0-255).
     * @param b Blue component (0-255).
     * @param a Alpha component (0-255).
     * @return Shared pointer to the created texture.
     */
    static Core::Ref<Texture> CreateSolidColor(
        const Core::Ref<RHI::RHIDevice>& device,
        uint8_t r,
        uint8_t g,
        uint8_t b,
        uint8_t a = 255);

    /**
     * @brief Create a default white texture.
     * @param device The logical device.
     * @return Shared pointer to the created texture.
     */
    static Core::Ref<Texture> CreateWhite(const Core::Ref<RHI::RHIDevice>& device);

    /**
     * @brief Create a default black texture.
     * @param device The logical device.
     * @return Shared pointer to the created texture.
     */
    static Core::Ref<Texture> CreateBlack(const Core::Ref<RHI::RHIDevice>& device);

    /**
     * @brief Create a default normal map texture (flat surface).
     * @param device The logical device.
     * @return Shared pointer to the created texture.
     */
    static Core::Ref<Texture> CreateDefaultNormal(const Core::Ref<RHI::RHIDevice>& device);

    /**
     * @brief Check if a file extension is supported.
     *
     * @param extension File extension (e.g., ".png", ".jpg").
     * @return true if the format is supported.
     */
    static bool IsFormatSupported(const std::string& extension);

    /**
     * @brief Get a list of supported file extensions.
     *
     * @return Vector of supported extensions.
     */
    static std::vector<std::string> GetSupportedExtensions();
};

} // namespace Resources
