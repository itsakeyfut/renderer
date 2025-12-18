/**
 * @file TestRHIPhysicalDevice.cpp
 * @brief Test file for RHI/RHIPhysicalDevice.h using GoogleTest.
 *
 * This file tests that the RHIPhysicalDevice class correctly enumerates,
 * selects, and queries physical devices (GPUs).
 *
 * Note: These tests require a Vulkan-capable system to pass. On systems without
 * Vulkan support, some tests may be skipped.
 */

#include <gtest/gtest.h>
#include "RHI/RHIPhysicalDevice.h"
#include "RHI/RHIInstance.h"
#include "Platform/Window.h"
#include "Core/Log.h"

// =============================================================================
// Test Fixture for RHIPhysicalDevice Tests
// =============================================================================

class RHIPhysicalDeviceTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!Core::Log::IsInitialized()) {
            Core::Log::Init();
        }

        // Create a window to initialize GLFW and get a surface
        Platform::WindowConfig windowConfig;
        windowConfig.Width = 100;
        windowConfig.Height = 100;
        windowConfig.Title = "RHI Physical Device Test Window";
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
    }

    void TearDown() override {
        if (m_Surface != VK_NULL_HANDLE && m_Instance) {
            vkDestroySurfaceKHR(m_Instance->GetHandle(), m_Surface, nullptr);
            m_Surface = VK_NULL_HANDLE;
        }
        m_Instance.reset();
        m_Window.reset();
    }

    std::unique_ptr<Platform::Window> m_Window;
    Core::Ref<RHI::RHIInstance> m_Instance;
    VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
};

// =============================================================================
// Device Enumeration Tests
// =============================================================================

TEST_F(RHIPhysicalDeviceTest, EnumeratesDevices) {
    ASSERT_NE(m_Instance, nullptr);
    ASSERT_NE(m_Surface, VK_NULL_HANDLE);

    auto devices = RHI::RHIPhysicalDevice::EnumerateDevices(m_Instance, m_Surface);

    // At least one Vulkan-capable device should exist on test system
    EXPECT_GT(devices.size(), 0u);
}

TEST_F(RHIPhysicalDeviceTest, EnumeratedDevicesHaveValidProperties) {
    ASSERT_NE(m_Instance, nullptr);
    ASSERT_NE(m_Surface, VK_NULL_HANDLE);

    auto devices = RHI::RHIPhysicalDevice::EnumerateDevices(m_Instance, m_Surface);
    ASSERT_GT(devices.size(), 0u);

    for (const auto& device : devices) {
        // Device handle should be valid
        EXPECT_NE(device.Device, VK_NULL_HANDLE);

        // Device should have a name
        EXPECT_FALSE(device.GetDeviceName().empty());

        // Device type should be valid
        EXPECT_GE(device.Properties.deviceType, VK_PHYSICAL_DEVICE_TYPE_OTHER);
        EXPECT_LE(device.Properties.deviceType, VK_PHYSICAL_DEVICE_TYPE_CPU);

        // API version should be non-zero
        EXPECT_GT(device.Properties.apiVersion, 0u);
    }
}

TEST_F(RHIPhysicalDeviceTest, EnumeratedDevicesHaveQueueFamilies) {
    ASSERT_NE(m_Instance, nullptr);
    ASSERT_NE(m_Surface, VK_NULL_HANDLE);

    auto devices = RHI::RHIPhysicalDevice::EnumerateDevices(m_Instance, m_Surface);
    ASSERT_GT(devices.size(), 0u);

    // At least one device should have complete queue families
    bool foundCompleteDevice = false;
    for (const auto& device : devices) {
        if (device.QueueFamilies.IsComplete()) {
            foundCompleteDevice = true;
            EXPECT_TRUE(device.QueueFamilies.GraphicsFamily.has_value());
            EXPECT_TRUE(device.QueueFamilies.PresentFamily.has_value());
            break;
        }
    }

    EXPECT_TRUE(foundCompleteDevice) << "No device with complete queue families found";
}

// =============================================================================
// Device Selection Tests
// =============================================================================

TEST_F(RHIPhysicalDeviceTest, SelectsPhysicalDevice) {
    ASSERT_NE(m_Instance, nullptr);
    ASSERT_NE(m_Surface, VK_NULL_HANDLE);

    auto physicalDevice = RHI::RHIPhysicalDevice::Select(m_Instance, m_Surface);

    ASSERT_NE(physicalDevice, nullptr);
    EXPECT_NE(physicalDevice->GetHandle(), VK_NULL_HANDLE);
}

TEST_F(RHIPhysicalDeviceTest, SelectedDeviceHasValidHandle) {
    ASSERT_NE(m_Instance, nullptr);
    ASSERT_NE(m_Surface, VK_NULL_HANDLE);

    auto physicalDevice = RHI::RHIPhysicalDevice::Select(m_Instance, m_Surface);
    ASSERT_NE(physicalDevice, nullptr);

    VkPhysicalDevice handle = physicalDevice->GetHandle();
    EXPECT_NE(handle, VK_NULL_HANDLE);
}

TEST_F(RHIPhysicalDeviceTest, SelectedDeviceHasCompleteQueueFamilies) {
    ASSERT_NE(m_Instance, nullptr);
    ASSERT_NE(m_Surface, VK_NULL_HANDLE);

    auto physicalDevice = RHI::RHIPhysicalDevice::Select(m_Instance, m_Surface);
    ASSERT_NE(physicalDevice, nullptr);

    const auto& queueFamilies = physicalDevice->GetQueueFamilies();
    EXPECT_TRUE(queueFamilies.IsComplete());
    EXPECT_TRUE(queueFamilies.GraphicsFamily.has_value());
    EXPECT_TRUE(queueFamilies.PresentFamily.has_value());
}

// =============================================================================
// Device Info Tests
// =============================================================================

TEST_F(RHIPhysicalDeviceTest, GetInfoReturnsValidData) {
    ASSERT_NE(m_Instance, nullptr);
    ASSERT_NE(m_Surface, VK_NULL_HANDLE);

    auto physicalDevice = RHI::RHIPhysicalDevice::Select(m_Instance, m_Surface);
    ASSERT_NE(physicalDevice, nullptr);

    const auto& info = physicalDevice->GetInfo();

    // Validate device info
    EXPECT_NE(info.Device, VK_NULL_HANDLE);
    EXPECT_FALSE(info.GetDeviceName().empty());
    EXPECT_FALSE(info.GetDeviceTypeString().empty());
}

TEST_F(RHIPhysicalDeviceTest, GetPropertiesReturnsValidData) {
    ASSERT_NE(m_Instance, nullptr);
    ASSERT_NE(m_Surface, VK_NULL_HANDLE);

    auto physicalDevice = RHI::RHIPhysicalDevice::Select(m_Instance, m_Surface);
    ASSERT_NE(physicalDevice, nullptr);

    const auto& properties = physicalDevice->GetProperties();

    // Device name should be valid
    EXPECT_NE(properties.deviceName[0], '\0');

    // API version should be at least Vulkan 1.0
    EXPECT_GE(VK_VERSION_MAJOR(properties.apiVersion), 1u);

    // Max image dimension should be reasonable
    EXPECT_GT(properties.limits.maxImageDimension2D, 0u);
}

TEST_F(RHIPhysicalDeviceTest, GetMemoryPropertiesReturnsValidData) {
    ASSERT_NE(m_Instance, nullptr);
    ASSERT_NE(m_Surface, VK_NULL_HANDLE);

    auto physicalDevice = RHI::RHIPhysicalDevice::Select(m_Instance, m_Surface);
    ASSERT_NE(physicalDevice, nullptr);

    const auto& memoryProps = physicalDevice->GetMemoryProperties();

    // Should have at least one memory type and heap
    EXPECT_GT(memoryProps.memoryTypeCount, 0u);
    EXPECT_GT(memoryProps.memoryHeapCount, 0u);
}

TEST_F(RHIPhysicalDeviceTest, GetDeviceLocalMemorySizeReturnsPositive) {
    ASSERT_NE(m_Instance, nullptr);
    ASSERT_NE(m_Surface, VK_NULL_HANDLE);

    auto physicalDevice = RHI::RHIPhysicalDevice::Select(m_Instance, m_Surface);
    ASSERT_NE(physicalDevice, nullptr);

    VkDeviceSize vramSize = physicalDevice->GetInfo().GetDeviceLocalMemorySize();

    // Should have at least some device local memory
    EXPECT_GT(vramSize, 0u);
}

// =============================================================================
// Queue Family Tests
// =============================================================================

TEST_F(RHIPhysicalDeviceTest, QueueFamilyIndicesIsCompleteReturnsCorrectly) {
    RHI::QueueFamilyIndices indices;

    // Initially not complete
    EXPECT_FALSE(indices.IsComplete());

    // Only graphics is not complete
    indices.GraphicsFamily = 0;
    EXPECT_FALSE(indices.IsComplete());

    // Both graphics and present is complete
    indices.PresentFamily = 0;
    EXPECT_TRUE(indices.IsComplete());
}

TEST_F(RHIPhysicalDeviceTest, QueueFamilyIndicesHasAllFamiliesReturnsCorrectly) {
    RHI::QueueFamilyIndices indices;

    // Initially false
    EXPECT_FALSE(indices.HasAllFamilies());

    indices.GraphicsFamily = 0;
    EXPECT_FALSE(indices.HasAllFamilies());

    indices.PresentFamily = 0;
    EXPECT_FALSE(indices.HasAllFamilies());

    indices.ComputeFamily = 1;
    EXPECT_FALSE(indices.HasAllFamilies());

    indices.TransferFamily = 2;
    EXPECT_TRUE(indices.HasAllFamilies());
}

TEST_F(RHIPhysicalDeviceTest, QueueFamilyIndicesHasDedicatedTransferQueue) {
    RHI::QueueFamilyIndices indices;

    // No transfer family
    indices.GraphicsFamily = 0;
    EXPECT_FALSE(indices.HasDedicatedTransferQueue());

    // Same family
    indices.TransferFamily = 0;
    EXPECT_FALSE(indices.HasDedicatedTransferQueue());

    // Different family = dedicated
    indices.TransferFamily = 1;
    EXPECT_TRUE(indices.HasDedicatedTransferQueue());
}

// =============================================================================
// Device Type Tests
// =============================================================================

TEST_F(RHIPhysicalDeviceTest, PhysicalDeviceInfoReportsDeviceTypeCorrectly) {
    RHI::PhysicalDeviceInfo info;

    info.Properties.deviceType = VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
    EXPECT_TRUE(info.IsDiscreteGPU());
    EXPECT_FALSE(info.IsIntegratedGPU());
    EXPECT_EQ(info.GetDeviceTypeString(), "Discrete GPU");

    info.Properties.deviceType = VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;
    EXPECT_FALSE(info.IsDiscreteGPU());
    EXPECT_TRUE(info.IsIntegratedGPU());
    EXPECT_EQ(info.GetDeviceTypeString(), "Integrated GPU");

    info.Properties.deviceType = VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU;
    EXPECT_EQ(info.GetDeviceTypeString(), "Virtual GPU");

    info.Properties.deviceType = VK_PHYSICAL_DEVICE_TYPE_CPU;
    EXPECT_EQ(info.GetDeviceTypeString(), "CPU");

    info.Properties.deviceType = VK_PHYSICAL_DEVICE_TYPE_OTHER;
    EXPECT_EQ(info.GetDeviceTypeString(), "Other");
}

// =============================================================================
// Extension Support Tests
// =============================================================================

TEST_F(RHIPhysicalDeviceTest, ChecksSwapchainExtensionSupport) {
    ASSERT_NE(m_Instance, nullptr);
    ASSERT_NE(m_Surface, VK_NULL_HANDLE);

    auto physicalDevice = RHI::RHIPhysicalDevice::Select(m_Instance, m_Surface);
    ASSERT_NE(physicalDevice, nullptr);

    // Any device that can present should support swapchain
    bool supportsSwapchain = physicalDevice->IsExtensionSupported(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    EXPECT_TRUE(supportsSwapchain);
}

TEST_F(RHIPhysicalDeviceTest, GetSupportedExtensionsReturnsNonEmpty) {
    ASSERT_NE(m_Instance, nullptr);
    ASSERT_NE(m_Surface, VK_NULL_HANDLE);

    auto physicalDevice = RHI::RHIPhysicalDevice::Select(m_Instance, m_Surface);
    ASSERT_NE(physicalDevice, nullptr);

    const auto& extensions = physicalDevice->GetSupportedExtensions();

    // Any Vulkan device should support at least some extensions
    EXPECT_GT(extensions.size(), 0u);
}

// =============================================================================
// Multiple Selection Tests
// =============================================================================

TEST_F(RHIPhysicalDeviceTest, MultipleSelectionsReturnSameDevice) {
    ASSERT_NE(m_Instance, nullptr);
    ASSERT_NE(m_Surface, VK_NULL_HANDLE);

    auto physicalDevice1 = RHI::RHIPhysicalDevice::Select(m_Instance, m_Surface);
    auto physicalDevice2 = RHI::RHIPhysicalDevice::Select(m_Instance, m_Surface);

    ASSERT_NE(physicalDevice1, nullptr);
    ASSERT_NE(physicalDevice2, nullptr);

    // Should select the same physical device (best scored device)
    EXPECT_EQ(physicalDevice1->GetHandle(), physicalDevice2->GetHandle());
}
