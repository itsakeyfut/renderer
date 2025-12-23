/**
 * @file CascadedShadowMap.cpp
 * @brief Cascaded Shadow Map implementation.
 */

#include "Renderer/CascadedShadowMap.h"
#include "RHI/RHICommandBuffer.h"
#include "RHI/RHIDeletionQueue.h"
#include "RHI/RHIDevice.h"
#include "RHI/RHISampler.h"
#include "Scene/Camera.h"
#include "Core/Log.h"

#include <vk_mem_alloc.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

namespace Renderer
{
    Core::Ref<CascadedShadowMap> CascadedShadowMap::Create(
        const Core::Ref<RHI::RHIDevice>& device,
        const Core::Ref<RHI::RHIDeletionQueue>& deletionQueue,
        const CascadedShadowMapDesc& desc)
    {
        auto csm = Core::Ref<CascadedShadowMap>(new CascadedShadowMap());
        if (!csm->Initialize(device, deletionQueue, desc))
        {
            return nullptr;
        }
        return csm;
    }

    CascadedShadowMap::~CascadedShadowMap()
    {
        // Queue cascade image views for deferred deletion
        if (m_DeletionQueue)
        {
            VkDevice device = m_Device;

            // Delete per-cascade image views
            for (uint32_t i = 0; i < CASCADE_COUNT; ++i)
            {
                if (m_CascadeImageViews[i] != VK_NULL_HANDLE)
                {
                    VkImageView imageView = m_CascadeImageViews[i];
                    m_DeletionQueue->Push([device, imageView]() {
                        vkDestroyImageView(device, imageView, nullptr);
                    });
                    m_CascadeImageViews[i] = VK_NULL_HANDLE;
                }
            }

            // Delete array image view
            if (m_ArrayImageView != VK_NULL_HANDLE)
            {
                VkImageView imageView = m_ArrayImageView;
                m_DeletionQueue->Push([device, imageView]() {
                    vkDestroyImageView(device, imageView, nullptr);
                    LOG_DEBUG("Destroyed CSM array image view (deferred)");
                });
                m_ArrayImageView = VK_NULL_HANDLE;
            }

            // Delete image
            if (m_Image != VK_NULL_HANDLE && m_Allocator != nullptr)
            {
                VmaAllocator allocator = m_Allocator;
                VkImage image = m_Image;
                VmaAllocation allocation = m_Allocation;
                m_DeletionQueue->Push([allocator, image, allocation]() {
                    vmaDestroyImage(allocator, image, allocation);
                    LOG_DEBUG("Destroyed CSM image (deferred)");
                });
                m_Image = VK_NULL_HANDLE;
                m_Allocation = nullptr;
            }

            // Delete sampler
            if (m_Sampler)
            {
                Core::Ref<RHI::RHISampler> sampler = std::move(m_Sampler);
                m_DeletionQueue->Push([sampler]() {
                    LOG_DEBUG("Destroyed CSM sampler (deferred)");
                });
            }
        }
        m_Sampler.reset();
    }

    bool CascadedShadowMap::Initialize(
        const Core::Ref<RHI::RHIDevice>& device,
        const Core::Ref<RHI::RHIDeletionQueue>& deletionQueue,
        const CascadedShadowMapDesc& desc)
    {
        if (!deletionQueue)
        {
            LOG_ERROR("Deletion queue cannot be null");
            return false;
        }

        m_Device = device->GetHandle();
        m_Allocator = device->GetAllocator();
        m_DeletionQueue = deletionQueue;
        m_Resolution = desc.Resolution;
        m_Format = desc.Format;
        m_SplitLambda = desc.SplitLambda;

        if (m_Resolution == 0)
        {
            LOG_ERROR("Invalid CSM resolution: {}", m_Resolution);
            return false;
        }

        // Initialize cascade layouts to undefined
        for (uint32_t i = 0; i < CASCADE_COUNT; ++i)
        {
            m_CascadeLayouts[i] = VK_IMAGE_LAYOUT_UNDEFINED;
        }

        if (!CreateImage(device))
        {
            return false;
        }

        if (!CreateImageViews(device))
        {
            return false;
        }

        if (!CreateSampler(device))
        {
            return false;
        }

        LOG_DEBUG("Created cascaded shadow map: {}x{} x {} cascades",
                  m_Resolution, m_Resolution, CASCADE_COUNT);
        return true;
    }

    bool CascadedShadowMap::CreateImage(const Core::Ref<RHI::RHIDevice>& device)
    {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = m_Resolution;
        imageInfo.extent.height = m_Resolution;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = CASCADE_COUNT;
        imageInfo.format = m_Format;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                          VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        VkResult result = vmaCreateImage(
            device->GetAllocator(),
            &imageInfo,
            &allocInfo,
            &m_Image,
            &m_Allocation,
            nullptr);

        if (result != VK_SUCCESS)
        {
            LOG_ERROR("Failed to create CSM image: {}", static_cast<int>(result));
            return false;
        }

        return true;
    }

    bool CascadedShadowMap::CreateImageViews(const Core::Ref<RHI::RHIDevice>& device)
    {
        // Create array image view for sampling all cascades
        VkImageViewCreateInfo arrayViewInfo{};
        arrayViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        arrayViewInfo.image = m_Image;
        arrayViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        arrayViewInfo.format = m_Format;
        arrayViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        arrayViewInfo.subresourceRange.baseMipLevel = 0;
        arrayViewInfo.subresourceRange.levelCount = 1;
        arrayViewInfo.subresourceRange.baseArrayLayer = 0;
        arrayViewInfo.subresourceRange.layerCount = CASCADE_COUNT;

        VkResult result = vkCreateImageView(
            device->GetHandle(),
            &arrayViewInfo,
            nullptr,
            &m_ArrayImageView);

        if (result != VK_SUCCESS)
        {
            LOG_ERROR("Failed to create CSM array image view: {}", static_cast<int>(result));
            return false;
        }

        // Create per-cascade image views for rendering
        for (uint32_t i = 0; i < CASCADE_COUNT; ++i)
        {
            VkImageViewCreateInfo cascadeViewInfo{};
            cascadeViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            cascadeViewInfo.image = m_Image;
            cascadeViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            cascadeViewInfo.format = m_Format;
            cascadeViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            cascadeViewInfo.subresourceRange.baseMipLevel = 0;
            cascadeViewInfo.subresourceRange.levelCount = 1;
            cascadeViewInfo.subresourceRange.baseArrayLayer = i;
            cascadeViewInfo.subresourceRange.layerCount = 1;

            result = vkCreateImageView(
                device->GetHandle(),
                &cascadeViewInfo,
                nullptr,
                &m_CascadeImageViews[i]);

            if (result != VK_SUCCESS)
            {
                LOG_ERROR("Failed to create CSM cascade {} image view: {}",
                          i, static_cast<int>(result));
                return false;
            }
        }

        return true;
    }

    bool CascadedShadowMap::CreateSampler(const Core::Ref<RHI::RHIDevice>& device)
    {
        m_Sampler = RHI::RHISampler::CreateShadow(device);
        if (!m_Sampler)
        {
            LOG_ERROR("Failed to create CSM sampler");
            return false;
        }
        return true;
    }

    void CascadedShadowMap::CalculateCascadeSplits(float nearClip, float farClip)
    {
        // Use practical split scheme (logarithmic/uniform blend)
        // From "GPU Pro: Advanced Rendering Techniques"

        float range = farClip - nearClip;
        float ratio = farClip / nearClip;

        for (uint32_t i = 0; i < CASCADE_COUNT + 1; ++i)
        {
            float p = static_cast<float>(i) / static_cast<float>(CASCADE_COUNT);

            // Logarithmic split: gives more resolution to near range
            float logSplit = nearClip * std::pow(ratio, p);

            // Uniform split: equal depth range per cascade
            float uniformSplit = nearClip + range * p;

            // Blend between logarithmic and uniform using lambda
            m_CascadeSplits[i] = m_SplitLambda * logSplit +
                                  (1.0f - m_SplitLambda) * uniformSplit;
        }
    }

    glm::mat4 CascadedShadowMap::CalculateLightSpaceMatrix(
        const Scene::Camera& camera,
        float nearSplit,
        float farSplit,
        const glm::vec3& lightDir)
    {
        // Calculate frustum corners in world space using inverse view-projection matrix
        glm::mat4 invViewProj = glm::inverse(camera.GetProjectionMatrix() * camera.GetViewMatrix());

        // NDC corners (near and far planes)
        std::array<glm::vec3, 8> frustumCorners;

        // Calculate the Z values in NDC for our split range
        // With regular depth [0,1]: near = 0, far = 1
        float nearZ = (nearSplit - camera.GetNearPlane()) /
                      (camera.GetFarPlane() - camera.GetNearPlane());
        float farZ = (farSplit - camera.GetNearPlane()) /
                     (camera.GetFarPlane() - camera.GetNearPlane());

        // Define NDC frustum corners
        const glm::vec4 ndcCorners[8] = {
            // Near plane
            glm::vec4(-1.0f,  1.0f, nearZ, 1.0f),  // top-left
            glm::vec4( 1.0f,  1.0f, nearZ, 1.0f),  // top-right
            glm::vec4( 1.0f, -1.0f, nearZ, 1.0f),  // bottom-right
            glm::vec4(-1.0f, -1.0f, nearZ, 1.0f),  // bottom-left
            // Far plane
            glm::vec4(-1.0f,  1.0f, farZ, 1.0f),
            glm::vec4( 1.0f,  1.0f, farZ, 1.0f),
            glm::vec4( 1.0f, -1.0f, farZ, 1.0f),
            glm::vec4(-1.0f, -1.0f, farZ, 1.0f)
        };

        // Transform to world space
        for (int i = 0; i < 8; ++i)
        {
            glm::vec4 worldPos = invViewProj * ndcCorners[i];
            frustumCorners[i] = glm::vec3(worldPos) / worldPos.w;
        }

        // Calculate frustum center
        glm::vec3 frustumCenter(0.0f);
        for (const auto& corner : frustumCorners)
        {
            frustumCenter += corner;
        }
        frustumCenter /= 8.0f;

        // Calculate bounding sphere radius for stable shadows
        float radius = 0.0f;
        for (const auto& corner : frustumCorners)
        {
            float distance = glm::length(corner - frustumCenter);
            radius = std::max(radius, distance);
        }

        // Round up radius to reduce shadow edge swimming
        radius = std::ceil(radius * 16.0f) / 16.0f;

        // Create orthographic projection bounds
        glm::vec3 maxExtents = glm::vec3(radius);
        glm::vec3 minExtents = -maxExtents;

        // Create light view matrix
        glm::vec3 lightPos = frustumCenter - glm::normalize(lightDir) * radius;
        glm::mat4 lightView = glm::lookAt(
            lightPos,
            frustumCenter,
            glm::vec3(0.0f, 1.0f, 0.0f));

        // Create orthographic projection
        // Use larger depth range for safety
        float zMult = 3.0f;  // Extend shadow map depth range
        glm::mat4 lightProjection = glm::ortho(
            minExtents.x, maxExtents.x,
            minExtents.y, maxExtents.y,
            -maxExtents.z * zMult, maxExtents.z * zMult);

        // Apply Vulkan Y-flip
        lightProjection[1][1] *= -1.0f;

        // Texel snapping to reduce shadow edge swimming
        glm::mat4 shadowMatrix = lightProjection * lightView;
        glm::vec4 shadowOrigin = shadowMatrix * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        shadowOrigin = shadowOrigin * (static_cast<float>(m_Resolution) / 2.0f);

        glm::vec4 roundedOrigin = glm::round(shadowOrigin);
        glm::vec4 roundOffset = roundedOrigin - shadowOrigin;
        roundOffset = roundOffset * (2.0f / static_cast<float>(m_Resolution));
        roundOffset.z = 0.0f;
        roundOffset.w = 0.0f;

        lightProjection[3] += roundOffset;

        return lightProjection * lightView;
    }

    void CascadedShadowMap::Update(const Scene::Camera& camera, const glm::vec3& lightDirection)
    {
        float nearClip = camera.GetNearPlane();
        float farClip = camera.GetFarPlane();

        // Limit far plane for shadow calculation to avoid overly large cascades
        float shadowFar = std::min(farClip, 200.0f);

        // Calculate cascade split depths
        CalculateCascadeSplits(nearClip, shadowFar);

        // Calculate light-space matrices for each cascade
        for (uint32_t i = 0; i < CASCADE_COUNT; ++i)
        {
            float nearSplit = m_CascadeSplits[i];
            float farSplit = m_CascadeSplits[i + 1];

            m_Cascades[i].ViewProjection = CalculateLightSpaceMatrix(
                camera, nearSplit, farSplit, lightDirection);

            // Store split depth in clip space for shader comparison
            // Convert view space depth to clip space
            glm::vec4 clipPos = camera.GetProjectionMatrix() *
                                glm::vec4(0.0f, 0.0f, -farSplit, 1.0f);
            m_Cascades[i].SplitDepth = clipPos.z / clipPos.w;

            // Initialize padding
            m_Cascades[i].Padding[0] = 0.0f;
            m_Cascades[i].Padding[1] = 0.0f;
            m_Cascades[i].Padding[2] = 0.0f;
        }
    }

    void CascadedShadowMap::BeginCascade(
        const Core::Ref<RHI::RHICommandBuffer>& cmdBuffer,
        uint32_t cascadeIndex)
    {
        if (cascadeIndex >= CASCADE_COUNT)
        {
            LOG_ERROR("Invalid cascade index: {}", cascadeIndex);
            return;
        }

        // Transition cascade layer to attachment optimal
        if (m_CascadeLayouts[cascadeIndex] != VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
        {
            TransitionCascadeLayout(
                cmdBuffer,
                cascadeIndex,
                m_CascadeLayouts[cascadeIndex],
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
        }

        // Configure rendering for shadow pass (depth-only)
        RHI::RenderingConfig config;
        config.RenderArea = {{0, 0}, {m_Resolution, m_Resolution}};
        config.LayerCount = 1;

        // No color attachments for shadow pass
        config.ColorAttachments.clear();

        // Depth attachment configuration
        config.Depth.ImageView = m_CascadeImageViews[cascadeIndex];
        config.Depth.Layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        config.Depth.LoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        config.Depth.StoreOp = VK_ATTACHMENT_STORE_OP_STORE;
        config.Depth.ClearValue = {1.0f, 0};

        cmdBuffer->BeginRendering(config);

        // Set viewport and scissor
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(m_Resolution);
        viewport.height = static_cast<float>(m_Resolution);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        cmdBuffer->SetViewport(viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = {m_Resolution, m_Resolution};
        cmdBuffer->SetScissor(scissor);
    }

    void CascadedShadowMap::EndCascade(
        const Core::Ref<RHI::RHICommandBuffer>& cmdBuffer,
        uint32_t cascadeIndex)
    {
        if (cascadeIndex >= CASCADE_COUNT)
        {
            LOG_ERROR("Invalid cascade index: {}", cascadeIndex);
            return;
        }

        cmdBuffer->EndRendering();
    }

    void CascadedShadowMap::TransitionToShaderRead(
        const Core::Ref<RHI::RHICommandBuffer>& cmdBuffer)
    {
        // Transition all cascade layers to shader read optimal
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = m_Image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = CASCADE_COUNT;
        barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        cmdBuffer->PipelineBarrier(
            VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            {}, {}, {barrier});

        for (uint32_t i = 0; i < CASCADE_COUNT; ++i)
        {
            m_CascadeLayouts[i] = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }
    }

    void CascadedShadowMap::TransitionCascadeLayout(
        const Core::Ref<RHI::RHICommandBuffer>& cmdBuffer,
        uint32_t cascadeIndex,
        VkImageLayout oldLayout,
        VkImageLayout newLayout)
    {
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = m_Image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = cascadeIndex;
        barrier.subresourceRange.layerCount = 1;

        VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;

        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
            newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
        {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            dstStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL &&
                 newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
        {
            barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            srcStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            dstStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL &&
                 newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            srcStage = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        else
        {
            LOG_WARN("Unsupported CSM cascade layout transition: {} -> {}",
                     static_cast<int>(oldLayout), static_cast<int>(newLayout));
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        }

        cmdBuffer->PipelineBarrier(srcStage, dstStage, {}, {}, {barrier});
        m_CascadeLayouts[cascadeIndex] = newLayout;
    }

    const glm::mat4& CascadedShadowMap::GetLightSpaceMatrix(uint32_t cascadeIndex) const
    {
        if (cascadeIndex >= CASCADE_COUNT)
        {
            static glm::mat4 identity(1.0f);
            return identity;
        }
        return m_Cascades[cascadeIndex].ViewProjection;
    }

    float CascadedShadowMap::GetSplitDepth(uint32_t cascadeIndex) const
    {
        if (cascadeIndex >= CASCADE_COUNT)
        {
            return 1.0f;
        }
        return m_Cascades[cascadeIndex].SplitDepth;
    }

    VkImageView CascadedShadowMap::GetCascadeImageView(uint32_t cascadeIndex) const
    {
        if (cascadeIndex >= CASCADE_COUNT)
        {
            return VK_NULL_HANDLE;
        }
        return m_CascadeImageViews[cascadeIndex];
    }

    CascadedShadowMapUBO CascadedShadowMap::GetUBOData() const
    {
        CascadedShadowMapUBO ubo;

        for (uint32_t i = 0; i < CASCADE_COUNT; ++i)
        {
            ubo.Cascades[i] = m_Cascades[i];
        }

        ubo.ShadowBias = m_ShadowBias;
        ubo.NormalBias = m_NormalBias;
        ubo.ShadowMapSize = static_cast<float>(m_Resolution);
        ubo.Padding = 0.0f;

        return ubo;
    }

} // namespace Renderer
