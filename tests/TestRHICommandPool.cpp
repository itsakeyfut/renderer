/**
 * @file TestRHICommandPool.cpp
 * @brief Test file for RHI/RHICommandPool.h using GoogleTest.
 *
 * This file tests that the RHICommandPool class correctly handles
 * pool creation, configuration, reset, and lifecycle management.
 *
 * Note: These tests require a Vulkan-capable system to pass.
 */

#include <gtest/gtest.h>
#include "RHI/RHICommandPool.h"
#include "RHI/RHIDevice.h"
#include "RHI/RHIPhysicalDevice.h"
#include "RHI/RHIInstance.h"
#include "Platform/Window.h"
#include "Core/Log.h"

// =============================================================================
// Test Fixture for RHICommandPool Tests
// =============================================================================

class RHICommandPoolTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!Core::Log::IsInitialized()) {
            Core::Log::Init();
        }

        // Create a window to initialize GLFW and get a surface
        Platform::WindowConfig windowConfig;
        windowConfig.Width = 100;
        windowConfig.Height = 100;
        windowConfig.Title = "RHI CommandPool Test Window";
        windowConfig.Visible = false;
        m_Window = Core::CreateScope<Platform::Window>(windowConfig);

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

    Core::Scope<Platform::Window> m_Window;
    Core::Ref<RHI::RHIInstance> m_Instance;
    Core::Ref<RHI::RHIPhysicalDevice> m_PhysicalDevice;
    Core::Ref<RHI::RHIDevice> m_Device;
    Core::Ref<RHI::RHICommandPool> m_CommandPool;
    VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
};

// =============================================================================
// Creation Tests
// =============================================================================

TEST_F(RHICommandPoolTest, CreatesGraphicsCommandPool) {
    ASSERT_NE(m_Device, nullptr);

    RHI::RHICommandPoolConfig config;
    config.QueueType = RHI::CommandPoolQueueType::Graphics;
    config.AllowReset = true;
    config.Transient = false;

    m_CommandPool = RHI::RHICommandPool::Create(m_Device, config);

    ASSERT_NE(m_CommandPool, nullptr);
    EXPECT_NE(m_CommandPool->GetHandle(), VK_NULL_HANDLE);
    EXPECT_EQ(m_CommandPool->GetQueueType(), RHI::CommandPoolQueueType::Graphics);
}

TEST_F(RHICommandPoolTest, CreatesWithDefaultConfig) {
    ASSERT_NE(m_Device, nullptr);

    m_CommandPool = RHI::RHICommandPool::Create(m_Device);

    ASSERT_NE(m_CommandPool, nullptr);
    EXPECT_NE(m_CommandPool->GetHandle(), VK_NULL_HANDLE);
    EXPECT_EQ(m_CommandPool->GetQueueType(), RHI::CommandPoolQueueType::Graphics);
}

TEST_F(RHICommandPoolTest, CreatesTransientPool) {
    ASSERT_NE(m_Device, nullptr);

    RHI::RHICommandPoolConfig config;
    config.QueueType = RHI::CommandPoolQueueType::Graphics;
    config.AllowReset = true;
    config.Transient = true;

    m_CommandPool = RHI::RHICommandPool::Create(m_Device, config);

    ASSERT_NE(m_CommandPool, nullptr);
    EXPECT_NE(m_CommandPool->GetHandle(), VK_NULL_HANDLE);
}

TEST_F(RHICommandPoolTest, CreatesNonResettablePool) {
    ASSERT_NE(m_Device, nullptr);

    RHI::RHICommandPoolConfig config;
    config.QueueType = RHI::CommandPoolQueueType::Graphics;
    config.AllowReset = false;
    config.Transient = false;

    m_CommandPool = RHI::RHICommandPool::Create(m_Device, config);

    ASSERT_NE(m_CommandPool, nullptr);
    EXPECT_NE(m_CommandPool->GetHandle(), VK_NULL_HANDLE);
}

// =============================================================================
// Queue Family Tests
// =============================================================================

TEST_F(RHICommandPoolTest, GraphicsPoolHasValidQueueFamilyIndex) {
    ASSERT_NE(m_Device, nullptr);

    RHI::RHICommandPoolConfig config;
    config.QueueType = RHI::CommandPoolQueueType::Graphics;

    m_CommandPool = RHI::RHICommandPool::Create(m_Device, config);

    ASSERT_NE(m_CommandPool, nullptr);
    // Queue family index should be a valid index (not UINT32_MAX or similar invalid value)
    EXPECT_LT(m_CommandPool->GetQueueFamilyIndex(), 100u);  // Reasonable upper bound
}

// =============================================================================
// Reset Tests
// =============================================================================

TEST_F(RHICommandPoolTest, ResetDoesNotCrash) {
    ASSERT_NE(m_Device, nullptr);

    RHI::RHICommandPoolConfig config;
    config.QueueType = RHI::CommandPoolQueueType::Graphics;
    config.AllowReset = true;

    m_CommandPool = RHI::RHICommandPool::Create(m_Device, config);
    ASSERT_NE(m_CommandPool, nullptr);

    // Reset without releasing resources
    EXPECT_NO_THROW(m_CommandPool->Reset(false));
}

TEST_F(RHICommandPoolTest, ResetWithReleaseResourcesDoesNotCrash) {
    ASSERT_NE(m_Device, nullptr);

    RHI::RHICommandPoolConfig config;
    config.QueueType = RHI::CommandPoolQueueType::Graphics;
    config.AllowReset = true;

    m_CommandPool = RHI::RHICommandPool::Create(m_Device, config);
    ASSERT_NE(m_CommandPool, nullptr);

    // Reset with releasing resources
    EXPECT_NO_THROW(m_CommandPool->Reset(true));
}

// =============================================================================
// Multiple Pool Tests
// =============================================================================

TEST_F(RHICommandPoolTest, CreateMultiplePoolsForSameQueue) {
    ASSERT_NE(m_Device, nullptr);

    RHI::RHICommandPoolConfig config;
    config.QueueType = RHI::CommandPoolQueueType::Graphics;

    auto pool1 = RHI::RHICommandPool::Create(m_Device, config);
    auto pool2 = RHI::RHICommandPool::Create(m_Device, config);
    auto pool3 = RHI::RHICommandPool::Create(m_Device, config);

    ASSERT_NE(pool1, nullptr);
    ASSERT_NE(pool2, nullptr);
    ASSERT_NE(pool3, nullptr);

    // Each pool should have a unique handle
    EXPECT_NE(pool1->GetHandle(), pool2->GetHandle());
    EXPECT_NE(pool2->GetHandle(), pool3->GetHandle());
    EXPECT_NE(pool1->GetHandle(), pool3->GetHandle());

    // All should have the same queue family index
    EXPECT_EQ(pool1->GetQueueFamilyIndex(), pool2->GetQueueFamilyIndex());
    EXPECT_EQ(pool2->GetQueueFamilyIndex(), pool3->GetQueueFamilyIndex());
}

// =============================================================================
// Destructor Tests
// =============================================================================

TEST_F(RHICommandPoolTest, DestructorCleansUpResources) {
    ASSERT_NE(m_Device, nullptr);

    VkCommandPool handle = VK_NULL_HANDLE;

    {
        RHI::RHICommandPoolConfig config;
        config.QueueType = RHI::CommandPoolQueueType::Graphics;

        auto pool = RHI::RHICommandPool::Create(m_Device, config);
        ASSERT_NE(pool, nullptr);
        handle = pool->GetHandle();
        EXPECT_NE(handle, VK_NULL_HANDLE);
        // Pool goes out of scope here
    }

    // After destruction, we can't directly verify the handle is invalid,
    // but we can verify the code didn't crash
    SUCCEED();
}
