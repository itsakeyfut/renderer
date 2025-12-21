/**
 * @file EnvironmentMap.cpp
 * @brief Implementation of HDR Environment Map loading.
 */

#include "Resources/EnvironmentMap.h"
#include "RHI/RHIDevice.h"
#include "RHI/RHIImage.h"
#include "RHI/RHISampler.h"
#include "RHI/RHIShader.h"
#include "RHI/RHIPipeline.h"
#include "RHI/RHICommandPool.h"
#include "RHI/RHICommandBuffer.h"
#include "RHI/RHIDescriptorPool.h"
#include "RHI/RHIDescriptorSet.h"
#include "RHI/RHIDescriptorSetLayout.h"
#include "RHI/RHIBuffer.h"
#include "Core/Assert.h"
#include "Core/Log.h"

// stb_image for HDR loading
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

#include <filesystem>
#include <cmath>
#include <algorithm>

namespace Resources
{

Core::Ref<EnvironmentMap> EnvironmentMap::LoadHDR(
    const Core::Ref<RHI::RHIDevice>& device,
    const std::string& filepath,
    uint32_t cubemapSize)
{
    ASSERT(device != nullptr);

    auto envMap = Core::Ref<EnvironmentMap>(new EnvironmentMap());
    if (!envMap->Initialize(device, filepath, cubemapSize))
    {
        LOG_ERROR("Failed to load HDR environment map: {}", filepath);
        return nullptr;
    }

    LOG_INFO("Loaded HDR environment map: {} (cubemap size: {})", filepath, cubemapSize);
    return envMap;
}

EnvironmentMap::~EnvironmentMap()
{
    m_IrradianceMap.reset();
    m_Cubemap.reset();
    m_Equirectangular.reset();
    m_Sampler.reset();
}

VkImageView EnvironmentMap::GetCubemapView() const
{
    return m_Cubemap ? m_Cubemap->GetImageView() : VK_NULL_HANDLE;
}

VkImageView EnvironmentMap::GetIrradianceMapView() const
{
    return m_IrradianceMap ? m_IrradianceMap->GetImageView() : VK_NULL_HANDLE;
}

bool EnvironmentMap::Initialize(
    const Core::Ref<RHI::RHIDevice>& device,
    const std::string& filepath,
    uint32_t cubemapSize)
{
    m_FilePath = filepath;
    m_CubemapSize = cubemapSize;

    // Load HDR data from file
    if (!LoadHDRData(filepath))
    {
        return false;
    }

    // Create equirectangular texture from loaded data
    if (!CreateEquirectangularTexture(device))
    {
        return false;
    }

    // Create cubemap texture
    if (!CreateCubemapTexture(device))
    {
        return false;
    }

    // Convert equirectangular to cubemap
    if (!ConvertToCubemap(device))
    {
        return false;
    }

    // Create irradiance map texture
    if (!CreateIrradianceMapTexture(device))
    {
        return false;
    }

    // Generate irradiance map for diffuse IBL
    if (!GenerateIrradianceMap(device))
    {
        return false;
    }

    // Clear CPU-side HDR data after GPU upload
    m_HDRData.clear();
    m_HDRData.shrink_to_fit();

    return true;
}

bool EnvironmentMap::LoadHDRData(const std::string& filepath)
{
    // Check if file exists
    if (!std::filesystem::exists(filepath))
    {
        LOG_ERROR("HDR file not found: {}", filepath);
        return false;
    }

    // Check file extension
    std::filesystem::path path(filepath);
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    if (ext != ".hdr")
    {
        LOG_ERROR("Unsupported file format for HDR loading: {} (expected .hdr)", ext);
        return false;
    }

    // Load HDR image using stbi_loadf for float data
    int width, height, channels;
    stbi_set_flip_vertically_on_load_thread(0);  // Keep top-left origin

    float* hdrData = stbi_loadf(filepath.c_str(), &width, &height, &channels, 0);
    if (!hdrData)
    {
        LOG_ERROR("Failed to load HDR file: {} - {}", filepath, stbi_failure_reason());
        return false;
    }

    m_HDRWidth = static_cast<uint32_t>(width);
    m_HDRHeight = static_cast<uint32_t>(height);

    // Convert to RGBA if needed (stbi returns RGB for HDR)
    size_t pixelCount = static_cast<size_t>(width) * height;
    m_HDRData.resize(pixelCount * 4);

    if (channels == 3)
    {
        // Convert RGB to RGBA
        for (size_t i = 0; i < pixelCount; ++i)
        {
            m_HDRData[i * 4 + 0] = hdrData[i * 3 + 0];
            m_HDRData[i * 4 + 1] = hdrData[i * 3 + 1];
            m_HDRData[i * 4 + 2] = hdrData[i * 3 + 2];
            m_HDRData[i * 4 + 3] = 1.0f;
        }
    }
    else if (channels == 4)
    {
        std::memcpy(m_HDRData.data(), hdrData, pixelCount * 4 * sizeof(float));
    }
    else
    {
        LOG_ERROR("Unsupported HDR channel count: {}", channels);
        stbi_image_free(hdrData);
        return false;
    }

    stbi_image_free(hdrData);

    LOG_DEBUG("Loaded HDR data: {}x{} ({} channels)", m_HDRWidth, m_HDRHeight, channels);
    return true;
}

bool EnvironmentMap::CreateEquirectangularTexture(const Core::Ref<RHI::RHIDevice>& device)
{
    // Create 2D texture for equirectangular map with HDR format
    RHI::ImageDesc desc;
    desc.Width = m_HDRWidth;
    desc.Height = m_HDRHeight;
    desc.Format = VK_FORMAT_R32G32B32A32_SFLOAT;  // Full 32-bit float for HDR precision
    desc.Usage = RHI::ImageUsage::Texture;
    desc.DebugName = "EquirectangularHDR";

    m_Equirectangular = RHI::RHIImage::Create(device, desc);
    if (!m_Equirectangular)
    {
        LOG_ERROR("Failed to create equirectangular texture");
        return false;
    }

    // Upload HDR data
    VkDeviceSize dataSize = static_cast<VkDeviceSize>(m_HDRData.size()) * sizeof(float);
    if (!m_Equirectangular->UploadData(device, m_HDRData.data(), dataSize))
    {
        LOG_ERROR("Failed to upload HDR data to texture");
        return false;
    }

    LOG_DEBUG("Created equirectangular texture: {}x{}", m_HDRWidth, m_HDRHeight);
    return true;
}

bool EnvironmentMap::CreateCubemapTexture(const Core::Ref<RHI::RHIDevice>& device)
{
    // Create cubemap texture (6 faces)
    RHI::ImageDesc desc;
    desc.Width = m_CubemapSize;
    desc.Height = m_CubemapSize;
    desc.ArrayLayers = 6;  // 6 faces for cubemap
    desc.Format = VK_FORMAT_R32G32B32A32_SFLOAT;  // HDR format
    desc.Usage = RHI::ImageUsage::Storage;  // Storage for compute shader write
    desc.IsCubemap = true;
    desc.DebugName = "EnvironmentCubemap";

    m_Cubemap = RHI::RHIImage::Create(device, desc);
    if (!m_Cubemap)
    {
        LOG_ERROR("Failed to create cubemap texture");
        return false;
    }

    LOG_DEBUG("Created cubemap texture: {} (6 faces)", m_CubemapSize);
    return true;
}

bool EnvironmentMap::ConvertToCubemap(const Core::Ref<RHI::RHIDevice>& device)
{
    // Create sampler for equirectangular texture
    RHI::SamplerDesc samplerDesc;
    samplerDesc.MagFilter = RHI::FilterMode::Linear;
    samplerDesc.MinFilter = RHI::FilterMode::Linear;
    samplerDesc.AddressModeU = RHI::AddressMode::Repeat;
    samplerDesc.AddressModeV = RHI::AddressMode::ClampToEdge;
    samplerDesc.AddressModeW = RHI::AddressMode::Repeat;
    samplerDesc.AnisotropyEnable = false;
    samplerDesc.DebugName = "EquirectSampler";

    m_Sampler = RHI::RHISampler::Create(device, samplerDesc);
    if (!m_Sampler)
    {
        LOG_ERROR("Failed to create sampler for equirectangular texture");
        return false;
    }

    // Load compute shader
    RHI::ShaderCompileConfig shaderConfig;
    shaderConfig.EntryPoint = "main";
    shaderConfig.IncludeDirs.push_back("shaders/hlsl");

    auto computeShader = RHI::RHIShader::CreateFromHLSL(
        device,
        "shaders/hlsl/compute/equirect_to_cubemap.hlsl",
        RHI::ShaderStage::Compute,
        shaderConfig);

    if (!computeShader)
    {
        LOG_ERROR("Failed to compile equirect_to_cubemap compute shader");
        return false;
    }

    // Create descriptor set layout
    // Binding 0: Input equirectangular texture (sampled)
    // Binding 1: Sampler
    // Binding 2: Output cubemap (storage)
    std::vector<RHI::DescriptorBinding> bindings = {
        {0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}
    };

    auto descriptorLayout = RHI::RHIDescriptorSetLayout::Create(device, bindings);
    if (!descriptorLayout)
    {
        LOG_ERROR("Failed to create descriptor set layout");
        return false;
    }

    // Create descriptor pool
    RHI::DescriptorPoolDesc poolDesc;
    poolDesc.MaxSets = 1;
    poolDesc.PoolSizes = {
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1},
        {VK_DESCRIPTOR_TYPE_SAMPLER, 1},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1}
    };

    auto descriptorPool = RHI::RHIDescriptorPool::Create(device, poolDesc);
    if (!descriptorPool)
    {
        LOG_ERROR("Failed to create descriptor pool");
        return false;
    }

    // Create descriptor set
    auto descriptorSet = RHI::RHIDescriptorSet::Create(device, descriptorPool, descriptorLayout);
    if (!descriptorSet)
    {
        LOG_ERROR("Failed to create descriptor set");
        return false;
    }

    // Update descriptor set with resources
    descriptorSet->UpdateImage(0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        m_Equirectangular->GetImageView(), VK_NULL_HANDLE,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    descriptorSet->UpdateImage(1, VK_DESCRIPTOR_TYPE_SAMPLER,
        VK_NULL_HANDLE, m_Sampler->GetHandle(),
        VK_IMAGE_LAYOUT_UNDEFINED);
    // Use GetStorageView() for compute shader storage access (2D array view for cubemaps)
    descriptorSet->UpdateImage(2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        m_Cubemap->GetStorageView(), VK_NULL_HANDLE,
        VK_IMAGE_LAYOUT_GENERAL);

    // Push constant for cubemap size
    struct PushConstants
    {
        uint32_t CubemapSize;
    };

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(PushConstants);

    // Create compute pipeline
    auto computePipeline = RHI::RHIPipeline::CreateCompute(
        device,
        computeShader,
        {descriptorLayout->GetHandle()},
        {pushConstantRange});

    if (!computePipeline)
    {
        LOG_ERROR("Failed to create compute pipeline");
        return false;
    }

    // Create command pool and buffer
    RHI::RHICommandPoolConfig poolConfig;
    poolConfig.QueueType = RHI::CommandPoolQueueType::Graphics;  // Use graphics queue for compute
    poolConfig.Transient = true;

    auto commandPool = RHI::RHICommandPool::Create(device, poolConfig);
    if (!commandPool)
    {
        LOG_ERROR("Failed to create command pool");
        return false;
    }

    auto commandBuffer = RHI::RHICommandBuffer::Create(device, commandPool);
    if (!commandBuffer)
    {
        LOG_ERROR("Failed to create command buffer");
        return false;
    }

    // Record compute dispatch
    commandBuffer->Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    // Transition cubemap to GENERAL layout for compute write
    VkImageMemoryBarrier barrierToGeneral{};
    barrierToGeneral.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrierToGeneral.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrierToGeneral.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrierToGeneral.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrierToGeneral.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrierToGeneral.image = m_Cubemap->GetHandle();
    barrierToGeneral.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrierToGeneral.subresourceRange.baseMipLevel = 0;
    barrierToGeneral.subresourceRange.levelCount = 1;
    barrierToGeneral.subresourceRange.baseArrayLayer = 0;
    barrierToGeneral.subresourceRange.layerCount = 6;
    barrierToGeneral.srcAccessMask = 0;
    barrierToGeneral.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

    commandBuffer->PipelineBarrier(
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        {}, {}, {barrierToGeneral});

    // Bind pipeline and descriptor set
    commandBuffer->BindPipeline(VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline->GetHandle());
    commandBuffer->BindDescriptorSets(
        VK_PIPELINE_BIND_POINT_COMPUTE,
        computePipeline->GetLayout(),
        0,
        {descriptorSet->GetHandle()});

    // Push constants
    PushConstants pushConstants{m_CubemapSize};
    commandBuffer->PushConstants(
        computePipeline->GetLayout(),
        VK_SHADER_STAGE_COMPUTE_BIT,
        0,
        sizeof(PushConstants),
        &pushConstants);

    // Dispatch compute shader (16x16 work groups, 6 faces)
    uint32_t groupCountX = (m_CubemapSize + 15) / 16;
    uint32_t groupCountY = (m_CubemapSize + 15) / 16;
    uint32_t groupCountZ = 6;  // 6 faces
    commandBuffer->Dispatch(groupCountX, groupCountY, groupCountZ);

    // Transition cubemap to SHADER_READ_ONLY for sampling
    VkImageMemoryBarrier barrierToRead{};
    barrierToRead.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrierToRead.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrierToRead.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrierToRead.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrierToRead.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrierToRead.image = m_Cubemap->GetHandle();
    barrierToRead.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrierToRead.subresourceRange.baseMipLevel = 0;
    barrierToRead.subresourceRange.levelCount = 1;
    barrierToRead.subresourceRange.baseArrayLayer = 0;
    barrierToRead.subresourceRange.layerCount = 6;
    barrierToRead.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrierToRead.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    commandBuffer->PipelineBarrier(
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        {}, {}, {barrierToRead});

    commandBuffer->End();

    // Submit and wait
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    VkCommandBuffer cmdHandle = commandBuffer->GetHandle();
    submitInfo.pCommandBuffers = &cmdHandle;

    VkResult result = vkQueueSubmit(device->GetGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
    if (result != VK_SUCCESS)
    {
        LOG_ERROR("Failed to submit compute command: VkResult {}", static_cast<int>(result));
        return false;
    }

    result = vkQueueWaitIdle(device->GetGraphicsQueue());
    if (result != VK_SUCCESS)
    {
        LOG_ERROR("Failed to wait for compute queue: VkResult {}", static_cast<int>(result));
        return false;
    }

    // Update cubemap layout tracking
    m_Cubemap->SetLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    LOG_DEBUG("Converted equirectangular to cubemap");
    return true;
}

bool EnvironmentMap::CreateIrradianceMapTexture(const Core::Ref<RHI::RHIDevice>& device)
{
    // Create irradiance cubemap texture (6 faces, low resolution)
    // 32x32 is sufficient for diffuse IBL since irradiance varies slowly
    RHI::ImageDesc desc;
    desc.Width = m_IrradianceMapSize;
    desc.Height = m_IrradianceMapSize;
    desc.ArrayLayers = 6;  // 6 faces for cubemap
    desc.Format = VK_FORMAT_R32G32B32A32_SFLOAT;  // HDR format
    desc.Usage = RHI::ImageUsage::Storage;  // Storage for compute shader write
    desc.IsCubemap = true;
    desc.DebugName = "IrradianceCubemap";

    m_IrradianceMap = RHI::RHIImage::Create(device, desc);
    if (!m_IrradianceMap)
    {
        LOG_ERROR("Failed to create irradiance map texture");
        return false;
    }

    LOG_DEBUG("Created irradiance map texture: {} (6 faces)", m_IrradianceMapSize);
    return true;
}

bool EnvironmentMap::GenerateIrradianceMap(const Core::Ref<RHI::RHIDevice>& device)
{
    // Load compute shader for irradiance map generation
    RHI::ShaderCompileConfig shaderConfig;
    shaderConfig.EntryPoint = "main";
    shaderConfig.IncludeDirs.push_back("shaders/hlsl");

    auto computeShader = RHI::RHIShader::CreateFromHLSL(
        device,
        "shaders/hlsl/compute/irradiance_map.hlsl",
        RHI::ShaderStage::Compute,
        shaderConfig);

    if (!computeShader)
    {
        LOG_ERROR("Failed to compile irradiance_map compute shader");
        return false;
    }

    // Create descriptor set layout
    // Binding 0: Input environment cubemap (sampled)
    // Binding 1: Sampler
    // Binding 2: Output irradiance map (storage)
    std::vector<RHI::DescriptorBinding> bindings = {
        {0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}
    };

    auto descriptorLayout = RHI::RHIDescriptorSetLayout::Create(device, bindings);
    if (!descriptorLayout)
    {
        LOG_ERROR("Failed to create descriptor set layout for irradiance map");
        return false;
    }

    // Create descriptor pool
    RHI::DescriptorPoolDesc poolDesc;
    poolDesc.MaxSets = 1;
    poolDesc.PoolSizes = {
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1},
        {VK_DESCRIPTOR_TYPE_SAMPLER, 1},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1}
    };

    auto descriptorPool = RHI::RHIDescriptorPool::Create(device, poolDesc);
    if (!descriptorPool)
    {
        LOG_ERROR("Failed to create descriptor pool for irradiance map");
        return false;
    }

    // Create descriptor set
    auto descriptorSet = RHI::RHIDescriptorSet::Create(device, descriptorPool, descriptorLayout);
    if (!descriptorSet)
    {
        LOG_ERROR("Failed to create descriptor set for irradiance map");
        return false;
    }

    // Update descriptor set with resources
    // Use the cubemap's regular view (VK_IMAGE_VIEW_TYPE_CUBE) for sampling
    descriptorSet->UpdateImage(0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        m_Cubemap->GetImageView(), VK_NULL_HANDLE,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    descriptorSet->UpdateImage(1, VK_DESCRIPTOR_TYPE_SAMPLER,
        VK_NULL_HANDLE, m_Sampler->GetHandle(),
        VK_IMAGE_LAYOUT_UNDEFINED);
    // Use GetStorageView() for compute shader storage access (2D array view for cubemaps)
    descriptorSet->UpdateImage(2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        m_IrradianceMap->GetStorageView(), VK_NULL_HANDLE,
        VK_IMAGE_LAYOUT_GENERAL);

    // Push constant for irradiance map size
    struct PushConstants
    {
        uint32_t IrradianceMapSize;
    };

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(PushConstants);

    // Create compute pipeline
    auto computePipeline = RHI::RHIPipeline::CreateCompute(
        device,
        computeShader,
        {descriptorLayout->GetHandle()},
        {pushConstantRange});

    if (!computePipeline)
    {
        LOG_ERROR("Failed to create compute pipeline for irradiance map");
        return false;
    }

    // Create command pool and buffer
    RHI::RHICommandPoolConfig poolConfig;
    poolConfig.QueueType = RHI::CommandPoolQueueType::Graphics;  // Use graphics queue for compute
    poolConfig.Transient = true;

    auto commandPool = RHI::RHICommandPool::Create(device, poolConfig);
    if (!commandPool)
    {
        LOG_ERROR("Failed to create command pool for irradiance map");
        return false;
    }

    auto commandBuffer = RHI::RHICommandBuffer::Create(device, commandPool);
    if (!commandBuffer)
    {
        LOG_ERROR("Failed to create command buffer for irradiance map");
        return false;
    }

    // Record compute dispatch
    commandBuffer->Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    // Transition irradiance map to GENERAL layout for compute write
    VkImageMemoryBarrier barrierToGeneral{};
    barrierToGeneral.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrierToGeneral.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrierToGeneral.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrierToGeneral.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrierToGeneral.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrierToGeneral.image = m_IrradianceMap->GetHandle();
    barrierToGeneral.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrierToGeneral.subresourceRange.baseMipLevel = 0;
    barrierToGeneral.subresourceRange.levelCount = 1;
    barrierToGeneral.subresourceRange.baseArrayLayer = 0;
    barrierToGeneral.subresourceRange.layerCount = 6;
    barrierToGeneral.srcAccessMask = 0;
    barrierToGeneral.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

    commandBuffer->PipelineBarrier(
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        {}, {}, {barrierToGeneral});

    // Bind pipeline and descriptor set
    commandBuffer->BindPipeline(VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline->GetHandle());
    commandBuffer->BindDescriptorSets(
        VK_PIPELINE_BIND_POINT_COMPUTE,
        computePipeline->GetLayout(),
        0,
        {descriptorSet->GetHandle()});

    // Push constants
    PushConstants pushConstants{m_IrradianceMapSize};
    commandBuffer->PushConstants(
        computePipeline->GetLayout(),
        VK_SHADER_STAGE_COMPUTE_BIT,
        0,
        sizeof(PushConstants),
        &pushConstants);

    // Dispatch compute shader (16x16 work groups, 6 faces)
    uint32_t groupCountX = (m_IrradianceMapSize + 15) / 16;
    uint32_t groupCountY = (m_IrradianceMapSize + 15) / 16;
    uint32_t groupCountZ = 6;  // 6 faces
    commandBuffer->Dispatch(groupCountX, groupCountY, groupCountZ);

    // Transition irradiance map to SHADER_READ_ONLY for sampling
    VkImageMemoryBarrier barrierToRead{};
    barrierToRead.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrierToRead.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrierToRead.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrierToRead.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrierToRead.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrierToRead.image = m_IrradianceMap->GetHandle();
    barrierToRead.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrierToRead.subresourceRange.baseMipLevel = 0;
    barrierToRead.subresourceRange.levelCount = 1;
    barrierToRead.subresourceRange.baseArrayLayer = 0;
    barrierToRead.subresourceRange.layerCount = 6;
    barrierToRead.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrierToRead.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    commandBuffer->PipelineBarrier(
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        {}, {}, {barrierToRead});

    commandBuffer->End();

    // Submit and wait
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    VkCommandBuffer cmdHandle = commandBuffer->GetHandle();
    submitInfo.pCommandBuffers = &cmdHandle;

    VkResult result = vkQueueSubmit(device->GetGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
    if (result != VK_SUCCESS)
    {
        LOG_ERROR("Failed to submit irradiance map compute command: VkResult {}", static_cast<int>(result));
        return false;
    }

    result = vkQueueWaitIdle(device->GetGraphicsQueue());
    if (result != VK_SUCCESS)
    {
        LOG_ERROR("Failed to wait for irradiance map compute queue: VkResult {}", static_cast<int>(result));
        return false;
    }

    // Update irradiance map layout tracking
    m_IrradianceMap->SetLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    LOG_DEBUG("Generated irradiance map for diffuse IBL");
    return true;
}

} // namespace Resources
