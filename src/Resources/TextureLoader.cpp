/**
 * @file TextureLoader.cpp
 * @brief Implementation of texture loader.
 */

#include "Resources/TextureLoader.h"
#include "RHI/RHIDevice.h"
#include "Core/Assert.h"
#include "Core/Log.h"

// stb_image is already implemented in ModelLoader.cpp via tinygltf
// Just include the header here
#ifdef _MSC_VER
#pragma warning(push, 0)
#elif defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

#include <stb_image.h>

#ifdef _MSC_VER
#pragma warning(pop)
#elif defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

#include <algorithm>
#include <filesystem>
#include <vector>

namespace Resources
{

Core::Ref<Texture> TextureLoader::Load(
    const Core::Ref<RHI::RHIDevice>& device,
    const std::string& filepath,
    const TextureLoadOptions& options)
{
    ASSERT(device != nullptr);

    // Check file exists
    if (!std::filesystem::exists(filepath))
    {
        LOG_ERROR("Texture file not found: {}", filepath);
        return nullptr;
    }

    // Configure stb_image
    stbi_set_flip_vertically_on_load(options.FlipY ? 1 : 0);

    // Load image data
    int width, height, channels;
    unsigned char* pixels = stbi_load(
        filepath.c_str(),
        &width,
        &height,
        &channels,
        STBI_rgb_alpha);  // Force 4 channels for Vulkan compatibility

    if (!pixels)
    {
        LOG_ERROR("Failed to load texture: {} - {}", filepath, stbi_failure_reason());
        return nullptr;
    }

    // Create texture
    auto texture = Core::CreateRef<Texture>();
    texture->Width = static_cast<uint32_t>(width);
    texture->Height = static_cast<uint32_t>(height);
    texture->Channels = 4;  // We forced RGBA
    texture->Path = filepath;

    // Create RHI image
    RHI::ImageDesc imageDesc;
    imageDesc.Width = texture->Width;
    imageDesc.Height = texture->Height;
    imageDesc.Format = options.SRGB ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
    imageDesc.Usage = RHI::ImageUsage::Texture;
    imageDesc.GenerateMipmaps = options.GenerateMipmaps;

    if (options.GenerateMipmaps)
    {
        // Calculate mip levels
        imageDesc.MipLevels = static_cast<uint32_t>(
            std::floor(std::log2(std::max(width, height)))) + 1;
    }

    VkDeviceSize imageSize = static_cast<VkDeviceSize>(width * height * 4);

    texture->Image = RHI::RHIImage::CreateWithData(
        device,
        imageDesc,
        pixels,
        imageSize);

    // Free CPU-side image data
    stbi_image_free(pixels);

    if (!texture->Image)
    {
        LOG_ERROR("Failed to create GPU image for texture: {}", filepath);
        return nullptr;
    }

    LOG_DEBUG("Loaded texture: {} ({}x{}, {} mips)",
              filepath, width, height, imageDesc.MipLevels);

    return texture;
}

Core::Ref<Texture> TextureLoader::CreateSolidColor(
    const Core::Ref<RHI::RHIDevice>& device,
    uint8_t r,
    uint8_t g,
    uint8_t b,
    uint8_t a)
{
    ASSERT(device != nullptr);

    auto texture = Core::CreateRef<Texture>();
    texture->Width = 1;
    texture->Height = 1;
    texture->Channels = 4;

    // Create pixel data
    uint8_t pixels[4] = {r, g, b, a};

    // Create RHI image
    RHI::ImageDesc imageDesc;
    imageDesc.Width = 1;
    imageDesc.Height = 1;
    imageDesc.Format = VK_FORMAT_R8G8B8A8_SRGB;
    imageDesc.Usage = RHI::ImageUsage::Texture;
    imageDesc.MipLevels = 1;

    texture->Image = RHI::RHIImage::CreateWithData(
        device,
        imageDesc,
        pixels,
        4);

    if (!texture->Image)
    {
        LOG_ERROR("Failed to create solid color texture");
        return nullptr;
    }

    return texture;
}

Core::Ref<Texture> TextureLoader::CreateWhite(const Core::Ref<RHI::RHIDevice>& device)
{
    return CreateSolidColor(device, 255, 255, 255, 255);
}

Core::Ref<Texture> TextureLoader::CreateBlack(const Core::Ref<RHI::RHIDevice>& device)
{
    return CreateSolidColor(device, 0, 0, 0, 255);
}

Core::Ref<Texture> TextureLoader::CreateDefaultNormal(const Core::Ref<RHI::RHIDevice>& device)
{
    ASSERT(device != nullptr);

    auto texture = Core::CreateRef<Texture>();
    texture->Width = 1;
    texture->Height = 1;
    texture->Channels = 4;

    // Normal pointing up (0, 0, 1) encoded as (0.5, 0.5, 1.0)
    // In 8-bit: (128, 128, 255, 255)
    uint8_t pixels[4] = {128, 128, 255, 255};

    // Create RHI image (linear format for normal maps)
    RHI::ImageDesc imageDesc;
    imageDesc.Width = 1;
    imageDesc.Height = 1;
    imageDesc.Format = VK_FORMAT_R8G8B8A8_UNORM;  // Linear for normal data
    imageDesc.Usage = RHI::ImageUsage::Texture;
    imageDesc.MipLevels = 1;

    texture->Image = RHI::RHIImage::CreateWithData(
        device,
        imageDesc,
        pixels,
        4);

    if (!texture->Image)
    {
        LOG_ERROR("Failed to create default normal texture");
        return nullptr;
    }

    return texture;
}

Core::Ref<Texture> TextureLoader::CreatePlaceholder(
    const Core::Ref<RHI::RHIDevice>& device,
    uint32_t size,
    uint32_t color1,
    uint32_t color2)
{
    ASSERT(device != nullptr);
    ASSERT(size > 0);

    // Make sure size is power of 2 for nice checkerboard
    if ((size & (size - 1)) != 0) {
        // Round up to next power of 2
        uint32_t originalSize = size;
        size = 1;
        while (size < originalSize) size <<= 1;
    }

    auto texture = Core::CreateRef<Texture>();
    texture->Width = size;
    texture->Height = size;
    texture->Channels = 4;
    texture->Path = "[placeholder]";

    // Create checkerboard pattern
    std::vector<uint8_t> pixels(size * size * 4);
    uint32_t checkSize = size / 8;  // 8x8 checkerboard
    if (checkSize == 0) checkSize = 1;

    for (uint32_t y = 0; y < size; ++y) {
        for (uint32_t x = 0; x < size; ++x) {
            uint32_t index = (y * size + x) * 4;
            bool isColor1 = ((x / checkSize) + (y / checkSize)) % 2 == 0;
            uint32_t color = isColor1 ? color1 : color2;

            // Colors are in RGBA format (low byte = R, high byte = A)
            pixels[index + 0] = static_cast<uint8_t>((color >> 24) & 0xFF); // R
            pixels[index + 1] = static_cast<uint8_t>((color >> 16) & 0xFF); // G
            pixels[index + 2] = static_cast<uint8_t>((color >> 8) & 0xFF);  // B
            pixels[index + 3] = static_cast<uint8_t>(color & 0xFF);         // A
        }
    }

    // Create RHI image
    RHI::ImageDesc imageDesc;
    imageDesc.Width = size;
    imageDesc.Height = size;
    imageDesc.Format = VK_FORMAT_R8G8B8A8_SRGB;
    imageDesc.Usage = RHI::ImageUsage::Texture;
    imageDesc.MipLevels = 1;

    texture->Image = RHI::RHIImage::CreateWithData(
        device,
        imageDesc,
        pixels.data(),
        static_cast<VkDeviceSize>(pixels.size()));

    if (!texture->Image) {
        LOG_ERROR("Failed to create placeholder texture");
        return nullptr;
    }

    LOG_DEBUG("Created placeholder texture ({}x{})", size, size);
    return texture;
}

bool TextureLoader::IsFormatSupported(const std::string& extension)
{
    std::string ext = extension;
    std::transform(
        ext.begin(),
        ext.end(),
        ext.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    return ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
           ext == ".bmp" || ext == ".tga" || ext == ".hdr" ||
           ext == ".psd" || ext == ".gif";
}

std::vector<std::string> TextureLoader::GetSupportedExtensions()
{
    return {".png", ".jpg", ".jpeg", ".bmp", ".tga", ".hdr", ".psd", ".gif"};
}

} // namespace Resources
