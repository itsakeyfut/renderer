/**
 * @file TestRHICommandBuffer.cpp
 * @brief Test file for RHI/RHICommandPool.h and RHI/RHICommandBuffer.h using GoogleTest.
 *
 * This file tests that the RHICommandPool and RHICommandBuffer classes correctly
 * create and manage Vulkan command pools and command buffers.
 *
 * Note: These tests require a Vulkan-capable system to pass.
 */

#include <gtest/gtest.h>
#include "RHI/RHICommandBuffer.h"
#include "RHI/RHICommandPool.h"
#include "RHI/RHIDevice.h"
#include "RHI/RHIPhysicalDevice.h"
#include "RHI/RHIInstance.h"
#include "Platform/Window.h"
#include "Core/Log.h"

// =============================================================================
// Test Fixture for RHICommandPool and RHICommandBuffer Tests
// =============================================================================

class RHICommandBufferTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!Core::Log::IsInitialized()) {
            Core::Log::Init();
        }

        // Create a window to initialize GLFW and get a surface
        Platform::WindowConfig windowConfig;
        windowConfig.Width = 100;
        windowConfig.Height = 100;
        windowConfig.Title = "RHI Command Buffer Test Window";
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
        // Ensure device is idle before cleanup
        if (m_Device) {
            m_Device->WaitIdle();
        }

        // Resources must be destroyed in reverse order of creation
        m_CommandBuffer.reset();
        m_CommandPool.reset();
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
    Core::Ref<RHI::RHICommandPool> m_CommandPool;
    Core::Ref<RHI::RHICommandBuffer> m_CommandBuffer;
    VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
};

// =============================================================================
// Command Pool Creation Tests
// =============================================================================

TEST_F(RHICommandBufferTest, CreatesCommandPoolWithDefaultConfig) {
    ASSERT_NE(m_Device, nullptr);

    m_CommandPool = RHI::RHICommandPool::Create(m_Device);

    ASSERT_NE(m_CommandPool, nullptr);
    EXPECT_NE(m_CommandPool->GetHandle(), VK_NULL_HANDLE);
    EXPECT_EQ(m_CommandPool->GetQueueType(), RHI::CommandPoolQueueType::Graphics);
}

TEST_F(RHICommandBufferTest, CreatesCommandPoolForGraphicsQueue) {
    ASSERT_NE(m_Device, nullptr);

    RHI::RHICommandPoolConfig config;
    config.QueueType = RHI::CommandPoolQueueType::Graphics;

    m_CommandPool = RHI::RHICommandPool::Create(m_Device, config);

    ASSERT_NE(m_CommandPool, nullptr);
    EXPECT_NE(m_CommandPool->GetHandle(), VK_NULL_HANDLE);
    EXPECT_EQ(m_CommandPool->GetQueueType(), RHI::CommandPoolQueueType::Graphics);

    const auto& queueFamilies = m_Device->GetQueueFamilies();
    EXPECT_EQ(m_CommandPool->GetQueueFamilyIndex(), queueFamilies.GraphicsFamily.value());
}

TEST_F(RHICommandBufferTest, CreatesCommandPoolForComputeQueue) {
    ASSERT_NE(m_Device, nullptr);

    const auto& queueFamilies = m_Device->GetQueueFamilies();
    if (!queueFamilies.ComputeFamily.has_value()) {
        GTEST_SKIP() << "Compute queue not available on this device";
    }

    RHI::RHICommandPoolConfig config;
    config.QueueType = RHI::CommandPoolQueueType::Compute;

    m_CommandPool = RHI::RHICommandPool::Create(m_Device, config);

    ASSERT_NE(m_CommandPool, nullptr);
    EXPECT_NE(m_CommandPool->GetHandle(), VK_NULL_HANDLE);
    EXPECT_EQ(m_CommandPool->GetQueueType(), RHI::CommandPoolQueueType::Compute);
}

TEST_F(RHICommandBufferTest, CreatesCommandPoolForTransferQueue) {
    ASSERT_NE(m_Device, nullptr);

    RHI::RHICommandPoolConfig config;
    config.QueueType = RHI::CommandPoolQueueType::Transfer;

    m_CommandPool = RHI::RHICommandPool::Create(m_Device, config);

    ASSERT_NE(m_CommandPool, nullptr);
    EXPECT_NE(m_CommandPool->GetHandle(), VK_NULL_HANDLE);
    EXPECT_EQ(m_CommandPool->GetQueueType(), RHI::CommandPoolQueueType::Transfer);
}

TEST_F(RHICommandBufferTest, CreatesTransientCommandPool) {
    ASSERT_NE(m_Device, nullptr);

    RHI::RHICommandPoolConfig config;
    config.Transient = true;

    m_CommandPool = RHI::RHICommandPool::Create(m_Device, config);

    ASSERT_NE(m_CommandPool, nullptr);
    EXPECT_NE(m_CommandPool->GetHandle(), VK_NULL_HANDLE);
}

TEST_F(RHICommandBufferTest, CommandPoolResetWorks) {
    ASSERT_NE(m_Device, nullptr);

    m_CommandPool = RHI::RHICommandPool::Create(m_Device);
    ASSERT_NE(m_CommandPool, nullptr);

    // Reset should complete without throwing
    EXPECT_NO_THROW(m_CommandPool->Reset());
    EXPECT_NO_THROW(m_CommandPool->Reset(true)); // with resource release
}

// =============================================================================
// Command Buffer Creation Tests
// =============================================================================

TEST_F(RHICommandBufferTest, CreatesCommandBuffer) {
    ASSERT_NE(m_Device, nullptr);

    m_CommandPool = RHI::RHICommandPool::Create(m_Device);
    ASSERT_NE(m_CommandPool, nullptr);

    m_CommandBuffer = RHI::RHICommandBuffer::Create(m_Device, m_CommandPool->GetHandle());

    ASSERT_NE(m_CommandBuffer, nullptr);
    EXPECT_NE(m_CommandBuffer->GetHandle(), VK_NULL_HANDLE);
    EXPECT_EQ(m_CommandBuffer->GetLevel(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    EXPECT_FALSE(m_CommandBuffer->IsRecording());
}

TEST_F(RHICommandBufferTest, CreatesPrimaryCommandBuffer) {
    ASSERT_NE(m_Device, nullptr);

    m_CommandPool = RHI::RHICommandPool::Create(m_Device);
    ASSERT_NE(m_CommandPool, nullptr);

    m_CommandBuffer = RHI::RHICommandBuffer::Create(
        m_Device,
        m_CommandPool->GetHandle(),
        VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    ASSERT_NE(m_CommandBuffer, nullptr);
    EXPECT_EQ(m_CommandBuffer->GetLevel(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
}

TEST_F(RHICommandBufferTest, CreatesSecondaryCommandBuffer) {
    ASSERT_NE(m_Device, nullptr);

    m_CommandPool = RHI::RHICommandPool::Create(m_Device);
    ASSERT_NE(m_CommandPool, nullptr);

    m_CommandBuffer = RHI::RHICommandBuffer::Create(
        m_Device,
        m_CommandPool->GetHandle(),
        VK_COMMAND_BUFFER_LEVEL_SECONDARY);

    ASSERT_NE(m_CommandBuffer, nullptr);
    EXPECT_EQ(m_CommandBuffer->GetLevel(), VK_COMMAND_BUFFER_LEVEL_SECONDARY);
}

// =============================================================================
// Command Buffer Recording Tests
// =============================================================================

TEST_F(RHICommandBufferTest, BeginRecordingWorks) {
    ASSERT_NE(m_Device, nullptr);

    m_CommandPool = RHI::RHICommandPool::Create(m_Device);
    ASSERT_NE(m_CommandPool, nullptr);

    m_CommandBuffer = RHI::RHICommandBuffer::Create(m_Device, m_CommandPool->GetHandle());
    ASSERT_NE(m_CommandBuffer, nullptr);

    EXPECT_FALSE(m_CommandBuffer->IsRecording());

    m_CommandBuffer->Begin();

    EXPECT_TRUE(m_CommandBuffer->IsRecording());

    m_CommandBuffer->End();

    EXPECT_FALSE(m_CommandBuffer->IsRecording());
}

TEST_F(RHICommandBufferTest, BeginWithOneTimeSubmitFlag) {
    ASSERT_NE(m_Device, nullptr);

    m_CommandPool = RHI::RHICommandPool::Create(m_Device);
    ASSERT_NE(m_CommandPool, nullptr);

    m_CommandBuffer = RHI::RHICommandBuffer::Create(m_Device, m_CommandPool->GetHandle());
    ASSERT_NE(m_CommandBuffer, nullptr);

    m_CommandBuffer->Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    EXPECT_TRUE(m_CommandBuffer->IsRecording());

    m_CommandBuffer->End();
}

TEST_F(RHICommandBufferTest, ResetCommandBuffer) {
    ASSERT_NE(m_Device, nullptr);

    m_CommandPool = RHI::RHICommandPool::Create(m_Device);
    ASSERT_NE(m_CommandPool, nullptr);

    m_CommandBuffer = RHI::RHICommandBuffer::Create(m_Device, m_CommandPool->GetHandle());
    ASSERT_NE(m_CommandBuffer, nullptr);

    // Record something
    m_CommandBuffer->Begin();
    m_CommandBuffer->End();

    // Reset should work
    EXPECT_NO_THROW(m_CommandBuffer->Reset());
    EXPECT_FALSE(m_CommandBuffer->IsRecording());

    // Should be able to record again
    m_CommandBuffer->Begin();
    EXPECT_TRUE(m_CommandBuffer->IsRecording());
    m_CommandBuffer->End();
}

TEST_F(RHICommandBufferTest, ResetWithResourceRelease) {
    ASSERT_NE(m_Device, nullptr);

    m_CommandPool = RHI::RHICommandPool::Create(m_Device);
    ASSERT_NE(m_CommandPool, nullptr);

    m_CommandBuffer = RHI::RHICommandBuffer::Create(m_Device, m_CommandPool->GetHandle());
    ASSERT_NE(m_CommandBuffer, nullptr);

    m_CommandBuffer->Begin();
    m_CommandBuffer->End();

    // Reset with resource release
    EXPECT_NO_THROW(m_CommandBuffer->Reset(true));
}

// =============================================================================
// Dynamic State Commands Tests
// =============================================================================

TEST_F(RHICommandBufferTest, SetViewportWorks) {
    ASSERT_NE(m_Device, nullptr);

    m_CommandPool = RHI::RHICommandPool::Create(m_Device);
    ASSERT_NE(m_CommandPool, nullptr);

    m_CommandBuffer = RHI::RHICommandBuffer::Create(m_Device, m_CommandPool->GetHandle());
    ASSERT_NE(m_CommandBuffer, nullptr);

    m_CommandBuffer->Begin();

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = 800.0f;
    viewport.height = 600.0f;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    EXPECT_NO_THROW(m_CommandBuffer->SetViewport(viewport));

    m_CommandBuffer->End();
}

TEST_F(RHICommandBufferTest, SetScissorWorks) {
    ASSERT_NE(m_Device, nullptr);

    m_CommandPool = RHI::RHICommandPool::Create(m_Device);
    ASSERT_NE(m_CommandPool, nullptr);

    m_CommandBuffer = RHI::RHICommandBuffer::Create(m_Device, m_CommandPool->GetHandle());
    ASSERT_NE(m_CommandBuffer, nullptr);

    m_CommandBuffer->Begin();

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = {800, 600};

    EXPECT_NO_THROW(m_CommandBuffer->SetScissor(scissor));

    m_CommandBuffer->End();
}

TEST_F(RHICommandBufferTest, SetLineWidthWorks) {
    ASSERT_NE(m_Device, nullptr);

    m_CommandPool = RHI::RHICommandPool::Create(m_Device);
    ASSERT_NE(m_CommandPool, nullptr);

    m_CommandBuffer = RHI::RHICommandBuffer::Create(m_Device, m_CommandPool->GetHandle());
    ASSERT_NE(m_CommandBuffer, nullptr);

    m_CommandBuffer->Begin();

    EXPECT_NO_THROW(m_CommandBuffer->SetLineWidth(1.0f));

    m_CommandBuffer->End();
}

TEST_F(RHICommandBufferTest, SetDepthBiasWorks) {
    ASSERT_NE(m_Device, nullptr);

    m_CommandPool = RHI::RHICommandPool::Create(m_Device);
    ASSERT_NE(m_CommandPool, nullptr);

    m_CommandBuffer = RHI::RHICommandBuffer::Create(m_Device, m_CommandPool->GetHandle());
    ASSERT_NE(m_CommandBuffer, nullptr);

    m_CommandBuffer->Begin();

    EXPECT_NO_THROW(m_CommandBuffer->SetDepthBias(0.0f, 0.0f, 0.0f));

    m_CommandBuffer->End();
}

TEST_F(RHICommandBufferTest, SetBlendConstantsWorks) {
    ASSERT_NE(m_Device, nullptr);

    m_CommandPool = RHI::RHICommandPool::Create(m_Device);
    ASSERT_NE(m_CommandPool, nullptr);

    m_CommandBuffer = RHI::RHICommandBuffer::Create(m_Device, m_CommandPool->GetHandle());
    ASSERT_NE(m_CommandBuffer, nullptr);

    m_CommandBuffer->Begin();

    const float blendConstants[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    EXPECT_NO_THROW(m_CommandBuffer->SetBlendConstants(blendConstants));

    m_CommandBuffer->End();
}

// =============================================================================
// Multiple Command Buffer Tests
// =============================================================================

TEST_F(RHICommandBufferTest, MultipleCommandBuffersFromSamePool) {
    ASSERT_NE(m_Device, nullptr);

    m_CommandPool = RHI::RHICommandPool::Create(m_Device);
    ASSERT_NE(m_CommandPool, nullptr);

    auto cmdBuffer1 = RHI::RHICommandBuffer::Create(m_Device, m_CommandPool->GetHandle());
    auto cmdBuffer2 = RHI::RHICommandBuffer::Create(m_Device, m_CommandPool->GetHandle());
    auto cmdBuffer3 = RHI::RHICommandBuffer::Create(m_Device, m_CommandPool->GetHandle());

    ASSERT_NE(cmdBuffer1, nullptr);
    ASSERT_NE(cmdBuffer2, nullptr);
    ASSERT_NE(cmdBuffer3, nullptr);

    EXPECT_NE(cmdBuffer1->GetHandle(), cmdBuffer2->GetHandle());
    EXPECT_NE(cmdBuffer2->GetHandle(), cmdBuffer3->GetHandle());

    // All should be able to record independently
    cmdBuffer1->Begin();
    cmdBuffer2->Begin();
    cmdBuffer3->Begin();

    cmdBuffer1->End();
    cmdBuffer2->End();
    cmdBuffer3->End();
}

TEST_F(RHICommandBufferTest, MultipleCommandPools) {
    ASSERT_NE(m_Device, nullptr);

    auto pool1 = RHI::RHICommandPool::Create(m_Device);
    auto pool2 = RHI::RHICommandPool::Create(m_Device);

    ASSERT_NE(pool1, nullptr);
    ASSERT_NE(pool2, nullptr);

    EXPECT_NE(pool1->GetHandle(), pool2->GetHandle());

    auto cmdBuffer1 = RHI::RHICommandBuffer::Create(m_Device, pool1->GetHandle());
    auto cmdBuffer2 = RHI::RHICommandBuffer::Create(m_Device, pool2->GetHandle());

    ASSERT_NE(cmdBuffer1, nullptr);
    ASSERT_NE(cmdBuffer2, nullptr);

    cmdBuffer1->Begin();
    cmdBuffer2->Begin();

    cmdBuffer1->End();
    cmdBuffer2->End();
}

// =============================================================================
// Compute Queue Tests
// =============================================================================

TEST_F(RHICommandBufferTest, CreatesCommandBufferOnComputeQueue) {
    ASSERT_NE(m_Device, nullptr);

    // Use compute queue if available
    RHI::RHICommandPoolConfig config;
    const auto& queueFamilies = m_Device->GetQueueFamilies();

    if (queueFamilies.ComputeFamily.has_value()) {
        config.QueueType = RHI::CommandPoolQueueType::Compute;
    } else {
        config.QueueType = RHI::CommandPoolQueueType::Graphics;
    }

    m_CommandPool = RHI::RHICommandPool::Create(m_Device, config);
    ASSERT_NE(m_CommandPool, nullptr);

    m_CommandBuffer = RHI::RHICommandBuffer::Create(m_Device, m_CommandPool->GetHandle());
    ASSERT_NE(m_CommandBuffer, nullptr);

    // Test basic recording works
    m_CommandBuffer->Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    EXPECT_TRUE(m_CommandBuffer->IsRecording());
    m_CommandBuffer->End();
    EXPECT_FALSE(m_CommandBuffer->IsRecording());
}

// =============================================================================
// Pipeline Barrier Tests
// =============================================================================

TEST_F(RHICommandBufferTest, PipelineBarrierWorks) {
    ASSERT_NE(m_Device, nullptr);

    m_CommandPool = RHI::RHICommandPool::Create(m_Device);
    ASSERT_NE(m_CommandPool, nullptr);

    m_CommandBuffer = RHI::RHICommandBuffer::Create(m_Device, m_CommandPool->GetHandle());
    ASSERT_NE(m_CommandBuffer, nullptr);

    m_CommandBuffer->Begin();

    // Simple memory barrier
    VkMemoryBarrier memoryBarrier{};
    memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    std::vector<VkMemoryBarrier> memoryBarriers = {memoryBarrier};

    EXPECT_NO_THROW(m_CommandBuffer->PipelineBarrier(
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        memoryBarriers));

    m_CommandBuffer->End();
}

TEST_F(RHICommandBufferTest, PipelineBarrierWithEmptyBarriers) {
    ASSERT_NE(m_Device, nullptr);

    m_CommandPool = RHI::RHICommandPool::Create(m_Device);
    ASSERT_NE(m_CommandPool, nullptr);

    m_CommandBuffer = RHI::RHICommandBuffer::Create(m_Device, m_CommandPool->GetHandle());
    ASSERT_NE(m_CommandBuffer, nullptr);

    m_CommandBuffer->Begin();

    // Empty barrier - just execution dependency
    EXPECT_NO_THROW(m_CommandBuffer->PipelineBarrier(
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT));

    m_CommandBuffer->End();
}

// =============================================================================
// Destruction Order Tests
// =============================================================================

TEST_F(RHICommandBufferTest, CommandBufferDestroyedBeforePool) {
    ASSERT_NE(m_Device, nullptr);

    m_CommandPool = RHI::RHICommandPool::Create(m_Device);
    ASSERT_NE(m_CommandPool, nullptr);

    m_CommandBuffer = RHI::RHICommandBuffer::Create(m_Device, m_CommandPool->GetHandle());
    ASSERT_NE(m_CommandBuffer, nullptr);

    // Destroy command buffer first
    EXPECT_NO_THROW(m_CommandBuffer.reset());

    // Pool should still be valid
    EXPECT_NE(m_CommandPool->GetHandle(), VK_NULL_HANDLE);

    // Should be able to create new command buffer from pool
    auto newBuffer = RHI::RHICommandBuffer::Create(m_Device, m_CommandPool->GetHandle());
    EXPECT_NE(newBuffer, nullptr);
}

TEST_F(RHICommandBufferTest, CommandPoolDestroyedWithCommandBuffers) {
    ASSERT_NE(m_Device, nullptr);

    auto pool = RHI::RHICommandPool::Create(m_Device);
    ASSERT_NE(pool, nullptr);

    auto cmdBuffer1 = RHI::RHICommandBuffer::Create(m_Device, pool->GetHandle());
    auto cmdBuffer2 = RHI::RHICommandBuffer::Create(m_Device, pool->GetHandle());

    ASSERT_NE(cmdBuffer1, nullptr);
    ASSERT_NE(cmdBuffer2, nullptr);

    // Free command buffers first, then destroy pool
    cmdBuffer1.reset();
    cmdBuffer2.reset();

    // Pool destruction should not crash
    EXPECT_NO_THROW(pool.reset());
}
