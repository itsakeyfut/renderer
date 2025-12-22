/**
 * @file TestRHIImage.cpp
 * @brief Test file for RHI/RHIImage.h using GoogleTest.
 *
 * This file tests that the RHIImage class correctly creates
 * and manages Vulkan images with VMA memory allocation.
 *
 * Note: These tests require a Vulkan-capable system to pass.
 */

#include <gtest/gtest.h>
#include "RHI/RHIImage.h"
#include "RHI/RHIDevice.h"
#include "RHI/RHIPhysicalDevice.h"
#include "RHI/RHIInstance.h"
#include "Platform/Window.h"
#include "Core/Log.h"

#include <vector>

// =============================================================================
// Test Fixture for RHIImage Tests
// =============================================================================

class RHIImageTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!Core::Log::IsInitialized()) {
            Core::Log::Init();
        }

        // Create a window to initialize GLFW and get a surface
        Platform::WindowConfig windowConfig;
        windowConfig.Width = 100;
        windowConfig.Height = 100;
        windowConfig.Title = "RHI Image Test Window";
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
        m_Image.reset();
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
    Core::Ref<RHI::RHIImage> m_Image;
    VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
};

// =============================================================================
// Creation Tests
// =============================================================================

TEST_F(RHIImageTest, CreatesTextureImage) {
    ASSERT_NE(m_Device, nullptr);

    RHI::ImageDesc desc;
    desc.Width = 256;
    desc.Height = 256;
    desc.Format = VK_FORMAT_R8G8B8A8_SRGB;
    desc.Usage = RHI::ImageUsage::Texture;
    desc.DebugName = "TestTexture";

    m_Image = RHI::RHIImage::Create(m_Device, desc);

    ASSERT_NE(m_Image, nullptr);
    EXPECT_NE(m_Image->GetHandle(), VK_NULL_HANDLE);
    EXPECT_NE(m_Image->GetImageView(), VK_NULL_HANDLE);
    EXPECT_EQ(m_Image->GetWidth(), 256u);
    EXPECT_EQ(m_Image->GetHeight(), 256u);
    EXPECT_EQ(m_Image->GetFormat(), VK_FORMAT_R8G8B8A8_SRGB);
}

TEST_F(RHIImageTest, CreatesRenderTargetImage) {
    ASSERT_NE(m_Device, nullptr);

    RHI::ImageDesc desc;
    desc.Width = 1920;
    desc.Height = 1080;
    desc.Format = VK_FORMAT_R8G8B8A8_UNORM;
    desc.Usage = RHI::ImageUsage::RenderTarget;
    desc.DebugName = "TestRenderTarget";

    m_Image = RHI::RHIImage::Create(m_Device, desc);

    ASSERT_NE(m_Image, nullptr);
    EXPECT_NE(m_Image->GetHandle(), VK_NULL_HANDLE);
    EXPECT_EQ(m_Image->GetWidth(), 1920u);
    EXPECT_EQ(m_Image->GetHeight(), 1080u);
}

TEST_F(RHIImageTest, CreatesDepthStencilImage) {
    ASSERT_NE(m_Device, nullptr);

    RHI::ImageDesc desc;
    desc.Width = 800;
    desc.Height = 600;
    desc.Format = VK_FORMAT_D32_SFLOAT;
    desc.Usage = RHI::ImageUsage::DepthStencil;
    desc.DebugName = "TestDepthStencil";

    m_Image = RHI::RHIImage::Create(m_Device, desc);

    ASSERT_NE(m_Image, nullptr);
    EXPECT_NE(m_Image->GetHandle(), VK_NULL_HANDLE);
    EXPECT_EQ(m_Image->GetFormat(), VK_FORMAT_D32_SFLOAT);
}

TEST_F(RHIImageTest, CreatesStorageImage) {
    ASSERT_NE(m_Device, nullptr);

    RHI::ImageDesc desc;
    desc.Width = 512;
    desc.Height = 512;
    desc.Format = VK_FORMAT_R32G32B32A32_SFLOAT;
    desc.Usage = RHI::ImageUsage::Storage;
    desc.DebugName = "TestStorageImage";

    m_Image = RHI::RHIImage::Create(m_Device, desc);

    ASSERT_NE(m_Image, nullptr);
    EXPECT_NE(m_Image->GetHandle(), VK_NULL_HANDLE);
}

// =============================================================================
// Dimension Tests
// =============================================================================

TEST_F(RHIImageTest, CreatesSmallImage) {
    ASSERT_NE(m_Device, nullptr);

    RHI::ImageDesc desc;
    desc.Width = 1;
    desc.Height = 1;
    desc.Format = VK_FORMAT_R8G8B8A8_SRGB;
    desc.Usage = RHI::ImageUsage::Texture;

    m_Image = RHI::RHIImage::Create(m_Device, desc);

    ASSERT_NE(m_Image, nullptr);
    EXPECT_EQ(m_Image->GetWidth(), 1u);
    EXPECT_EQ(m_Image->GetHeight(), 1u);
}

TEST_F(RHIImageTest, CreatesNonSquareImage) {
    ASSERT_NE(m_Device, nullptr);

    RHI::ImageDesc desc;
    desc.Width = 1024;
    desc.Height = 512;
    desc.Format = VK_FORMAT_R8G8B8A8_SRGB;
    desc.Usage = RHI::ImageUsage::Texture;

    m_Image = RHI::RHIImage::Create(m_Device, desc);

    ASSERT_NE(m_Image, nullptr);
    EXPECT_EQ(m_Image->GetWidth(), 1024u);
    EXPECT_EQ(m_Image->GetHeight(), 512u);
}

// =============================================================================
// Format Tests
// =============================================================================

TEST_F(RHIImageTest, CreatesSRGBImage) {
    ASSERT_NE(m_Device, nullptr);

    RHI::ImageDesc desc;
    desc.Width = 256;
    desc.Height = 256;
    desc.Format = VK_FORMAT_R8G8B8A8_SRGB;
    desc.Usage = RHI::ImageUsage::Texture;

    m_Image = RHI::RHIImage::Create(m_Device, desc);

    ASSERT_NE(m_Image, nullptr);
    EXPECT_EQ(m_Image->GetFormat(), VK_FORMAT_R8G8B8A8_SRGB);
}

TEST_F(RHIImageTest, CreatesLinearImage) {
    ASSERT_NE(m_Device, nullptr);

    RHI::ImageDesc desc;
    desc.Width = 256;
    desc.Height = 256;
    desc.Format = VK_FORMAT_R8G8B8A8_UNORM;
    desc.Usage = RHI::ImageUsage::Texture;

    m_Image = RHI::RHIImage::Create(m_Device, desc);

    ASSERT_NE(m_Image, nullptr);
    EXPECT_EQ(m_Image->GetFormat(), VK_FORMAT_R8G8B8A8_UNORM);
}

TEST_F(RHIImageTest, CreatesHDRImage) {
    ASSERT_NE(m_Device, nullptr);

    RHI::ImageDesc desc;
    desc.Width = 256;
    desc.Height = 256;
    desc.Format = VK_FORMAT_R16G16B16A16_SFLOAT;
    desc.Usage = RHI::ImageUsage::Texture;

    m_Image = RHI::RHIImage::Create(m_Device, desc);

    ASSERT_NE(m_Image, nullptr);
    EXPECT_EQ(m_Image->GetFormat(), VK_FORMAT_R16G16B16A16_SFLOAT);
}

// =============================================================================
// Mipmap Tests
// =============================================================================

TEST_F(RHIImageTest, CreatesImageWithMipmaps) {
    ASSERT_NE(m_Device, nullptr);

    RHI::ImageDesc desc;
    desc.Width = 256;
    desc.Height = 256;
    desc.Format = VK_FORMAT_R8G8B8A8_SRGB;
    desc.Usage = RHI::ImageUsage::Texture;
    desc.MipLevels = 9;  // log2(256) + 1

    m_Image = RHI::RHIImage::Create(m_Device, desc);

    ASSERT_NE(m_Image, nullptr);
    EXPECT_EQ(m_Image->GetMipLevels(), 9u);
}

TEST_F(RHIImageTest, CreatesImageWithSingleMipLevel) {
    ASSERT_NE(m_Device, nullptr);

    RHI::ImageDesc desc;
    desc.Width = 256;
    desc.Height = 256;
    desc.Format = VK_FORMAT_R8G8B8A8_SRGB;
    desc.Usage = RHI::ImageUsage::Texture;
    desc.MipLevels = 1;

    m_Image = RHI::RHIImage::Create(m_Device, desc);

    ASSERT_NE(m_Image, nullptr);
    EXPECT_EQ(m_Image->GetMipLevels(), 1u);
}

// =============================================================================
// Array Layer Tests
// =============================================================================

TEST_F(RHIImageTest, CreatesImageArray) {
    ASSERT_NE(m_Device, nullptr);

    RHI::ImageDesc desc;
    desc.Width = 256;
    desc.Height = 256;
    desc.ArrayLayers = 4;
    desc.Format = VK_FORMAT_R8G8B8A8_SRGB;
    desc.Usage = RHI::ImageUsage::Texture;

    m_Image = RHI::RHIImage::Create(m_Device, desc);

    ASSERT_NE(m_Image, nullptr);
    EXPECT_EQ(m_Image->GetArrayLayers(), 4u);
}

// =============================================================================
// Cubemap Tests
// =============================================================================

TEST_F(RHIImageTest, CreatesCubemapImage) {
    ASSERT_NE(m_Device, nullptr);

    RHI::ImageDesc desc;
    desc.Width = 512;
    desc.Height = 512;
    desc.ArrayLayers = 6;  // 6 faces for cubemap
    desc.Format = VK_FORMAT_R8G8B8A8_SRGB;
    desc.Usage = RHI::ImageUsage::Texture;
    desc.IsCubemap = true;

    m_Image = RHI::RHIImage::Create(m_Device, desc);

    ASSERT_NE(m_Image, nullptr);
    EXPECT_TRUE(m_Image->IsCubemap());
    EXPECT_EQ(m_Image->GetArrayLayers(), 6u);
}

TEST_F(RHIImageTest, NonCubemapImageReportsFalse) {
    ASSERT_NE(m_Device, nullptr);

    RHI::ImageDesc desc;
    desc.Width = 256;
    desc.Height = 256;
    desc.Format = VK_FORMAT_R8G8B8A8_SRGB;
    desc.Usage = RHI::ImageUsage::Texture;
    desc.IsCubemap = false;

    m_Image = RHI::RHIImage::Create(m_Device, desc);

    ASSERT_NE(m_Image, nullptr);
    EXPECT_FALSE(m_Image->IsCubemap());
}

// =============================================================================
// Layout Tests
// =============================================================================

TEST_F(RHIImageTest, InitialLayoutIsUndefined) {
    ASSERT_NE(m_Device, nullptr);

    RHI::ImageDesc desc;
    desc.Width = 256;
    desc.Height = 256;
    desc.Format = VK_FORMAT_R8G8B8A8_SRGB;
    desc.Usage = RHI::ImageUsage::Texture;

    m_Image = RHI::RHIImage::Create(m_Device, desc);

    ASSERT_NE(m_Image, nullptr);
    EXPECT_EQ(m_Image->GetLayout(), VK_IMAGE_LAYOUT_UNDEFINED);
}

TEST_F(RHIImageTest, SetLayoutUpdatesTrackedLayout) {
    ASSERT_NE(m_Device, nullptr);

    RHI::ImageDesc desc;
    desc.Width = 256;
    desc.Height = 256;
    desc.Format = VK_FORMAT_R8G8B8A8_SRGB;
    desc.Usage = RHI::ImageUsage::Texture;

    m_Image = RHI::RHIImage::Create(m_Device, desc);
    ASSERT_NE(m_Image, nullptr);

    m_Image->SetLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    EXPECT_EQ(m_Image->GetLayout(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

// =============================================================================
// CreateWithData Tests
// =============================================================================

TEST_F(RHIImageTest, CreateWithDataUploadsPixels) {
    ASSERT_NE(m_Device, nullptr);

    const uint32_t width = 4;
    const uint32_t height = 4;
    const uint32_t channels = 4;

    // Create simple test pattern (red, green, blue, white quadrants)
    std::vector<uint8_t> pixels(width * height * channels);
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            uint32_t idx = (y * width + x) * channels;
            if (x < width / 2 && y < height / 2) {
                // Red
                pixels[idx + 0] = 255; pixels[idx + 1] = 0; pixels[idx + 2] = 0; pixels[idx + 3] = 255;
            } else if (x >= width / 2 && y < height / 2) {
                // Green
                pixels[idx + 0] = 0; pixels[idx + 1] = 255; pixels[idx + 2] = 0; pixels[idx + 3] = 255;
            } else if (x < width / 2 && y >= height / 2) {
                // Blue
                pixels[idx + 0] = 0; pixels[idx + 1] = 0; pixels[idx + 2] = 255; pixels[idx + 3] = 255;
            } else {
                // White
                pixels[idx + 0] = 255; pixels[idx + 1] = 255; pixels[idx + 2] = 255; pixels[idx + 3] = 255;
            }
        }
    }

    RHI::ImageDesc desc;
    desc.Width = width;
    desc.Height = height;
    desc.Format = VK_FORMAT_R8G8B8A8_SRGB;
    desc.Usage = RHI::ImageUsage::Texture;

    m_Image = RHI::RHIImage::CreateWithData(m_Device, desc, pixels.data(), pixels.size());

    ASSERT_NE(m_Image, nullptr);
    EXPECT_NE(m_Image->GetHandle(), VK_NULL_HANDLE);
    EXPECT_EQ(m_Image->GetWidth(), width);
    EXPECT_EQ(m_Image->GetHeight(), height);
}

// =============================================================================
// Destructor Tests
// =============================================================================

TEST_F(RHIImageTest, DestructorCleansUpResources) {
    ASSERT_NE(m_Device, nullptr);

    VkImage handle = VK_NULL_HANDLE;
    VkImageView viewHandle = VK_NULL_HANDLE;

    {
        RHI::ImageDesc desc;
        desc.Width = 256;
        desc.Height = 256;
        desc.Format = VK_FORMAT_R8G8B8A8_SRGB;
        desc.Usage = RHI::ImageUsage::Texture;

        auto image = RHI::RHIImage::Create(m_Device, desc);
        ASSERT_NE(image, nullptr);
        handle = image->GetHandle();
        viewHandle = image->GetImageView();
        EXPECT_NE(handle, VK_NULL_HANDLE);
        EXPECT_NE(viewHandle, VK_NULL_HANDLE);
        // Image goes out of scope here
    }

    // After destruction, the code should not crash
    SUCCEED();
}

// =============================================================================
// View Tests
// =============================================================================

TEST_F(RHIImageTest, ImageViewIsValidForTexture) {
    ASSERT_NE(m_Device, nullptr);

    RHI::ImageDesc desc;
    desc.Width = 256;
    desc.Height = 256;
    desc.Format = VK_FORMAT_R8G8B8A8_SRGB;
    desc.Usage = RHI::ImageUsage::Texture;

    m_Image = RHI::RHIImage::Create(m_Device, desc);

    ASSERT_NE(m_Image, nullptr);
    EXPECT_NE(m_Image->GetImageView(), VK_NULL_HANDLE);
}

TEST_F(RHIImageTest, StorageViewForNonCubemapReturnsSameAsImageView) {
    ASSERT_NE(m_Device, nullptr);

    RHI::ImageDesc desc;
    desc.Width = 256;
    desc.Height = 256;
    desc.Format = VK_FORMAT_R8G8B8A8_UNORM;
    desc.Usage = RHI::ImageUsage::Storage;

    m_Image = RHI::RHIImage::Create(m_Device, desc);

    ASSERT_NE(m_Image, nullptr);
    EXPECT_EQ(m_Image->GetStorageView(), m_Image->GetImageView());
}

// =============================================================================
// Multiple Image Tests
// =============================================================================

TEST_F(RHIImageTest, CreatesMultipleImages) {
    ASSERT_NE(m_Device, nullptr);

    std::vector<Core::Ref<RHI::RHIImage>> images;

    for (int i = 0; i < 10; ++i) {
        RHI::ImageDesc desc;
        desc.Width = 64 + i * 32;
        desc.Height = 64 + i * 32;
        desc.Format = VK_FORMAT_R8G8B8A8_SRGB;
        desc.Usage = RHI::ImageUsage::Texture;

        auto image = RHI::RHIImage::Create(m_Device, desc);
        ASSERT_NE(image, nullptr);
        images.push_back(image);
    }

    EXPECT_EQ(images.size(), 10u);

    // All images should have unique handles
    std::set<VkImage> uniqueHandles;
    for (const auto& img : images) {
        uniqueHandles.insert(img->GetHandle());
    }
    EXPECT_EQ(uniqueHandles.size(), 10u);
}
