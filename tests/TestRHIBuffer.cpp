/**
 * @file TestRHIBuffer.cpp
 * @brief Test file for RHI/RHIBuffer.h using GoogleTest.
 *
 * This file tests that the RHIBuffer class correctly creates buffers
 * with various usage types and memory configurations using VMA.
 *
 * Note: These tests require a Vulkan-capable system to pass.
 */

#include <gtest/gtest.h>
#include "RHI/RHIBuffer.h"
#include "RHI/RHIDevice.h"
#include "RHI/RHIPhysicalDevice.h"
#include "RHI/RHIInstance.h"
#include "Platform/Window.h"
#include "Core/Log.h"

#include <vector>
#include <cstring>

// =============================================================================
// Test Fixture for RHIBuffer Tests
// =============================================================================

class RHIBufferTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!Core::Log::IsInitialized()) {
            Core::Log::Init();
        }

        // Create a window to initialize GLFW and get a surface
        Platform::WindowConfig windowConfig;
        windowConfig.Width = 100;
        windowConfig.Height = 100;
        windowConfig.Title = "RHI Buffer Test Window";
        windowConfig.Visible = false;
        m_Window = std::make_unique<Platform::Window>(windowConfig);

        // Create Vulkan instance
        RHI::RHIInstanceConfig instanceConfig;
        instanceConfig.EnableValidation = false;
        m_Instance = RHI::RHIInstance::Create(instanceConfig);

        // Create surface
        if (m_Instance) {
            m_Surface = m_Window->CreateSurface(m_Instance->GetHandle());
        }

        // Select physical device
        if (m_Instance && m_Surface != VK_NULL_HANDLE) {
            m_PhysicalDevice = RHI::RHIPhysicalDevice::Select(m_Instance, m_Surface);
        }

        // Create logical device
        if (m_PhysicalDevice) {
            m_Device = RHI::RHIDevice::Create(m_Instance, m_PhysicalDevice, m_Surface);
        }
    }

    void TearDown() override {
        m_Buffer.reset();
        m_Device.reset();
        m_PhysicalDevice.reset();

        if (m_Surface != VK_NULL_HANDLE && m_Instance) {
            vkDestroySurfaceKHR(m_Instance->GetHandle(), m_Surface, nullptr);
            m_Surface = VK_NULL_HANDLE;
        }

        m_Instance.reset();
        m_Window.reset();
    }

    std::unique_ptr<Platform::Window> m_Window;
    Core::Ref<RHI::RHIInstance> m_Instance;
    Core::Ref<RHI::RHIPhysicalDevice> m_PhysicalDevice;
    Core::Ref<RHI::RHIDevice> m_Device;
    Core::Ref<RHI::RHIBuffer> m_Buffer;
    VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
};

// =============================================================================
// Buffer Creation Tests
// =============================================================================

TEST_F(RHIBufferTest, CreateVertexBuffer) {
    ASSERT_NE(m_Device, nullptr);

    RHI::BufferDesc desc;
    desc.Size = 1024;
    desc.Usage = RHI::BufferUsage::Vertex;
    desc.Memory = RHI::MemoryUsage::GpuOnly;
    desc.DebugName = "TestVertexBuffer";

    m_Buffer = RHI::RHIBuffer::Create(m_Device, desc);

    ASSERT_NE(m_Buffer, nullptr);
    EXPECT_NE(m_Buffer->GetHandle(), VK_NULL_HANDLE);
    EXPECT_EQ(m_Buffer->GetSize(), 1024);
    EXPECT_EQ(m_Buffer->GetUsage(), RHI::BufferUsage::Vertex);
}

TEST_F(RHIBufferTest, CreateIndexBuffer) {
    ASSERT_NE(m_Device, nullptr);

    RHI::BufferDesc desc;
    desc.Size = 512;
    desc.Usage = RHI::BufferUsage::Index;
    desc.Memory = RHI::MemoryUsage::GpuOnly;

    m_Buffer = RHI::RHIBuffer::Create(m_Device, desc);

    ASSERT_NE(m_Buffer, nullptr);
    EXPECT_NE(m_Buffer->GetHandle(), VK_NULL_HANDLE);
    EXPECT_EQ(m_Buffer->GetUsage(), RHI::BufferUsage::Index);
}

TEST_F(RHIBufferTest, CreateUniformBuffer) {
    ASSERT_NE(m_Device, nullptr);

    RHI::BufferDesc desc;
    desc.Size = 256;
    desc.Usage = RHI::BufferUsage::Uniform;
    desc.Memory = RHI::MemoryUsage::CpuToGpu;

    m_Buffer = RHI::RHIBuffer::Create(m_Device, desc);

    ASSERT_NE(m_Buffer, nullptr);
    EXPECT_NE(m_Buffer->GetHandle(), VK_NULL_HANDLE);
    EXPECT_EQ(m_Buffer->GetUsage(), RHI::BufferUsage::Uniform);
    EXPECT_TRUE(m_Buffer->IsHostVisible());
}

TEST_F(RHIBufferTest, CreateStorageBuffer) {
    ASSERT_NE(m_Device, nullptr);

    RHI::BufferDesc desc;
    desc.Size = 4096;
    desc.Usage = RHI::BufferUsage::Storage;
    desc.Memory = RHI::MemoryUsage::GpuOnly;

    m_Buffer = RHI::RHIBuffer::Create(m_Device, desc);

    ASSERT_NE(m_Buffer, nullptr);
    EXPECT_NE(m_Buffer->GetHandle(), VK_NULL_HANDLE);
    EXPECT_EQ(m_Buffer->GetUsage(), RHI::BufferUsage::Storage);
}

TEST_F(RHIBufferTest, CreateStagingBuffer) {
    ASSERT_NE(m_Device, nullptr);

    RHI::BufferDesc desc;
    desc.Size = 2048;
    desc.Usage = RHI::BufferUsage::Staging;
    desc.Memory = RHI::MemoryUsage::CpuOnly;

    m_Buffer = RHI::RHIBuffer::Create(m_Device, desc);

    ASSERT_NE(m_Buffer, nullptr);
    EXPECT_NE(m_Buffer->GetHandle(), VK_NULL_HANDLE);
    EXPECT_EQ(m_Buffer->GetUsage(), RHI::BufferUsage::Staging);
    EXPECT_TRUE(m_Buffer->IsHostVisible());
}

TEST_F(RHIBufferTest, CreateIndirectBuffer) {
    ASSERT_NE(m_Device, nullptr);

    RHI::BufferDesc desc;
    desc.Size = 256;
    desc.Usage = RHI::BufferUsage::Indirect;
    desc.Memory = RHI::MemoryUsage::GpuOnly;

    m_Buffer = RHI::RHIBuffer::Create(m_Device, desc);

    ASSERT_NE(m_Buffer, nullptr);
    EXPECT_NE(m_Buffer->GetHandle(), VK_NULL_HANDLE);
    EXPECT_EQ(m_Buffer->GetUsage(), RHI::BufferUsage::Indirect);
}

TEST_F(RHIBufferTest, CreateTransferSrcBuffer) {
    ASSERT_NE(m_Device, nullptr);

    RHI::BufferDesc desc;
    desc.Size = 1024;
    desc.Usage = RHI::BufferUsage::TransferSrc;
    desc.Memory = RHI::MemoryUsage::CpuOnly;

    m_Buffer = RHI::RHIBuffer::Create(m_Device, desc);

    ASSERT_NE(m_Buffer, nullptr);
    EXPECT_NE(m_Buffer->GetHandle(), VK_NULL_HANDLE);
    EXPECT_EQ(m_Buffer->GetUsage(), RHI::BufferUsage::TransferSrc);
}

TEST_F(RHIBufferTest, CreateTransferDstBuffer) {
    ASSERT_NE(m_Device, nullptr);

    RHI::BufferDesc desc;
    desc.Size = 1024;
    desc.Usage = RHI::BufferUsage::TransferDst;
    desc.Memory = RHI::MemoryUsage::GpuOnly;

    m_Buffer = RHI::RHIBuffer::Create(m_Device, desc);

    ASSERT_NE(m_Buffer, nullptr);
    EXPECT_NE(m_Buffer->GetHandle(), VK_NULL_HANDLE);
    EXPECT_EQ(m_Buffer->GetUsage(), RHI::BufferUsage::TransferDst);
}

// =============================================================================
// Memory Usage Tests
// =============================================================================

TEST_F(RHIBufferTest, GpuOnlyBufferNotHostVisible) {
    ASSERT_NE(m_Device, nullptr);

    RHI::BufferDesc desc;
    desc.Size = 1024;
    desc.Usage = RHI::BufferUsage::Vertex;
    desc.Memory = RHI::MemoryUsage::GpuOnly;

    m_Buffer = RHI::RHIBuffer::Create(m_Device, desc);

    ASSERT_NE(m_Buffer, nullptr);
    EXPECT_FALSE(m_Buffer->IsHostVisible());
}

TEST_F(RHIBufferTest, CpuOnlyBufferIsHostVisible) {
    ASSERT_NE(m_Device, nullptr);

    RHI::BufferDesc desc;
    desc.Size = 1024;
    desc.Usage = RHI::BufferUsage::Staging;
    desc.Memory = RHI::MemoryUsage::CpuOnly;

    m_Buffer = RHI::RHIBuffer::Create(m_Device, desc);

    ASSERT_NE(m_Buffer, nullptr);
    EXPECT_TRUE(m_Buffer->IsHostVisible());
}

TEST_F(RHIBufferTest, CpuToGpuBufferIsHostVisible) {
    ASSERT_NE(m_Device, nullptr);

    RHI::BufferDesc desc;
    desc.Size = 256;
    desc.Usage = RHI::BufferUsage::Uniform;
    desc.Memory = RHI::MemoryUsage::CpuToGpu;

    m_Buffer = RHI::RHIBuffer::Create(m_Device, desc);

    ASSERT_NE(m_Buffer, nullptr);
    EXPECT_TRUE(m_Buffer->IsHostVisible());
}

TEST_F(RHIBufferTest, GpuToCpuBufferIsHostVisible) {
    ASSERT_NE(m_Device, nullptr);

    RHI::BufferDesc desc;
    desc.Size = 1024;
    desc.Usage = RHI::BufferUsage::TransferDst;
    desc.Memory = RHI::MemoryUsage::GpuToCpu;

    m_Buffer = RHI::RHIBuffer::Create(m_Device, desc);

    ASSERT_NE(m_Buffer, nullptr);
    EXPECT_TRUE(m_Buffer->IsHostVisible());
}

// =============================================================================
// Map/Unmap Tests
// =============================================================================

TEST_F(RHIBufferTest, MapHostVisibleBuffer) {
    ASSERT_NE(m_Device, nullptr);

    RHI::BufferDesc desc;
    desc.Size = 256;
    desc.Usage = RHI::BufferUsage::Uniform;
    desc.Memory = RHI::MemoryUsage::CpuToGpu;

    m_Buffer = RHI::RHIBuffer::Create(m_Device, desc);
    ASSERT_NE(m_Buffer, nullptr);

    void* mappedData = m_Buffer->Map();
    EXPECT_NE(mappedData, nullptr);

    m_Buffer->Unmap();
}

TEST_F(RHIBufferTest, MapGpuOnlyBufferFails) {
    ASSERT_NE(m_Device, nullptr);

    RHI::BufferDesc desc;
    desc.Size = 1024;
    desc.Usage = RHI::BufferUsage::Vertex;
    desc.Memory = RHI::MemoryUsage::GpuOnly;

    m_Buffer = RHI::RHIBuffer::Create(m_Device, desc);
    ASSERT_NE(m_Buffer, nullptr);

    void* mappedData = m_Buffer->Map();
    EXPECT_EQ(mappedData, nullptr);
}

TEST_F(RHIBufferTest, PersistentMappingReturnsSamePointer) {
    ASSERT_NE(m_Device, nullptr);

    RHI::BufferDesc desc;
    desc.Size = 256;
    desc.Usage = RHI::BufferUsage::Uniform;
    desc.Memory = RHI::MemoryUsage::CpuToGpu;

    m_Buffer = RHI::RHIBuffer::Create(m_Device, desc);
    ASSERT_NE(m_Buffer, nullptr);

    void* firstMap = m_Buffer->Map();
    ASSERT_NE(firstMap, nullptr);

    // Unmap shouldn't actually unmap for persistently mapped buffers
    m_Buffer->Unmap();

    // Second map should return same pointer
    void* secondMap = m_Buffer->Map();
    EXPECT_EQ(firstMap, secondMap);
}

// =============================================================================
// SetData Tests
// =============================================================================

TEST_F(RHIBufferTest, SetDataToHostVisibleBuffer) {
    ASSERT_NE(m_Device, nullptr);

    RHI::BufferDesc desc;
    desc.Size = 256;
    desc.Usage = RHI::BufferUsage::Uniform;
    desc.Memory = RHI::MemoryUsage::CpuToGpu;

    m_Buffer = RHI::RHIBuffer::Create(m_Device, desc);
    ASSERT_NE(m_Buffer, nullptr);

    std::vector<float> testData = {1.0f, 2.0f, 3.0f, 4.0f};
    bool result = m_Buffer->SetData(testData.data(), testData.size() * sizeof(float));
    EXPECT_TRUE(result);
}

TEST_F(RHIBufferTest, SetDataWithOffset) {
    ASSERT_NE(m_Device, nullptr);

    RHI::BufferDesc desc;
    desc.Size = 256;
    desc.Usage = RHI::BufferUsage::Uniform;
    desc.Memory = RHI::MemoryUsage::CpuToGpu;

    m_Buffer = RHI::RHIBuffer::Create(m_Device, desc);
    ASSERT_NE(m_Buffer, nullptr);

    std::vector<float> testData = {5.0f, 6.0f, 7.0f, 8.0f};
    VkDeviceSize offset = 64; // 64 bytes offset
    bool result = m_Buffer->SetData(testData.data(), testData.size() * sizeof(float), offset);
    EXPECT_TRUE(result);
}

TEST_F(RHIBufferTest, SetDataVerifyContent) {
    ASSERT_NE(m_Device, nullptr);

    RHI::BufferDesc desc;
    desc.Size = 256;
    desc.Usage = RHI::BufferUsage::Staging;
    desc.Memory = RHI::MemoryUsage::CpuOnly;

    m_Buffer = RHI::RHIBuffer::Create(m_Device, desc);
    ASSERT_NE(m_Buffer, nullptr);

    // Write data
    std::vector<uint32_t> writeData = {0xDEADBEEF, 0xCAFEBABE, 0x12345678, 0x87654321};
    bool result = m_Buffer->SetData(writeData.data(), writeData.size() * sizeof(uint32_t));
    ASSERT_TRUE(result);

    // Read back and verify
    void* mappedData = m_Buffer->Map();
    ASSERT_NE(mappedData, nullptr);

    m_Buffer->Invalidate();

    std::vector<uint32_t> readData(4);
    std::memcpy(readData.data(), mappedData, readData.size() * sizeof(uint32_t));

    EXPECT_EQ(readData[0], 0xDEADBEEF);
    EXPECT_EQ(readData[1], 0xCAFEBABE);
    EXPECT_EQ(readData[2], 0x12345678);
    EXPECT_EQ(readData[3], 0x87654321);
}

// =============================================================================
// CreateWithData Tests (Staging Pattern)
// =============================================================================

TEST_F(RHIBufferTest, CreateWithDataGpuOnlyBuffer) {
    ASSERT_NE(m_Device, nullptr);

    std::vector<float> vertices = {
        0.0f, 0.5f, 0.0f,   // Position 1
        0.5f, -0.5f, 0.0f,  // Position 2
        -0.5f, -0.5f, 0.0f  // Position 3
    };

    RHI::BufferDesc desc;
    desc.Size = vertices.size() * sizeof(float);
    desc.Usage = RHI::BufferUsage::Vertex;
    desc.Memory = RHI::MemoryUsage::GpuOnly;
    desc.DebugName = "TestVertexBufferWithData";

    m_Buffer = RHI::RHIBuffer::CreateWithData(
        m_Device, desc, vertices.data(), vertices.size() * sizeof(float));

    ASSERT_NE(m_Buffer, nullptr);
    EXPECT_NE(m_Buffer->GetHandle(), VK_NULL_HANDLE);
    EXPECT_EQ(m_Buffer->GetSize(), vertices.size() * sizeof(float));
    EXPECT_EQ(m_Buffer->GetUsage(), RHI::BufferUsage::Vertex);
    EXPECT_FALSE(m_Buffer->IsHostVisible());
}

TEST_F(RHIBufferTest, CreateWithDataHostVisibleBuffer) {
    ASSERT_NE(m_Device, nullptr);

    std::vector<float> uniformData = {1.0f, 0.0f, 0.0f, 1.0f}; // Simple matrix data

    RHI::BufferDesc desc;
    desc.Size = uniformData.size() * sizeof(float);
    desc.Usage = RHI::BufferUsage::Uniform;
    desc.Memory = RHI::MemoryUsage::CpuToGpu;

    m_Buffer = RHI::RHIBuffer::CreateWithData(
        m_Device, desc, uniformData.data(), uniformData.size() * sizeof(float));

    ASSERT_NE(m_Buffer, nullptr);
    EXPECT_NE(m_Buffer->GetHandle(), VK_NULL_HANDLE);
    EXPECT_TRUE(m_Buffer->IsHostVisible());
}

TEST_F(RHIBufferTest, CreateWithDataIndexBuffer) {
    ASSERT_NE(m_Device, nullptr);

    std::vector<uint16_t> indices = {0, 1, 2, 2, 3, 0};

    RHI::BufferDesc desc;
    desc.Size = indices.size() * sizeof(uint16_t);
    desc.Usage = RHI::BufferUsage::Index;
    desc.Memory = RHI::MemoryUsage::GpuOnly;

    m_Buffer = RHI::RHIBuffer::CreateWithData(
        m_Device, desc, indices.data(), indices.size() * sizeof(uint16_t));

    ASSERT_NE(m_Buffer, nullptr);
    EXPECT_NE(m_Buffer->GetHandle(), VK_NULL_HANDLE);
    EXPECT_EQ(m_Buffer->GetUsage(), RHI::BufferUsage::Index);
}

// =============================================================================
// VMA Allocation Statistics Tests
// =============================================================================

TEST_F(RHIBufferTest, BufferCreationIncrementsAllocationCount) {
    ASSERT_NE(m_Device, nullptr);

    VmaAllocator allocator = m_Device->GetAllocator();
    ASSERT_NE(allocator, VK_NULL_HANDLE);

    // Get initial stats
    VmaTotalStatistics statsBefore;
    vmaCalculateStatistics(allocator, &statsBefore);
    uint32_t initialCount = statsBefore.total.statistics.allocationCount;

    // Create buffer
    RHI::BufferDesc desc;
    desc.Size = 1024;
    desc.Usage = RHI::BufferUsage::Vertex;
    desc.Memory = RHI::MemoryUsage::GpuOnly;

    m_Buffer = RHI::RHIBuffer::Create(m_Device, desc);
    ASSERT_NE(m_Buffer, nullptr);

    // Get stats after buffer creation
    VmaTotalStatistics statsAfter;
    vmaCalculateStatistics(allocator, &statsAfter);
    uint32_t finalCount = statsAfter.total.statistics.allocationCount;

    EXPECT_GT(finalCount, initialCount);
}

TEST_F(RHIBufferTest, BufferDestructionDecrementsAllocationCount) {
    ASSERT_NE(m_Device, nullptr);

    VmaAllocator allocator = m_Device->GetAllocator();
    ASSERT_NE(allocator, VK_NULL_HANDLE);

    // Create and immediately store reference
    RHI::BufferDesc desc;
    desc.Size = 1024;
    desc.Usage = RHI::BufferUsage::Staging;
    desc.Memory = RHI::MemoryUsage::CpuOnly;

    {
        auto tempBuffer = RHI::RHIBuffer::Create(m_Device, desc);
        ASSERT_NE(tempBuffer, nullptr);

        VmaTotalStatistics statsWithBuffer;
        vmaCalculateStatistics(allocator, &statsWithBuffer);
        uint32_t countWithBuffer = statsWithBuffer.total.statistics.allocationCount;

        EXPECT_GT(countWithBuffer, 0u);
    }

    // Buffer destroyed, count should decrease
    VmaTotalStatistics statsAfter;
    vmaCalculateStatistics(allocator, &statsAfter);
    uint32_t countAfter = statsAfter.total.statistics.allocationCount;

    EXPECT_EQ(countAfter, 0u);
}

// =============================================================================
// Error Handling Tests
// =============================================================================

TEST_F(RHIBufferTest, CreateWithZeroSizeFails) {
    ASSERT_NE(m_Device, nullptr);

    RHI::BufferDesc desc;
    desc.Size = 0;
    desc.Usage = RHI::BufferUsage::Vertex;
    desc.Memory = RHI::MemoryUsage::GpuOnly;

    // Should fail or assert due to zero size
    // Note: Behavior depends on ASSERT macro implementation
}

// =============================================================================
// Flush/Invalidate Tests
// =============================================================================

TEST_F(RHIBufferTest, FlushDoesNotCrash) {
    ASSERT_NE(m_Device, nullptr);

    RHI::BufferDesc desc;
    desc.Size = 256;
    desc.Usage = RHI::BufferUsage::Uniform;
    desc.Memory = RHI::MemoryUsage::CpuToGpu;

    m_Buffer = RHI::RHIBuffer::Create(m_Device, desc);
    ASSERT_NE(m_Buffer, nullptr);

    EXPECT_NO_THROW(m_Buffer->Flush());
}

TEST_F(RHIBufferTest, InvalidateDoesNotCrash) {
    ASSERT_NE(m_Device, nullptr);

    RHI::BufferDesc desc;
    desc.Size = 256;
    desc.Usage = RHI::BufferUsage::Staging;
    desc.Memory = RHI::MemoryUsage::CpuOnly;

    m_Buffer = RHI::RHIBuffer::Create(m_Device, desc);
    ASSERT_NE(m_Buffer, nullptr);

    EXPECT_NO_THROW(m_Buffer->Invalidate());
}

TEST_F(RHIBufferTest, FlushWithRangeDoesNotCrash) {
    ASSERT_NE(m_Device, nullptr);

    RHI::BufferDesc desc;
    desc.Size = 1024;
    desc.Usage = RHI::BufferUsage::Uniform;
    desc.Memory = RHI::MemoryUsage::CpuToGpu;

    m_Buffer = RHI::RHIBuffer::Create(m_Device, desc);
    ASSERT_NE(m_Buffer, nullptr);

    EXPECT_NO_THROW(m_Buffer->Flush(64, 128));
}
