/**
 * @file RHIBuffer.h
 * @brief Vulkan Buffer wrapper for the RHI layer.
 *
 * Manages VkBuffer and VmaAllocation for GPU memory including
 * vertex buffers, index buffers, uniform buffers, and staging buffers.
 */

#pragma once

#include "Core/Types.h"
#include "RHI/RHIDevice.h"

#include <vulkan/vulkan.h>

namespace RHI
{
    /**
     * @brief Buffer usage types for simplified buffer creation
     */
    enum class BufferUsage
    {
        Vertex,         ///< Vertex buffer (device local, requires staging)
        Index,          ///< Index buffer (device local, requires staging)
        Uniform,        ///< Uniform buffer (host visible, frequently updated)
        Storage,        ///< Storage buffer (device local or host visible)
        Staging,        ///< Staging buffer for CPU->GPU transfers
        TransferSrc,    ///< Source for transfer operations
        TransferDst     ///< Destination for transfer operations
    };

    /**
     * @brief Memory location preference for buffer allocation
     */
    enum class MemoryUsage
    {
        GpuOnly,        ///< Device local memory (fastest GPU access)
        CpuOnly,        ///< Host memory (for staging)
        CpuToGpu,       ///< Host visible, device local (for frequent updates)
        GpuToCpu        ///< Device local, host readable (for readback)
    };

    /**
     * @brief Configuration for buffer creation
     */
    struct BufferDesc
    {
        /**
         * @brief Size of the buffer in bytes
         */
        VkDeviceSize Size = 0;

        /**
         * @brief Buffer usage type (determines VkBufferUsageFlags)
         */
        BufferUsage Usage = BufferUsage::Vertex;

        /**
         * @brief Memory location preference
         */
        MemoryUsage Memory = MemoryUsage::GpuOnly;

        /**
         * @brief Debug name for the buffer (optional)
         */
        const char* DebugName = nullptr;
    };

    /**
     * @brief Vulkan Buffer wrapper class
     *
     * Manages VkBuffer lifecycle with VMA memory allocation.
     * Supports various buffer types with appropriate memory properties.
     *
     * Usage:
     * @code
     * // Create a vertex buffer with data
     * std::vector<Vertex> vertices = {...};
     * BufferDesc desc;
     * desc.Size = sizeof(Vertex) * vertices.size();
     * desc.Usage = BufferUsage::Vertex;
     * desc.Memory = MemoryUsage::CpuToGpu;
     *
     * auto buffer = RHIBuffer::Create(device, desc);
     * buffer->SetData(vertices.data(), desc.Size);
     *
     * // Use in command buffer
     * commandBuffer->BindVertexBuffer(buffer->GetHandle());
     * @endcode
     */
    class RHIBuffer
    {
    public:
        /**
         * @brief Factory method to create a buffer
         * @param device The logical device
         * @param desc Buffer description
         * @return Shared pointer to the created buffer, or nullptr on failure
         */
        static Core::Ref<RHIBuffer> Create(
            const Core::Ref<RHIDevice>& device,
            const BufferDesc& desc);

        /**
         * @brief Factory method to create a buffer with initial data
         *
         * Creates a GPU-only buffer and uploads data using a staging buffer.
         * Blocks until the transfer is complete.
         *
         * @param device The logical device
         * @param desc Buffer description
         * @param data Pointer to initial data
         * @param dataSize Size of data to upload
         * @return Shared pointer to the created buffer, or nullptr on failure
         */
        static Core::Ref<RHIBuffer> CreateWithData(
            const Core::Ref<RHIDevice>& device,
            const BufferDesc& desc,
            const void* data,
            VkDeviceSize dataSize);

        /**
         * @brief Destructor - destroys VkBuffer and frees VMA allocation
         */
        ~RHIBuffer();

        // Non-copyable
        RHIBuffer(const RHIBuffer&) = delete;
        RHIBuffer& operator=(const RHIBuffer&) = delete;

        // Non-movable
        RHIBuffer(RHIBuffer&&) = delete;
        RHIBuffer& operator=(RHIBuffer&&) = delete;

        /**
         * @brief Get the native VkBuffer handle
         * @return VkBuffer handle
         */
        VkBuffer GetHandle() const { return m_Buffer; }

        /**
         * @brief Get the buffer size
         * @return Size in bytes
         */
        VkDeviceSize GetSize() const { return m_Size; }

        /**
         * @brief Get the buffer usage type
         * @return BufferUsage enum value
         */
        BufferUsage GetUsage() const { return m_Usage; }

        /**
         * @brief Check if the buffer is host visible (mappable)
         * @return true if buffer can be mapped to CPU
         */
        bool IsHostVisible() const { return m_IsMapped || m_MemoryUsage != MemoryUsage::GpuOnly; }

        /**
         * @brief Map the buffer to CPU memory
         *
         * Only valid for host-visible buffers (CpuOnly, CpuToGpu, GpuToCpu).
         * The buffer remains mapped until Unmap() is called.
         *
         * @return Pointer to mapped memory, or nullptr on failure
         */
        void* Map();

        /**
         * @brief Unmap the buffer from CPU memory
         */
        void Unmap();

        /**
         * @brief Set data in a host-visible buffer
         *
         * Convenience method that maps, copies, and unmaps.
         * Only valid for host-visible buffers.
         *
         * @param data Pointer to data to copy
         * @param size Size of data in bytes
         * @param offset Offset into the buffer
         * @return true on success, false on failure
         */
        bool SetData(const void* data, VkDeviceSize size, VkDeviceSize offset = 0);

        /**
         * @brief Flush mapped memory to make CPU writes visible to GPU
         *
         * Required for non-coherent memory after writing.
         * Only needed if using Map() directly.
         *
         * @param offset Offset of the range to flush
         * @param size Size of the range to flush (VK_WHOLE_SIZE for entire buffer)
         */
        void Flush(VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE);

        /**
         * @brief Invalidate mapped memory to make GPU writes visible to CPU
         *
         * Required for non-coherent memory before reading.
         *
         * @param offset Offset of the range to invalidate
         * @param size Size of the range to invalidate (VK_WHOLE_SIZE for entire buffer)
         */
        void Invalidate(VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE);

    private:
        /**
         * @brief Private constructor - use Create() factory method
         */
        RHIBuffer() = default;

        /**
         * @brief Initialize the buffer
         * @param device The logical device
         * @param desc Buffer description
         * @return true on success, false on failure
         */
        bool Initialize(const Core::Ref<RHIDevice>& device, const BufferDesc& desc);

        /**
         * @brief Convert BufferUsage to VkBufferUsageFlags
         * @param usage BufferUsage enum value
         * @return Corresponding VkBufferUsageFlags
         */
        static VkBufferUsageFlags ToVkBufferUsage(BufferUsage usage);

        /**
         * @brief Convert MemoryUsage to VMA memory usage
         * @param memory MemoryUsage enum value
         * @return Corresponding VmaMemoryUsage
         */
        static VmaMemoryUsage ToVmaMemoryUsage(MemoryUsage memory);

        VmaAllocator m_Allocator = VK_NULL_HANDLE;
        VkBuffer m_Buffer = VK_NULL_HANDLE;
        VmaAllocation m_Allocation = VK_NULL_HANDLE;
        VkDeviceSize m_Size = 0;
        BufferUsage m_Usage = BufferUsage::Vertex;
        MemoryUsage m_MemoryUsage = MemoryUsage::GpuOnly;
        void* m_MappedData = nullptr;
        bool m_IsMapped = false;
    };

} // namespace RHI
