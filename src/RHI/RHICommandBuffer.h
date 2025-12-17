/**
 * @file RHICommandBuffer.h
 * @brief Vulkan Command Buffer wrapper for the RHI layer.
 *
 * Manages VkCommandBuffer and provides a high-level interface for
 * recording GPU commands including rendering, pipeline binding, and draw calls.
 */

#pragma once

#include "Core/Types.h"

#include <vulkan/vulkan.h>
#include <vector>

namespace RHI
{
    // Forward declarations
    class RHIDevice;
    class RHICommandPool;

    /**
     * @brief Vulkan Command Buffer wrapper class
     *
     * Provides a high-level interface for recording GPU commands.
     * Command buffers are allocated from a command pool and can be
     * recorded, reset, and submitted to queues.
     *
     * Primary command buffers can be submitted directly to queues.
     * Secondary command buffers must be executed from primary buffers.
     *
     * Usage:
     * @code
     * auto cmdBuffer = RHICommandBuffer::Create(device, pool->GetHandle());
     * cmdBuffer->Begin();
     * cmdBuffer->BeginRenderPass(renderPassInfo);
     * cmdBuffer->BindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
     * cmdBuffer->Draw(3, 1, 0, 0);
     * cmdBuffer->EndRenderPass();
     * cmdBuffer->End();
     * @endcode
     */
    class RHICommandBuffer
    {
    public:
        /**
         * @brief Factory method to create a Command Buffer
         * @param device The logical device
         * @param pool The command pool to allocate from
         * @param level Command buffer level (primary or secondary)
         * @return Shared pointer to the created command buffer, or nullptr on failure
         */
        static Core::Ref<RHICommandBuffer> Create(
            const Core::Ref<RHIDevice>& device,
            VkCommandPool pool,
            VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);

        /**
         * @brief Destructor - frees VkCommandBuffer
         */
        ~RHICommandBuffer();

        // Non-copyable
        RHICommandBuffer(const RHICommandBuffer&) = delete;
        RHICommandBuffer& operator=(const RHICommandBuffer&) = delete;

        // Non-movable
        RHICommandBuffer(RHICommandBuffer&&) = delete;
        RHICommandBuffer& operator=(RHICommandBuffer&&) = delete;

        /**
         * @brief Get the native VkCommandBuffer handle
         * @return VkCommandBuffer handle
         */
        VkCommandBuffer GetHandle() const { return m_CommandBuffer; }

        /**
         * @brief Get the command buffer level
         * @return VkCommandBufferLevel
         */
        VkCommandBufferLevel GetLevel() const { return m_Level; }

        /**
         * @brief Check if the command buffer is currently recording
         * @return true if recording, false otherwise
         */
        bool IsRecording() const { return m_IsRecording; }

        // ============================================================
        // Recording Control
        // ============================================================

        /**
         * @brief Begin command buffer recording
         * @param flags Usage flags (e.g., VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT)
         */
        void Begin(VkCommandBufferUsageFlags flags = 0);

        /**
         * @brief End command buffer recording
         */
        void End();

        /**
         * @brief Reset the command buffer to initial state
         * @param releaseResources If true, resources are returned to the pool
         */
        void Reset(bool releaseResources = false);

        // ============================================================
        // Render Pass Commands (Legacy)
        // ============================================================

        /**
         * @brief Begin a render pass
         * @param info Render pass begin info
         * @param contents How commands in the render pass will be provided
         */
        void BeginRenderPass(
            const VkRenderPassBeginInfo& info,
            VkSubpassContents contents = VK_SUBPASS_CONTENTS_INLINE);

        /**
         * @brief End the current render pass
         */
        void EndRenderPass();

        /**
         * @brief Move to the next subpass
         * @param contents How commands in the next subpass will be provided
         */
        void NextSubpass(VkSubpassContents contents = VK_SUBPASS_CONTENTS_INLINE);

        // ============================================================
        // Dynamic Rendering (Vulkan 1.3)
        // ============================================================

        /**
         * @brief Begin dynamic rendering
         * @param renderingInfo Rendering info structure
         */
        void BeginRendering(const VkRenderingInfo& renderingInfo);

        /**
         * @brief End dynamic rendering
         */
        void EndRendering();

        // ============================================================
        // Pipeline Commands
        // ============================================================

        /**
         * @brief Bind a pipeline
         * @param bindPoint Pipeline bind point (graphics, compute, ray tracing)
         * @param pipeline Pipeline to bind
         */
        void BindPipeline(VkPipelineBindPoint bindPoint, VkPipeline pipeline);

        // ============================================================
        // Vertex/Index Buffer Commands
        // ============================================================

        /**
         * @brief Bind a vertex buffer
         * @param buffer Vertex buffer to bind
         * @param offset Offset into the buffer
         * @param binding Binding index (default 0)
         */
        void BindVertexBuffer(VkBuffer buffer, VkDeviceSize offset = 0, uint32_t binding = 0);

        /**
         * @brief Bind multiple vertex buffers
         * @param firstBinding First binding index
         * @param buffers Vector of buffers to bind
         * @param offsets Vector of offsets into each buffer
         */
        void BindVertexBuffers(
            uint32_t firstBinding,
            const std::vector<VkBuffer>& buffers,
            const std::vector<VkDeviceSize>& offsets);

        /**
         * @brief Bind an index buffer
         * @param buffer Index buffer to bind
         * @param indexType Type of indices (UINT16 or UINT32)
         * @param offset Offset into the buffer
         */
        void BindIndexBuffer(VkBuffer buffer, VkIndexType indexType, VkDeviceSize offset = 0);

        // ============================================================
        // Descriptor Set Commands
        // ============================================================

        /**
         * @brief Bind descriptor sets
         * @param bindPoint Pipeline bind point
         * @param layout Pipeline layout
         * @param firstSet First set number
         * @param sets Descriptor sets to bind
         * @param dynamicOffsets Dynamic offsets for dynamic uniform/storage buffers
         */
        void BindDescriptorSets(
            VkPipelineBindPoint bindPoint,
            VkPipelineLayout layout,
            uint32_t firstSet,
            const std::vector<VkDescriptorSet>& sets,
            const std::vector<uint32_t>& dynamicOffsets = {});

        // ============================================================
        // Draw Commands
        // ============================================================

        /**
         * @brief Draw non-indexed primitives
         * @param vertexCount Number of vertices to draw
         * @param instanceCount Number of instances to draw
         * @param firstVertex Index of the first vertex
         * @param firstInstance Instance ID of the first instance
         */
        void Draw(
            uint32_t vertexCount,
            uint32_t instanceCount = 1,
            uint32_t firstVertex = 0,
            uint32_t firstInstance = 0);

        /**
         * @brief Draw indexed primitives
         * @param indexCount Number of indices to draw
         * @param instanceCount Number of instances to draw
         * @param firstIndex Index of the first index
         * @param vertexOffset Value added to vertex index before indexing into vertex buffer
         * @param firstInstance Instance ID of the first instance
         */
        void DrawIndexed(
            uint32_t indexCount,
            uint32_t instanceCount = 1,
            uint32_t firstIndex = 0,
            int32_t vertexOffset = 0,
            uint32_t firstInstance = 0);

        /**
         * @brief Draw non-indexed primitives with indirect parameters
         * @param buffer Buffer containing draw parameters
         * @param offset Offset into the buffer
         * @param drawCount Number of draws to execute
         * @param stride Stride between successive draw parameter sets
         */
        void DrawIndirect(
            VkBuffer buffer,
            VkDeviceSize offset,
            uint32_t drawCount,
            uint32_t stride);

        /**
         * @brief Draw indexed primitives with indirect parameters
         * @param buffer Buffer containing draw parameters
         * @param offset Offset into the buffer
         * @param drawCount Number of draws to execute
         * @param stride Stride between successive draw parameter sets
         */
        void DrawIndexedIndirect(
            VkBuffer buffer,
            VkDeviceSize offset,
            uint32_t drawCount,
            uint32_t stride);

        // ============================================================
        // Compute Commands
        // ============================================================

        /**
         * @brief Dispatch compute work
         * @param groupCountX Number of work groups in X dimension
         * @param groupCountY Number of work groups in Y dimension
         * @param groupCountZ Number of work groups in Z dimension
         */
        void Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);

        /**
         * @brief Dispatch compute work with indirect parameters
         * @param buffer Buffer containing dispatch parameters
         * @param offset Offset into the buffer
         */
        void DispatchIndirect(VkBuffer buffer, VkDeviceSize offset);

        // ============================================================
        // Dynamic State Commands
        // ============================================================

        /**
         * @brief Set viewport dynamically
         * @param viewport Viewport parameters
         * @param firstViewport Index of the first viewport (default 0)
         */
        void SetViewport(const VkViewport& viewport, uint32_t firstViewport = 0);

        /**
         * @brief Set multiple viewports dynamically
         * @param firstViewport Index of the first viewport
         * @param viewports Vector of viewport parameters
         */
        void SetViewports(uint32_t firstViewport, const std::vector<VkViewport>& viewports);

        /**
         * @brief Set scissor rectangle dynamically
         * @param scissor Scissor rectangle
         * @param firstScissor Index of the first scissor (default 0)
         */
        void SetScissor(const VkRect2D& scissor, uint32_t firstScissor = 0);

        /**
         * @brief Set multiple scissor rectangles dynamically
         * @param firstScissor Index of the first scissor
         * @param scissors Vector of scissor rectangles
         */
        void SetScissors(uint32_t firstScissor, const std::vector<VkRect2D>& scissors);

        /**
         * @brief Set line width dynamically
         * @param lineWidth Line width
         */
        void SetLineWidth(float lineWidth);

        /**
         * @brief Set depth bias dynamically
         * @param constantFactor Constant depth value
         * @param clamp Maximum depth bias
         * @param slopeFactor Slope factor for depth bias
         */
        void SetDepthBias(float constantFactor, float clamp, float slopeFactor);

        /**
         * @brief Set blend constants dynamically
         * @param blendConstants Array of 4 blend constant values (RGBA)
         */
        void SetBlendConstants(const float blendConstants[4]);

        /**
         * @brief Set depth bounds dynamically
         * @param minDepthBounds Minimum depth bound
         * @param maxDepthBounds Maximum depth bound
         */
        void SetDepthBounds(float minDepthBounds, float maxDepthBounds);

        /**
         * @brief Set stencil compare mask dynamically
         * @param faceMask Stencil face mask
         * @param compareMask Compare mask value
         */
        void SetStencilCompareMask(VkStencilFaceFlags faceMask, uint32_t compareMask);

        /**
         * @brief Set stencil write mask dynamically
         * @param faceMask Stencil face mask
         * @param writeMask Write mask value
         */
        void SetStencilWriteMask(VkStencilFaceFlags faceMask, uint32_t writeMask);

        /**
         * @brief Set stencil reference value dynamically
         * @param faceMask Stencil face mask
         * @param reference Reference value
         */
        void SetStencilReference(VkStencilFaceFlags faceMask, uint32_t reference);

        // ============================================================
        // Push Constants
        // ============================================================

        /**
         * @brief Update push constant values
         * @param layout Pipeline layout
         * @param stageFlags Shader stages that will use the push constants
         * @param offset Offset into the push constant block
         * @param size Size of the data to update
         * @param data Pointer to the data
         */
        void PushConstants(
            VkPipelineLayout layout,
            VkShaderStageFlags stageFlags,
            uint32_t offset,
            uint32_t size,
            const void* data);

        // ============================================================
        // Synchronization Commands
        // ============================================================

        /**
         * @brief Insert a pipeline barrier
         * @param srcStageMask Source pipeline stage mask
         * @param dstStageMask Destination pipeline stage mask
         * @param memoryBarriers Memory barriers
         * @param bufferBarriers Buffer memory barriers
         * @param imageBarriers Image memory barriers
         * @param dependencyFlags Dependency flags
         */
        void PipelineBarrier(
            VkPipelineStageFlags srcStageMask,
            VkPipelineStageFlags dstStageMask,
            const std::vector<VkMemoryBarrier>& memoryBarriers = {},
            const std::vector<VkBufferMemoryBarrier>& bufferBarriers = {},
            const std::vector<VkImageMemoryBarrier>& imageBarriers = {},
            VkDependencyFlags dependencyFlags = 0);

        // ============================================================
        // Copy Commands
        // ============================================================

        /**
         * @brief Copy data between buffers
         * @param srcBuffer Source buffer
         * @param dstBuffer Destination buffer
         * @param regions Copy regions
         */
        void CopyBuffer(
            VkBuffer srcBuffer,
            VkBuffer dstBuffer,
            const std::vector<VkBufferCopy>& regions);

        /**
         * @brief Copy data from buffer to image
         * @param srcBuffer Source buffer
         * @param dstImage Destination image
         * @param dstImageLayout Current layout of destination image
         * @param regions Copy regions
         */
        void CopyBufferToImage(
            VkBuffer srcBuffer,
            VkImage dstImage,
            VkImageLayout dstImageLayout,
            const std::vector<VkBufferImageCopy>& regions);

        /**
         * @brief Copy data from image to buffer
         * @param srcImage Source image
         * @param srcImageLayout Current layout of source image
         * @param dstBuffer Destination buffer
         * @param regions Copy regions
         */
        void CopyImageToBuffer(
            VkImage srcImage,
            VkImageLayout srcImageLayout,
            VkBuffer dstBuffer,
            const std::vector<VkBufferImageCopy>& regions);

        /**
         * @brief Copy data between images
         * @param srcImage Source image
         * @param srcImageLayout Current layout of source image
         * @param dstImage Destination image
         * @param dstImageLayout Current layout of destination image
         * @param regions Copy regions
         */
        void CopyImage(
            VkImage srcImage,
            VkImageLayout srcImageLayout,
            VkImage dstImage,
            VkImageLayout dstImageLayout,
            const std::vector<VkImageCopy>& regions);

        /**
         * @brief Blit (scale/convert) image regions
         * @param srcImage Source image
         * @param srcImageLayout Current layout of source image
         * @param dstImage Destination image
         * @param dstImageLayout Current layout of destination image
         * @param regions Blit regions
         * @param filter Filtering mode for the blit
         */
        void BlitImage(
            VkImage srcImage,
            VkImageLayout srcImageLayout,
            VkImage dstImage,
            VkImageLayout dstImageLayout,
            const std::vector<VkImageBlit>& regions,
            VkFilter filter);

        // ============================================================
        // Clear Commands
        // ============================================================

        /**
         * @brief Clear color image to a specific value
         * @param image Image to clear
         * @param imageLayout Current image layout
         * @param clearColor Clear color value
         * @param ranges Subresource ranges to clear
         */
        void ClearColorImage(
            VkImage image,
            VkImageLayout imageLayout,
            const VkClearColorValue& clearColor,
            const std::vector<VkImageSubresourceRange>& ranges);

        /**
         * @brief Clear depth/stencil image to a specific value
         * @param image Image to clear
         * @param imageLayout Current image layout
         * @param depthStencil Clear depth/stencil value
         * @param ranges Subresource ranges to clear
         */
        void ClearDepthStencilImage(
            VkImage image,
            VkImageLayout imageLayout,
            const VkClearDepthStencilValue& depthStencil,
            const std::vector<VkImageSubresourceRange>& ranges);

        // ============================================================
        // Secondary Command Buffer Execution
        // ============================================================

        /**
         * @brief Execute secondary command buffers
         * @param commandBuffers Secondary command buffers to execute
         */
        void ExecuteCommands(const std::vector<VkCommandBuffer>& commandBuffers);

    private:
        /**
         * @brief Private constructor - use Create() factory method
         */
        RHICommandBuffer() = default;

        /**
         * @brief Initialize the command buffer
         * @param device The logical device
         * @param pool The command pool
         * @param level Command buffer level
         * @return true on success, false on failure
         */
        bool Initialize(
            const Core::Ref<RHIDevice>& device,
            VkCommandPool pool,
            VkCommandBufferLevel level);

        VkDevice m_Device = VK_NULL_HANDLE;
        VkCommandPool m_CommandPool = VK_NULL_HANDLE;
        VkCommandBuffer m_CommandBuffer = VK_NULL_HANDLE;
        VkCommandBufferLevel m_Level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        bool m_IsRecording = false;
    };

} // namespace RHI
