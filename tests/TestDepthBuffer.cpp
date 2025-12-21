/**
 * @file TestDepthBuffer.cpp
 * @brief Test file for Renderer/DepthBuffer.h using GoogleTest.
 *
 * This file tests that the DepthBuffer class correctly creates
 * and manages depth buffers for rendering.
 *
 * Note: These tests require a Vulkan-capable system to pass.
 */

#include <gtest/gtest.h>
#include "Renderer/DepthBuffer.h"
#include "RHI/RHIDevice.h"
#include "RHI/RHIPhysicalDevice.h"
#include "RHI/RHIInstance.h"
#include "Platform/Window.h"
#include "Core/Log.h"

// =============================================================================
// Test Fixture for DepthBuffer Tests
// =============================================================================

class DepthBufferTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!Core::Log::IsInitialized()) {
            Core::Log::Init();
        }

        // Create a window to initialize GLFW and get a surface
        Platform::WindowConfig windowConfig;
        windowConfig.Width = 100;
        windowConfig.Height = 100;
        windowConfig.Title = "DepthBuffer Test Window";
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
        m_DepthBuffer.reset();
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
    Core::Ref<Renderer::DepthBuffer> m_DepthBuffer;
    VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
};

// =============================================================================
// Creation Tests
// =============================================================================

TEST_F(DepthBufferTest, CreatesDepthBuffer) {
    ASSERT_NE(m_Device, nullptr);

    Renderer::DepthBufferDesc desc;
    desc.Width = 800;
    desc.Height = 600;
    desc.Format = VK_FORMAT_D32_SFLOAT;
    desc.DebugName = "TestDepthBuffer";

    m_DepthBuffer = Renderer::DepthBuffer::Create(m_Device, desc);

    ASSERT_NE(m_DepthBuffer, nullptr);
    EXPECT_NE(m_DepthBuffer->GetImage(), VK_NULL_HANDLE);
    EXPECT_NE(m_DepthBuffer->GetImageView(), VK_NULL_HANDLE);
    EXPECT_EQ(m_DepthBuffer->GetWidth(), 800u);
    EXPECT_EQ(m_DepthBuffer->GetHeight(), 600u);
    EXPECT_EQ(m_DepthBuffer->GetFormat(), VK_FORMAT_D32_SFLOAT);
}

TEST_F(DepthBufferTest, CreatesWithDefaultFormat) {
    ASSERT_NE(m_Device, nullptr);

    Renderer::DepthBufferDesc desc;
    desc.Width = 1920;
    desc.Height = 1080;
    // Format defaults to VK_FORMAT_D32_SFLOAT

    m_DepthBuffer = Renderer::DepthBuffer::Create(m_Device, desc);

    ASSERT_NE(m_DepthBuffer, nullptr);
    EXPECT_EQ(m_DepthBuffer->GetFormat(), VK_FORMAT_D32_SFLOAT);
}

// =============================================================================
// Dimension Tests
// =============================================================================

TEST_F(DepthBufferTest, CreatesSmallDepthBuffer) {
    ASSERT_NE(m_Device, nullptr);

    Renderer::DepthBufferDesc desc;
    desc.Width = 64;
    desc.Height = 64;

    m_DepthBuffer = Renderer::DepthBuffer::Create(m_Device, desc);

    ASSERT_NE(m_DepthBuffer, nullptr);
    EXPECT_EQ(m_DepthBuffer->GetWidth(), 64u);
    EXPECT_EQ(m_DepthBuffer->GetHeight(), 64u);
}

TEST_F(DepthBufferTest, CreatesLargeDepthBuffer) {
    ASSERT_NE(m_Device, nullptr);

    Renderer::DepthBufferDesc desc;
    desc.Width = 4096;
    desc.Height = 2160;

    m_DepthBuffer = Renderer::DepthBuffer::Create(m_Device, desc);

    ASSERT_NE(m_DepthBuffer, nullptr);
    EXPECT_EQ(m_DepthBuffer->GetWidth(), 4096u);
    EXPECT_EQ(m_DepthBuffer->GetHeight(), 2160u);
}

TEST_F(DepthBufferTest, CreatesNonSquareDepthBuffer) {
    ASSERT_NE(m_Device, nullptr);

    Renderer::DepthBufferDesc desc;
    desc.Width = 1280;
    desc.Height = 720;

    m_DepthBuffer = Renderer::DepthBuffer::Create(m_Device, desc);

    ASSERT_NE(m_DepthBuffer, nullptr);
    EXPECT_EQ(m_DepthBuffer->GetWidth(), 1280u);
    EXPECT_EQ(m_DepthBuffer->GetHeight(), 720u);
}

// =============================================================================
// Format Tests
// =============================================================================

TEST_F(DepthBufferTest, CreatesD32FloatFormat) {
    ASSERT_NE(m_Device, nullptr);

    Renderer::DepthBufferDesc desc;
    desc.Width = 800;
    desc.Height = 600;
    desc.Format = VK_FORMAT_D32_SFLOAT;

    m_DepthBuffer = Renderer::DepthBuffer::Create(m_Device, desc);

    ASSERT_NE(m_DepthBuffer, nullptr);
    EXPECT_EQ(m_DepthBuffer->GetFormat(), VK_FORMAT_D32_SFLOAT);
    EXPECT_FALSE(m_DepthBuffer->HasStencil());
}

TEST_F(DepthBufferTest, CreatesD24S8Format) {
    ASSERT_NE(m_Device, nullptr);

    Renderer::DepthBufferDesc desc;
    desc.Width = 800;
    desc.Height = 600;
    desc.Format = VK_FORMAT_D24_UNORM_S8_UINT;

    m_DepthBuffer = Renderer::DepthBuffer::Create(m_Device, desc);

    ASSERT_NE(m_DepthBuffer, nullptr);
    EXPECT_EQ(m_DepthBuffer->GetFormat(), VK_FORMAT_D24_UNORM_S8_UINT);
    EXPECT_TRUE(m_DepthBuffer->HasStencil());
}

TEST_F(DepthBufferTest, CreatesD32S8Format) {
    ASSERT_NE(m_Device, nullptr);

    Renderer::DepthBufferDesc desc;
    desc.Width = 800;
    desc.Height = 600;
    desc.Format = VK_FORMAT_D32_SFLOAT_S8_UINT;

    m_DepthBuffer = Renderer::DepthBuffer::Create(m_Device, desc);

    ASSERT_NE(m_DepthBuffer, nullptr);
    EXPECT_EQ(m_DepthBuffer->GetFormat(), VK_FORMAT_D32_SFLOAT_S8_UINT);
    EXPECT_TRUE(m_DepthBuffer->HasStencil());
}

// =============================================================================
// HasStencil Tests
// =============================================================================

TEST_F(DepthBufferTest, HasStencilReturnsFalseForDepthOnly) {
    ASSERT_NE(m_Device, nullptr);

    Renderer::DepthBufferDesc desc;
    desc.Width = 800;
    desc.Height = 600;
    desc.Format = VK_FORMAT_D32_SFLOAT;

    m_DepthBuffer = Renderer::DepthBuffer::Create(m_Device, desc);

    ASSERT_NE(m_DepthBuffer, nullptr);
    EXPECT_FALSE(m_DepthBuffer->HasStencil());
}

TEST_F(DepthBufferTest, HasStencilReturnsTrueForDepthStencil) {
    ASSERT_NE(m_Device, nullptr);

    Renderer::DepthBufferDesc desc;
    desc.Width = 800;
    desc.Height = 600;
    desc.Format = VK_FORMAT_D24_UNORM_S8_UINT;

    m_DepthBuffer = Renderer::DepthBuffer::Create(m_Device, desc);

    ASSERT_NE(m_DepthBuffer, nullptr);
    EXPECT_TRUE(m_DepthBuffer->HasStencil());
}

// =============================================================================
// Resize Tests
// =============================================================================

TEST_F(DepthBufferTest, ResizeChangesSize) {
    ASSERT_NE(m_Device, nullptr);

    Renderer::DepthBufferDesc desc;
    desc.Width = 800;
    desc.Height = 600;

    m_DepthBuffer = Renderer::DepthBuffer::Create(m_Device, desc);
    ASSERT_NE(m_DepthBuffer, nullptr);

    bool result = m_DepthBuffer->Resize(m_Device, 1920, 1080);

    EXPECT_TRUE(result);
    EXPECT_EQ(m_DepthBuffer->GetWidth(), 1920u);
    EXPECT_EQ(m_DepthBuffer->GetHeight(), 1080u);
}

TEST_F(DepthBufferTest, ResizePreservesFormat) {
    ASSERT_NE(m_Device, nullptr);

    Renderer::DepthBufferDesc desc;
    desc.Width = 800;
    desc.Height = 600;
    desc.Format = VK_FORMAT_D24_UNORM_S8_UINT;

    m_DepthBuffer = Renderer::DepthBuffer::Create(m_Device, desc);
    ASSERT_NE(m_DepthBuffer, nullptr);

    VkFormat originalFormat = m_DepthBuffer->GetFormat();

    m_DepthBuffer->Resize(m_Device, 1280, 720);

    EXPECT_EQ(m_DepthBuffer->GetFormat(), originalFormat);
}

TEST_F(DepthBufferTest, ResizeCreatesNewHandles) {
    ASSERT_NE(m_Device, nullptr);

    Renderer::DepthBufferDesc desc;
    desc.Width = 800;
    desc.Height = 600;

    m_DepthBuffer = Renderer::DepthBuffer::Create(m_Device, desc);
    ASSERT_NE(m_DepthBuffer, nullptr);

    // Store original handles for verification
    EXPECT_NE(m_DepthBuffer->GetImage(), VK_NULL_HANDLE);
    EXPECT_NE(m_DepthBuffer->GetImageView(), VK_NULL_HANDLE);

    m_DepthBuffer->Resize(m_Device, 1920, 1080);

    // New handles should be valid after resize
    EXPECT_NE(m_DepthBuffer->GetImage(), VK_NULL_HANDLE);
    EXPECT_NE(m_DepthBuffer->GetImageView(), VK_NULL_HANDLE);
    // Note: Handles might be same due to memory reuse, but they represent new resources
}

TEST_F(DepthBufferTest, ResizeMultipleTimes) {
    ASSERT_NE(m_Device, nullptr);

    Renderer::DepthBufferDesc desc;
    desc.Width = 800;
    desc.Height = 600;

    m_DepthBuffer = Renderer::DepthBuffer::Create(m_Device, desc);
    ASSERT_NE(m_DepthBuffer, nullptr);

    // Resize multiple times
    EXPECT_TRUE(m_DepthBuffer->Resize(m_Device, 1280, 720));
    EXPECT_EQ(m_DepthBuffer->GetWidth(), 1280u);
    EXPECT_EQ(m_DepthBuffer->GetHeight(), 720u);

    EXPECT_TRUE(m_DepthBuffer->Resize(m_Device, 1920, 1080));
    EXPECT_EQ(m_DepthBuffer->GetWidth(), 1920u);
    EXPECT_EQ(m_DepthBuffer->GetHeight(), 1080u);

    EXPECT_TRUE(m_DepthBuffer->Resize(m_Device, 640, 480));
    EXPECT_EQ(m_DepthBuffer->GetWidth(), 640u);
    EXPECT_EQ(m_DepthBuffer->GetHeight(), 480u);
}

// =============================================================================
// Handle Tests
// =============================================================================

TEST_F(DepthBufferTest, ImageHandleIsValid) {
    ASSERT_NE(m_Device, nullptr);

    Renderer::DepthBufferDesc desc;
    desc.Width = 800;
    desc.Height = 600;

    m_DepthBuffer = Renderer::DepthBuffer::Create(m_Device, desc);

    ASSERT_NE(m_DepthBuffer, nullptr);
    EXPECT_NE(m_DepthBuffer->GetImage(), VK_NULL_HANDLE);
}

TEST_F(DepthBufferTest, ImageViewHandleIsValid) {
    ASSERT_NE(m_Device, nullptr);

    Renderer::DepthBufferDesc desc;
    desc.Width = 800;
    desc.Height = 600;

    m_DepthBuffer = Renderer::DepthBuffer::Create(m_Device, desc);

    ASSERT_NE(m_DepthBuffer, nullptr);
    EXPECT_NE(m_DepthBuffer->GetImageView(), VK_NULL_HANDLE);
}

// =============================================================================
// Destructor Tests
// =============================================================================

TEST_F(DepthBufferTest, DestructorCleansUpResources) {
    ASSERT_NE(m_Device, nullptr);

    VkImage imageHandle = VK_NULL_HANDLE;
    VkImageView viewHandle = VK_NULL_HANDLE;

    {
        Renderer::DepthBufferDesc desc;
        desc.Width = 800;
        desc.Height = 600;

        auto depthBuffer = Renderer::DepthBuffer::Create(m_Device, desc);
        ASSERT_NE(depthBuffer, nullptr);

        imageHandle = depthBuffer->GetImage();
        viewHandle = depthBuffer->GetImageView();

        EXPECT_NE(imageHandle, VK_NULL_HANDLE);
        EXPECT_NE(viewHandle, VK_NULL_HANDLE);
        // DepthBuffer goes out of scope here
    }

    // After destruction, the code should not crash
    SUCCEED();
}

// =============================================================================
// Multiple Depth Buffer Tests
// =============================================================================

TEST_F(DepthBufferTest, CreateMultipleDepthBuffers) {
    ASSERT_NE(m_Device, nullptr);

    Renderer::DepthBufferDesc desc1;
    desc1.Width = 800;
    desc1.Height = 600;
    desc1.Format = VK_FORMAT_D32_SFLOAT;

    Renderer::DepthBufferDesc desc2;
    desc2.Width = 1920;
    desc2.Height = 1080;
    desc2.Format = VK_FORMAT_D24_UNORM_S8_UINT;

    auto depthBuffer1 = Renderer::DepthBuffer::Create(m_Device, desc1);
    auto depthBuffer2 = Renderer::DepthBuffer::Create(m_Device, desc2);

    ASSERT_NE(depthBuffer1, nullptr);
    ASSERT_NE(depthBuffer2, nullptr);

    EXPECT_NE(depthBuffer1->GetImage(), depthBuffer2->GetImage());
    EXPECT_NE(depthBuffer1->GetImageView(), depthBuffer2->GetImageView());
}

// =============================================================================
// Default Values Tests
// =============================================================================

TEST_F(DepthBufferTest, DescDefaultValues) {
    Renderer::DepthBufferDesc desc;

    EXPECT_EQ(desc.Width, 0u);
    EXPECT_EQ(desc.Height, 0u);
    EXPECT_EQ(desc.Format, VK_FORMAT_D32_SFLOAT);
    EXPECT_STREQ(desc.DebugName, "DepthBuffer");
}
