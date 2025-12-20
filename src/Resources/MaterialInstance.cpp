/**
 * @file MaterialInstance.cpp
 * @brief Implementation of MaterialInstance class.
 */

#include "Resources/MaterialInstance.h"

#include "Core/Log.h"
#include "RHI/RHIBuffer.h"
#include "RHI/RHIDescriptorPool.h"
#include "RHI/RHIDescriptorSet.h"
#include "RHI/RHIDescriptorSetLayout.h"
#include "RHI/RHIDevice.h"
#include "RHI/RHISampler.h"
#include "RHI/RHITexture.h"

#include <algorithm>
#include <cstring>

namespace Resources {

// ============================================================================
// Static Factory Methods
// ============================================================================

Core::Ref<MaterialInstance> MaterialInstance::Create(
    const Core::Ref<RHI::RHIDevice>& device,
    const Core::Ref<RHI::RHIDescriptorPool>& pool,
    const Core::Ref<RHI::RHIDescriptorSetLayout>& layout,
    const Core::Ref<RHI::RHISampler>& sampler,
    const Core::Ref<RHI::RHITexture>& defaultTexture)
{
    auto instance = Core::Ref<MaterialInstance>(new MaterialInstance());
    if (!instance->Initialize(device, pool, layout, sampler, defaultTexture))
    {
        return nullptr;
    }
    return instance;
}

Core::Ref<RHI::RHIDescriptorSetLayout> MaterialInstance::CreateDescriptorSetLayout(
    const Core::Ref<RHI::RHIDevice>& device)
{
    if (!device)
    {
        LOG_ERROR("Cannot create material descriptor set layout: device is null");
        return nullptr;
    }

    // Define bindings for material descriptor set
    // Layout:
    //   binding 0: Material UBO (uniform buffer)
    //   binding 1: Base color texture (combined image sampler)
    //   binding 2: Normal texture (combined image sampler)
    //   binding 3: Metallic-roughness texture (combined image sampler)
    //   binding 4: Occlusion texture (combined image sampler)
    //   binding 5: Emissive texture (combined image sampler)
    std::vector<RHI::DescriptorBinding> bindings = {
        // Material UBO
        {
            0,                                          // Binding
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,          // Type
            1,                                          // Count
            VK_SHADER_STAGE_FRAGMENT_BIT                // Stages
        },
        // Base color texture
        {
            1,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            1,
            VK_SHADER_STAGE_FRAGMENT_BIT
        },
        // Normal texture
        {
            2,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            1,
            VK_SHADER_STAGE_FRAGMENT_BIT
        },
        // Metallic-roughness texture
        {
            3,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            1,
            VK_SHADER_STAGE_FRAGMENT_BIT
        },
        // Occlusion texture
        {
            4,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            1,
            VK_SHADER_STAGE_FRAGMENT_BIT
        },
        // Emissive texture
        {
            5,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            1,
            VK_SHADER_STAGE_FRAGMENT_BIT
        }
    };

    auto layout = RHI::RHIDescriptorSetLayout::Create(device, bindings);
    if (!layout)
    {
        LOG_ERROR("Failed to create material descriptor set layout");
        return nullptr;
    }

    LOG_DEBUG("Created material descriptor set layout with {} bindings", bindings.size());
    return layout;
}

// ============================================================================
// Initialization
// ============================================================================

bool MaterialInstance::Initialize(
    const Core::Ref<RHI::RHIDevice>& device,
    const Core::Ref<RHI::RHIDescriptorPool>& pool,
    const Core::Ref<RHI::RHIDescriptorSetLayout>& layout,
    const Core::Ref<RHI::RHISampler>& sampler,
    const Core::Ref<RHI::RHITexture>& defaultTexture)
{
    if (!device)
    {
        LOG_ERROR("Cannot create MaterialInstance: device is null");
        return false;
    }

    if (!pool)
    {
        LOG_ERROR("Cannot create MaterialInstance: descriptor pool is null");
        return false;
    }

    if (!layout)
    {
        LOG_ERROR("Cannot create MaterialInstance: descriptor layout is null");
        return false;
    }

    if (!sampler)
    {
        LOG_ERROR("Cannot create MaterialInstance: sampler is null");
        return false;
    }

    if (!defaultTexture)
    {
        LOG_ERROR("Cannot create MaterialInstance: default texture is null");
        return false;
    }

    m_Device = device;
    m_Sampler = sampler;
    m_DefaultTexture = defaultTexture;

    // Initialize all texture slots to default
    for (auto& tex : m_Textures)
    {
        tex = nullptr;
    }

    // Create uniform buffer for material data
    RHI::BufferDesc bufferDesc;
    bufferDesc.Size = sizeof(MaterialGPUData);
    bufferDesc.Usage = RHI::BufferUsage::Uniform;
    bufferDesc.Memory = RHI::MemoryUsage::CpuToGpu;
    bufferDesc.DebugName = "MaterialInstance UBO";

    m_UniformBuffer = RHI::RHIBuffer::Create(device, bufferDesc);
    if (!m_UniformBuffer)
    {
        LOG_ERROR("Failed to create material uniform buffer");
        return false;
    }

    // Create descriptor set
    m_DescriptorSet = RHI::RHIDescriptorSet::Create(device, pool, layout);
    if (!m_DescriptorSet)
    {
        LOG_ERROR("Failed to create material descriptor set");
        return false;
    }

    // Initial upload and descriptor update
    UploadToGPU();
    UpdateDescriptorSet();

    LOG_DEBUG("Created MaterialInstance");
    return true;
}

// ============================================================================
// Texture Management
// ============================================================================

void MaterialInstance::SetTexture(TextureSlot slot, const Core::Ref<RHI::RHITexture>& texture)
{
    if (slot >= TextureSlot::Count)
    {
        LOG_WARN("Invalid texture slot: {}", static_cast<uint32_t>(slot));
        return;
    }

    size_t index = static_cast<size_t>(slot);
    m_Textures[index] = texture;

    // Update GPU data texture presence flags
    switch (slot)
    {
        case TextureSlot::BaseColor:
            m_MaterialData.HasBaseColorTexture = texture ? 1 : 0;
            break;
        case TextureSlot::Normal:
            m_MaterialData.HasNormalTexture = texture ? 1 : 0;
            break;
        case TextureSlot::MetallicRoughness:
            m_MaterialData.HasMetallicRoughnessTexture = texture ? 1 : 0;
            break;
        case TextureSlot::Occlusion:
            m_MaterialData.HasOcclusionTexture = texture ? 1 : 0;
            break;
        case TextureSlot::Emissive:
            m_MaterialData.HasEmissiveTexture = texture ? 1 : 0;
            break;
        default:
            break;
    }

    MarkDirty();
    UpdateDescriptorSet();
}

bool MaterialInstance::SetTexture(const std::string& name, const Core::Ref<RHI::RHITexture>& texture)
{
    TextureSlot slot = NameToSlot(name);
    if (slot == TextureSlot::Count)
    {
        LOG_WARN("Unknown texture slot name: {}", name);
        return false;
    }

    SetTexture(slot, texture);
    return true;
}

Core::Ref<RHI::RHITexture> MaterialInstance::GetTexture(TextureSlot slot) const
{
    if (slot >= TextureSlot::Count)
    {
        return nullptr;
    }
    return m_Textures[static_cast<size_t>(slot)];
}

bool MaterialInstance::HasTexture(TextureSlot slot) const
{
    if (slot >= TextureSlot::Count)
    {
        return false;
    }
    return m_Textures[static_cast<size_t>(slot)] != nullptr;
}

// ============================================================================
// Parameter Setters
// ============================================================================

void MaterialInstance::SetBaseColor(const glm::vec4& color)
{
    m_MaterialData.BaseColor = color;
    MarkDirty();
}

void MaterialInstance::SetMetallic(float metallic)
{
    m_MaterialData.Metallic = std::clamp(metallic, 0.0f, 1.0f);
    MarkDirty();
}

void MaterialInstance::SetRoughness(float roughness)
{
    m_MaterialData.Roughness = std::clamp(roughness, 0.0f, 1.0f);
    MarkDirty();
}

void MaterialInstance::SetAmbientOcclusion(float ao)
{
    m_MaterialData.AmbientOcclusion = std::clamp(ao, 0.0f, 1.0f);
    MarkDirty();
}

void MaterialInstance::SetNormalScale(float scale)
{
    m_MaterialData.NormalScale = scale;
    MarkDirty();
}

void MaterialInstance::SetEmissiveFactor(const glm::vec3& emissive)
{
    m_MaterialData.EmissiveFactor = emissive;
    MarkDirty();
}

void MaterialInstance::SetAlphaCutoff(float cutoff)
{
    m_MaterialData.AlphaCutoff = std::clamp(cutoff, 0.0f, 1.0f);
    MarkDirty();
}

bool MaterialInstance::SetParameter(const std::string& name, float value)
{
    if (name == "metallic")
    {
        SetMetallic(value);
        return true;
    }
    else if (name == "roughness")
    {
        SetRoughness(value);
        return true;
    }
    else if (name == "ambientOcclusion" || name == "ao")
    {
        SetAmbientOcclusion(value);
        return true;
    }
    else if (name == "normalScale")
    {
        SetNormalScale(value);
        return true;
    }
    else if (name == "alphaCutoff")
    {
        SetAlphaCutoff(value);
        return true;
    }

    LOG_WARN("Unknown float parameter: {}", name);
    return false;
}

bool MaterialInstance::SetParameter(const std::string& name, const glm::vec4& value)
{
    if (name == "baseColor")
    {
        SetBaseColor(value);
        return true;
    }
    else if (name == "emissive" || name == "emissiveFactor")
    {
        SetEmissiveFactor(glm::vec3(value));
        return true;
    }

    LOG_WARN("Unknown vec4 parameter: {}", name);
    return false;
}

// ============================================================================
// GPU Resource Management
// ============================================================================

void MaterialInstance::UploadToGPU()
{
    if (!m_Dirty || !m_UniformBuffer)
    {
        return;
    }

    // Upload material data to uniform buffer
    if (!m_UniformBuffer->SetData(&m_MaterialData, sizeof(MaterialGPUData)))
    {
        LOG_ERROR("Failed to upload material data to GPU");
        return;
    }

    m_Dirty = false;
}

void MaterialInstance::UpdateDescriptorSet()
{
    if (!m_DescriptorSet || !m_UniformBuffer || !m_Sampler || !m_DefaultTexture)
    {
        return;
    }

    // Ensure data is uploaded
    UploadToGPU();

    // Update uniform buffer binding (binding 0)
    m_DescriptorSet->UpdateBuffer(
        0,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        m_UniformBuffer);

    // Get sampler handle once
    VkSampler samplerHandle = m_Sampler->GetHandle();

    // Update texture bindings (bindings 1-5)
    std::vector<RHI::DescriptorImageWrite> imageWrites;
    imageWrites.reserve(static_cast<size_t>(TextureSlot::Count));

    for (uint32_t i = 0; i < static_cast<uint32_t>(TextureSlot::Count); ++i)
    {
        const auto& texture = m_Textures[i];
        const auto& effectiveTexture = texture ? texture : m_DefaultTexture;

        RHI::DescriptorImageWrite write;
        write.Binding = i + 1; // Textures start at binding 1
        write.Type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.ImageView = effectiveTexture->GetImageView();
        write.Sampler = samplerHandle;
        write.ImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        write.ArrayElement = 0;

        imageWrites.push_back(write);
    }

    m_DescriptorSet->UpdateImages(imageWrites);
}

// ============================================================================
// Helper Methods
// ============================================================================

TextureSlot MaterialInstance::NameToSlot(const std::string& name)
{
    if (name == "baseColor" || name == "albedo" || name == "diffuse")
    {
        return TextureSlot::BaseColor;
    }
    else if (name == "normal" || name == "normalMap")
    {
        return TextureSlot::Normal;
    }
    else if (name == "metallicRoughness" || name == "metallicRoughnessMap")
    {
        return TextureSlot::MetallicRoughness;
    }
    else if (name == "occlusion" || name == "ao" || name == "ambientOcclusion")
    {
        return TextureSlot::Occlusion;
    }
    else if (name == "emissive" || name == "emissiveMap")
    {
        return TextureSlot::Emissive;
    }

    return TextureSlot::Count; // Invalid
}

} // namespace Resources
