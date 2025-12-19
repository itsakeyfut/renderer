/**
 * @file RHISampler.cpp
 * @brief Implementation of the Vulkan Sampler wrapper.
 */

#include "RHI/RHISampler.h"
#include "RHI/RHIDevice.h"
#include "Core/Assert.h"
#include "Core/Log.h"

namespace RHI
{
    Core::Ref<RHISampler> RHISampler::Create(
        const Core::Ref<RHIDevice>& device,
        const SamplerDesc& desc)
    {
        ASSERT(device != nullptr);

        auto sampler = Core::Ref<RHISampler>(new RHISampler());
        if (!sampler->Initialize(device, desc))
        {
            LOG_ERROR("Failed to create RHISampler");
            return nullptr;
        }

        if (desc.DebugName)
        {
            LOG_DEBUG("Created sampler '{}'", desc.DebugName);
        }

        return sampler;
    }

    Core::Ref<RHISampler> RHISampler::CreateLinear(const Core::Ref<RHIDevice>& device)
    {
        SamplerDesc desc;
        desc.MagFilter = FilterMode::Linear;
        desc.MinFilter = FilterMode::Linear;
        desc.MipmapMode = FilterMode::Linear;
        desc.AnisotropyEnable = true;
        desc.MaxAnisotropy = 16.0f;
        desc.DebugName = "LinearSampler";
        return Create(device, desc);
    }

    Core::Ref<RHISampler> RHISampler::CreateNearest(const Core::Ref<RHIDevice>& device)
    {
        SamplerDesc desc;
        desc.MagFilter = FilterMode::Nearest;
        desc.MinFilter = FilterMode::Nearest;
        desc.MipmapMode = FilterMode::Nearest;
        desc.AnisotropyEnable = false;
        desc.DebugName = "NearestSampler";
        return Create(device, desc);
    }

    Core::Ref<RHISampler> RHISampler::CreateShadow(const Core::Ref<RHIDevice>& device)
    {
        SamplerDesc desc;
        desc.MagFilter = FilterMode::Linear;
        desc.MinFilter = FilterMode::Linear;
        desc.MipmapMode = FilterMode::Nearest;
        desc.AddressModeU = AddressMode::ClampToBorder;
        desc.AddressModeV = AddressMode::ClampToBorder;
        desc.AddressModeW = AddressMode::ClampToBorder;
        desc.Border = BorderColor::OpaqueWhite;
        desc.AnisotropyEnable = false;
        desc.CompareEnable = true;
        desc.CompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        desc.DebugName = "ShadowSampler";
        return Create(device, desc);
    }

    RHISampler::~RHISampler()
    {
        if (m_Sampler != VK_NULL_HANDLE && m_Device != VK_NULL_HANDLE)
        {
            vkDestroySampler(m_Device, m_Sampler, nullptr);
            m_Sampler = VK_NULL_HANDLE;
        }
    }

    bool RHISampler::Initialize(
        const Core::Ref<RHIDevice>& device,
        const SamplerDesc& desc)
    {
        m_Device = device->GetHandle();

        // Get device limits for anisotropy
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(device->GetPhysicalDevice(), &properties);

        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = ToVkFilter(desc.MagFilter);
        samplerInfo.minFilter = ToVkFilter(desc.MinFilter);
        samplerInfo.mipmapMode = ToVkMipmapMode(desc.MipmapMode);
        samplerInfo.addressModeU = ToVkAddressMode(desc.AddressModeU);
        samplerInfo.addressModeV = ToVkAddressMode(desc.AddressModeV);
        samplerInfo.addressModeW = ToVkAddressMode(desc.AddressModeW);
        samplerInfo.mipLodBias = desc.MipLodBias;
        samplerInfo.anisotropyEnable = desc.AnisotropyEnable ? VK_TRUE : VK_FALSE;
        samplerInfo.maxAnisotropy = desc.AnisotropyEnable ?
            std::min(desc.MaxAnisotropy, properties.limits.maxSamplerAnisotropy) : 1.0f;
        samplerInfo.compareEnable = desc.CompareEnable ? VK_TRUE : VK_FALSE;
        samplerInfo.compareOp = desc.CompareOp;
        samplerInfo.minLod = desc.MinLod;
        samplerInfo.maxLod = desc.MaxLod;
        samplerInfo.borderColor = ToVkBorderColor(desc.Border);
        samplerInfo.unnormalizedCoordinates = VK_FALSE;

        VkResult result = vkCreateSampler(m_Device, &samplerInfo, nullptr, &m_Sampler);
        if (result != VK_SUCCESS)
        {
            LOG_ERROR("Failed to create sampler: VkResult {}", static_cast<int>(result));
            return false;
        }

        return true;
    }

    VkFilter RHISampler::ToVkFilter(FilterMode mode)
    {
        switch (mode)
        {
        case FilterMode::Nearest:
            return VK_FILTER_NEAREST;
        case FilterMode::Linear:
            return VK_FILTER_LINEAR;
        case FilterMode::Cubic:
            // VK_FILTER_CUBIC_EXT requires VK_EXT_filter_cubic extension
            // Fall back to Linear until extension support is implemented
            LOG_WARN("Cubic filter requested but VK_EXT_filter_cubic extension "
                     "support not implemented, falling back to Linear");
            return VK_FILTER_LINEAR;
        default:
            return VK_FILTER_LINEAR;
        }
    }

    VkSamplerMipmapMode RHISampler::ToVkMipmapMode(FilterMode mode)
    {
        switch (mode)
        {
        case FilterMode::Nearest:
            return VK_SAMPLER_MIPMAP_MODE_NEAREST;
        case FilterMode::Linear:
        case FilterMode::Cubic:
            return VK_SAMPLER_MIPMAP_MODE_LINEAR;
        default:
            return VK_SAMPLER_MIPMAP_MODE_LINEAR;
        }
    }

    VkSamplerAddressMode RHISampler::ToVkAddressMode(AddressMode mode)
    {
        switch (mode)
        {
        case AddressMode::Repeat:
            return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case AddressMode::MirroredRepeat:
            return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case AddressMode::ClampToEdge:
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case AddressMode::ClampToBorder:
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        case AddressMode::MirrorClampToEdge:
            return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
        default:
            return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        }
    }

    VkBorderColor RHISampler::ToVkBorderColor(BorderColor color)
    {
        switch (color)
        {
        case BorderColor::TransparentBlack:
            return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
        case BorderColor::OpaqueBlack:
            return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
        case BorderColor::OpaqueWhite:
            return VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        default:
            return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
        }
    }

} // namespace RHI
