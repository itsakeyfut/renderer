/**
 * @file TestFrameManager.cpp
 * @brief Test file for Renderer/FrameManager.h using GoogleTest.
 *
 * This file tests that the FrameManager class correctly creates and manages
 * frame-in-flight resources including command buffers, semaphores, and fences.
 *
 * Note: These tests require a Vulkan-capable system to pass.
 */

#include <gtest/gtest.h>
#include "Renderer/FrameManager.h"
#include "RHI/RHICommandBuffer.h"
#include "RHI/RHIFence.h"
#include "RHI/RHISemaphore.h"
#include "RHI/RHIDevice.h"
#include "RHI/RHIPhysicalDevice.h"
#include "RHI/RHIInstance.h"
#include "Platform/Window.h"
#include "Core/Log.h"

// =============================================================================
// Test Fixture for FrameManager Tests
// =============================================================================

class FrameManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!Core::Log::IsInitialized()) {
            Core::Log::Init();
        }

        // Create a window to initialize GLFW and get a surface
        Platform::WindowConfig windowConfig;
        windowConfig.Width = 100;
        windowConfig.Height = 100;
        windowConfig.Title = "FrameManager Test Window";
        windowConfig.Visible = false;  // Hide window during tests
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
        // Resources must be destroyed in proper order
        m_FrameManager.reset();
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
    Core::Ref<Renderer::FrameManager> m_FrameManager;
    VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
};

// =============================================================================
// FrameManager Creation Tests
// =============================================================================

TEST_F(FrameManagerTest, CreatesFrameManager) {
    ASSERT_NE(m_Device, nullptr);

    m_FrameManager = Renderer::FrameManager::Create(m_Device);

    ASSERT_NE(m_FrameManager, nullptr);
}

TEST_F(FrameManagerTest, InitialFrameIndexIsZero) {
    ASSERT_NE(m_Device, nullptr);

    m_FrameManager = Renderer::FrameManager::Create(m_Device);
    ASSERT_NE(m_FrameManager, nullptr);

    EXPECT_EQ(m_FrameManager->GetCurrentFrameIndex(), 0u);
}

TEST_F(FrameManagerTest, FrameCountMatchesConstant) {
    EXPECT_EQ(Renderer::FrameManager::GetFrameCount(), Renderer::MAX_FRAMES_IN_FLIGHT);
    EXPECT_EQ(Renderer::MAX_FRAMES_IN_FLIGHT, 2u);
}

// =============================================================================
// Frame Resource Tests
// =============================================================================

TEST_F(FrameManagerTest, CurrentFrameHasValidCommandBuffer) {
    ASSERT_NE(m_Device, nullptr);

    m_FrameManager = Renderer::FrameManager::Create(m_Device);
    ASSERT_NE(m_FrameManager, nullptr);

    auto& frame = m_FrameManager->GetCurrentFrame();
    ASSERT_NE(frame.CommandBuffer, nullptr);
    EXPECT_NE(frame.CommandBuffer->GetHandle(), VK_NULL_HANDLE);
}

TEST_F(FrameManagerTest, CurrentFrameHasValidSemaphores) {
    ASSERT_NE(m_Device, nullptr);

    m_FrameManager = Renderer::FrameManager::Create(m_Device);
    ASSERT_NE(m_FrameManager, nullptr);

    auto& frame = m_FrameManager->GetCurrentFrame();
    ASSERT_NE(frame.ImageAvailableSemaphore, nullptr);
    EXPECT_NE(frame.ImageAvailableSemaphore->GetHandle(), VK_NULL_HANDLE);
}

TEST_F(FrameManagerTest, CurrentFrameHasValidFence) {
    ASSERT_NE(m_Device, nullptr);

    m_FrameManager = Renderer::FrameManager::Create(m_Device);
    ASSERT_NE(m_FrameManager, nullptr);

    auto& frame = m_FrameManager->GetCurrentFrame();
    ASSERT_NE(frame.InFlightFence, nullptr);
    EXPECT_NE(frame.InFlightFence->GetHandle(), VK_NULL_HANDLE);
}

TEST_F(FrameManagerTest, AllFramesHaveValidResources) {
    ASSERT_NE(m_Device, nullptr);

    m_FrameManager = Renderer::FrameManager::Create(m_Device);
    ASSERT_NE(m_FrameManager, nullptr);

    for (uint32_t i = 0; i < Renderer::MAX_FRAMES_IN_FLIGHT; ++i) {
        auto& frame = m_FrameManager->GetFrame(i);
        ASSERT_NE(frame.CommandBuffer, nullptr) << "Frame " << i << " has null command buffer";
        ASSERT_NE(frame.ImageAvailableSemaphore, nullptr) << "Frame " << i << " has null image available semaphore";
        ASSERT_NE(frame.InFlightFence, nullptr) << "Frame " << i << " has null in-flight fence";
    }
}

TEST_F(FrameManagerTest, EachFrameHasDistinctResources) {
    ASSERT_NE(m_Device, nullptr);

    m_FrameManager = Renderer::FrameManager::Create(m_Device);
    ASSERT_NE(m_FrameManager, nullptr);

    auto& frame0 = m_FrameManager->GetFrame(0);
    auto& frame1 = m_FrameManager->GetFrame(1);

    // Each frame should have distinct resources
    EXPECT_NE(frame0.CommandBuffer->GetHandle(), frame1.CommandBuffer->GetHandle());
    EXPECT_NE(frame0.ImageAvailableSemaphore->GetHandle(), frame1.ImageAvailableSemaphore->GetHandle());
    EXPECT_NE(frame0.InFlightFence->GetHandle(), frame1.InFlightFence->GetHandle());
}

// =============================================================================
// Frame Index Tests
// =============================================================================

TEST_F(FrameManagerTest, NextFrameAdvancesIndex) {
    ASSERT_NE(m_Device, nullptr);

    m_FrameManager = Renderer::FrameManager::Create(m_Device);
    ASSERT_NE(m_FrameManager, nullptr);

    EXPECT_EQ(m_FrameManager->GetCurrentFrameIndex(), 0u);

    m_FrameManager->NextFrame();
    EXPECT_EQ(m_FrameManager->GetCurrentFrameIndex(), 1u);
}

TEST_F(FrameManagerTest, NextFrameWrapsAround) {
    ASSERT_NE(m_Device, nullptr);

    m_FrameManager = Renderer::FrameManager::Create(m_Device);
    ASSERT_NE(m_FrameManager, nullptr);

    // Advance through all frames and wrap around
    for (uint32_t i = 0; i < Renderer::MAX_FRAMES_IN_FLIGHT; ++i) {
        m_FrameManager->NextFrame();
    }

    // Should have wrapped back to 0
    EXPECT_EQ(m_FrameManager->GetCurrentFrameIndex(), 0u);
}

TEST_F(FrameManagerTest, MultipleWraparounds) {
    ASSERT_NE(m_Device, nullptr);

    m_FrameManager = Renderer::FrameManager::Create(m_Device);
    ASSERT_NE(m_FrameManager, nullptr);

    // Advance through multiple cycles
    for (uint32_t cycle = 0; cycle < 5; ++cycle) {
        for (uint32_t i = 0; i < Renderer::MAX_FRAMES_IN_FLIGHT; ++i) {
            uint32_t expectedIndex = i;
            EXPECT_EQ(m_FrameManager->GetCurrentFrameIndex(), expectedIndex);
            m_FrameManager->NextFrame();
        }
    }
}

// =============================================================================
// Convenience Accessor Tests
// =============================================================================

TEST_F(FrameManagerTest, GetImageAvailableSemaphoreReturnsValidHandle) {
    ASSERT_NE(m_Device, nullptr);

    m_FrameManager = Renderer::FrameManager::Create(m_Device);
    ASSERT_NE(m_FrameManager, nullptr);

    VkSemaphore semaphore = m_FrameManager->GetImageAvailableSemaphore();
    EXPECT_NE(semaphore, VK_NULL_HANDLE);
}

TEST_F(FrameManagerTest, GetRenderFinishedSemaphoreReturnsValidHandle) {
    ASSERT_NE(m_Device, nullptr);

    m_FrameManager = Renderer::FrameManager::Create(m_Device);
    ASSERT_NE(m_FrameManager, nullptr);

    // Create render finished semaphores for 3 swapchain images
    constexpr uint32_t imageCount = 3;
    ASSERT_TRUE(m_FrameManager->CreateRenderFinishedSemaphores(m_Device, imageCount));

    // Each image index should return a valid semaphore
    for (uint32_t i = 0; i < imageCount; ++i) {
        VkSemaphore semaphore = m_FrameManager->GetRenderFinishedSemaphore(i);
        EXPECT_NE(semaphore, VK_NULL_HANDLE) << "Image " << i << " has null render finished semaphore";
    }
}

TEST_F(FrameManagerTest, GetInFlightFenceReturnsValidHandle) {
    ASSERT_NE(m_Device, nullptr);

    m_FrameManager = Renderer::FrameManager::Create(m_Device);
    ASSERT_NE(m_FrameManager, nullptr);

    VkFence fence = m_FrameManager->GetInFlightFence();
    EXPECT_NE(fence, VK_NULL_HANDLE);
}

TEST_F(FrameManagerTest, ConvenienceAccessorsMatchCurrentFrame) {
    ASSERT_NE(m_Device, nullptr);

    m_FrameManager = Renderer::FrameManager::Create(m_Device);
    ASSERT_NE(m_FrameManager, nullptr);

    auto& frame = m_FrameManager->GetCurrentFrame();

    EXPECT_EQ(m_FrameManager->GetImageAvailableSemaphore(), frame.ImageAvailableSemaphore->GetHandle());
    EXPECT_EQ(m_FrameManager->GetInFlightFence(), frame.InFlightFence->GetHandle());
}

TEST_F(FrameManagerTest, ConvenienceAccessorsUpdateAfterNextFrame) {
    ASSERT_NE(m_Device, nullptr);

    m_FrameManager = Renderer::FrameManager::Create(m_Device);
    ASSERT_NE(m_FrameManager, nullptr);

    VkSemaphore semaphore0 = m_FrameManager->GetImageAvailableSemaphore();

    m_FrameManager->NextFrame();

    VkSemaphore semaphore1 = m_FrameManager->GetImageAvailableSemaphore();

    // Should return different semaphores for different frames
    EXPECT_NE(semaphore0, semaphore1);
}

// =============================================================================
// Fence State Tests
// =============================================================================

TEST_F(FrameManagerTest, InitialFencesAreSignaled) {
    ASSERT_NE(m_Device, nullptr);

    m_FrameManager = Renderer::FrameManager::Create(m_Device);
    ASSERT_NE(m_FrameManager, nullptr);

    // All fences should be created in signaled state
    for (uint32_t i = 0; i < Renderer::MAX_FRAMES_IN_FLIGHT; ++i) {
        auto& frame = m_FrameManager->GetFrame(i);
        EXPECT_TRUE(frame.InFlightFence->IsSignaled()) << "Frame " << i << " fence should be signaled";
    }
}

TEST_F(FrameManagerTest, WaitForFrameSucceedsOnSignaledFence) {
    ASSERT_NE(m_Device, nullptr);

    m_FrameManager = Renderer::FrameManager::Create(m_Device);
    ASSERT_NE(m_FrameManager, nullptr);

    // First frame's fence is signaled, so wait should succeed immediately
    bool result = m_FrameManager->WaitForFrame();
    EXPECT_TRUE(result);
}

TEST_F(FrameManagerTest, WaitForFrameResetsFence) {
    ASSERT_NE(m_Device, nullptr);

    m_FrameManager = Renderer::FrameManager::Create(m_Device);
    ASSERT_NE(m_FrameManager, nullptr);

    auto& frame = m_FrameManager->GetCurrentFrame();

    // Fence is signaled initially
    EXPECT_TRUE(frame.InFlightFence->IsSignaled());

    // Wait should reset the fence
    bool result = m_FrameManager->WaitForFrame();
    EXPECT_TRUE(result);

    // After wait, fence should be reset (unsignaled)
    EXPECT_FALSE(frame.InFlightFence->IsSignaled());
}

// =============================================================================
// Command Buffer Tests
// =============================================================================

TEST_F(FrameManagerTest, CommandBufferCanBeRecorded) {
    ASSERT_NE(m_Device, nullptr);

    m_FrameManager = Renderer::FrameManager::Create(m_Device);
    ASSERT_NE(m_FrameManager, nullptr);

    auto& cmdBuffer = m_FrameManager->GetCommandBuffer();
    ASSERT_NE(cmdBuffer, nullptr);

    // Should be able to begin and end recording
    EXPECT_NO_THROW(cmdBuffer->Begin());
    EXPECT_TRUE(cmdBuffer->IsRecording());
    EXPECT_NO_THROW(cmdBuffer->End());
    EXPECT_FALSE(cmdBuffer->IsRecording());
}

TEST_F(FrameManagerTest, CommandBufferIsPrimaryLevel) {
    ASSERT_NE(m_Device, nullptr);

    m_FrameManager = Renderer::FrameManager::Create(m_Device);
    ASSERT_NE(m_FrameManager, nullptr);

    auto& cmdBuffer = m_FrameManager->GetCommandBuffer();
    EXPECT_EQ(cmdBuffer->GetLevel(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
}

// =============================================================================
// GetFrame Tests
// =============================================================================

TEST_F(FrameManagerTest, GetFrameReturnsCorrectFrame) {
    ASSERT_NE(m_Device, nullptr);

    m_FrameManager = Renderer::FrameManager::Create(m_Device);
    ASSERT_NE(m_FrameManager, nullptr);

    // GetFrame(0) should match GetCurrentFrame() initially
    EXPECT_EQ(&m_FrameManager->GetFrame(0), &m_FrameManager->GetCurrentFrame());

    m_FrameManager->NextFrame();

    // After NextFrame(), GetFrame(1) should match GetCurrentFrame()
    EXPECT_EQ(&m_FrameManager->GetFrame(1), &m_FrameManager->GetCurrentFrame());
}

// =============================================================================
// Destruction Tests
// =============================================================================

TEST_F(FrameManagerTest, DestructionDoesNotCrash) {
    ASSERT_NE(m_Device, nullptr);

    m_FrameManager = Renderer::FrameManager::Create(m_Device);
    ASSERT_NE(m_FrameManager, nullptr);

    // Destruction should be clean
    EXPECT_NO_THROW(m_FrameManager.reset());
}

TEST_F(FrameManagerTest, CanRecreateAfterDestruction) {
    ASSERT_NE(m_Device, nullptr);

    m_FrameManager = Renderer::FrameManager::Create(m_Device);
    ASSERT_NE(m_FrameManager, nullptr);

    m_FrameManager.reset();

    // Should be able to create a new FrameManager
    m_FrameManager = Renderer::FrameManager::Create(m_Device);
    ASSERT_NE(m_FrameManager, nullptr);
}
