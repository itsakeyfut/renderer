/**
 * @file RHICommandBuffer.cpp
 * @brief Implementation of Vulkan Command Buffer wrapper.
 */

#include "RHI/RHICommandBuffer.h"
#include "RHI/RHIDevice.h"
#include "Core/Assert.h"
#include "Core/Log.h"

namespace RHI
{
    Core::Ref<RHICommandBuffer> RHICommandBuffer::Create(
        const Core::Ref<RHIDevice>& device,
        VkCommandPool pool,
        VkCommandBufferLevel level)
    {
        auto buffer = Core::Ref<RHICommandBuffer>(new RHICommandBuffer());

        if (!buffer->Initialize(device, pool, level))
        {
            LOG_ERROR("Failed to initialize RHI Command Buffer");
            return nullptr;
        }

        return buffer;
    }

    RHICommandBuffer::~RHICommandBuffer()
    {
        if (m_CommandBuffer != VK_NULL_HANDLE &&
            m_CommandPool != VK_NULL_HANDLE &&
            m_Device != VK_NULL_HANDLE)
        {
            vkFreeCommandBuffers(m_Device, m_CommandPool, 1, &m_CommandBuffer);
            m_CommandBuffer = VK_NULL_HANDLE;
            LOG_DEBUG("Vulkan command buffer freed");
        }
    }

    bool RHICommandBuffer::Initialize(
        const Core::Ref<RHIDevice>& device,
        VkCommandPool pool,
        VkCommandBufferLevel level)
    {
        ASSERT(device != nullptr);
        ASSERT(pool != VK_NULL_HANDLE);

        m_Device = device->GetHandle();
        m_CommandPool = pool;
        m_Level = level;

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = pool;
        allocInfo.level = level;
        allocInfo.commandBufferCount = 1;

        VkResult result = vkAllocateCommandBuffers(m_Device, &allocInfo, &m_CommandBuffer);
        if (result != VK_SUCCESS)
        {
            LOG_ERROR("Failed to allocate Vulkan command buffer, VkResult: {}", static_cast<int>(result));
            return false;
        }

        LOG_DEBUG("Vulkan command buffer allocated ({} level)",
            (level == VK_COMMAND_BUFFER_LEVEL_PRIMARY) ? "Primary" : "Secondary");

        return true;
    }

    // ============================================================
    // Recording Control
    // ============================================================

    void RHICommandBuffer::Begin(VkCommandBufferUsageFlags flags)
    {
        ASSERT(m_CommandBuffer != VK_NULL_HANDLE);
        ASSERT(!m_IsRecording);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = flags;
        beginInfo.pInheritanceInfo = nullptr;

        VkResult result = vkBeginCommandBuffer(m_CommandBuffer, &beginInfo);
        if (result != VK_SUCCESS)
        {
            LOG_ERROR("Failed to begin command buffer recording, VkResult: {}", static_cast<int>(result));
            return;
        }

        m_IsRecording = true;
    }

    void RHICommandBuffer::End()
    {
        ASSERT(m_CommandBuffer != VK_NULL_HANDLE);
        ASSERT(m_IsRecording);

        VkResult result = vkEndCommandBuffer(m_CommandBuffer);
        if (result != VK_SUCCESS)
        {
            LOG_ERROR("Failed to end command buffer recording, VkResult: {}", static_cast<int>(result));
        }

        m_IsRecording = false;
    }

    void RHICommandBuffer::Reset(bool releaseResources)
    {
        ASSERT(m_CommandBuffer != VK_NULL_HANDLE);

        VkCommandBufferResetFlags flags = 0;
        if (releaseResources)
        {
            flags |= VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT;
        }

        VkResult result = vkResetCommandBuffer(m_CommandBuffer, flags);
        if (result != VK_SUCCESS)
        {
            LOG_ERROR("Failed to reset command buffer, VkResult: {}", static_cast<int>(result));
        }

        m_IsRecording = false;
    }

    // ============================================================
    // Render Pass Commands (Legacy)
    // ============================================================

    void RHICommandBuffer::BeginRenderPass(const VkRenderPassBeginInfo& info, VkSubpassContents contents)
    {
        ASSERT(m_CommandBuffer != VK_NULL_HANDLE);
        ASSERT(m_IsRecording);

        vkCmdBeginRenderPass(m_CommandBuffer, &info, contents);
    }

    void RHICommandBuffer::EndRenderPass()
    {
        ASSERT(m_CommandBuffer != VK_NULL_HANDLE);
        ASSERT(m_IsRecording);

        vkCmdEndRenderPass(m_CommandBuffer);
    }

    void RHICommandBuffer::NextSubpass(VkSubpassContents contents)
    {
        ASSERT(m_CommandBuffer != VK_NULL_HANDLE);
        ASSERT(m_IsRecording);

        vkCmdNextSubpass(m_CommandBuffer, contents);
    }

    // ============================================================
    // Dynamic Rendering (Vulkan 1.3)
    // ============================================================

    void RHICommandBuffer::BeginRendering(const VkRenderingInfo& renderingInfo)
    {
        ASSERT(m_CommandBuffer != VK_NULL_HANDLE);
        ASSERT(m_IsRecording);

        vkCmdBeginRendering(m_CommandBuffer, &renderingInfo);
    }

    void RHICommandBuffer::EndRendering()
    {
        ASSERT(m_CommandBuffer != VK_NULL_HANDLE);
        ASSERT(m_IsRecording);

        vkCmdEndRendering(m_CommandBuffer);
    }

    // ============================================================
    // Pipeline Commands
    // ============================================================

    void RHICommandBuffer::BindPipeline(VkPipelineBindPoint bindPoint, VkPipeline pipeline)
    {
        ASSERT(m_CommandBuffer != VK_NULL_HANDLE);
        ASSERT(m_IsRecording);
        ASSERT(pipeline != VK_NULL_HANDLE);

        vkCmdBindPipeline(m_CommandBuffer, bindPoint, pipeline);
    }

    // ============================================================
    // Vertex/Index Buffer Commands
    // ============================================================

    void RHICommandBuffer::BindVertexBuffer(VkBuffer buffer, VkDeviceSize offset, uint32_t binding)
    {
        ASSERT(m_CommandBuffer != VK_NULL_HANDLE);
        ASSERT(m_IsRecording);
        ASSERT(buffer != VK_NULL_HANDLE);

        vkCmdBindVertexBuffers(m_CommandBuffer, binding, 1, &buffer, &offset);
    }

    void RHICommandBuffer::BindVertexBuffers(
        uint32_t firstBinding,
        const std::vector<VkBuffer>& buffers,
        const std::vector<VkDeviceSize>& offsets)
    {
        ASSERT(m_CommandBuffer != VK_NULL_HANDLE);
        ASSERT(m_IsRecording);
        ASSERT(!buffers.empty());
        ASSERT(buffers.size() == offsets.size());

        vkCmdBindVertexBuffers(
            m_CommandBuffer,
            firstBinding,
            static_cast<uint32_t>(buffers.size()),
            buffers.data(),
            offsets.data());
    }

    void RHICommandBuffer::BindIndexBuffer(VkBuffer buffer, VkIndexType indexType, VkDeviceSize offset)
    {
        ASSERT(m_CommandBuffer != VK_NULL_HANDLE);
        ASSERT(m_IsRecording);
        ASSERT(buffer != VK_NULL_HANDLE);

        vkCmdBindIndexBuffer(m_CommandBuffer, buffer, offset, indexType);
    }

    // ============================================================
    // Descriptor Set Commands
    // ============================================================

    void RHICommandBuffer::BindDescriptorSets(
        VkPipelineBindPoint bindPoint,
        VkPipelineLayout layout,
        uint32_t firstSet,
        const std::vector<VkDescriptorSet>& sets,
        const std::vector<uint32_t>& dynamicOffsets)
    {
        ASSERT(m_CommandBuffer != VK_NULL_HANDLE);
        ASSERT(m_IsRecording);
        ASSERT(layout != VK_NULL_HANDLE);
        ASSERT(!sets.empty());

        vkCmdBindDescriptorSets(
            m_CommandBuffer,
            bindPoint,
            layout,
            firstSet,
            static_cast<uint32_t>(sets.size()),
            sets.data(),
            static_cast<uint32_t>(dynamicOffsets.size()),
            dynamicOffsets.empty() ? nullptr : dynamicOffsets.data());
    }

    // ============================================================
    // Draw Commands
    // ============================================================

    void RHICommandBuffer::Draw(
        uint32_t vertexCount,
        uint32_t instanceCount,
        uint32_t firstVertex,
        uint32_t firstInstance)
    {
        ASSERT(m_CommandBuffer != VK_NULL_HANDLE);
        ASSERT(m_IsRecording);

        vkCmdDraw(m_CommandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
    }

    void RHICommandBuffer::DrawIndexed(
        uint32_t indexCount,
        uint32_t instanceCount,
        uint32_t firstIndex,
        int32_t vertexOffset,
        uint32_t firstInstance)
    {
        ASSERT(m_CommandBuffer != VK_NULL_HANDLE);
        ASSERT(m_IsRecording);

        vkCmdDrawIndexed(m_CommandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
    }

    void RHICommandBuffer::DrawIndirect(
        VkBuffer buffer,
        VkDeviceSize offset,
        uint32_t drawCount,
        uint32_t stride)
    {
        ASSERT(m_CommandBuffer != VK_NULL_HANDLE);
        ASSERT(m_IsRecording);
        ASSERT(buffer != VK_NULL_HANDLE);

        vkCmdDrawIndirect(m_CommandBuffer, buffer, offset, drawCount, stride);
    }

    void RHICommandBuffer::DrawIndexedIndirect(
        VkBuffer buffer,
        VkDeviceSize offset,
        uint32_t drawCount,
        uint32_t stride)
    {
        ASSERT(m_CommandBuffer != VK_NULL_HANDLE);
        ASSERT(m_IsRecording);
        ASSERT(buffer != VK_NULL_HANDLE);

        vkCmdDrawIndexedIndirect(m_CommandBuffer, buffer, offset, drawCount, stride);
    }

    // ============================================================
    // Compute Commands
    // ============================================================

    void RHICommandBuffer::Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
    {
        ASSERT(m_CommandBuffer != VK_NULL_HANDLE);
        ASSERT(m_IsRecording);

        vkCmdDispatch(m_CommandBuffer, groupCountX, groupCountY, groupCountZ);
    }

    void RHICommandBuffer::DispatchIndirect(VkBuffer buffer, VkDeviceSize offset)
    {
        ASSERT(m_CommandBuffer != VK_NULL_HANDLE);
        ASSERT(m_IsRecording);
        ASSERT(buffer != VK_NULL_HANDLE);

        vkCmdDispatchIndirect(m_CommandBuffer, buffer, offset);
    }

    // ============================================================
    // Dynamic State Commands
    // ============================================================

    void RHICommandBuffer::SetViewport(const VkViewport& viewport, uint32_t firstViewport)
    {
        ASSERT(m_CommandBuffer != VK_NULL_HANDLE);
        ASSERT(m_IsRecording);

        vkCmdSetViewport(m_CommandBuffer, firstViewport, 1, &viewport);
    }

    void RHICommandBuffer::SetViewports(uint32_t firstViewport, const std::vector<VkViewport>& viewports)
    {
        ASSERT(m_CommandBuffer != VK_NULL_HANDLE);
        ASSERT(m_IsRecording);
        ASSERT(!viewports.empty());

        vkCmdSetViewport(m_CommandBuffer, firstViewport, static_cast<uint32_t>(viewports.size()), viewports.data());
    }

    void RHICommandBuffer::SetScissor(const VkRect2D& scissor, uint32_t firstScissor)
    {
        ASSERT(m_CommandBuffer != VK_NULL_HANDLE);
        ASSERT(m_IsRecording);

        vkCmdSetScissor(m_CommandBuffer, firstScissor, 1, &scissor);
    }

    void RHICommandBuffer::SetScissors(uint32_t firstScissor, const std::vector<VkRect2D>& scissors)
    {
        ASSERT(m_CommandBuffer != VK_NULL_HANDLE);
        ASSERT(m_IsRecording);
        ASSERT(!scissors.empty());

        vkCmdSetScissor(m_CommandBuffer, firstScissor, static_cast<uint32_t>(scissors.size()), scissors.data());
    }

    void RHICommandBuffer::SetLineWidth(float lineWidth)
    {
        ASSERT(m_CommandBuffer != VK_NULL_HANDLE);
        ASSERT(m_IsRecording);

        vkCmdSetLineWidth(m_CommandBuffer, lineWidth);
    }

    void RHICommandBuffer::SetDepthBias(float constantFactor, float clamp, float slopeFactor)
    {
        ASSERT(m_CommandBuffer != VK_NULL_HANDLE);
        ASSERT(m_IsRecording);

        vkCmdSetDepthBias(m_CommandBuffer, constantFactor, clamp, slopeFactor);
    }

    void RHICommandBuffer::SetBlendConstants(const float blendConstants[4])
    {
        ASSERT(m_CommandBuffer != VK_NULL_HANDLE);
        ASSERT(m_IsRecording);

        vkCmdSetBlendConstants(m_CommandBuffer, blendConstants);
    }

    void RHICommandBuffer::SetDepthBounds(float minDepthBounds, float maxDepthBounds)
    {
        ASSERT(m_CommandBuffer != VK_NULL_HANDLE);
        ASSERT(m_IsRecording);

        vkCmdSetDepthBounds(m_CommandBuffer, minDepthBounds, maxDepthBounds);
    }

    void RHICommandBuffer::SetStencilCompareMask(VkStencilFaceFlags faceMask, uint32_t compareMask)
    {
        ASSERT(m_CommandBuffer != VK_NULL_HANDLE);
        ASSERT(m_IsRecording);

        vkCmdSetStencilCompareMask(m_CommandBuffer, faceMask, compareMask);
    }

    void RHICommandBuffer::SetStencilWriteMask(VkStencilFaceFlags faceMask, uint32_t writeMask)
    {
        ASSERT(m_CommandBuffer != VK_NULL_HANDLE);
        ASSERT(m_IsRecording);

        vkCmdSetStencilWriteMask(m_CommandBuffer, faceMask, writeMask);
    }

    void RHICommandBuffer::SetStencilReference(VkStencilFaceFlags faceMask, uint32_t reference)
    {
        ASSERT(m_CommandBuffer != VK_NULL_HANDLE);
        ASSERT(m_IsRecording);

        vkCmdSetStencilReference(m_CommandBuffer, faceMask, reference);
    }

    // ============================================================
    // Push Constants
    // ============================================================

    void RHICommandBuffer::PushConstants(
        VkPipelineLayout layout,
        VkShaderStageFlags stageFlags,
        uint32_t offset,
        uint32_t size,
        const void* data)
    {
        ASSERT(m_CommandBuffer != VK_NULL_HANDLE);
        ASSERT(m_IsRecording);
        ASSERT(layout != VK_NULL_HANDLE);
        ASSERT(data != nullptr);
        ASSERT(size > 0);

        vkCmdPushConstants(m_CommandBuffer, layout, stageFlags, offset, size, data);
    }

    // ============================================================
    // Synchronization Commands
    // ============================================================

    void RHICommandBuffer::PipelineBarrier(
        VkPipelineStageFlags srcStageMask,
        VkPipelineStageFlags dstStageMask,
        const std::vector<VkMemoryBarrier>& memoryBarriers,
        const std::vector<VkBufferMemoryBarrier>& bufferBarriers,
        const std::vector<VkImageMemoryBarrier>& imageBarriers,
        VkDependencyFlags dependencyFlags)
    {
        ASSERT(m_CommandBuffer != VK_NULL_HANDLE);
        ASSERT(m_IsRecording);

        vkCmdPipelineBarrier(
            m_CommandBuffer,
            srcStageMask,
            dstStageMask,
            dependencyFlags,
            static_cast<uint32_t>(memoryBarriers.size()),
            memoryBarriers.empty() ? nullptr : memoryBarriers.data(),
            static_cast<uint32_t>(bufferBarriers.size()),
            bufferBarriers.empty() ? nullptr : bufferBarriers.data(),
            static_cast<uint32_t>(imageBarriers.size()),
            imageBarriers.empty() ? nullptr : imageBarriers.data());
    }

    // ============================================================
    // Copy Commands
    // ============================================================

    void RHICommandBuffer::CopyBuffer(
        VkBuffer srcBuffer,
        VkBuffer dstBuffer,
        const std::vector<VkBufferCopy>& regions)
    {
        ASSERT(m_CommandBuffer != VK_NULL_HANDLE);
        ASSERT(m_IsRecording);
        ASSERT(srcBuffer != VK_NULL_HANDLE);
        ASSERT(dstBuffer != VK_NULL_HANDLE);
        ASSERT(!regions.empty());

        vkCmdCopyBuffer(m_CommandBuffer, srcBuffer, dstBuffer, static_cast<uint32_t>(regions.size()), regions.data());
    }

    void RHICommandBuffer::CopyBufferToImage(
        VkBuffer srcBuffer,
        VkImage dstImage,
        VkImageLayout dstImageLayout,
        const std::vector<VkBufferImageCopy>& regions)
    {
        ASSERT(m_CommandBuffer != VK_NULL_HANDLE);
        ASSERT(m_IsRecording);
        ASSERT(srcBuffer != VK_NULL_HANDLE);
        ASSERT(dstImage != VK_NULL_HANDLE);
        ASSERT(!regions.empty());

        vkCmdCopyBufferToImage(
            m_CommandBuffer,
            srcBuffer,
            dstImage,
            dstImageLayout,
            static_cast<uint32_t>(regions.size()),
            regions.data());
    }

    void RHICommandBuffer::CopyImageToBuffer(
        VkImage srcImage,
        VkImageLayout srcImageLayout,
        VkBuffer dstBuffer,
        const std::vector<VkBufferImageCopy>& regions)
    {
        ASSERT(m_CommandBuffer != VK_NULL_HANDLE);
        ASSERT(m_IsRecording);
        ASSERT(srcImage != VK_NULL_HANDLE);
        ASSERT(dstBuffer != VK_NULL_HANDLE);
        ASSERT(!regions.empty());

        vkCmdCopyImageToBuffer(
            m_CommandBuffer,
            srcImage,
            srcImageLayout,
            dstBuffer,
            static_cast<uint32_t>(regions.size()),
            regions.data());
    }

    void RHICommandBuffer::CopyImage(
        VkImage srcImage,
        VkImageLayout srcImageLayout,
        VkImage dstImage,
        VkImageLayout dstImageLayout,
        const std::vector<VkImageCopy>& regions)
    {
        ASSERT(m_CommandBuffer != VK_NULL_HANDLE);
        ASSERT(m_IsRecording);
        ASSERT(srcImage != VK_NULL_HANDLE);
        ASSERT(dstImage != VK_NULL_HANDLE);
        ASSERT(!regions.empty());

        vkCmdCopyImage(
            m_CommandBuffer,
            srcImage,
            srcImageLayout,
            dstImage,
            dstImageLayout,
            static_cast<uint32_t>(regions.size()),
            regions.data());
    }

    void RHICommandBuffer::BlitImage(
        VkImage srcImage,
        VkImageLayout srcImageLayout,
        VkImage dstImage,
        VkImageLayout dstImageLayout,
        const std::vector<VkImageBlit>& regions,
        VkFilter filter)
    {
        ASSERT(m_CommandBuffer != VK_NULL_HANDLE);
        ASSERT(m_IsRecording);
        ASSERT(srcImage != VK_NULL_HANDLE);
        ASSERT(dstImage != VK_NULL_HANDLE);
        ASSERT(!regions.empty());

        vkCmdBlitImage(
            m_CommandBuffer,
            srcImage,
            srcImageLayout,
            dstImage,
            dstImageLayout,
            static_cast<uint32_t>(regions.size()),
            regions.data(),
            filter);
    }

    // ============================================================
    // Clear Commands
    // ============================================================

    void RHICommandBuffer::ClearColorImage(
        VkImage image,
        VkImageLayout imageLayout,
        const VkClearColorValue& clearColor,
        const std::vector<VkImageSubresourceRange>& ranges)
    {
        ASSERT(m_CommandBuffer != VK_NULL_HANDLE);
        ASSERT(m_IsRecording);
        ASSERT(image != VK_NULL_HANDLE);
        ASSERT(!ranges.empty());

        vkCmdClearColorImage(
            m_CommandBuffer,
            image,
            imageLayout,
            &clearColor,
            static_cast<uint32_t>(ranges.size()),
            ranges.data());
    }

    void RHICommandBuffer::ClearDepthStencilImage(
        VkImage image,
        VkImageLayout imageLayout,
        const VkClearDepthStencilValue& depthStencil,
        const std::vector<VkImageSubresourceRange>& ranges)
    {
        ASSERT(m_CommandBuffer != VK_NULL_HANDLE);
        ASSERT(m_IsRecording);
        ASSERT(image != VK_NULL_HANDLE);
        ASSERT(!ranges.empty());

        vkCmdClearDepthStencilImage(
            m_CommandBuffer,
            image,
            imageLayout,
            &depthStencil,
            static_cast<uint32_t>(ranges.size()),
            ranges.data());
    }

    // ============================================================
    // Secondary Command Buffer Execution
    // ============================================================

    void RHICommandBuffer::ExecuteCommands(const std::vector<VkCommandBuffer>& commandBuffers)
    {
        ASSERT(m_CommandBuffer != VK_NULL_HANDLE);
        ASSERT(m_IsRecording);
        ASSERT(m_Level == VK_COMMAND_BUFFER_LEVEL_PRIMARY);
        ASSERT(!commandBuffers.empty());

        vkCmdExecuteCommands(
            m_CommandBuffer,
            static_cast<uint32_t>(commandBuffers.size()),
            commandBuffers.data());
    }

} // namespace RHI
