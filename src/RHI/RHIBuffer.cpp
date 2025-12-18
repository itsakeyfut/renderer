/**
 * @file RHIBuffer.cpp
 * @brief Implementation of the Vulkan Buffer wrapper.
 */

#include "RHI/RHIBuffer.h"
#include "RHI/RHICommandPool.h"
#include "RHI/RHICommandBuffer.h"
#include "Core/Assert.h"
#include "Core/Log.h"

#include <cstring>

namespace RHI
{
    Core::Ref<RHIBuffer> RHIBuffer::Create(
        const Core::Ref<RHIDevice>& device,
        const BufferDesc& desc)
    {
        ASSERT(device != nullptr);
        ASSERT(desc.Size > 0);

        auto buffer = Core::Ref<RHIBuffer>(new RHIBuffer());
        if (!buffer->Initialize(device, desc))
        {
            LOG_ERROR("Failed to create RHIBuffer");
            return nullptr;
        }

        if (desc.DebugName)
        {
            LOG_DEBUG("Created buffer '{}': {} bytes", desc.DebugName, desc.Size);
        }

        return buffer;
    }

    Core::Ref<RHIBuffer> RHIBuffer::CreateWithData(
        const Core::Ref<RHIDevice>& device,
        const BufferDesc& desc,
        const void* data,
        VkDeviceSize dataSize)
    {
        ASSERT(device != nullptr);
        ASSERT(desc.Size > 0);
        ASSERT(data != nullptr);
        ASSERT(dataSize <= desc.Size);

        // For GPU-only buffers, use staging
        if (desc.Memory == MemoryUsage::GpuOnly)
        {
            // Create staging buffer
            BufferDesc stagingDesc;
            stagingDesc.Size = dataSize;
            stagingDesc.Usage = BufferUsage::Staging;
            stagingDesc.Memory = MemoryUsage::CpuOnly;

            auto stagingBuffer = Create(device, stagingDesc);
            if (!stagingBuffer)
            {
                LOG_ERROR("Failed to create staging buffer");
                return nullptr;
            }

            // Copy data to staging buffer
            if (!stagingBuffer->SetData(data, dataSize))
            {
                LOG_ERROR("Failed to copy data to staging buffer");
                return nullptr;
            }

            // Create destination buffer
            auto dstBuffer = Create(device, desc);
            if (!dstBuffer)
            {
                LOG_ERROR("Failed to create destination buffer");
                return nullptr;
            }

            // Create command pool and buffer for transfer
            RHICommandPoolConfig poolConfig;
            poolConfig.QueueType = CommandPoolQueueType::Graphics;
            poolConfig.Transient = true;
            auto commandPool = RHICommandPool::Create(device, poolConfig);
            if (!commandPool)
            {
                LOG_ERROR("Failed to create command pool for transfer");
                return nullptr;
            }

            auto commandBuffer = RHICommandBuffer::Create(device, commandPool);
            if (!commandBuffer)
            {
                LOG_ERROR("Failed to create command buffer for transfer");
                return nullptr;
            }

            // Record and execute transfer command
            commandBuffer->Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

            VkBufferCopy copyRegion{};
            copyRegion.srcOffset = 0;
            copyRegion.dstOffset = 0;
            copyRegion.size = dataSize;
            commandBuffer->CopyBuffer(
                stagingBuffer->GetHandle(),
                dstBuffer->GetHandle(),
                {copyRegion});

            commandBuffer->End();

            // Submit and wait
            VkSubmitInfo submitInfo{};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.commandBufferCount = 1;
            VkCommandBuffer cmdHandle = commandBuffer->GetHandle();
            submitInfo.pCommandBuffers = &cmdHandle;

            VkResult submitResult = vkQueueSubmit(device->GetGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
            if (submitResult != VK_SUCCESS)
            {
                LOG_ERROR("Failed to submit transfer command: VkResult {}", static_cast<int>(submitResult));
                return nullptr;
            }
            vkQueueWaitIdle(device->GetGraphicsQueue());

            return dstBuffer;
        }
        else
        {
            // For host-visible buffers, create and upload directly
            auto buffer = Create(device, desc);
            if (!buffer)
            {
                return nullptr;
            }

            if (!buffer->SetData(data, dataSize))
            {
                LOG_ERROR("Failed to set buffer data");
                return nullptr;
            }

            return buffer;
        }
    }

    RHIBuffer::~RHIBuffer()
    {
        if (m_IsMapped)
        {
            Unmap();
        }

        if (m_Buffer != VK_NULL_HANDLE && m_Allocator != VK_NULL_HANDLE)
        {
            vmaDestroyBuffer(m_Allocator, m_Buffer, m_Allocation);
            m_Buffer = VK_NULL_HANDLE;
            m_Allocation = VK_NULL_HANDLE;
        }
    }

    bool RHIBuffer::Initialize(const Core::Ref<RHIDevice>& device, const BufferDesc& desc)
    {
        m_Allocator = device->GetAllocator();
        m_Size = desc.Size;
        m_Usage = desc.Usage;
        m_MemoryUsage = desc.Memory;

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = desc.Size;
        bufferInfo.usage = ToVkBufferUsage(desc.Usage);
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = ToVmaMemoryUsage(desc.Memory);

        // For host-visible memory, enable mapping
        if (desc.Memory != MemoryUsage::GpuOnly)
        {
            allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                              VMA_ALLOCATION_CREATE_MAPPED_BIT;
        }

        VmaAllocationInfo allocationInfo;
        VkResult result = vmaCreateBuffer(
            m_Allocator,
            &bufferInfo,
            &allocInfo,
            &m_Buffer,
            &m_Allocation,
            &allocationInfo);

        if (result != VK_SUCCESS)
        {
            LOG_ERROR("Failed to create buffer: VkResult {}", static_cast<int>(result));
            return false;
        }

        // If VMA mapped the buffer automatically, store the pointer
        if (allocationInfo.pMappedData != nullptr)
        {
            m_MappedData = allocationInfo.pMappedData;
            m_IsMapped = true;
            m_PersistentlyMapped = true;
        }

        return true;
    }

    void* RHIBuffer::Map()
    {
        if (m_IsMapped)
        {
            return m_MappedData;
        }

        if (m_MemoryUsage == MemoryUsage::GpuOnly)
        {
            LOG_ERROR("Cannot map GPU-only buffer");
            return nullptr;
        }

        VkResult result = vmaMapMemory(m_Allocator, m_Allocation, &m_MappedData);
        if (result != VK_SUCCESS)
        {
            LOG_ERROR("Failed to map buffer memory: VkResult {}", static_cast<int>(result));
            return nullptr;
        }

        m_IsMapped = true;
        return m_MappedData;
    }

    void RHIBuffer::Unmap()
    {
        if (!m_IsMapped)
        {
            return;
        }

        // Don't unmap persistently mapped buffers (created with VMA_ALLOCATION_CREATE_MAPPED_BIT)
        if (m_PersistentlyMapped)
        {
            return;
        }

        vmaUnmapMemory(m_Allocator, m_Allocation);
        m_MappedData = nullptr;
        m_IsMapped = false;
    }

    bool RHIBuffer::SetData(const void* data, VkDeviceSize size, VkDeviceSize offset)
    {
        ASSERT(data != nullptr);
        ASSERT(offset + size <= m_Size);

        void* mappedData = Map();
        if (!mappedData)
        {
            return false;
        }

        std::memcpy(static_cast<char*>(mappedData) + offset, data, size);

        // Flush if memory is not host coherent
        Flush(offset, size);

        return true;
    }

    void RHIBuffer::Flush(VkDeviceSize offset, VkDeviceSize size)
    {
        vmaFlushAllocation(m_Allocator, m_Allocation, offset, size);
    }

    void RHIBuffer::Invalidate(VkDeviceSize offset, VkDeviceSize size)
    {
        vmaInvalidateAllocation(m_Allocator, m_Allocation, offset, size);
    }

    VkBufferUsageFlags RHIBuffer::ToVkBufferUsage(BufferUsage usage)
    {
        switch (usage)
        {
        case BufferUsage::Vertex:
            return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        case BufferUsage::Index:
            return VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        case BufferUsage::Uniform:
            return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

        case BufferUsage::Storage:
            return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        case BufferUsage::Staging:
            return VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

        case BufferUsage::TransferSrc:
            return VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

        case BufferUsage::TransferDst:
            return VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        default:
            return VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        }
    }

    VmaMemoryUsage RHIBuffer::ToVmaMemoryUsage(MemoryUsage memory)
    {
        switch (memory)
        {
        case MemoryUsage::GpuOnly:
            return VMA_MEMORY_USAGE_GPU_ONLY;

        case MemoryUsage::CpuOnly:
            return VMA_MEMORY_USAGE_CPU_ONLY;

        case MemoryUsage::CpuToGpu:
            return VMA_MEMORY_USAGE_CPU_TO_GPU;

        case MemoryUsage::GpuToCpu:
            return VMA_MEMORY_USAGE_GPU_TO_CPU;

        default:
            return VMA_MEMORY_USAGE_AUTO;
        }
    }

} // namespace RHI
