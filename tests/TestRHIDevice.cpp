/**
 * @file TestRHIDevice.cpp
 * @brief Test file for RHI/RHIDevice.h using GoogleTest.
 *
 * This file tests that the RHIDevice class correctly creates a logical device,
 * retrieves queues, and initializes the VMA allocator.
 *
 * Note: These tests require a Vulkan-capable system to pass.
 */

#include <gtest/gtest.h>
#include "RHI/RHIDevice.h"
#include "RHI/RHIPhysicalDevice.h"
#include "RHI/RHIInstance.h"
#include "Platform/Window.h"
#include "Core/Log.h"

// =============================================================================
// Test Fixture for RHIDevice Tests
// =============================================================================

class RHIDeviceTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!Core::Log::IsInitialized()) {
            Core::Log::Init();
        }

        // Create a window to initialize GLFW and get a surface
        Platform::WindowConfig windowConfig;
        windowConfig.Width = 100;
        windowConfig.Height = 100;
        windowConfig.Title = "RHI Device Test Window";
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
    }

    void TearDown() override {
        // Device must be destroyed before surface and instance
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
    VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
};

// =============================================================================
// Device Creation Tests
// =============================================================================

TEST_F(RHIDeviceTest, CreatesDevice) {
    ASSERT_NE(m_Instance, nullptr);
    ASSERT_NE(m_PhysicalDevice, nullptr);
    ASSERT_NE(m_Surface, VK_NULL_HANDLE);

    m_Device = RHI::RHIDevice::Create(m_Instance, m_PhysicalDevice, m_Surface);

    ASSERT_NE(m_Device, nullptr);
    EXPECT_NE(m_Device->GetHandle(), VK_NULL_HANDLE);
}

TEST_F(RHIDeviceTest, CreatesDeviceWithDefaultConfig) {
    ASSERT_NE(m_Instance, nullptr);
    ASSERT_NE(m_PhysicalDevice, nullptr);
    ASSERT_NE(m_Surface, VK_NULL_HANDLE);

    // Create with default config
    m_Device = RHI::RHIDevice::Create(m_Instance, m_PhysicalDevice, m_Surface);

    ASSERT_NE(m_Device, nullptr);
    EXPECT_NE(m_Device->GetHandle(), VK_NULL_HANDLE);
}

TEST_F(RHIDeviceTest, CreatesDeviceWithCustomConfig) {
    ASSERT_NE(m_Instance, nullptr);
    ASSERT_NE(m_PhysicalDevice, nullptr);
    ASSERT_NE(m_Surface, VK_NULL_HANDLE);

    // Create with custom config
    RHI::RHIDeviceConfig config;
    config.QueuePriority = 0.5f;
    config.EnableRequiredFeatures = true;

    m_Device = RHI::RHIDevice::Create(m_Instance, m_PhysicalDevice, m_Surface, config);

    ASSERT_NE(m_Device, nullptr);
    EXPECT_NE(m_Device->GetHandle(), VK_NULL_HANDLE);
}

// =============================================================================
// Handle Tests
// =============================================================================

TEST_F(RHIDeviceTest, GetHandleReturnsValidHandle) {
    ASSERT_NE(m_Instance, nullptr);
    ASSERT_NE(m_PhysicalDevice, nullptr);
    ASSERT_NE(m_Surface, VK_NULL_HANDLE);

    m_Device = RHI::RHIDevice::Create(m_Instance, m_PhysicalDevice, m_Surface);
    ASSERT_NE(m_Device, nullptr);

    VkDevice handle = m_Device->GetHandle();
    EXPECT_NE(handle, VK_NULL_HANDLE);
}

TEST_F(RHIDeviceTest, GetPhysicalDeviceReturnsValidHandle) {
    ASSERT_NE(m_Instance, nullptr);
    ASSERT_NE(m_PhysicalDevice, nullptr);
    ASSERT_NE(m_Surface, VK_NULL_HANDLE);

    m_Device = RHI::RHIDevice::Create(m_Instance, m_PhysicalDevice, m_Surface);
    ASSERT_NE(m_Device, nullptr);

    VkPhysicalDevice physicalDevice = m_Device->GetPhysicalDevice();
    EXPECT_NE(physicalDevice, VK_NULL_HANDLE);
    EXPECT_EQ(physicalDevice, m_PhysicalDevice->GetHandle());
}

// =============================================================================
// Queue Tests
// =============================================================================

TEST_F(RHIDeviceTest, GetGraphicsQueueReturnsValidQueue) {
    ASSERT_NE(m_Instance, nullptr);
    ASSERT_NE(m_PhysicalDevice, nullptr);
    ASSERT_NE(m_Surface, VK_NULL_HANDLE);

    m_Device = RHI::RHIDevice::Create(m_Instance, m_PhysicalDevice, m_Surface);
    ASSERT_NE(m_Device, nullptr);

    VkQueue graphicsQueue = m_Device->GetGraphicsQueue();
    EXPECT_NE(graphicsQueue, VK_NULL_HANDLE);
}

TEST_F(RHIDeviceTest, GetPresentQueueReturnsValidQueue) {
    ASSERT_NE(m_Instance, nullptr);
    ASSERT_NE(m_PhysicalDevice, nullptr);
    ASSERT_NE(m_Surface, VK_NULL_HANDLE);

    m_Device = RHI::RHIDevice::Create(m_Instance, m_PhysicalDevice, m_Surface);
    ASSERT_NE(m_Device, nullptr);

    VkQueue presentQueue = m_Device->GetPresentQueue();
    EXPECT_NE(presentQueue, VK_NULL_HANDLE);
}

TEST_F(RHIDeviceTest, GetComputeQueueReturnsQueueWhenAvailable) {
    ASSERT_NE(m_Instance, nullptr);
    ASSERT_NE(m_PhysicalDevice, nullptr);
    ASSERT_NE(m_Surface, VK_NULL_HANDLE);

    m_Device = RHI::RHIDevice::Create(m_Instance, m_PhysicalDevice, m_Surface);
    ASSERT_NE(m_Device, nullptr);

    // Compute queue may or may not be available
    VkQueue computeQueue = m_Device->GetComputeQueue();
    const auto& queueFamilies = m_Device->GetQueueFamilies();

    if (queueFamilies.ComputeFamily.has_value()) {
        EXPECT_NE(computeQueue, VK_NULL_HANDLE);
    } else {
        EXPECT_EQ(computeQueue, VK_NULL_HANDLE);
    }
}

TEST_F(RHIDeviceTest, GetTransferQueueReturnsQueueWhenAvailable) {
    ASSERT_NE(m_Instance, nullptr);
    ASSERT_NE(m_PhysicalDevice, nullptr);
    ASSERT_NE(m_Surface, VK_NULL_HANDLE);

    m_Device = RHI::RHIDevice::Create(m_Instance, m_PhysicalDevice, m_Surface);
    ASSERT_NE(m_Device, nullptr);

    // Transfer queue may or may not be available
    VkQueue transferQueue = m_Device->GetTransferQueue();
    const auto& queueFamilies = m_Device->GetQueueFamilies();

    if (queueFamilies.TransferFamily.has_value()) {
        EXPECT_NE(transferQueue, VK_NULL_HANDLE);
    } else {
        EXPECT_EQ(transferQueue, VK_NULL_HANDLE);
    }
}

// =============================================================================
// Queue Family Tests
// =============================================================================

TEST_F(RHIDeviceTest, GetQueueFamiliesReturnsCompleteIndices) {
    ASSERT_NE(m_Instance, nullptr);
    ASSERT_NE(m_PhysicalDevice, nullptr);
    ASSERT_NE(m_Surface, VK_NULL_HANDLE);

    m_Device = RHI::RHIDevice::Create(m_Instance, m_PhysicalDevice, m_Surface);
    ASSERT_NE(m_Device, nullptr);

    const auto& queueFamilies = m_Device->GetQueueFamilies();

    EXPECT_TRUE(queueFamilies.IsComplete());
    EXPECT_TRUE(queueFamilies.GraphicsFamily.has_value());
    EXPECT_TRUE(queueFamilies.PresentFamily.has_value());
}

TEST_F(RHIDeviceTest, QueueFamiliesMatchPhysicalDevice) {
    ASSERT_NE(m_Instance, nullptr);
    ASSERT_NE(m_PhysicalDevice, nullptr);
    ASSERT_NE(m_Surface, VK_NULL_HANDLE);

    m_Device = RHI::RHIDevice::Create(m_Instance, m_PhysicalDevice, m_Surface);
    ASSERT_NE(m_Device, nullptr);

    const auto& deviceQueueFamilies = m_Device->GetQueueFamilies();
    const auto& physicalDeviceQueueFamilies = m_PhysicalDevice->GetQueueFamilies();

    EXPECT_EQ(deviceQueueFamilies.GraphicsFamily, physicalDeviceQueueFamilies.GraphicsFamily);
    EXPECT_EQ(deviceQueueFamilies.PresentFamily, physicalDeviceQueueFamilies.PresentFamily);
    EXPECT_EQ(deviceQueueFamilies.ComputeFamily, physicalDeviceQueueFamilies.ComputeFamily);
    EXPECT_EQ(deviceQueueFamilies.TransferFamily, physicalDeviceQueueFamilies.TransferFamily);
}

// =============================================================================
// VMA Allocator Tests
// =============================================================================

TEST_F(RHIDeviceTest, GetAllocatorReturnsValidHandle) {
    ASSERT_NE(m_Instance, nullptr);
    ASSERT_NE(m_PhysicalDevice, nullptr);
    ASSERT_NE(m_Surface, VK_NULL_HANDLE);

    m_Device = RHI::RHIDevice::Create(m_Instance, m_PhysicalDevice, m_Surface);
    ASSERT_NE(m_Device, nullptr);

    VmaAllocator allocator = m_Device->GetAllocator();
    EXPECT_NE(allocator, VK_NULL_HANDLE);
}

TEST_F(RHIDeviceTest, AllocatorCanQueryStatistics) {
    ASSERT_NE(m_Instance, nullptr);
    ASSERT_NE(m_PhysicalDevice, nullptr);
    ASSERT_NE(m_Surface, VK_NULL_HANDLE);

    m_Device = RHI::RHIDevice::Create(m_Instance, m_PhysicalDevice, m_Surface);
    ASSERT_NE(m_Device, nullptr);

    VmaAllocator allocator = m_Device->GetAllocator();
    ASSERT_NE(allocator, VK_NULL_HANDLE);

    // Query statistics to verify allocator is functional
    VmaTotalStatistics stats;
    vmaCalculateStatistics(allocator, &stats);

    // Initial state should have no allocations
    EXPECT_EQ(stats.total.statistics.allocationCount, 0u);
}

// =============================================================================
// Extension Tests
// =============================================================================

TEST_F(RHIDeviceTest, GetEnabledExtensionsReturnsSwapchain) {
    ASSERT_NE(m_Instance, nullptr);
    ASSERT_NE(m_PhysicalDevice, nullptr);
    ASSERT_NE(m_Surface, VK_NULL_HANDLE);

    m_Device = RHI::RHIDevice::Create(m_Instance, m_PhysicalDevice, m_Surface);
    ASSERT_NE(m_Device, nullptr);

    const auto& extensions = m_Device->GetEnabledExtensions();

    // Should have at least swapchain extension
    EXPECT_GT(extensions.size(), 0u);

    bool hasSwapchain = false;
    for (const char* ext : extensions) {
        if (std::strcmp(ext, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
            hasSwapchain = true;
            break;
        }
    }
    EXPECT_TRUE(hasSwapchain);
}

// =============================================================================
// Wait Idle Tests
// =============================================================================

TEST_F(RHIDeviceTest, WaitIdleCompletes) {
    ASSERT_NE(m_Instance, nullptr);
    ASSERT_NE(m_PhysicalDevice, nullptr);
    ASSERT_NE(m_Surface, VK_NULL_HANDLE);

    m_Device = RHI::RHIDevice::Create(m_Instance, m_PhysicalDevice, m_Surface);
    ASSERT_NE(m_Device, nullptr);

    // WaitIdle should complete without throwing
    EXPECT_NO_THROW(m_Device->WaitIdle());
}

// =============================================================================
// Multiple Device Creation Tests
// =============================================================================

TEST_F(RHIDeviceTest, MultipleDevicesCanBeCreated) {
    ASSERT_NE(m_Instance, nullptr);
    ASSERT_NE(m_PhysicalDevice, nullptr);
    ASSERT_NE(m_Surface, VK_NULL_HANDLE);

    // Create first device
    m_Device = RHI::RHIDevice::Create(m_Instance, m_PhysicalDevice, m_Surface);
    ASSERT_NE(m_Device, nullptr);

    // Store first device handle
    VkDevice firstHandle = m_Device->GetHandle();

    // Destroy first device
    m_Device.reset();

    // Create second device
    m_Device = RHI::RHIDevice::Create(m_Instance, m_PhysicalDevice, m_Surface);
    ASSERT_NE(m_Device, nullptr);

    // Second device should have a valid handle
    EXPECT_NE(m_Device->GetHandle(), VK_NULL_HANDLE);

    // Note: Handle values may or may not be the same (implementation-defined)
    // We just verify it's a valid handle
    (void)firstHandle;
}

// =============================================================================
// Destruction Order Tests
// =============================================================================

TEST_F(RHIDeviceTest, DeviceCanBeDestroyedBeforePhysicalDevice) {
    ASSERT_NE(m_Instance, nullptr);
    ASSERT_NE(m_PhysicalDevice, nullptr);
    ASSERT_NE(m_Surface, VK_NULL_HANDLE);

    m_Device = RHI::RHIDevice::Create(m_Instance, m_PhysicalDevice, m_Surface);
    ASSERT_NE(m_Device, nullptr);

    // Destroying device should not crash
    EXPECT_NO_THROW(m_Device.reset());

    // Physical device should still be valid
    EXPECT_NE(m_PhysicalDevice, nullptr);
    EXPECT_NE(m_PhysicalDevice->GetHandle(), VK_NULL_HANDLE);
}
