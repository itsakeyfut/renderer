/**
 * @file TestMipmapGenerator.cpp
 * @brief Test file for RHI/MipmapGenerator.h using GoogleTest.
 *
 * Tests the MipmapGenerator utility class for mip level calculation
 * and mipmap generation functionality.
 */

#include <gtest/gtest.h>
#include "RHI/MipmapGenerator.h"
#include "RHI/RHIDevice.h"
#include "RHI/RHIPhysicalDevice.h"
#include "RHI/RHIInstance.h"
#include "Platform/Window.h"
#include "Core/Log.h"

// =============================================================================
// Unit Tests for CalculateMipLevels (No Vulkan required)
// =============================================================================

class MipmapCalculationTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!Core::Log::IsInitialized()) {
            Core::Log::Init();
        }
    }
};

TEST_F(MipmapCalculationTest, CalculateMipLevels_ZeroDimension) {
    // Zero dimensions should return 1 (defensive handling)
    EXPECT_EQ(RHI::MipmapGenerator::CalculateMipLevels(0, 0), 1u);
    EXPECT_EQ(RHI::MipmapGenerator::CalculateMipLevels(0, 100), 1u);
    EXPECT_EQ(RHI::MipmapGenerator::CalculateMipLevels(100, 0), 1u);
}

TEST_F(MipmapCalculationTest, CalculateMipLevels_1x1) {
    // 1x1 texture should have 1 mip level
    uint32_t levels = RHI::MipmapGenerator::CalculateMipLevels(1, 1);
    EXPECT_EQ(levels, 1u);
}

TEST_F(MipmapCalculationTest, CalculateMipLevels_2x2) {
    // 2x2 -> 1x1 = 2 mip levels
    uint32_t levels = RHI::MipmapGenerator::CalculateMipLevels(2, 2);
    EXPECT_EQ(levels, 2u);
}

TEST_F(MipmapCalculationTest, CalculateMipLevels_4x4) {
    // 4x4 -> 2x2 -> 1x1 = 3 mip levels
    uint32_t levels = RHI::MipmapGenerator::CalculateMipLevels(4, 4);
    EXPECT_EQ(levels, 3u);
}

TEST_F(MipmapCalculationTest, CalculateMipLevels_8x8) {
    // 8x8 -> 4x4 -> 2x2 -> 1x1 = 4 mip levels
    uint32_t levels = RHI::MipmapGenerator::CalculateMipLevels(8, 8);
    EXPECT_EQ(levels, 4u);
}

TEST_F(MipmapCalculationTest, CalculateMipLevels_256x256) {
    // 256 = 2^8, so 256x256 -> ... -> 1x1 = 9 mip levels
    uint32_t levels = RHI::MipmapGenerator::CalculateMipLevels(256, 256);
    EXPECT_EQ(levels, 9u);
}

TEST_F(MipmapCalculationTest, CalculateMipLevels_1024x1024) {
    // 1024 = 2^10, so 1024x1024 -> ... -> 1x1 = 11 mip levels
    uint32_t levels = RHI::MipmapGenerator::CalculateMipLevels(1024, 1024);
    EXPECT_EQ(levels, 11u);
}

TEST_F(MipmapCalculationTest, CalculateMipLevels_NonSquare_Wide) {
    // 512x256: max(512, 256) = 512 = 2^9, so 10 mip levels
    uint32_t levels = RHI::MipmapGenerator::CalculateMipLevels(512, 256);
    EXPECT_EQ(levels, 10u);
}

TEST_F(MipmapCalculationTest, CalculateMipLevels_NonSquare_Tall) {
    // 256x512: max(256, 512) = 512 = 2^9, so 10 mip levels
    uint32_t levels = RHI::MipmapGenerator::CalculateMipLevels(256, 512);
    EXPECT_EQ(levels, 10u);
}

TEST_F(MipmapCalculationTest, CalculateMipLevels_NonPowerOfTwo) {
    // 300x200: max(300, 200) = 300, floor(log2(300)) = 8, so 9 mip levels
    uint32_t levels = RHI::MipmapGenerator::CalculateMipLevels(300, 200);
    EXPECT_EQ(levels, 9u);
}

TEST_F(MipmapCalculationTest, CalculateMipLevels_VeryWide) {
    // 1920x1080: max = 1920, floor(log2(1920)) = 10, so 11 mip levels
    uint32_t levels = RHI::MipmapGenerator::CalculateMipLevels(1920, 1080);
    EXPECT_EQ(levels, 11u);
}

TEST_F(MipmapCalculationTest, CalculateMipLevels_4K) {
    // 3840x2160: max = 3840, floor(log2(3840)) = 11, so 12 mip levels
    uint32_t levels = RHI::MipmapGenerator::CalculateMipLevels(3840, 2160);
    EXPECT_EQ(levels, 12u);
}

// =============================================================================
// Test Fixture for Format Support Tests (Requires Vulkan)
// =============================================================================

class MipmapGeneratorTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!Core::Log::IsInitialized()) {
            Core::Log::Init();
        }

        // Create a window to initialize GLFW and get a surface
        Platform::WindowConfig windowConfig;
        windowConfig.Width = 100;
        windowConfig.Height = 100;
        windowConfig.Title = "MipmapGenerator Test Window";
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
// Format Support Tests
// =============================================================================

TEST_F(MipmapGeneratorTest, SupportsLinearFiltering_RGBA8) {
    ASSERT_NE(m_PhysicalDevice, nullptr);

    // RGBA8 is universally supported for linear filtering
    bool supported = RHI::MipmapGenerator::SupportsLinearFiltering(
        m_PhysicalDevice->GetHandle(),
        VK_FORMAT_R8G8B8A8_UNORM);

    EXPECT_TRUE(supported);
}

TEST_F(MipmapGeneratorTest, SupportsLinearFiltering_SRGB) {
    ASSERT_NE(m_PhysicalDevice, nullptr);

    // sRGB formats are typically supported
    bool supported = RHI::MipmapGenerator::SupportsLinearFiltering(
        m_PhysicalDevice->GetHandle(),
        VK_FORMAT_R8G8B8A8_SRGB);

    EXPECT_TRUE(supported);
}

TEST_F(MipmapGeneratorTest, SupportsLinearFiltering_BC1) {
    ASSERT_NE(m_PhysicalDevice, nullptr);

    // BC1 (DXT1) compressed format - typically supported
    bool supported = RHI::MipmapGenerator::SupportsLinearFiltering(
        m_PhysicalDevice->GetHandle(),
        VK_FORMAT_BC1_RGB_UNORM_BLOCK);

    // Note: This may be true or false depending on hardware
    // We just verify the function doesn't crash
    (void)supported;
}

TEST_F(MipmapGeneratorTest, SupportsLinearFiltering_DepthFormat) {
    ASSERT_NE(m_PhysicalDevice, nullptr);

    // Depth formats may or may not support linear filtering
    bool supported = RHI::MipmapGenerator::SupportsLinearFiltering(
        m_PhysicalDevice->GetHandle(),
        VK_FORMAT_D32_SFLOAT);

    // Just verify function doesn't crash
    (void)supported;
}
