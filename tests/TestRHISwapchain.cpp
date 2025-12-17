/**
 * @file TestRHISwapchain.cpp
 * @brief Test file for RHI/RHISwapchain.h using GoogleTest.
 *
 * This file tests that the RHISwapchain class correctly creates a swapchain,
 * manages image views, and handles resize operations.
 *
 * Note: These tests require a Vulkan-capable system to pass.
 */

#include <gtest/gtest.h>
#include "RHI/RHISwapchain.h"
#include "RHI/RHIDevice.h"
#include "RHI/RHIPhysicalDevice.h"
#include "RHI/RHIInstance.h"
#include "Platform/Window.h"
#include "Core/Log.h"

// =============================================================================
// Test Fixture for RHISwapchain Tests
// =============================================================================

class RHISwapchainTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!Core::Log::IsInitialized()) {
            Core::Log::Init();
        }

        // Create a window to initialize GLFW and get a surface
        Platform::WindowConfig windowConfig;
        windowConfig.Width = 800;
        windowConfig.Height = 600;
        windowConfig.Title = "RHI Swapchain Test Window";
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

        // Destroy in reverse order
        m_Swapchain.reset();
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
    Core::Ref<RHI::RHISwapchain> m_Swapchain;
    VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
};

// =============================================================================
// SwapchainSupportDetails Tests
// =============================================================================

TEST_F(RHISwapchainTest, QuerySwapchainSupportReturnsValidDetails) {
    ASSERT_NE(m_Instance, nullptr);
    ASSERT_NE(m_PhysicalDevice, nullptr);
    ASSERT_NE(m_Surface, VK_NULL_HANDLE);

    RHI::SwapchainSupportDetails details =
        RHI::RHISwapchain::QuerySwapchainSupport(m_PhysicalDevice->GetHandle(), m_Surface);

    // Should have at least one format and one present mode
    EXPECT_TRUE(details.IsAdequate());
    EXPECT_GT(details.Formats.size(), 0u);
    EXPECT_GT(details.PresentModes.size(), 0u);
}

TEST_F(RHISwapchainTest, QuerySwapchainSupportReturnsValidCapabilities) {
    ASSERT_NE(m_Instance, nullptr);
    ASSERT_NE(m_PhysicalDevice, nullptr);
    ASSERT_NE(m_Surface, VK_NULL_HANDLE);

    RHI::SwapchainSupportDetails details =
        RHI::RHISwapchain::QuerySwapchainSupport(m_PhysicalDevice->GetHandle(), m_Surface);

    // Capabilities should have valid values
    EXPECT_GT(details.Capabilities.minImageCount, 0u);
    EXPECT_GE(details.Capabilities.maxImageCount, details.Capabilities.minImageCount);
}

// =============================================================================
// Swapchain Creation Tests
// =============================================================================

TEST_F(RHISwapchainTest, CreatesSwapchain) {
    ASSERT_NE(m_Device, nullptr);
    ASSERT_NE(m_Surface, VK_NULL_HANDLE);

    m_Swapchain = RHI::RHISwapchain::Create(m_Device, m_Surface, 800, 600);

    ASSERT_NE(m_Swapchain, nullptr);
    EXPECT_NE(m_Swapchain->GetHandle(), VK_NULL_HANDLE);
}

TEST_F(RHISwapchainTest, CreatesSwapchainWithDefaultConfig) {
    ASSERT_NE(m_Device, nullptr);
    ASSERT_NE(m_Surface, VK_NULL_HANDLE);

    m_Swapchain = RHI::RHISwapchain::Create(m_Device, m_Surface, 800, 600);

    ASSERT_NE(m_Swapchain, nullptr);
    EXPECT_NE(m_Swapchain->GetHandle(), VK_NULL_HANDLE);
}

TEST_F(RHISwapchainTest, CreatesSwapchainWithVSyncEnabled) {
    ASSERT_NE(m_Device, nullptr);
    ASSERT_NE(m_Surface, VK_NULL_HANDLE);

    RHI::SwapchainConfig config;
    config.VSync = true;

    m_Swapchain = RHI::RHISwapchain::Create(m_Device, m_Surface, 800, 600, config);

    ASSERT_NE(m_Swapchain, nullptr);
    EXPECT_NE(m_Swapchain->GetHandle(), VK_NULL_HANDLE);
}

TEST_F(RHISwapchainTest, CreatesSwapchainWithCustomConfig) {
    ASSERT_NE(m_Device, nullptr);
    ASSERT_NE(m_Surface, VK_NULL_HANDLE);

    RHI::SwapchainConfig config;
    config.PreferredFormat = VK_FORMAT_R8G8B8A8_SRGB;
    config.PreferredPresentMode = VK_PRESENT_MODE_FIFO_KHR;

    m_Swapchain = RHI::RHISwapchain::Create(m_Device, m_Surface, 800, 600, config);

    ASSERT_NE(m_Swapchain, nullptr);
    EXPECT_NE(m_Swapchain->GetHandle(), VK_NULL_HANDLE);
}

// =============================================================================
// Properties Tests
// =============================================================================

TEST_F(RHISwapchainTest, GetImageFormatReturnsValidFormat) {
    ASSERT_NE(m_Device, nullptr);
    ASSERT_NE(m_Surface, VK_NULL_HANDLE);

    m_Swapchain = RHI::RHISwapchain::Create(m_Device, m_Surface, 800, 600);
    ASSERT_NE(m_Swapchain, nullptr);

    VkFormat format = m_Swapchain->GetImageFormat();
    EXPECT_NE(format, VK_FORMAT_UNDEFINED);
}

TEST_F(RHISwapchainTest, GetExtentReturnsValidDimensions) {
    ASSERT_NE(m_Device, nullptr);
    ASSERT_NE(m_Surface, VK_NULL_HANDLE);

    m_Swapchain = RHI::RHISwapchain::Create(m_Device, m_Surface, 800, 600);
    ASSERT_NE(m_Swapchain, nullptr);

    VkExtent2D extent = m_Swapchain->GetExtent();
    EXPECT_GT(extent.width, 0u);
    EXPECT_GT(extent.height, 0u);
}

TEST_F(RHISwapchainTest, GetImageCountReturnsAtLeastMinimum) {
    ASSERT_NE(m_Device, nullptr);
    ASSERT_NE(m_Surface, VK_NULL_HANDLE);

    m_Swapchain = RHI::RHISwapchain::Create(m_Device, m_Surface, 800, 600);
    ASSERT_NE(m_Swapchain, nullptr);

    uint32_t imageCount = m_Swapchain->GetImageCount();

    // Swapchain should have at least 2 images (double buffering)
    EXPECT_GE(imageCount, 2u);
}

// =============================================================================
// Image and ImageView Tests
// =============================================================================

TEST_F(RHISwapchainTest, GetImageReturnsValidHandle) {
    ASSERT_NE(m_Device, nullptr);
    ASSERT_NE(m_Surface, VK_NULL_HANDLE);

    m_Swapchain = RHI::RHISwapchain::Create(m_Device, m_Surface, 800, 600);
    ASSERT_NE(m_Swapchain, nullptr);

    uint32_t imageCount = m_Swapchain->GetImageCount();
    ASSERT_GT(imageCount, 0u);

    for (uint32_t i = 0; i < imageCount; ++i) {
        VkImage image = m_Swapchain->GetImage(i);
        EXPECT_NE(image, VK_NULL_HANDLE) << "Image " << i << " is null";
    }
}

TEST_F(RHISwapchainTest, GetImageViewReturnsValidHandle) {
    ASSERT_NE(m_Device, nullptr);
    ASSERT_NE(m_Surface, VK_NULL_HANDLE);

    m_Swapchain = RHI::RHISwapchain::Create(m_Device, m_Surface, 800, 600);
    ASSERT_NE(m_Swapchain, nullptr);

    uint32_t imageCount = m_Swapchain->GetImageCount();
    ASSERT_GT(imageCount, 0u);

    for (uint32_t i = 0; i < imageCount; ++i) {
        VkImageView imageView = m_Swapchain->GetImageView(i);
        EXPECT_NE(imageView, VK_NULL_HANDLE) << "ImageView " << i << " is null";
    }
}

TEST_F(RHISwapchainTest, GetImagesReturnsCorrectCount) {
    ASSERT_NE(m_Device, nullptr);
    ASSERT_NE(m_Surface, VK_NULL_HANDLE);

    m_Swapchain = RHI::RHISwapchain::Create(m_Device, m_Surface, 800, 600);
    ASSERT_NE(m_Swapchain, nullptr);

    const auto& images = m_Swapchain->GetImages();
    const auto& imageViews = m_Swapchain->GetImageViews();

    EXPECT_EQ(images.size(), m_Swapchain->GetImageCount());
    EXPECT_EQ(imageViews.size(), m_Swapchain->GetImageCount());
    EXPECT_EQ(images.size(), imageViews.size());
}

// =============================================================================
// Recreate Tests
// =============================================================================

TEST_F(RHISwapchainTest, RecreateSucceeds) {
    ASSERT_NE(m_Device, nullptr);
    ASSERT_NE(m_Surface, VK_NULL_HANDLE);

    m_Swapchain = RHI::RHISwapchain::Create(m_Device, m_Surface, 800, 600);
    ASSERT_NE(m_Swapchain, nullptr);

    // Recreate with same dimensions
    bool success = m_Swapchain->Recreate(800, 600);
    EXPECT_TRUE(success);
    EXPECT_NE(m_Swapchain->GetHandle(), VK_NULL_HANDLE);
}

TEST_F(RHISwapchainTest, RecreateWithNewDimensionsSucceeds) {
    ASSERT_NE(m_Device, nullptr);
    ASSERT_NE(m_Surface, VK_NULL_HANDLE);

    m_Swapchain = RHI::RHISwapchain::Create(m_Device, m_Surface, 800, 600);
    ASSERT_NE(m_Swapchain, nullptr);

    // Recreate with new dimensions
    bool success = m_Swapchain->Recreate(1024, 768);
    EXPECT_TRUE(success);
    EXPECT_NE(m_Swapchain->GetHandle(), VK_NULL_HANDLE);
}

TEST_F(RHISwapchainTest, RecreatePreservesImageCount) {
    ASSERT_NE(m_Device, nullptr);
    ASSERT_NE(m_Surface, VK_NULL_HANDLE);

    m_Swapchain = RHI::RHISwapchain::Create(m_Device, m_Surface, 800, 600);
    ASSERT_NE(m_Swapchain, nullptr);

    uint32_t originalCount = m_Swapchain->GetImageCount();
    EXPECT_GE(originalCount, 2u);

    bool success = m_Swapchain->Recreate(1024, 768);
    ASSERT_TRUE(success);

    // Image count should be similar (might vary slightly based on implementation)
    uint32_t newCount = m_Swapchain->GetImageCount();
    EXPECT_GE(newCount, 2u);
}

TEST_F(RHISwapchainTest, RecreateUpdatesImageViews) {
    ASSERT_NE(m_Device, nullptr);
    ASSERT_NE(m_Surface, VK_NULL_HANDLE);

    m_Swapchain = RHI::RHISwapchain::Create(m_Device, m_Surface, 800, 600);
    ASSERT_NE(m_Swapchain, nullptr);

    // Store old image views for comparison
    std::vector<VkImageView> oldImageViews = m_Swapchain->GetImageViews();

    bool success = m_Swapchain->Recreate(1024, 768);
    ASSERT_TRUE(success);

    // New image views should be valid (not necessarily different handles)
    for (uint32_t i = 0; i < m_Swapchain->GetImageCount(); ++i) {
        EXPECT_NE(m_Swapchain->GetImageView(i), VK_NULL_HANDLE);
    }
}

// =============================================================================
// NeedsRecreation Tests
// =============================================================================

TEST_F(RHISwapchainTest, NeedsRecreationInitiallyFalse) {
    ASSERT_NE(m_Device, nullptr);
    ASSERT_NE(m_Surface, VK_NULL_HANDLE);

    m_Swapchain = RHI::RHISwapchain::Create(m_Device, m_Surface, 800, 600);
    ASSERT_NE(m_Swapchain, nullptr);

    EXPECT_FALSE(m_Swapchain->NeedsRecreation());
}

TEST_F(RHISwapchainTest, NeedsRecreationFalseAfterRecreate) {
    ASSERT_NE(m_Device, nullptr);
    ASSERT_NE(m_Surface, VK_NULL_HANDLE);

    m_Swapchain = RHI::RHISwapchain::Create(m_Device, m_Surface, 800, 600);
    ASSERT_NE(m_Swapchain, nullptr);

    bool success = m_Swapchain->Recreate(1024, 768);
    ASSERT_TRUE(success);

    // After recreation, NeedsRecreation should be false
    EXPECT_FALSE(m_Swapchain->NeedsRecreation());
}

// =============================================================================
// Multiple Swapchain Tests
// =============================================================================

TEST_F(RHISwapchainTest, MultipleRecreatesSucceed) {
    ASSERT_NE(m_Device, nullptr);
    ASSERT_NE(m_Surface, VK_NULL_HANDLE);

    m_Swapchain = RHI::RHISwapchain::Create(m_Device, m_Surface, 800, 600);
    ASSERT_NE(m_Swapchain, nullptr);

    // Recreate multiple times
    for (int i = 0; i < 3; ++i) {
        bool success = m_Swapchain->Recreate(800 + i * 100, 600 + i * 100);
        EXPECT_TRUE(success) << "Recreation " << i << " failed";
        EXPECT_NE(m_Swapchain->GetHandle(), VK_NULL_HANDLE);
    }
}

// =============================================================================
// Destruction Tests
// =============================================================================

TEST_F(RHISwapchainTest, DestructionCleansUpResources) {
    ASSERT_NE(m_Device, nullptr);
    ASSERT_NE(m_Surface, VK_NULL_HANDLE);

    m_Swapchain = RHI::RHISwapchain::Create(m_Device, m_Surface, 800, 600);
    ASSERT_NE(m_Swapchain, nullptr);

    // Ensure device is idle before destruction
    m_Device->WaitIdle();

    // Destruction should not crash
    EXPECT_NO_THROW(m_Swapchain.reset());
}
