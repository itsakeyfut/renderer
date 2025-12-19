/**
 * @file RHITexture.cpp
 * @brief Implementation of the high-level texture wrapper.
 */

#include "RHI/RHITexture.h"
#include "RHI/RHIBuffer.h"
#include "RHI/RHICommandPool.h"
#include "RHI/RHICommandBuffer.h"
#include "Core/Assert.h"
#include "Core/Log.h"

// stb_image implementation is in stb_impl.cpp
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
#include <cmath>
#include <cstring>
#include <filesystem>

namespace RHI
{

Core::Ref<RHITexture> RHITexture::Create(
    const Core::Ref<RHIDevice>& device,
    const TextureDesc& desc)
{
    ASSERT(device != nullptr);
    ASSERT(desc.Width > 0 && desc.Height > 0);

    auto texture = Core::Ref<RHITexture>(new RHITexture());
    if (!texture->Initialize(device, desc))
    {
        LOG_ERROR("Failed to create RHITexture");
        return nullptr;
    }

    if (desc.DebugName)
    {
        LOG_DEBUG("Created texture '{}': {}x{}, format={}",
                  desc.DebugName, desc.Width, desc.Height, static_cast<int>(desc.Format));
    }

    return texture;
}

Core::Ref<RHITexture> RHITexture::LoadFromFile(
    const Core::Ref<RHIDevice>& device,
    const std::string& filepath,
    bool sRGB,
    bool generateMips)
{
    ASSERT(device != nullptr);

    // Check if file exists
    if (!std::filesystem::exists(filepath))
    {
        LOG_ERROR("Texture file not found: {}", filepath);
        return nullptr;
    }

    // Load image with stb_image
    int width, height, channels;
    stbi_set_flip_vertically_on_load_thread(0);  // Vulkan uses top-left origin (thread-safe)

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

    // Calculate image size
    VkDeviceSize imageSize = static_cast<VkDeviceSize>(width) * height * 4;

    // Calculate mip levels if requested
    uint32_t mipLevels = 1;
    if (generateMips)
    {
        mipLevels = static_cast<uint32_t>(
            std::floor(std::log2(std::max(width, height)))) + 1;
    }

    // Create texture description
    TextureDesc desc;
    desc.Width = static_cast<uint32_t>(width);
    desc.Height = static_cast<uint32_t>(height);
    desc.MipLevels = mipLevels;
    desc.Format = sRGB ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
    desc.Usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    desc.GenerateMips = generateMips;

    // Add transfer source if generating mipmaps
    if (generateMips && mipLevels > 1)
    {
        desc.Usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }

    // Create texture
    auto texture = Core::Ref<RHITexture>(new RHITexture());
    texture->m_FilePath = filepath;
    texture->m_Desc = desc;

    if (!texture->Initialize(device, desc))
    {
        stbi_image_free(pixels);
        LOG_ERROR("Failed to create texture from file: {}", filepath);
        return nullptr;
    }

    // Upload pixel data
    if (!texture->UploadData(device, pixels, imageSize))
    {
        stbi_image_free(pixels);
        LOG_ERROR("Failed to upload texture data: {}", filepath);
        return nullptr;
    }

    // Free stb_image's allocation
    stbi_image_free(pixels);

    // Generate mipmaps if requested
    if (generateMips && mipLevels > 1)
    {
        if (!texture->GenerateMipmaps(device))
        {
            LOG_WARN("Failed to generate mipmaps for texture: {}", filepath);
            // Texture is still usable, just without mipmaps
        }
    }

    LOG_DEBUG("Loaded texture from file: {} ({}x{}, {} mips, format={})",
              filepath, width, height, mipLevels, static_cast<int>(desc.Format));

    return texture;
}

RHITexture::~RHITexture()
{
    // RHIImage handles cleanup in its destructor
    m_Image.reset();
}

bool RHITexture::Initialize(
    const Core::Ref<RHIDevice>& device,
    const TextureDesc& desc)
{
    m_Desc = desc;

    // Convert TextureDesc to ImageDesc
    ImageDesc imageDesc;
    imageDesc.Width = desc.Width;
    imageDesc.Height = desc.Height;
    imageDesc.MipLevels = desc.MipLevels;
    imageDesc.Format = desc.Format;
    imageDesc.Usage = ImageUsage::Texture;  // RHIImage uses high-level enum
    imageDesc.GenerateMipmaps = desc.GenerateMips;
    imageDesc.DebugName = desc.DebugName;

    // Create the underlying RHIImage
    m_Image = RHIImage::Create(device, imageDesc);
    if (!m_Image)
    {
        return false;
    }

    return true;
}

bool RHITexture::UploadData(
    const Core::Ref<RHIDevice>& device,
    const void* data,
    VkDeviceSize dataSize)
{
    ASSERT(m_Image != nullptr);
    return m_Image->UploadData(device, data, dataSize);
}

void RHITexture::TransitionLayout(
    VkCommandBuffer cmd,
    VkImageLayout oldLayout,
    VkImageLayout newLayout,
    VkPipelineStageFlags srcStage,
    VkPipelineStageFlags dstStage)
{
    ASSERT(m_Image != nullptr);
    ASSERT(cmd != VK_NULL_HANDLE);

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_Image->GetHandle();
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = m_Image->GetMipLevels();
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    // Determine access masks based on layouts
    barrier.srcAccessMask = GetAccessMask(oldLayout);
    barrier.dstAccessMask = GetAccessMask(newLayout);

    vkCmdPipelineBarrier(
        cmd,
        srcStage,
        dstStage,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier);

    // Update the tracked layout state
    m_Image->SetLayout(newLayout);
}

bool RHITexture::GenerateMipmaps(const Core::Ref<RHIDevice>& device)
{
    ASSERT(m_Image != nullptr);
    return m_Image->GenerateMipmaps(device);
}

VkImage RHITexture::GetImage() const
{
    return m_Image ? m_Image->GetHandle() : VK_NULL_HANDLE;
}

VkImageView RHITexture::GetImageView() const
{
    return m_Image ? m_Image->GetImageView() : VK_NULL_HANDLE;
}

VkFormat RHITexture::GetFormat() const
{
    return m_Image ? m_Image->GetFormat() : VK_FORMAT_UNDEFINED;
}

uint32_t RHITexture::GetWidth() const
{
    return m_Image ? m_Image->GetWidth() : 0;
}

uint32_t RHITexture::GetHeight() const
{
    return m_Image ? m_Image->GetHeight() : 0;
}

uint32_t RHITexture::GetMipLevels() const
{
    return m_Image ? m_Image->GetMipLevels() : 0;
}

VkImageLayout RHITexture::GetLayout() const
{
    return m_Image ? m_Image->GetLayout() : VK_IMAGE_LAYOUT_UNDEFINED;
}

VkAccessFlags RHITexture::GetAccessMask(VkImageLayout layout)
{
    switch (layout)
    {
    case VK_IMAGE_LAYOUT_UNDEFINED:
        return 0;

    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        return VK_ACCESS_TRANSFER_WRITE_BIT;

    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        return VK_ACCESS_TRANSFER_READ_BIT;

    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        return VK_ACCESS_SHADER_READ_BIT;

    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
        return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
               VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
        return 0;

    case VK_IMAGE_LAYOUT_GENERAL:
        return VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;

    default:
        LOG_WARN("Unknown image layout for access mask: {}", static_cast<int>(layout));
        return VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    }
}

} // namespace RHI
