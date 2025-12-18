/**
 * @file Texture.h
 * @brief Texture resource placeholder.
 *
 * This is a stub implementation. Full GPU texture support will be added
 * in a separate issue when the RHI layer texture support is implemented.
 */

#pragma once

#include "Core/Types.h"
#include <string>
#include <cstdint>

namespace Resources {

/**
 * @brief Texture format enumeration.
 */
enum class TextureFormat : uint8_t {
    Unknown = 0,
    RGBA8_UNORM,
    RGBA8_SRGB,
    BGRA8_UNORM,
    BGRA8_SRGB,
    R8_UNORM,
    RG8_UNORM,
    RGBA16_FLOAT,
    RGBA32_FLOAT,
    BC1_UNORM,  // DXT1
    BC3_UNORM,  // DXT5
    BC5_UNORM,  // Normal maps
    BC7_UNORM,  // High quality
};

/**
 * @brief Texture type enumeration.
 */
enum class TextureType : uint8_t {
    Texture2D,
    TextureCube,
    Texture2DArray,
};

/**
 * @brief Texture resource class.
 *
 * Represents a GPU texture resource. Currently a stub implementation
 * that stores metadata. Full GPU support will be added when integrating
 * with the RHI layer.
 */
class Texture {
public:
    /**
     * @brief Constructs a texture with the given path.
     * @param path File path this texture was loaded from.
     */
    explicit Texture(const std::string& path)
        : m_Path(path)
    {
    }

    /**
     * @brief Constructs a texture with full metadata.
     * @param path File path.
     * @param width Width in pixels.
     * @param height Height in pixels.
     * @param format Pixel format.
     */
    Texture(const std::string& path, uint32_t width, uint32_t height, TextureFormat format)
        : m_Path(path)
        , m_Width(width)
        , m_Height(height)
        , m_Format(format)
    {
    }

    /**
     * @brief Gets the file path.
     * @return Path this texture was loaded from.
     */
    const std::string& GetPath() const { return m_Path; }

    /**
     * @brief Gets the texture width.
     * @return Width in pixels.
     */
    uint32_t GetWidth() const { return m_Width; }

    /**
     * @brief Gets the texture height.
     * @return Height in pixels.
     */
    uint32_t GetHeight() const { return m_Height; }

    /**
     * @brief Gets the mip level count.
     * @return Number of mip levels.
     */
    uint32_t GetMipLevels() const { return m_MipLevels; }

    /**
     * @brief Gets the pixel format.
     * @return TextureFormat enum value.
     */
    TextureFormat GetFormat() const { return m_Format; }

    /**
     * @brief Gets the texture type.
     * @return TextureType enum value.
     */
    TextureType GetType() const { return m_Type; }

    /**
     * @brief Checks if the texture has mipmaps.
     * @return true if mip levels > 1.
     */
    bool HasMipmaps() const { return m_MipLevels > 1; }

    /**
     * @brief Estimates memory usage of this texture.
     * @return Approximate memory usage in bytes.
     */
    size_t GetMemorySize() const
    {
        size_t bytesPerPixel = GetBytesPerPixel(m_Format);
        size_t baseSize = static_cast<size_t>(m_Width) * m_Height * bytesPerPixel;

        // Approximate mipmap chain size (sum of geometric series)
        if (m_MipLevels > 1) {
            return static_cast<size_t>(baseSize * 1.33f);
        }
        return baseSize;
    }

    /**
     * @brief Sets texture dimensions (for loading).
     * @param width Width in pixels.
     * @param height Height in pixels.
     */
    void SetDimensions(uint32_t width, uint32_t height)
    {
        m_Width = width;
        m_Height = height;
    }

    /**
     * @brief Sets mip level count.
     * @param levels Number of mip levels.
     */
    void SetMipLevels(uint32_t levels) { m_MipLevels = levels; }

    /**
     * @brief Sets the pixel format.
     * @param format TextureFormat value.
     */
    void SetFormat(TextureFormat format) { m_Format = format; }

    /**
     * @brief Sets the texture type.
     * @param type TextureType value.
     */
    void SetType(TextureType type) { m_Type = type; }

private:
    static size_t GetBytesPerPixel(TextureFormat format)
    {
        switch (format) {
            case TextureFormat::R8_UNORM:
                return 1;
            case TextureFormat::RG8_UNORM:
                return 2;
            case TextureFormat::RGBA8_UNORM:
            case TextureFormat::RGBA8_SRGB:
            case TextureFormat::BGRA8_UNORM:
            case TextureFormat::BGRA8_SRGB:
                return 4;
            case TextureFormat::RGBA16_FLOAT:
                return 8;
            case TextureFormat::RGBA32_FLOAT:
                return 16;
            case TextureFormat::BC1_UNORM:
                return 1;  // 0.5 bytes per pixel, rounded up
            case TextureFormat::BC3_UNORM:
            case TextureFormat::BC5_UNORM:
            case TextureFormat::BC7_UNORM:
                return 1;  // Block compressed
            default:
                return 4;
        }
    }

    std::string m_Path;
    uint32_t m_Width = 0;
    uint32_t m_Height = 0;
    uint32_t m_MipLevels = 1;
    TextureFormat m_Format = TextureFormat::RGBA8_SRGB;
    TextureType m_Type = TextureType::Texture2D;
};

} // namespace Resources
