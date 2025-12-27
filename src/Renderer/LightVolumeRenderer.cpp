/**
 * @file LightVolumeRenderer.cpp
 * @brief Light volume rendering implementation.
 */

#include "Renderer/LightVolumeRenderer.h"
#include "Renderer/GBuffer.h"
#include "RHI/RHIBuffer.h"
#include "RHI/RHICommandBuffer.h"
#include "RHI/RHIDeletionQueue.h"
#include "RHI/RHIDevice.h"
#include "RHI/RHIPipeline.h"
#include "RHI/RHIShader.h"
#include "Core/Log.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cmath>
#include <vector>

namespace Renderer
{
    // Vertex structure for light volume meshes (position only)
    struct LightVolumeVertex
    {
        glm::vec3 Position;
    };

    Core::Ref<LightVolumeRenderer> LightVolumeRenderer::Create(
        const Core::Ref<RHI::RHIDevice>& device,
        const Core::Ref<RHI::RHIDeletionQueue>& deletionQueue,
        const LightVolumeRendererDesc& desc)
    {
        auto renderer = Core::Ref<LightVolumeRenderer>(new LightVolumeRenderer());
        if (!renderer->Initialize(device, deletionQueue, desc))
        {
            return nullptr;
        }
        return renderer;
    }

    LightVolumeRenderer::~LightVolumeRenderer()
    {
        // Resources are automatically cleaned up through shared_ptr destruction
        // Pipelines and buffers will be properly destroyed
        LOG_DEBUG("LightVolumeRenderer destroyed");
    }

    bool LightVolumeRenderer::Initialize(
        const Core::Ref<RHI::RHIDevice>& device,
        const Core::Ref<RHI::RHIDeletionQueue>& deletionQueue,
        const LightVolumeRendererDesc& desc)
    {
        if (!device)
        {
            LOG_ERROR("Device cannot be null");
            return false;
        }

        m_Device = device;
        m_DeletionQueue = deletionQueue;
        m_ColorFormat = desc.ColorFormat;
        m_DepthStencilFormat = desc.DepthStencilFormat;

        // Generate sphere mesh for point lights
        if (!GenerateSphereMesh(device, desc.SphereSegments))
        {
            LOG_ERROR("Failed to generate sphere mesh");
            return false;
        }

        // Generate cone mesh for spot lights
        if (!GenerateConeMesh(device, desc.ConeSegments))
        {
            LOG_ERROR("Failed to generate cone mesh");
            return false;
        }

        // Create shaders
        if (!CreateShaders(device))
        {
            LOG_ERROR("Failed to create shaders");
            return false;
        }

        LOG_DEBUG("LightVolumeRenderer initialized: sphere vertices={}, indices={}, cone vertices={}, indices={}",
                  m_SphereVertexCount, m_SphereIndexCount,
                  m_ConeVertexCount, m_ConeIndexCount);

        return true;
    }

    bool LightVolumeRenderer::GenerateSphereMesh(
        const Core::Ref<RHI::RHIDevice>& device,
        uint32_t segments)
    {
        // Generate a unit sphere using latitude/longitude parameterization
        // The sphere will be scaled to the light radius at render time

        std::vector<LightVolumeVertex> vertices;
        std::vector<uint32_t> indices;

        const uint32_t latSegments = segments;
        const uint32_t lonSegments = segments * 2;

        // Generate vertices
        for (uint32_t lat = 0; lat <= latSegments; ++lat)
        {
            float theta = static_cast<float>(lat) * glm::pi<float>() / static_cast<float>(latSegments);
            float sinTheta = std::sin(theta);
            float cosTheta = std::cos(theta);

            for (uint32_t lon = 0; lon <= lonSegments; ++lon)
            {
                float phi = static_cast<float>(lon) * 2.0f * glm::pi<float>() / static_cast<float>(lonSegments);
                float sinPhi = std::sin(phi);
                float cosPhi = std::cos(phi);

                LightVolumeVertex vertex;
                vertex.Position.x = cosPhi * sinTheta;
                vertex.Position.y = cosTheta;
                vertex.Position.z = sinPhi * sinTheta;

                vertices.push_back(vertex);
            }
        }

        // Generate indices
        for (uint32_t lat = 0; lat < latSegments; ++lat)
        {
            for (uint32_t lon = 0; lon < lonSegments; ++lon)
            {
                uint32_t current = lat * (lonSegments + 1) + lon;
                uint32_t next = current + lonSegments + 1;

                // First triangle
                indices.push_back(current);
                indices.push_back(next);
                indices.push_back(current + 1);

                // Second triangle
                indices.push_back(current + 1);
                indices.push_back(next);
                indices.push_back(next + 1);
            }
        }

        m_SphereVertexCount = static_cast<uint32_t>(vertices.size());
        m_SphereIndexCount = static_cast<uint32_t>(indices.size());

        // Create vertex buffer
        RHI::BufferDesc vertexBufferDesc{};
        vertexBufferDesc.Size = sizeof(LightVolumeVertex) * vertices.size();
        vertexBufferDesc.Usage = RHI::BufferUsage::Vertex;
        vertexBufferDesc.Memory = RHI::MemoryUsage::GpuOnly;
        vertexBufferDesc.DebugName = "LightVolumeSphereVertexBuffer";

        m_SphereVertexBuffer = RHI::RHIBuffer::CreateWithData(
            device, vertexBufferDesc, vertices.data(), vertexBufferDesc.Size);
        if (!m_SphereVertexBuffer)
        {
            LOG_ERROR("Failed to create sphere vertex buffer");
            return false;
        }

        // Create index buffer
        RHI::BufferDesc indexBufferDesc{};
        indexBufferDesc.Size = sizeof(uint32_t) * indices.size();
        indexBufferDesc.Usage = RHI::BufferUsage::Index;
        indexBufferDesc.Memory = RHI::MemoryUsage::GpuOnly;
        indexBufferDesc.DebugName = "LightVolumeSphereIndexBuffer";

        m_SphereIndexBuffer = RHI::RHIBuffer::CreateWithData(
            device, indexBufferDesc, indices.data(), indexBufferDesc.Size);
        if (!m_SphereIndexBuffer)
        {
            LOG_ERROR("Failed to create sphere index buffer");
            return false;
        }

        return true;
    }

    bool LightVolumeRenderer::GenerateConeMesh(
        const Core::Ref<RHI::RHIDevice>& device,
        uint32_t segments)
    {
        // Generate a unit cone pointing in -Z direction with apex at origin
        // The cone has unit height and unit base radius
        // Will be scaled and rotated to match spot light direction at render time

        std::vector<LightVolumeVertex> vertices;
        std::vector<uint32_t> indices;

        // Apex vertex at origin
        vertices.push_back({{0.0f, 0.0f, 0.0f}});

        // Base center vertex
        vertices.push_back({{0.0f, 0.0f, -1.0f}});

        // Base edge vertices
        for (uint32_t i = 0; i <= segments; ++i)
        {
            float angle = static_cast<float>(i) * 2.0f * glm::pi<float>() / static_cast<float>(segments);
            float x = std::cos(angle);
            float y = std::sin(angle);

            vertices.push_back({{x, y, -1.0f}});
        }

        // Side triangles (apex to base edge)
        for (uint32_t i = 0; i < segments; ++i)
        {
            indices.push_back(0);                   // Apex
            indices.push_back(2 + i);               // Current base vertex
            indices.push_back(2 + ((i + 1) % (segments + 1))); // Next base vertex
        }

        // Base cap triangles (fan from center)
        for (uint32_t i = 0; i < segments; ++i)
        {
            indices.push_back(1);                   // Base center
            indices.push_back(2 + ((i + 1) % (segments + 1))); // Next base vertex (reversed for correct winding)
            indices.push_back(2 + i);               // Current base vertex
        }

        m_ConeVertexCount = static_cast<uint32_t>(vertices.size());
        m_ConeIndexCount = static_cast<uint32_t>(indices.size());

        // Create vertex buffer
        RHI::BufferDesc vertexBufferDesc{};
        vertexBufferDesc.Size = sizeof(LightVolumeVertex) * vertices.size();
        vertexBufferDesc.Usage = RHI::BufferUsage::Vertex;
        vertexBufferDesc.Memory = RHI::MemoryUsage::GpuOnly;
        vertexBufferDesc.DebugName = "LightVolumeConeVertexBuffer";

        m_ConeVertexBuffer = RHI::RHIBuffer::CreateWithData(
            device, vertexBufferDesc, vertices.data(), vertexBufferDesc.Size);
        if (!m_ConeVertexBuffer)
        {
            LOG_ERROR("Failed to create cone vertex buffer");
            return false;
        }

        // Create index buffer
        RHI::BufferDesc indexBufferDesc{};
        indexBufferDesc.Size = sizeof(uint32_t) * indices.size();
        indexBufferDesc.Usage = RHI::BufferUsage::Index;
        indexBufferDesc.Memory = RHI::MemoryUsage::GpuOnly;
        indexBufferDesc.DebugName = "LightVolumeConeIndexBuffer";

        m_ConeIndexBuffer = RHI::RHIBuffer::CreateWithData(
            device, indexBufferDesc, indices.data(), indexBufferDesc.Size);
        if (!m_ConeIndexBuffer)
        {
            LOG_ERROR("Failed to create cone index buffer");
            return false;
        }

        return true;
    }

    bool LightVolumeRenderer::CreateShaders(const Core::Ref<RHI::RHIDevice>& device)
    {
        // Create vertex shader for light volumes
        m_VolumeVertexShader = RHI::RHIShader::CreateFromHLSL(
            device,
            "shaders/hlsl/vertex/light_volume.hlsl",
            RHI::ShaderStage::Vertex);
        if (!m_VolumeVertexShader)
        {
            LOG_ERROR("Failed to create light volume vertex shader");
            return false;
        }

        // Create pixel shader for point lights
        m_PointLightPixelShader = RHI::RHIShader::CreateFromHLSL(
            device,
            "shaders/hlsl/pixel/light_volume_point.hlsl",
            RHI::ShaderStage::Fragment);
        if (!m_PointLightPixelShader)
        {
            LOG_ERROR("Failed to create point light pixel shader");
            return false;
        }

        // Create pixel shader for spot lights
        m_SpotLightPixelShader = RHI::RHIShader::CreateFromHLSL(
            device,
            "shaders/hlsl/pixel/light_volume_spot.hlsl",
            RHI::ShaderStage::Fragment);
        if (!m_SpotLightPixelShader)
        {
            LOG_ERROR("Failed to create spot light pixel shader");
            return false;
        }

        return true;
    }

    bool LightVolumeRenderer::InitializePipelines(
        VkDescriptorSetLayout gBufferLayout,
        VkDescriptorSetLayout lightLayout)
    {
        if (!CreateStencilPipeline(m_Device, gBufferLayout, lightLayout))
        {
            LOG_ERROR("Failed to create stencil pipeline");
            return false;
        }

        if (!CreatePointLightPipeline(m_Device, gBufferLayout, lightLayout))
        {
            LOG_ERROR("Failed to create point light pipeline");
            return false;
        }

        if (!CreateSpotLightPipeline(m_Device, gBufferLayout, lightLayout))
        {
            LOG_ERROR("Failed to create spot light pipeline");
            return false;
        }

        m_PipelinesInitialized = true;
        LOG_DEBUG("LightVolumeRenderer pipelines initialized");
        return true;
    }

    bool LightVolumeRenderer::CreateStencilPipeline(
        const Core::Ref<RHI::RHIDevice>& device,
        VkDescriptorSetLayout gBufferLayout,
        VkDescriptorSetLayout lightLayout)
    {
        RHI::GraphicsPipelineDesc desc{};
        desc.VertexShader = m_VolumeVertexShader;
        // No fragment shader for stencil-only pass

        // Vertex input for position-only vertices
        VkVertexInputBindingDescription binding{};
        binding.binding = 0;
        binding.stride = sizeof(LightVolumeVertex);
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        desc.VertexBindings.push_back(binding);

        VkVertexInputAttributeDescription posAttr{};
        posAttr.location = 0;
        posAttr.binding = 0;
        posAttr.format = VK_FORMAT_R32G32B32_SFLOAT;
        posAttr.offset = offsetof(LightVolumeVertex, Position);
        desc.VertexAttributes.push_back(posAttr);

        // Rasterization: render back-faces only for stencil marking
        desc.CullMode = VK_CULL_MODE_FRONT_BIT;  // Cull front, render back
        desc.FrontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

        // Depth: test but don't write
        desc.DepthTestEnable = true;
        desc.DepthWriteEnable = false;
        desc.DepthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL; // Fail when behind geometry

        // Stencil: increment on depth test failure
        desc.StencilTestEnable = true;

        VkStencilOpState stencilOp{};
        stencilOp.failOp = VK_STENCIL_OP_KEEP;
        stencilOp.passOp = VK_STENCIL_OP_KEEP;
        stencilOp.depthFailOp = VK_STENCIL_OP_INCREMENT_AND_WRAP; // Mark when depth fails
        stencilOp.compareOp = VK_COMPARE_OP_ALWAYS;
        stencilOp.compareMask = 0xFF;
        stencilOp.writeMask = 0xFF;
        stencilOp.reference = 0;

        desc.StencilFront = stencilOp;
        desc.StencilBack = stencilOp;

        // No color output for stencil pass
        desc.ColorBlendAttachments.clear();

        // Render target formats (use combined depth-stencil format for both)
        desc.ColorAttachmentFormats.push_back(m_ColorFormat);
        desc.DepthAttachmentFormat = m_DepthStencilFormat;
        desc.StencilAttachmentFormat = m_DepthStencilFormat;

        // Descriptor set layouts
        desc.DescriptorSetLayouts = {gBufferLayout, lightLayout};

        // Push constants for per-light transforms
        VkPushConstantRange pushRange{};
        pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pushRange.offset = 0;
        pushRange.size = sizeof(LightVolumePushConstants);
        desc.PushConstantRanges.push_back(pushRange);

        m_StencilPipeline = RHI::RHIPipeline::CreateGraphics(device, desc);
        return m_StencilPipeline != nullptr;
    }

    bool LightVolumeRenderer::CreatePointLightPipeline(
        const Core::Ref<RHI::RHIDevice>& device,
        VkDescriptorSetLayout gBufferLayout,
        VkDescriptorSetLayout lightLayout)
    {
        RHI::GraphicsPipelineDesc desc{};
        desc.VertexShader = m_VolumeVertexShader;
        desc.FragmentShader = m_PointLightPixelShader;

        // Vertex input
        VkVertexInputBindingDescription binding{};
        binding.binding = 0;
        binding.stride = sizeof(LightVolumeVertex);
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        desc.VertexBindings.push_back(binding);

        VkVertexInputAttributeDescription posAttr{};
        posAttr.location = 0;
        posAttr.binding = 0;
        posAttr.format = VK_FORMAT_R32G32B32_SFLOAT;
        posAttr.offset = offsetof(LightVolumeVertex, Position);
        desc.VertexAttributes.push_back(posAttr);

        // Rasterization: render front-faces for lighting
        desc.CullMode = VK_CULL_MODE_BACK_BIT;
        desc.FrontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

        // Depth: test but don't write (we're just doing lighting)
        desc.DepthTestEnable = true;
        desc.DepthWriteEnable = false;
        desc.DepthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;

        // Stencil: only shade where stencil is non-zero, then clear it
        desc.StencilTestEnable = true;

        VkStencilOpState stencilOp{};
        stencilOp.failOp = VK_STENCIL_OP_KEEP;
        stencilOp.passOp = VK_STENCIL_OP_ZERO;  // Clear stencil after shading
        stencilOp.depthFailOp = VK_STENCIL_OP_ZERO;  // Also clear on depth fail
        stencilOp.compareOp = VK_COMPARE_OP_NOT_EQUAL;
        stencilOp.compareMask = 0xFF;
        stencilOp.writeMask = 0xFF;  // Allow writing to clear stencil
        stencilOp.reference = 0;

        desc.StencilFront = stencilOp;
        desc.StencilBack = stencilOp;

        // Additive blending for light accumulation
        RHI::ColorBlendAttachment blendAttachment{};
        blendAttachment.BlendEnable = true;
        blendAttachment.SrcColorFactor = VK_BLEND_FACTOR_ONE;
        blendAttachment.DstColorFactor = VK_BLEND_FACTOR_ONE;
        blendAttachment.ColorBlendOp = VK_BLEND_OP_ADD;
        blendAttachment.SrcAlphaFactor = VK_BLEND_FACTOR_ONE;
        blendAttachment.DstAlphaFactor = VK_BLEND_FACTOR_ZERO;
        blendAttachment.AlphaBlendOp = VK_BLEND_OP_ADD;
        desc.ColorBlendAttachments.push_back(blendAttachment);

        // Render target formats (use combined depth-stencil format for both)
        desc.ColorAttachmentFormats.push_back(m_ColorFormat);
        desc.DepthAttachmentFormat = m_DepthStencilFormat;
        desc.StencilAttachmentFormat = m_DepthStencilFormat;

        // Descriptor set layouts
        desc.DescriptorSetLayouts = {gBufferLayout, lightLayout};

        // Push constants
        VkPushConstantRange pushRange{};
        pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushRange.offset = 0;
        pushRange.size = sizeof(LightVolumePushConstants);
        desc.PushConstantRanges.push_back(pushRange);

        m_PointLightPipeline = RHI::RHIPipeline::CreateGraphics(device, desc);
        return m_PointLightPipeline != nullptr;
    }

    bool LightVolumeRenderer::CreateSpotLightPipeline(
        const Core::Ref<RHI::RHIDevice>& device,
        VkDescriptorSetLayout gBufferLayout,
        VkDescriptorSetLayout lightLayout)
    {
        RHI::GraphicsPipelineDesc desc{};
        desc.VertexShader = m_VolumeVertexShader;
        desc.FragmentShader = m_SpotLightPixelShader;

        // Vertex input
        VkVertexInputBindingDescription binding{};
        binding.binding = 0;
        binding.stride = sizeof(LightVolumeVertex);
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        desc.VertexBindings.push_back(binding);

        VkVertexInputAttributeDescription posAttr{};
        posAttr.location = 0;
        posAttr.binding = 0;
        posAttr.format = VK_FORMAT_R32G32B32_SFLOAT;
        posAttr.offset = offsetof(LightVolumeVertex, Position);
        desc.VertexAttributes.push_back(posAttr);

        // Rasterization
        desc.CullMode = VK_CULL_MODE_BACK_BIT;
        desc.FrontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

        // Depth
        desc.DepthTestEnable = true;
        desc.DepthWriteEnable = false;
        desc.DepthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;

        // Stencil: only shade where stencil is non-zero, then clear it
        desc.StencilTestEnable = true;

        VkStencilOpState stencilOp{};
        stencilOp.failOp = VK_STENCIL_OP_KEEP;
        stencilOp.passOp = VK_STENCIL_OP_ZERO;  // Clear stencil after shading
        stencilOp.depthFailOp = VK_STENCIL_OP_ZERO;  // Also clear on depth fail
        stencilOp.compareOp = VK_COMPARE_OP_NOT_EQUAL;
        stencilOp.compareMask = 0xFF;
        stencilOp.writeMask = 0xFF;  // Allow writing to clear stencil
        stencilOp.reference = 0;

        desc.StencilFront = stencilOp;
        desc.StencilBack = stencilOp;

        // Additive blending
        RHI::ColorBlendAttachment blendAttachment{};
        blendAttachment.BlendEnable = true;
        blendAttachment.SrcColorFactor = VK_BLEND_FACTOR_ONE;
        blendAttachment.DstColorFactor = VK_BLEND_FACTOR_ONE;
        blendAttachment.ColorBlendOp = VK_BLEND_OP_ADD;
        blendAttachment.SrcAlphaFactor = VK_BLEND_FACTOR_ONE;
        blendAttachment.DstAlphaFactor = VK_BLEND_FACTOR_ZERO;
        blendAttachment.AlphaBlendOp = VK_BLEND_OP_ADD;
        desc.ColorBlendAttachments.push_back(blendAttachment);

        // Render target formats (use combined depth-stencil format for both)
        desc.ColorAttachmentFormats.push_back(m_ColorFormat);
        desc.DepthAttachmentFormat = m_DepthStencilFormat;
        desc.StencilAttachmentFormat = m_DepthStencilFormat;

        // Descriptor set layouts
        desc.DescriptorSetLayouts = {gBufferLayout, lightLayout};

        // Push constants
        VkPushConstantRange pushRange{};
        pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushRange.offset = 0;
        pushRange.size = sizeof(LightVolumePushConstants);
        desc.PushConstantRanges.push_back(pushRange);

        m_SpotLightPipeline = RHI::RHIPipeline::CreateGraphics(device, desc);
        return m_SpotLightPipeline != nullptr;
    }

    glm::mat4 LightVolumeRenderer::CalculatePointLightMatrix(const Scene::PointLight& light) const
    {
        // Create model matrix: translate to light position, scale by radius
        glm::mat4 model = glm::translate(glm::mat4(1.0f), light.Position);
        model = glm::scale(model, glm::vec3(light.Radius));
        return model;
    }

    glm::mat4 LightVolumeRenderer::CalculateSpotLightMatrix(const Scene::SpotLight& light) const
    {
        // Calculate cone dimensions from outer angle
        // outerConeAngle is stored as cos(angle), so we need acos to get the angle
        float angle = std::acos(light.OuterConeAngle);

        // Estimate effective range from intensity using attenuation threshold
        // When attenuation falls below threshold, light contribution is negligible
        // Using inverse-square law: intensity / (distance^2) = threshold
        // Solving for distance: range = sqrt(intensity / threshold)
        const float attenuationThreshold = 0.01f;
        float range = std::sqrt(light.Intensity / attenuationThreshold);

        // Clamp range to reasonable bounds
        range = std::max(1.0f, std::min(range, 100.0f));

        // Cone base radius at the end of range
        float baseRadius = range * std::tan(angle);

        // Create rotation from default -Z direction to light direction
        glm::vec3 defaultDir(0.0f, 0.0f, -1.0f);
        glm::vec3 lightDir = glm::normalize(light.Direction);

        glm::mat4 rotation(1.0f);
        float dotProduct = glm::dot(defaultDir, lightDir);

        if (dotProduct > 0.9999f)
        {
            // Already pointing in the same direction
            rotation = glm::mat4(1.0f);
        }
        else if (dotProduct < -0.9999f)
        {
            // Pointing in opposite direction, rotate 180 degrees around any perpendicular axis
            rotation = glm::rotate(glm::mat4(1.0f), glm::pi<float>(), glm::vec3(0.0f, 1.0f, 0.0f));
        }
        else
        {
            // General case: rotate around the cross product axis
            glm::vec3 axis = glm::normalize(glm::cross(defaultDir, lightDir));
            float angleRot = std::acos(dotProduct);
            rotation = glm::rotate(glm::mat4(1.0f), angleRot, axis);
        }

        // Model matrix: translate, rotate, then scale
        glm::mat4 model = glm::translate(glm::mat4(1.0f), light.Position);
        model = model * rotation;
        model = glm::scale(model, glm::vec3(baseRadius, baseRadius, range));

        return model;
    }

    void LightVolumeRenderer::RenderPointLights(
        const Core::Ref<RHI::RHICommandBuffer>& cmdBuffer,
        const std::vector<Scene::PointLight>& lights,
        const Scene::Camera& camera,
        VkDescriptorSet gBufferSet,
        VkDescriptorSet lightSet)
    {
        if (lights.empty() || !m_PipelinesInitialized)
        {
            return;
        }

        glm::mat4 viewProj = camera.GetProjectionMatrix() * camera.GetViewMatrix();

        // Bind sphere mesh
        cmdBuffer->BindVertexBuffer(m_SphereVertexBuffer->GetHandle());
        cmdBuffer->BindIndexBuffer(m_SphereIndexBuffer->GetHandle(), VK_INDEX_TYPE_UINT32);

        for (size_t i = 0; i < lights.size(); ++i)
        {
            const auto& light = lights[i];
            glm::mat4 model = CalculatePointLightMatrix(light);

            LightVolumePushConstants pushConstants{};
            pushConstants.ModelViewProjection = viewProj * model;
            pushConstants.Model = model;
            pushConstants.LightIndex = static_cast<uint32_t>(i);

            // Pass 1: Stencil marking (back-faces)
            cmdBuffer->BindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, m_StencilPipeline->GetHandle());
            cmdBuffer->BindDescriptorSets(
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                m_StencilPipeline->GetLayout(),
                0,
                {gBufferSet, lightSet});
            cmdBuffer->PushConstants(
                m_StencilPipeline->GetLayout(),
                VK_SHADER_STAGE_VERTEX_BIT,
                0,
                sizeof(LightVolumePushConstants),
                &pushConstants);
            cmdBuffer->DrawIndexed(m_SphereIndexCount);

            // Pass 2: Lighting (front-faces with stencil test)
            cmdBuffer->BindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, m_PointLightPipeline->GetHandle());
            cmdBuffer->BindDescriptorSets(
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                m_PointLightPipeline->GetLayout(),
                0,
                {gBufferSet, lightSet});
            cmdBuffer->PushConstants(
                m_PointLightPipeline->GetLayout(),
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0,
                sizeof(LightVolumePushConstants),
                &pushConstants);
            cmdBuffer->DrawIndexed(m_SphereIndexCount);

            // Note: Stencil is automatically cleared by the lighting pass
            // (passOp = VK_STENCIL_OP_ZERO) so no explicit clear is needed
        }
    }

    void LightVolumeRenderer::RenderSpotLights(
        const Core::Ref<RHI::RHICommandBuffer>& cmdBuffer,
        const std::vector<Scene::SpotLight>& lights,
        const Scene::Camera& camera,
        VkDescriptorSet gBufferSet,
        VkDescriptorSet lightSet)
    {
        if (lights.empty() || !m_PipelinesInitialized)
        {
            return;
        }

        glm::mat4 viewProj = camera.GetProjectionMatrix() * camera.GetViewMatrix();

        // Bind cone mesh
        cmdBuffer->BindVertexBuffer(m_ConeVertexBuffer->GetHandle());
        cmdBuffer->BindIndexBuffer(m_ConeIndexBuffer->GetHandle(), VK_INDEX_TYPE_UINT32);

        for (size_t i = 0; i < lights.size(); ++i)
        {
            const auto& light = lights[i];
            glm::mat4 model = CalculateSpotLightMatrix(light);

            LightVolumePushConstants pushConstants{};
            pushConstants.ModelViewProjection = viewProj * model;
            pushConstants.Model = model;
            pushConstants.LightIndex = static_cast<uint32_t>(i);

            // Pass 1: Stencil marking (back-faces)
            cmdBuffer->BindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, m_StencilPipeline->GetHandle());
            cmdBuffer->BindDescriptorSets(
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                m_StencilPipeline->GetLayout(),
                0,
                {gBufferSet, lightSet});
            cmdBuffer->PushConstants(
                m_StencilPipeline->GetLayout(),
                VK_SHADER_STAGE_VERTEX_BIT,
                0,
                sizeof(LightVolumePushConstants),
                &pushConstants);
            cmdBuffer->DrawIndexed(m_ConeIndexCount);

            // Pass 2: Lighting (front-faces with stencil test)
            cmdBuffer->BindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, m_SpotLightPipeline->GetHandle());
            cmdBuffer->BindDescriptorSets(
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                m_SpotLightPipeline->GetLayout(),
                0,
                {gBufferSet, lightSet});
            cmdBuffer->PushConstants(
                m_SpotLightPipeline->GetLayout(),
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0,
                sizeof(LightVolumePushConstants),
                &pushConstants);
            cmdBuffer->DrawIndexed(m_ConeIndexCount);

            // Note: Stencil is automatically cleared by the lighting pass
            // (passOp = VK_STENCIL_OP_ZERO) so no explicit clear is needed
        }
    }

} // namespace Renderer
