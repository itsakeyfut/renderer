/**
 * @file LightManager.cpp
 * @brief Implementation of the LightManager class.
 */

#include "LightManager.h"
#include "Core/Assert.h"
#include "Core/Log.h"

namespace Renderer {

/**
 * @brief GPU-compatible structure matching ShadowParams in shadow.hlsli
 */
struct ShadowUBO
{
    glm::mat4 LightSpaceMatrix;   // Combined light view-projection matrix
    float     ShadowBias;         // Depth bias for shadow acne prevention
    float     NormalBias;         // Normal-based bias offset
    glm::vec2 ShadowMapSize;      // Shadow map dimensions (width, height)
    float     ShadowStrength;     // Shadow darkness multiplier (0=no shadow, 1=full)
    float     Padding[3];         // Padding for std140 alignment
};
static_assert(sizeof(ShadowUBO) == 96, "ShadowUBO size must be 96 bytes for std140 layout");

Core::Ref<LightManager> LightManager::Create(
    const Core::Ref<RHI::RHIDevice>& device,
    const LightManagerConfig& config)
{
    ASSERT(device != nullptr);

    auto manager = Core::Ref<LightManager>(new LightManager());
    if (!manager->Initialize(device, config)) {
        LOG_ERROR("Failed to initialize LightManager");
        return nullptr;
    }

    return manager;
}

bool LightManager::Initialize(
    const Core::Ref<RHI::RHIDevice>& device,
    const LightManagerConfig& config)
{
    m_Device = device;
    m_MaxPointLights = config.MaxPointLights;
    m_MaxSpotLights = config.MaxSpotLights;

    // Reserve capacity for light arrays
    m_PointLights.reserve(m_MaxPointLights);
    m_SpotLights.reserve(m_MaxSpotLights);

    // Initialize default directional light (sun-like from upper right)
    m_DirectionalLight = Scene::DirectionalLight::Create(
        glm::vec3(1.0f, 1.0f, 1.0f),  // Direction (normalized in Create)
        glm::vec3(1.0f, 0.98f, 0.95f), // Warm white color
        1.0f);                          // Intensity

    // Create descriptor set layout
    // Binding 0: LightUBO (uniform buffer)
    // Binding 1: PointLight storage buffer
    // Binding 2: SpotLight storage buffer
    // Binding 3: Shadow map (combined image sampler with comparison)
    // Binding 4: ShadowData UBO (uniform buffer)
    m_DescriptorSetLayout = RHI::RHIDescriptorSetLayout::Create(device, {
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
        {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
        {3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
        {4, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT}
    });

    if (!m_DescriptorSetLayout) {
        LOG_ERROR("Failed to create light descriptor set layout");
        return false;
    }

    // Create descriptor pool (one set per frame in flight)
    // Need: 2 UBOs (light + shadow), 2 storage buffers (point + spot), 1 image sampler (shadow map)
    m_DescriptorPool = RHI::RHIDescriptorPool::Create(
        device,
        MAX_FRAMES_IN_FLIGHT,
        {
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_FRAMES_IN_FLIGHT * 2},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, MAX_FRAMES_IN_FLIGHT * 2},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_FRAMES_IN_FLIGHT}
        });

    if (!m_DescriptorPool) {
        LOG_ERROR("Failed to create light descriptor pool");
        return false;
    }

    // Create fallback shadow sampler (comparison sampler for depth testing)
    m_FallbackShadowSampler = RHI::RHISampler::CreateShadow(device);
    if (!m_FallbackShadowSampler) {
        LOG_ERROR("Failed to create fallback shadow sampler");
        return false;
    }

    // Create fallback shadow map (1x1 texture for when no shadow map is bound)
    // Note: Use R32_SFLOAT for compatibility; shader treats it as depth via comparison
    {
        RHI::TextureDesc texDesc;
        texDesc.Width = 1;
        texDesc.Height = 1;
        texDesc.Format = VK_FORMAT_R32_SFLOAT;  // Use color format for simplicity
        texDesc.MipLevels = 1;
        texDesc.DebugName = "Fallback Shadow Map";

        m_FallbackShadowMap = RHI::RHITexture::Create(device, texDesc);
        if (!m_FallbackShadowMap) {
            LOG_ERROR("Failed to create fallback shadow map");
            return false;
        }
        // Initialize with max depth (1.0 = fully lit, all shadow tests pass)
        float whiteDepth = 1.0f;
        m_FallbackShadowMap->UploadData(device, &whiteDepth, sizeof(whiteDepth));
    }

    // Create per-frame GPU resources
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        // Light UBO
        RHI::BufferDesc uboDesc;
        uboDesc.Size = sizeof(Scene::LightUBO);
        uboDesc.Usage = RHI::BufferUsage::Uniform;
        uboDesc.Memory = RHI::MemoryUsage::CpuToGpu;
        uboDesc.DebugName = "Light UBO";

        m_LightUBOs[i] = RHI::RHIBuffer::Create(device, uboDesc);
        if (!m_LightUBOs[i]) {
            LOG_ERROR("Failed to create light UBO for frame {}", i);
            return false;
        }

        // Point light storage buffer (always allocate at least 1 element for valid binding)
        RHI::BufferDesc pointLightDesc;
        pointLightDesc.Size = sizeof(Scene::PointLight) * std::max(m_MaxPointLights, 1u);
        pointLightDesc.Usage = RHI::BufferUsage::Storage;
        pointLightDesc.Memory = RHI::MemoryUsage::CpuToGpu;
        pointLightDesc.DebugName = "Point Light Buffer";

        m_PointLightBuffers[i] = RHI::RHIBuffer::Create(device, pointLightDesc);
        if (!m_PointLightBuffers[i]) {
            LOG_ERROR("Failed to create point light buffer for frame {}", i);
            return false;
        }

        // Spot light storage buffer (always allocate at least 1 element for valid binding)
        RHI::BufferDesc spotLightDesc;
        spotLightDesc.Size = sizeof(Scene::SpotLight) * std::max(m_MaxSpotLights, 1u);
        spotLightDesc.Usage = RHI::BufferUsage::Storage;
        spotLightDesc.Memory = RHI::MemoryUsage::CpuToGpu;
        spotLightDesc.DebugName = "Spot Light Buffer";

        m_SpotLightBuffers[i] = RHI::RHIBuffer::Create(device, spotLightDesc);
        if (!m_SpotLightBuffers[i]) {
            LOG_ERROR("Failed to create spot light buffer for frame {}", i);
            return false;
        }

        // Shadow UBO
        RHI::BufferDesc shadowUboDesc;
        shadowUboDesc.Size = sizeof(ShadowUBO);
        shadowUboDesc.Usage = RHI::BufferUsage::Uniform;
        shadowUboDesc.Memory = RHI::MemoryUsage::CpuToGpu;
        shadowUboDesc.DebugName = "Shadow UBO";

        m_ShadowUBOs[i] = RHI::RHIBuffer::Create(device, shadowUboDesc);
        if (!m_ShadowUBOs[i]) {
            LOG_ERROR("Failed to create shadow UBO for frame {}", i);
            return false;
        }

        // Create and update descriptor set
        m_DescriptorSets[i] = RHI::RHIDescriptorSet::Create(device, m_DescriptorPool, m_DescriptorSetLayout);
        if (!m_DescriptorSets[i]) {
            LOG_ERROR("Failed to create light descriptor set for frame {}", i);
            return false;
        }

        // Bind buffers to descriptor set
        m_DescriptorSets[i]->UpdateBuffer(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, m_LightUBOs[i]);
        m_DescriptorSets[i]->UpdateBuffer(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, m_PointLightBuffers[i]);
        m_DescriptorSets[i]->UpdateBuffer(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, m_SpotLightBuffers[i]);

        // Bind fallback shadow map initially (binding 3)
        // Uses comparison sampler for proper shadow testing
        m_DescriptorSets[i]->UpdateImage(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            m_FallbackShadowMap->GetImageView(),
            m_FallbackShadowSampler->GetHandle(),
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        // Bind shadow UBO (binding 4)
        m_DescriptorSets[i]->UpdateBuffer(4, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, m_ShadowUBOs[i]);
    }

    LOG_INFO("LightManager created (max point lights: {}, max spot lights: {}, shadow support: enabled)",
             m_MaxPointLights, m_MaxSpotLights);

    return true;
}

void LightManager::SetDirectionalLight(const Scene::DirectionalLight& light)
{
    m_DirectionalLight = light;
    MarkDirty();
}

void LightManager::ClearPointLights()
{
    m_PointLights.clear();
    MarkDirty();
}

int32_t LightManager::AddPointLight(const Scene::PointLight& light)
{
    if (m_PointLights.size() >= m_MaxPointLights) {
        LOG_WARN("Maximum point light count ({}) reached", m_MaxPointLights);
        return -1;
    }

    m_PointLights.push_back(light);
    MarkDirty();
    return static_cast<int32_t>(m_PointLights.size() - 1);
}

bool LightManager::UpdatePointLight(size_t index, const Scene::PointLight& light)
{
    if (index >= m_PointLights.size()) {
        return false;
    }

    m_PointLights[index] = light;
    MarkDirty();
    return true;
}

const Scene::PointLight* LightManager::GetPointLight(size_t index) const
{
    if (index >= m_PointLights.size()) {
        return nullptr;
    }
    return &m_PointLights[index];
}

void LightManager::ClearSpotLights()
{
    m_SpotLights.clear();
    MarkDirty();
}

int32_t LightManager::AddSpotLight(const Scene::SpotLight& light)
{
    if (m_SpotLights.size() >= m_MaxSpotLights) {
        LOG_WARN("Maximum spot light count ({}) reached", m_MaxSpotLights);
        return -1;
    }

    m_SpotLights.push_back(light);
    MarkDirty();
    return static_cast<int32_t>(m_SpotLights.size() - 1);
}

bool LightManager::UpdateSpotLight(size_t index, const Scene::SpotLight& light)
{
    if (index >= m_SpotLights.size()) {
        return false;
    }

    m_SpotLights[index] = light;
    MarkDirty();
    return true;
}

const Scene::SpotLight* LightManager::GetSpotLight(size_t index) const
{
    if (index >= m_SpotLights.size()) {
        return nullptr;
    }
    return &m_SpotLights[index];
}

void LightManager::SetShadowMap(const Core::Ref<ShadowMap>& shadowMap)
{
    m_ShadowMap = shadowMap;

    // Update all descriptor sets with the new shadow map
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        if (m_ShadowMap)
        {
            // Bind actual shadow map with its comparison sampler
            m_DescriptorSets[i]->UpdateImage(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                m_ShadowMap->GetImageView(),
                m_ShadowMap->GetSampler()->GetHandle(),
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
        else
        {
            // Bind fallback shadow map with comparison sampler
            m_DescriptorSets[i]->UpdateImage(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                m_FallbackShadowMap->GetImageView(),
                m_FallbackShadowSampler->GetHandle(),
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
    }

    MarkDirty();
    LOG_DEBUG("Shadow map {} for LightManager", shadowMap ? "set" : "cleared");
}

void LightManager::UpdateGPUBuffers(uint32_t frameIndex)
{
    ASSERT(frameIndex < MAX_FRAMES_IN_FLIGHT);

    // Build and upload LightUBO
    Scene::LightUBO lightData;
    lightData.DirectionalLightData = m_DirectionalLight;
    lightData.NumPointLights = static_cast<uint32_t>(m_PointLights.size());
    lightData.NumSpotLights = static_cast<uint32_t>(m_SpotLights.size());

    m_LightUBOs[frameIndex]->SetData(&lightData, sizeof(lightData));

    // Upload point lights (if any)
    if (!m_PointLights.empty()) {
        m_PointLightBuffers[frameIndex]->SetData(
            m_PointLights.data(),
            sizeof(Scene::PointLight) * m_PointLights.size());
    }

    // Upload spot lights (if any)
    if (!m_SpotLights.empty()) {
        m_SpotLightBuffers[frameIndex]->SetData(
            m_SpotLights.data(),
            sizeof(Scene::SpotLight) * m_SpotLights.size());
    }

    // Build and upload ShadowUBO
    ShadowUBO shadowData{};
    if (m_ShadowMap)
    {
        shadowData.LightSpaceMatrix = m_ShadowMap->GetLightSpaceMatrix();
        shadowData.ShadowMapSize = glm::vec2(
            static_cast<float>(m_ShadowMap->GetResolution()),
            static_cast<float>(m_ShadowMap->GetResolution()));
    }
    else
    {
        shadowData.LightSpaceMatrix = glm::mat4(1.0f);
        shadowData.ShadowMapSize = glm::vec2(1.0f, 1.0f);
    }
    shadowData.ShadowBias = m_ShadowBias;
    shadowData.NormalBias = m_NormalBias;
    shadowData.ShadowStrength = m_ShadowStrength;

    m_ShadowUBOs[frameIndex]->SetData(&shadowData, sizeof(shadowData));

    // Decrement dirty frame count after updating this frame's buffer
    if (m_DirtyFrameCount > 0) {
        --m_DirtyFrameCount;
    }
}

} // namespace Renderer
