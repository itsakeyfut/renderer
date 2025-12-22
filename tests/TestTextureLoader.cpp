/**
 * @file TestTextureLoader.cpp
 * @brief Test file for Resources/TextureLoader.h using GoogleTest.
 *
 * This file tests that the TextureLoader class correctly creates
 * textures, especially default/fallback textures.
 *
 * Note: These tests require a Vulkan-capable system to pass.
 */

#include <gtest/gtest.h>
#include "Resources/TextureLoader.h"
#include "RHI/RHIDevice.h"
#include "RHI/RHIPhysicalDevice.h"
#include "RHI/RHIInstance.h"
#include "Platform/Window.h"
#include "Core/Log.h"

// =============================================================================
// Test Fixture for TextureLoader Tests
// =============================================================================

class TextureLoaderTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!Core::Log::IsInitialized()) {
            Core::Log::Init();
        }

        // Create a window to initialize GLFW and get a surface
        Platform::WindowConfig windowConfig;
        windowConfig.Width = 100;
        windowConfig.Height = 100;
        windowConfig.Title = "TextureLoader Test Window";
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

    Core::Scope<Platform::Window> m_Window;
    Core::Ref<RHI::RHIInstance> m_Instance;
    Core::Ref<RHI::RHIPhysicalDevice> m_PhysicalDevice;
    Core::Ref<RHI::RHIDevice> m_Device;
    Core::Ref<Resources::Texture> m_Texture;
    VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
};

// =============================================================================
// Format Support Tests
// =============================================================================

TEST_F(TextureLoaderTest, SupportsPngFormat) {
    EXPECT_TRUE(Resources::TextureLoader::IsFormatSupported(".png"));
    EXPECT_TRUE(Resources::TextureLoader::IsFormatSupported(".PNG"));
}

TEST_F(TextureLoaderTest, SupportsJpgFormat) {
    EXPECT_TRUE(Resources::TextureLoader::IsFormatSupported(".jpg"));
    EXPECT_TRUE(Resources::TextureLoader::IsFormatSupported(".jpeg"));
    EXPECT_TRUE(Resources::TextureLoader::IsFormatSupported(".JPG"));
    EXPECT_TRUE(Resources::TextureLoader::IsFormatSupported(".JPEG"));
}

TEST_F(TextureLoaderTest, SupportsBmpFormat) {
    EXPECT_TRUE(Resources::TextureLoader::IsFormatSupported(".bmp"));
    EXPECT_TRUE(Resources::TextureLoader::IsFormatSupported(".BMP"));
}

TEST_F(TextureLoaderTest, SupportsTgaFormat) {
    EXPECT_TRUE(Resources::TextureLoader::IsFormatSupported(".tga"));
    EXPECT_TRUE(Resources::TextureLoader::IsFormatSupported(".TGA"));
}

TEST_F(TextureLoaderTest, SupportsHdrFormat) {
    EXPECT_TRUE(Resources::TextureLoader::IsFormatSupported(".hdr"));
    EXPECT_TRUE(Resources::TextureLoader::IsFormatSupported(".HDR"));
}

TEST_F(TextureLoaderTest, UnsupportedFormatReturnsFalse) {
    EXPECT_FALSE(Resources::TextureLoader::IsFormatSupported(".webp"));
    EXPECT_FALSE(Resources::TextureLoader::IsFormatSupported(".txt"));
    EXPECT_FALSE(Resources::TextureLoader::IsFormatSupported(".doc"));
    EXPECT_FALSE(Resources::TextureLoader::IsFormatSupported(".exe"));
}

TEST_F(TextureLoaderTest, GetSupportedExtensionsReturnsNonEmpty) {
    auto extensions = Resources::TextureLoader::GetSupportedExtensions();

    EXPECT_FALSE(extensions.empty());
    EXPECT_GE(extensions.size(), 5u);  // At least png, jpg, bmp, tga, hdr
}

// =============================================================================
// Solid Color Texture Tests
// =============================================================================

TEST_F(TextureLoaderTest, CreateSolidColorRed) {
    ASSERT_NE(m_Device, nullptr);

    m_Texture = Resources::TextureLoader::CreateSolidColor(m_Device, 255, 0, 0, 255);

    ASSERT_NE(m_Texture, nullptr);
    EXPECT_NE(m_Texture->Image, nullptr);
    EXPECT_EQ(m_Texture->Width, 1u);
    EXPECT_EQ(m_Texture->Height, 1u);
    EXPECT_EQ(m_Texture->Channels, 4u);
}

TEST_F(TextureLoaderTest, CreateSolidColorGreen) {
    ASSERT_NE(m_Device, nullptr);

    m_Texture = Resources::TextureLoader::CreateSolidColor(m_Device, 0, 255, 0, 255);

    ASSERT_NE(m_Texture, nullptr);
    EXPECT_NE(m_Texture->Image, nullptr);
}

TEST_F(TextureLoaderTest, CreateSolidColorBlue) {
    ASSERT_NE(m_Device, nullptr);

    m_Texture = Resources::TextureLoader::CreateSolidColor(m_Device, 0, 0, 255, 255);

    ASSERT_NE(m_Texture, nullptr);
    EXPECT_NE(m_Texture->Image, nullptr);
}

TEST_F(TextureLoaderTest, CreateSolidColorTransparent) {
    ASSERT_NE(m_Device, nullptr);

    m_Texture = Resources::TextureLoader::CreateSolidColor(m_Device, 255, 255, 255, 0);

    ASSERT_NE(m_Texture, nullptr);
    EXPECT_NE(m_Texture->Image, nullptr);
}

TEST_F(TextureLoaderTest, CreateSolidColorDefaultAlpha) {
    ASSERT_NE(m_Device, nullptr);

    // Default alpha should be 255
    m_Texture = Resources::TextureLoader::CreateSolidColor(m_Device, 128, 128, 128);

    ASSERT_NE(m_Texture, nullptr);
    EXPECT_NE(m_Texture->Image, nullptr);
}

// =============================================================================
// Default Texture Tests
// =============================================================================

TEST_F(TextureLoaderTest, CreateWhiteTexture) {
    ASSERT_NE(m_Device, nullptr);

    m_Texture = Resources::TextureLoader::CreateWhite(m_Device);

    ASSERT_NE(m_Texture, nullptr);
    EXPECT_NE(m_Texture->Image, nullptr);
    EXPECT_EQ(m_Texture->Width, 1u);
    EXPECT_EQ(m_Texture->Height, 1u);
}

TEST_F(TextureLoaderTest, CreateBlackTexture) {
    ASSERT_NE(m_Device, nullptr);

    m_Texture = Resources::TextureLoader::CreateBlack(m_Device);

    ASSERT_NE(m_Texture, nullptr);
    EXPECT_NE(m_Texture->Image, nullptr);
    EXPECT_EQ(m_Texture->Width, 1u);
    EXPECT_EQ(m_Texture->Height, 1u);
}

TEST_F(TextureLoaderTest, CreateDefaultNormalTexture) {
    ASSERT_NE(m_Device, nullptr);

    m_Texture = Resources::TextureLoader::CreateDefaultNormal(m_Device);

    ASSERT_NE(m_Texture, nullptr);
    EXPECT_NE(m_Texture->Image, nullptr);
    EXPECT_EQ(m_Texture->Width, 1u);
    EXPECT_EQ(m_Texture->Height, 1u);
}

// =============================================================================
// Placeholder Texture Tests
// =============================================================================

TEST_F(TextureLoaderTest, CreatePlaceholderDefaultSize) {
    ASSERT_NE(m_Device, nullptr);

    m_Texture = Resources::TextureLoader::CreatePlaceholder(m_Device);

    ASSERT_NE(m_Texture, nullptr);
    EXPECT_NE(m_Texture->Image, nullptr);
    EXPECT_EQ(m_Texture->Width, 64u);
    EXPECT_EQ(m_Texture->Height, 64u);
}

TEST_F(TextureLoaderTest, CreatePlaceholderCustomSize) {
    ASSERT_NE(m_Device, nullptr);

    m_Texture = Resources::TextureLoader::CreatePlaceholder(m_Device, 128);

    ASSERT_NE(m_Texture, nullptr);
    EXPECT_NE(m_Texture->Image, nullptr);
    EXPECT_EQ(m_Texture->Width, 128u);
    EXPECT_EQ(m_Texture->Height, 128u);
}

TEST_F(TextureLoaderTest, CreatePlaceholderCustomColors) {
    ASSERT_NE(m_Device, nullptr);

    // Custom colors: red and blue checkerboard
    uint32_t color1 = 0xFF0000FF;  // Red
    uint32_t color2 = 0x0000FFFF;  // Blue

    m_Texture = Resources::TextureLoader::CreatePlaceholder(m_Device, 32, color1, color2);

    ASSERT_NE(m_Texture, nullptr);
    EXPECT_NE(m_Texture->Image, nullptr);
    EXPECT_EQ(m_Texture->Width, 32u);
    EXPECT_EQ(m_Texture->Height, 32u);
}

TEST_F(TextureLoaderTest, CreatePlaceholderSmallSize) {
    ASSERT_NE(m_Device, nullptr);

    m_Texture = Resources::TextureLoader::CreatePlaceholder(m_Device, 2);

    ASSERT_NE(m_Texture, nullptr);
    EXPECT_NE(m_Texture->Image, nullptr);
    EXPECT_EQ(m_Texture->Width, 2u);
    EXPECT_EQ(m_Texture->Height, 2u);
}

// =============================================================================
// TextureData Tests (CPU-only loading)
// =============================================================================

TEST_F(TextureLoaderTest, LoadCPUNonexistentFileReturnsInvalid) {
    Resources::TextureData data = Resources::TextureLoader::LoadCPU("nonexistent_file.png");

    EXPECT_FALSE(data.IsValid());
    EXPECT_TRUE(data.Pixels.empty());
    EXPECT_EQ(data.Width, 0u);
    EXPECT_EQ(data.Height, 0u);
}

TEST_F(TextureLoaderTest, TextureDataIsValidCheck) {
    // Test the IsValid() helper method
    Resources::TextureData validData;
    validData.Pixels.resize(16);  // 2x2 RGBA
    validData.Width = 2;
    validData.Height = 2;

    EXPECT_TRUE(validData.IsValid());

    Resources::TextureData invalidData;
    EXPECT_FALSE(invalidData.IsValid());

    Resources::TextureData emptyPixels;
    emptyPixels.Width = 2;
    emptyPixels.Height = 2;
    EXPECT_FALSE(emptyPixels.IsValid());

    Resources::TextureData zeroWidth;
    zeroWidth.Pixels.resize(16);
    zeroWidth.Width = 0;
    zeroWidth.Height = 2;
    EXPECT_FALSE(zeroWidth.IsValid());
}

// =============================================================================
// Load Nonexistent File Tests
// =============================================================================

TEST_F(TextureLoaderTest, LoadNonexistentFileReturnsNull) {
    ASSERT_NE(m_Device, nullptr);

    m_Texture = Resources::TextureLoader::Load(m_Device, "this_file_does_not_exist.png");

    EXPECT_EQ(m_Texture, nullptr);
}

// =============================================================================
// CreateFromData Tests
// =============================================================================

TEST_F(TextureLoaderTest, CreateFromDataWithInvalidDataReturnsNull) {
    ASSERT_NE(m_Device, nullptr);

    Resources::TextureData invalidData;
    m_Texture = Resources::TextureLoader::CreateFromData(m_Device, invalidData);

    EXPECT_EQ(m_Texture, nullptr);
}

// =============================================================================
// Multiple Texture Creation Tests
// =============================================================================

TEST_F(TextureLoaderTest, CreateMultipleDefaultTextures) {
    ASSERT_NE(m_Device, nullptr);

    auto white = Resources::TextureLoader::CreateWhite(m_Device);
    auto black = Resources::TextureLoader::CreateBlack(m_Device);
    auto normal = Resources::TextureLoader::CreateDefaultNormal(m_Device);
    auto placeholder = Resources::TextureLoader::CreatePlaceholder(m_Device);

    ASSERT_NE(white, nullptr);
    ASSERT_NE(black, nullptr);
    ASSERT_NE(normal, nullptr);
    ASSERT_NE(placeholder, nullptr);

    // Each should have a unique image handle
    EXPECT_NE(white->Image->GetHandle(), black->Image->GetHandle());
    EXPECT_NE(black->Image->GetHandle(), normal->Image->GetHandle());
    EXPECT_NE(normal->Image->GetHandle(), placeholder->Image->GetHandle());
}

// =============================================================================
// Texture Properties Tests
// =============================================================================

TEST_F(TextureLoaderTest, TextureHasValidImageProperties) {
    ASSERT_NE(m_Device, nullptr);

    m_Texture = Resources::TextureLoader::CreateWhite(m_Device);
    ASSERT_NE(m_Texture, nullptr);

    EXPECT_NE(m_Texture->Image->GetHandle(), VK_NULL_HANDLE);
    EXPECT_NE(m_Texture->Image->GetImageView(), VK_NULL_HANDLE);
}

TEST_F(TextureLoaderTest, PlaceholderTextureHasCorrectChannels) {
    ASSERT_NE(m_Device, nullptr);

    m_Texture = Resources::TextureLoader::CreatePlaceholder(m_Device);
    ASSERT_NE(m_Texture, nullptr);

    EXPECT_EQ(m_Texture->Channels, 4u);  // RGBA
}

// =============================================================================
// TextureLoadOptions Tests
// =============================================================================

TEST_F(TextureLoaderTest, TextureLoadOptionsDefaults) {
    Resources::TextureLoadOptions options;

    EXPECT_TRUE(options.GenerateMipmaps);
    EXPECT_TRUE(options.SRGB);
    EXPECT_FALSE(options.FlipY);
}

TEST_F(TextureLoaderTest, TextureLoadOptionsCustom) {
    Resources::TextureLoadOptions options;
    options.GenerateMipmaps = false;
    options.SRGB = false;
    options.FlipY = true;

    EXPECT_FALSE(options.GenerateMipmaps);
    EXPECT_FALSE(options.SRGB);
    EXPECT_TRUE(options.FlipY);
}
