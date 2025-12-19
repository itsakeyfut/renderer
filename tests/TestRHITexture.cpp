/**
 * @file TestRHITexture.cpp
 * @brief Test file for RHI/RHITexture.h using GoogleTest.
 *
 * This file tests that the RHITexture class correctly creates and loads
 * textures from files and handles various texture operations.
 *
 * Note: These tests require a Vulkan-capable system to pass.
 */

#include <gtest/gtest.h>
#include "RHI/RHITexture.h"
#include "RHI/RHIDevice.h"
#include "RHI/RHIPhysicalDevice.h"
#include "RHI/RHIInstance.h"
#include "Platform/Window.h"
#include "Core/Assert.h"
#include "Core/Log.h"

#include <vector>
#include <cstring>
#include <filesystem>

// =============================================================================
// Test Fixture for RHITexture Tests
// =============================================================================

class RHITextureTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!Core::Log::IsInitialized()) {
            Core::Log::Init();
        }

        // Create a window to initialize GLFW and get a surface
        Platform::WindowConfig windowConfig;
        windowConfig.Width = 100;
        windowConfig.Height = 100;
        windowConfig.Title = "RHI Texture Test Window";
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
        m_Texture.reset();
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
    Core::Ref<RHI::RHITexture> m_Texture;
    VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
};

// =============================================================================
// Texture Creation Tests
// =============================================================================

TEST_F(RHITextureTest, CreateTextureWithDesc) {
    ASSERT_NE(m_Device, nullptr);

    RHI::TextureDesc desc;
    desc.Width = 64;
    desc.Height = 64;
    desc.Format = VK_FORMAT_R8G8B8A8_SRGB;
    desc.MipLevels = 1;
    desc.DebugName = "TestTexture";

    m_Texture = RHI::RHITexture::Create(m_Device, desc);

    ASSERT_NE(m_Texture, nullptr);
    EXPECT_NE(m_Texture->GetImage(), VK_NULL_HANDLE);
    EXPECT_NE(m_Texture->GetImageView(), VK_NULL_HANDLE);
    EXPECT_EQ(m_Texture->GetWidth(), 64u);
    EXPECT_EQ(m_Texture->GetHeight(), 64u);
    EXPECT_EQ(m_Texture->GetFormat(), VK_FORMAT_R8G8B8A8_SRGB);
    EXPECT_EQ(m_Texture->GetMipLevels(), 1u);
}

TEST_F(RHITextureTest, CreateTextureWithLinearFormat) {
    ASSERT_NE(m_Device, nullptr);

    RHI::TextureDesc desc;
    desc.Width = 32;
    desc.Height = 32;
    desc.Format = VK_FORMAT_R8G8B8A8_UNORM;  // Linear format for data textures
    desc.MipLevels = 1;

    m_Texture = RHI::RHITexture::Create(m_Device, desc);

    ASSERT_NE(m_Texture, nullptr);
    EXPECT_EQ(m_Texture->GetFormat(), VK_FORMAT_R8G8B8A8_UNORM);
}

TEST_F(RHITextureTest, CreateTextureWithMipmaps) {
    ASSERT_NE(m_Device, nullptr);

    RHI::TextureDesc desc;
    desc.Width = 256;
    desc.Height = 256;
    desc.Format = VK_FORMAT_R8G8B8A8_SRGB;
    desc.MipLevels = 0;  // Auto-calculate
    desc.GenerateMips = true;

    m_Texture = RHI::RHITexture::Create(m_Device, desc);

    ASSERT_NE(m_Texture, nullptr);
    // For 256x256, we expect log2(256) + 1 = 9 mip levels
    EXPECT_GE(m_Texture->GetMipLevels(), 1u);
}

TEST_F(RHITextureTest, CreateTextureNonPowerOfTwo) {
    ASSERT_NE(m_Device, nullptr);

    RHI::TextureDesc desc;
    desc.Width = 100;
    desc.Height = 75;
    desc.Format = VK_FORMAT_R8G8B8A8_SRGB;
    desc.MipLevels = 1;

    m_Texture = RHI::RHITexture::Create(m_Device, desc);

    ASSERT_NE(m_Texture, nullptr);
    EXPECT_EQ(m_Texture->GetWidth(), 100u);
    EXPECT_EQ(m_Texture->GetHeight(), 75u);
}

// =============================================================================
// Upload Data Tests
// =============================================================================

TEST_F(RHITextureTest, UploadPixelData) {
    ASSERT_NE(m_Device, nullptr);

    RHI::TextureDesc desc;
    desc.Width = 4;
    desc.Height = 4;
    desc.Format = VK_FORMAT_R8G8B8A8_SRGB;
    desc.MipLevels = 1;

    m_Texture = RHI::RHITexture::Create(m_Device, desc);
    ASSERT_NE(m_Texture, nullptr);

    // Create simple 4x4 RGBA pixel data
    std::vector<uint8_t> pixels(4 * 4 * 4);
    for (size_t i = 0; i < pixels.size(); i += 4) {
        pixels[i + 0] = 255;  // R
        pixels[i + 1] = 128;  // G
        pixels[i + 2] = 64;   // B
        pixels[i + 3] = 255;  // A
    }

    bool result = m_Texture->UploadData(m_Device, pixels.data(), pixels.size());
    EXPECT_TRUE(result);

    // After upload, layout should be SHADER_READ_ONLY_OPTIMAL
    EXPECT_EQ(m_Texture->GetLayout(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

// =============================================================================
// Load From File Tests
// =============================================================================

TEST_F(RHITextureTest, LoadFromFileNonExistent) {
    ASSERT_NE(m_Device, nullptr);

    m_Texture = RHI::RHITexture::LoadFromFile(
        m_Device,
        "nonexistent_texture.png",
        true,
        false);

    EXPECT_EQ(m_Texture, nullptr);
}

TEST_F(RHITextureTest, LoadFromFileExists) {
    ASSERT_NE(m_Device, nullptr);

    // Use an existing test texture
    const std::string testTexturePath = "assets/textures/Bricks102_1K-JPG/Bricks102.png";

    if (!std::filesystem::exists(testTexturePath)) {
        GTEST_SKIP() << "Test texture not found: " << testTexturePath;
    }

    m_Texture = RHI::RHITexture::LoadFromFile(
        m_Device,
        testTexturePath,
        true,   // sRGB
        false); // no mipmaps

    ASSERT_NE(m_Texture, nullptr);
    EXPECT_NE(m_Texture->GetImage(), VK_NULL_HANDLE);
    EXPECT_NE(m_Texture->GetImageView(), VK_NULL_HANDLE);
    EXPECT_GT(m_Texture->GetWidth(), 0u);
    EXPECT_GT(m_Texture->GetHeight(), 0u);
    EXPECT_EQ(m_Texture->GetFormat(), VK_FORMAT_R8G8B8A8_SRGB);
    EXPECT_EQ(m_Texture->GetFilePath(), testTexturePath);
}

TEST_F(RHITextureTest, LoadFromFileWithMipmaps) {
    ASSERT_NE(m_Device, nullptr);

    const std::string testTexturePath = "assets/textures/Bricks102_1K-JPG/Bricks102.png";

    if (!std::filesystem::exists(testTexturePath)) {
        GTEST_SKIP() << "Test texture not found: " << testTexturePath;
    }

    m_Texture = RHI::RHITexture::LoadFromFile(
        m_Device,
        testTexturePath,
        true,  // sRGB
        true); // with mipmaps

    ASSERT_NE(m_Texture, nullptr);
    EXPECT_GT(m_Texture->GetMipLevels(), 1u);
    EXPECT_EQ(m_Texture->GetLayout(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

TEST_F(RHITextureTest, LoadFromFileLinearFormat) {
    ASSERT_NE(m_Device, nullptr);

    const std::string testTexturePath = "assets/textures/Bricks102_1K-JPG/Bricks102.png";

    if (!std::filesystem::exists(testTexturePath)) {
        GTEST_SKIP() << "Test texture not found: " << testTexturePath;
    }

    // Load as linear (non-sRGB) format for data textures
    m_Texture = RHI::RHITexture::LoadFromFile(
        m_Device,
        testTexturePath,
        false,  // linear format
        false); // no mipmaps

    ASSERT_NE(m_Texture, nullptr);
    EXPECT_EQ(m_Texture->GetFormat(), VK_FORMAT_R8G8B8A8_UNORM);
}

// =============================================================================
// Accessor Tests
// =============================================================================

TEST_F(RHITextureTest, GetRHIImage) {
    ASSERT_NE(m_Device, nullptr);

    RHI::TextureDesc desc;
    desc.Width = 16;
    desc.Height = 16;
    desc.Format = VK_FORMAT_R8G8B8A8_SRGB;
    desc.MipLevels = 1;

    m_Texture = RHI::RHITexture::Create(m_Device, desc);
    ASSERT_NE(m_Texture, nullptr);

    const auto& rhiImage = m_Texture->GetRHIImage();
    EXPECT_NE(rhiImage, nullptr);
    EXPECT_EQ(rhiImage->GetHandle(), m_Texture->GetImage());
    EXPECT_EQ(rhiImage->GetImageView(), m_Texture->GetImageView());
}

TEST_F(RHITextureTest, EmptyTextureFilePath) {
    ASSERT_NE(m_Device, nullptr);

    RHI::TextureDesc desc;
    desc.Width = 8;
    desc.Height = 8;
    desc.Format = VK_FORMAT_R8G8B8A8_SRGB;
    desc.MipLevels = 1;

    m_Texture = RHI::RHITexture::Create(m_Device, desc);
    ASSERT_NE(m_Texture, nullptr);

    // Programmatically created texture should have empty file path
    EXPECT_TRUE(m_Texture->GetFilePath().empty());
}

// =============================================================================
// Error Handling Tests
// =============================================================================

TEST_F(RHITextureTest, CreateWithZeroWidthFails) {
    ASSERT_NE(m_Device, nullptr);

    RHI::TextureDesc desc;
    desc.Width = 0;
    desc.Height = 64;
    desc.Format = VK_FORMAT_R8G8B8A8_SRGB;

#if defined(_DEBUG) || defined(DEBUG)
    // In debug builds, ASSERT will trigger
    EXPECT_DEATH(RHI::RHITexture::Create(m_Device, desc), "");
#else
    // In release builds, assertion is removed - skip test
    GTEST_SKIP() << "Assertion not active in release builds";
#endif
}

TEST_F(RHITextureTest, CreateWithZeroHeightFails) {
    ASSERT_NE(m_Device, nullptr);

    RHI::TextureDesc desc;
    desc.Width = 64;
    desc.Height = 0;
    desc.Format = VK_FORMAT_R8G8B8A8_SRGB;

#if defined(_DEBUG) || defined(DEBUG)
    // In debug builds, ASSERT will trigger
    EXPECT_DEATH(RHI::RHITexture::Create(m_Device, desc), "");
#else
    // In release builds, assertion is removed - skip test
    GTEST_SKIP() << "Assertion not active in release builds";
#endif
}

// =============================================================================
// VMA Allocation Tests
// =============================================================================

TEST_F(RHITextureTest, TextureCreationAllocatesMemory) {
    ASSERT_NE(m_Device, nullptr);

    VmaAllocator allocator = m_Device->GetAllocator();
    ASSERT_NE(allocator, VK_NULL_HANDLE);

    // Get initial stats
    VmaTotalStatistics statsBefore;
    vmaCalculateStatistics(allocator, &statsBefore);
    uint32_t initialCount = statsBefore.total.statistics.allocationCount;

    // Create texture
    RHI::TextureDesc desc;
    desc.Width = 64;
    desc.Height = 64;
    desc.Format = VK_FORMAT_R8G8B8A8_SRGB;
    desc.MipLevels = 1;

    m_Texture = RHI::RHITexture::Create(m_Device, desc);
    ASSERT_NE(m_Texture, nullptr);

    // Get stats after texture creation
    VmaTotalStatistics statsAfter;
    vmaCalculateStatistics(allocator, &statsAfter);
    uint32_t finalCount = statsAfter.total.statistics.allocationCount;

    EXPECT_GT(finalCount, initialCount);
}

TEST_F(RHITextureTest, TextureDestructionFreesMemory) {
    ASSERT_NE(m_Device, nullptr);

    VmaAllocator allocator = m_Device->GetAllocator();
    ASSERT_NE(allocator, VK_NULL_HANDLE);

    {
        RHI::TextureDesc desc;
        desc.Width = 64;
        desc.Height = 64;
        desc.Format = VK_FORMAT_R8G8B8A8_SRGB;
        desc.MipLevels = 1;

        auto tempTexture = RHI::RHITexture::Create(m_Device, desc);
        ASSERT_NE(tempTexture, nullptr);

        VmaTotalStatistics statsWithTexture;
        vmaCalculateStatistics(allocator, &statsWithTexture);
        EXPECT_GT(statsWithTexture.total.statistics.allocationCount, 0u);
    }

    // Texture destroyed, allocation count should decrease
    VmaTotalStatistics statsAfter;
    vmaCalculateStatistics(allocator, &statsAfter);
    EXPECT_EQ(statsAfter.total.statistics.allocationCount, 0u);
}

// =============================================================================
// Image Layout Tests
// =============================================================================

TEST_F(RHITextureTest, InitialLayoutIsUndefined) {
    ASSERT_NE(m_Device, nullptr);

    RHI::TextureDesc desc;
    desc.Width = 8;
    desc.Height = 8;
    desc.Format = VK_FORMAT_R8G8B8A8_SRGB;
    desc.MipLevels = 1;

    m_Texture = RHI::RHITexture::Create(m_Device, desc);
    ASSERT_NE(m_Texture, nullptr);

    // Before uploading data, layout should be undefined
    EXPECT_EQ(m_Texture->GetLayout(), VK_IMAGE_LAYOUT_UNDEFINED);
}

TEST_F(RHITextureTest, LayoutAfterUploadIsShaderReadOnly) {
    ASSERT_NE(m_Device, nullptr);

    RHI::TextureDesc desc;
    desc.Width = 4;
    desc.Height = 4;
    desc.Format = VK_FORMAT_R8G8B8A8_SRGB;
    desc.MipLevels = 1;

    m_Texture = RHI::RHITexture::Create(m_Device, desc);
    ASSERT_NE(m_Texture, nullptr);

    std::vector<uint8_t> pixels(4 * 4 * 4, 128);
    bool result = m_Texture->UploadData(m_Device, pixels.data(), pixels.size());
    ASSERT_TRUE(result);

    EXPECT_EQ(m_Texture->GetLayout(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}
