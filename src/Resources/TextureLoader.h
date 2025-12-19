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
 * @brief CPU-side texture data loaded from file.
 *
 * Contains raw pixel data that can be uploaded to the GPU later.
 * Used for async loading where file I/O happens on worker threads
 * but GPU operations must happen on the main thread.
 */
struct TextureData
{
    std::vector<uint8_t> Pixels;  ///< Raw RGBA pixel data
    uint32_t Width = 0;           ///< Texture width in pixels
    uint32_t Height = 0;          ///< Texture height in pixels
    uint32_t Channels = 0;        ///< Number of channels (always 4 for RGBA)
    uint32_t MipLevels = 1;       ///< Number of mipmap levels to generate
    VkFormat Format = VK_FORMAT_R8G8B8A8_SRGB;  ///< Vulkan format
    std::string Path;             ///< Source file path

    /**
     * @brief Check if the texture data is valid.
     * @return true if pixel data was loaded successfully.
     */
    bool IsValid() const { return !Pixels.empty() && Width > 0 && Height > 0; }
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
     * Loads the image file and creates GPU resources. This performs GPU
     * operations and should only be called from the main/render thread.
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
     * @brief Load texture data from file (CPU-only, no GPU operations).
     *
     * Loads the image file into CPU memory without creating GPU resources.
     * This is safe to call from any thread. Use CreateFromData() on the
     * main thread to upload the data to the GPU.
     *
     * @param filepath Path to the image file.
     * @param options Loading options.
     * @return TextureData containing the raw pixel data, or invalid if failed.
     */
    static TextureData LoadCPU(
        const std::string& filepath,
        const TextureLoadOptions& options = {});

    /**
     * @brief Create a GPU texture from CPU-loaded texture data.
     *
     * Uploads the pixel data to the GPU. This performs GPU operations
     * and should only be called from the main/render thread.
     *
     * @param device The logical device.
     * @param data CPU-loaded texture data from LoadCPU().
     * @return Shared pointer to the created texture, or nullptr on failure.
     */
    static Core::Ref<Texture> CreateFromData(
        const Core::Ref<RHI::RHIDevice>& device,
        const TextureData& data);

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
     * @brief Create a placeholder texture for async loading.
     *
     * Creates a checkerboard pattern texture to indicate that
     * the actual texture is still being loaded.
     *
     * @param device The logical device.
     * @param size Size of the checkerboard (width and height).
     * @param color1 First checkerboard color (default: magenta).
     * @param color2 Second checkerboard color (default: black).
     * @return Shared pointer to the created texture.
     */
    static Core::Ref<Texture> CreatePlaceholder(
        const Core::Ref<RHI::RHIDevice>& device,
        uint32_t size = 64,
        uint32_t color1 = 0xFF00FFFF,  // Magenta
        uint32_t color2 = 0x000000FF); // Black

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
