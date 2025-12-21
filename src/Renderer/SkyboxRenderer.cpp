/**
 * @file SkyboxRenderer.cpp
 * @brief Implementation of skybox rendering.
 */

#include "Renderer/SkyboxRenderer.h"
#include "RHI/RHIDevice.h"
#include "RHI/RHIImage.h"
#include "RHI/RHISampler.h"
#include "RHI/RHIShader.h"
#include "RHI/RHIPipeline.h"
#include "RHI/RHICommandBuffer.h"
#include "RHI/RHIDescriptorPool.h"
#include "RHI/RHIDescriptorSet.h"
#include "RHI/RHIDescriptorSetLayout.h"
#include "Resources/EnvironmentMap.h"
#include "Core/Assert.h"
#include "Core/Log.h"

#include <glm/gtc/matrix_inverse.hpp>

namespace Renderer
{

Core::Ref<SkyboxRenderer> SkyboxRenderer::Create(
    const Core::Ref<RHI::RHIDevice>& device,
    VkFormat colorFormat,
    VkFormat depthFormat)
{
    ASSERT(device != nullptr);

    auto renderer = Core::Ref<SkyboxRenderer>(new SkyboxRenderer());
    if (!renderer->Initialize(device, colorFormat, depthFormat))
    {
        LOG_ERROR("Failed to initialize SkyboxRenderer");
        return nullptr;
    }

    LOG_INFO("SkyboxRenderer created successfully");
    return renderer;
}

SkyboxRenderer::~SkyboxRenderer()
{
    m_DescriptorSet.reset();
    m_DescriptorPool.reset();
    m_DescriptorLayout.reset();
    m_Pipeline.reset();
    m_Sampler.reset();
    m_PixelShader.reset();
    m_VertexShader.reset();
}

bool SkyboxRenderer::Initialize(
    const Core::Ref<RHI::RHIDevice>& device,
    VkFormat colorFormat,
    VkFormat depthFormat)
{
    // Create descriptors first (needed for pipeline layout)
    if (!CreateDescriptors(device))
    {
        LOG_ERROR("Failed to create skybox descriptors");
        return false;
    }

    // Create the pipeline
    if (!CreatePipeline(device, colorFormat, depthFormat))
    {
        LOG_ERROR("Failed to create skybox pipeline");
        return false;
    }

    return true;
}

bool SkyboxRenderer::CreateDescriptors(const Core::Ref<RHI::RHIDevice>& device)
{
    // Create sampler for cubemap
    RHI::SamplerDesc samplerDesc;
    samplerDesc.MagFilter = RHI::FilterMode::Linear;
    samplerDesc.MinFilter = RHI::FilterMode::Linear;
    samplerDesc.MipmapMode = RHI::FilterMode::Linear;
    samplerDesc.AddressModeU = RHI::AddressMode::ClampToEdge;
    samplerDesc.AddressModeV = RHI::AddressMode::ClampToEdge;
    samplerDesc.AddressModeW = RHI::AddressMode::ClampToEdge;
    samplerDesc.AnisotropyEnable = false;
    samplerDesc.DebugName = "SkyboxSampler";

    m_Sampler = RHI::RHISampler::Create(device, samplerDesc);
    if (!m_Sampler)
    {
        LOG_ERROR("Failed to create skybox sampler");
        return false;
    }

    // Create descriptor set layout
    // Binding 0: Cubemap texture (sampled)
    // Binding 1: Sampler
    std::vector<RHI::DescriptorBinding> bindings = {
        {0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}
    };

    m_DescriptorLayout = RHI::RHIDescriptorSetLayout::Create(device, bindings);
    if (!m_DescriptorLayout)
    {
        LOG_ERROR("Failed to create skybox descriptor set layout");
        return false;
    }

    // Create descriptor pool
    RHI::DescriptorPoolDesc poolDesc;
    poolDesc.MaxSets = 1;
    poolDesc.PoolSizes = {
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1},
        {VK_DESCRIPTOR_TYPE_SAMPLER, 1}
    };

    m_DescriptorPool = RHI::RHIDescriptorPool::Create(device, poolDesc);
    if (!m_DescriptorPool)
    {
        LOG_ERROR("Failed to create skybox descriptor pool");
        return false;
    }

    // Create descriptor set
    m_DescriptorSet = RHI::RHIDescriptorSet::Create(device, m_DescriptorPool, m_DescriptorLayout);
    if (!m_DescriptorSet)
    {
        LOG_ERROR("Failed to create skybox descriptor set");
        return false;
    }

    return true;
}

bool SkyboxRenderer::CreatePipeline(
    const Core::Ref<RHI::RHIDevice>& device,
    VkFormat colorFormat,
    VkFormat depthFormat)
{
    // Compile shaders
    RHI::ShaderCompileConfig shaderConfig;
    shaderConfig.EntryPoint = "main";
    shaderConfig.IncludeDirs.push_back("shaders/hlsl");

    m_VertexShader = RHI::RHIShader::CreateFromHLSL(
        device,
        "shaders/hlsl/vertex/skybox.hlsl",
        RHI::ShaderStage::Vertex,
        shaderConfig);

    if (!m_VertexShader)
    {
        LOG_ERROR("Failed to compile skybox vertex shader");
        return false;
    }

    m_PixelShader = RHI::RHIShader::CreateFromHLSL(
        device,
        "shaders/hlsl/pixel/skybox.hlsl",
        RHI::ShaderStage::Fragment,
        shaderConfig);

    if (!m_PixelShader)
    {
        LOG_ERROR("Failed to compile skybox pixel shader");
        return false;
    }

    // Push constant for inverse view-projection matrix
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(glm::mat4);  // 64 bytes

    // Configure graphics pipeline
    RHI::GraphicsPipelineDesc pipelineDesc;
    pipelineDesc.VertexShader = m_VertexShader;
    pipelineDesc.FragmentShader = m_PixelShader;

    // No vertex input - we generate fullscreen triangle from vertex ID
    pipelineDesc.VertexBindings = {};
    pipelineDesc.VertexAttributes = {};

    // Primitive topology
    pipelineDesc.Topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    // Rasterization - no culling for fullscreen triangle
    pipelineDesc.CullMode = VK_CULL_MODE_NONE;
    pipelineDesc.FrontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    // Depth state - test against depth buffer but don't write
    // Use LEQUAL to render at far plane (depth = 1.0)
    pipelineDesc.DepthTestEnable = true;
    pipelineDesc.DepthWriteEnable = false;
    pipelineDesc.DepthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

    // Color blend - no blending needed
    RHI::ColorBlendAttachment colorBlend;
    colorBlend.BlendEnable = false;
    pipelineDesc.ColorBlendAttachments = {colorBlend};

    // Dynamic rendering formats
    pipelineDesc.ColorAttachmentFormats = {colorFormat};
    pipelineDesc.DepthAttachmentFormat = depthFormat;

    // Pipeline layout
    pipelineDesc.DescriptorSetLayouts = {m_DescriptorLayout->GetHandle()};
    pipelineDesc.PushConstantRanges = {pushConstantRange};

    // Create pipeline
    m_Pipeline = RHI::RHIPipeline::CreateGraphics(device, pipelineDesc);
    if (!m_Pipeline)
    {
        LOG_ERROR("Failed to create skybox graphics pipeline");
        return false;
    }

    LOG_DEBUG("Skybox pipeline created successfully");
    return true;
}

bool SkyboxRenderer::SetEnvironmentMap(
    const Core::Ref<RHI::RHIDevice>& /* device */,
    const Core::Ref<Resources::EnvironmentMap>& environmentMap)
{
    if (!environmentMap)
    {
        m_HasEnvironmentMap = false;
        return true;
    }

    auto cubemap = environmentMap->GetCubemap();
    if (!cubemap)
    {
        LOG_ERROR("Environment map has no cubemap");
        return false;
    }

    // Update descriptor set with the cubemap
    m_DescriptorSet->UpdateImage(
        0,
        VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        cubemap->GetImageView(),
        VK_NULL_HANDLE,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    m_DescriptorSet->UpdateImage(
        1,
        VK_DESCRIPTOR_TYPE_SAMPLER,
        VK_NULL_HANDLE,
        m_Sampler->GetHandle(),
        VK_IMAGE_LAYOUT_UNDEFINED);

    m_HasEnvironmentMap = true;
    LOG_DEBUG("Skybox environment map set successfully");
    return true;
}

void SkyboxRenderer::Render(
    const Core::Ref<RHI::RHICommandBuffer>& commandBuffer,
    const glm::mat4& viewMatrix,
    const glm::mat4& projectionMatrix)
{
    if (!m_HasEnvironmentMap)
    {
        return;
    }

    // Create rotation-only view matrix (remove translation)
    glm::mat4 viewRotation = glm::mat4(glm::mat3(viewMatrix));

    // Calculate inverse view-projection for direction calculation
    glm::mat4 inverseViewProjection = glm::inverse(projectionMatrix * viewRotation);

    // Bind pipeline
    commandBuffer->BindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline->GetHandle());

    // Bind descriptor set
    commandBuffer->BindDescriptorSets(
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_Pipeline->GetLayout(),
        0,
        {m_DescriptorSet->GetHandle()});

    // Push inverse view-projection matrix
    commandBuffer->PushConstants(
        m_Pipeline->GetLayout(),
        VK_SHADER_STAGE_VERTEX_BIT,
        0,
        sizeof(glm::mat4),
        &inverseViewProjection);

    // Draw fullscreen triangle (3 vertices, no vertex buffer)
    commandBuffer->Draw(3, 1, 0, 0);
}

} // namespace Renderer
