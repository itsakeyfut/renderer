/**
 * @file TestRHISampler.cpp
 * @brief Test file for RHI/RHISampler.h using GoogleTest.
 *
 * This file tests that the RHISampler class correctly creates samplers
 * with various filtering, addressing, and anisotropy settings.
 *
 * Note: These tests require a Vulkan-capable system to pass.
 */

#include <gtest/gtest.h>
#include "RHI/RHISampler.h"
#include "RHI/RHIDevice.h"
#include "RHI/RHIPhysicalDevice.h"
#include "RHI/RHIInstance.h"
#include "Platform/Window.h"
#include "Core/Assert.h"
#include "Core/Log.h"

// =============================================================================
// Test Fixture for RHISampler Tests
// =============================================================================

class RHISamplerTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!Core::Log::IsInitialized()) {
            Core::Log::Init();
        }

        // Create a window to initialize GLFW and get a surface
        Platform::WindowConfig windowConfig;
        windowConfig.Width = 100;
        windowConfig.Height = 100;
        windowConfig.Title = "RHI Sampler Test Window";
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
        m_Sampler.reset();
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
    Core::Ref<RHI::RHISampler> m_Sampler;
    VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
};

// =============================================================================
// Sampler Creation Tests
// =============================================================================

TEST_F(RHISamplerTest, CreateWithDefaultDesc) {
    ASSERT_NE(m_Device, nullptr);

    RHI::SamplerDesc desc;
    desc.DebugName = "TestDefaultSampler";

    m_Sampler = RHI::RHISampler::Create(m_Device, desc);

    ASSERT_NE(m_Sampler, nullptr);
    EXPECT_NE(m_Sampler->GetHandle(), VK_NULL_HANDLE);
}

TEST_F(RHISamplerTest, CreateLinearSampler) {
    ASSERT_NE(m_Device, nullptr);

    m_Sampler = RHI::RHISampler::CreateLinear(m_Device);

    ASSERT_NE(m_Sampler, nullptr);
    EXPECT_NE(m_Sampler->GetHandle(), VK_NULL_HANDLE);
}

TEST_F(RHISamplerTest, CreateLinearClampSampler) {
    ASSERT_NE(m_Device, nullptr);

    m_Sampler = RHI::RHISampler::CreateLinearClamp(m_Device);

    ASSERT_NE(m_Sampler, nullptr);
    EXPECT_NE(m_Sampler->GetHandle(), VK_NULL_HANDLE);
}

TEST_F(RHISamplerTest, CreateNearestSampler) {
    ASSERT_NE(m_Device, nullptr);

    m_Sampler = RHI::RHISampler::CreateNearest(m_Device);

    ASSERT_NE(m_Sampler, nullptr);
    EXPECT_NE(m_Sampler->GetHandle(), VK_NULL_HANDLE);
}

TEST_F(RHISamplerTest, CreateShadowSampler) {
    ASSERT_NE(m_Device, nullptr);

    m_Sampler = RHI::RHISampler::CreateShadow(m_Device);

    ASSERT_NE(m_Sampler, nullptr);
    EXPECT_NE(m_Sampler->GetHandle(), VK_NULL_HANDLE);
}

// =============================================================================
// Filter Mode Tests
// =============================================================================

TEST_F(RHISamplerTest, CreateWithNearestFilter) {
    ASSERT_NE(m_Device, nullptr);

    RHI::SamplerDesc desc;
    desc.MagFilter = RHI::FilterMode::Nearest;
    desc.MinFilter = RHI::FilterMode::Nearest;
    desc.MipmapMode = RHI::FilterMode::Nearest;
    desc.AnisotropyEnable = false;
    desc.DebugName = "NearestFilterSampler";

    m_Sampler = RHI::RHISampler::Create(m_Device, desc);

    ASSERT_NE(m_Sampler, nullptr);
    EXPECT_NE(m_Sampler->GetHandle(), VK_NULL_HANDLE);
}

TEST_F(RHISamplerTest, CreateWithLinearFilter) {
    ASSERT_NE(m_Device, nullptr);

    RHI::SamplerDesc desc;
    desc.MagFilter = RHI::FilterMode::Linear;
    desc.MinFilter = RHI::FilterMode::Linear;
    desc.MipmapMode = RHI::FilterMode::Linear;
    desc.AnisotropyEnable = true;
    desc.DebugName = "LinearFilterSampler";

    m_Sampler = RHI::RHISampler::Create(m_Device, desc);

    ASSERT_NE(m_Sampler, nullptr);
    EXPECT_NE(m_Sampler->GetHandle(), VK_NULL_HANDLE);
}

TEST_F(RHISamplerTest, CreateWithMixedFilters) {
    ASSERT_NE(m_Device, nullptr);

    RHI::SamplerDesc desc;
    desc.MagFilter = RHI::FilterMode::Linear;
    desc.MinFilter = RHI::FilterMode::Nearest;
    desc.MipmapMode = RHI::FilterMode::Linear;
    desc.DebugName = "MixedFilterSampler";

    m_Sampler = RHI::RHISampler::Create(m_Device, desc);

    ASSERT_NE(m_Sampler, nullptr);
    EXPECT_NE(m_Sampler->GetHandle(), VK_NULL_HANDLE);
}

// =============================================================================
// Address Mode Tests
// =============================================================================

TEST_F(RHISamplerTest, CreateWithRepeatAddressMode) {
    ASSERT_NE(m_Device, nullptr);

    RHI::SamplerDesc desc;
    desc.AddressModeU = RHI::AddressMode::Repeat;
    desc.AddressModeV = RHI::AddressMode::Repeat;
    desc.AddressModeW = RHI::AddressMode::Repeat;
    desc.DebugName = "RepeatSampler";

    m_Sampler = RHI::RHISampler::Create(m_Device, desc);

    ASSERT_NE(m_Sampler, nullptr);
    EXPECT_NE(m_Sampler->GetHandle(), VK_NULL_HANDLE);
}

TEST_F(RHISamplerTest, CreateWithClampToEdgeAddressMode) {
    ASSERT_NE(m_Device, nullptr);

    RHI::SamplerDesc desc;
    desc.AddressModeU = RHI::AddressMode::ClampToEdge;
    desc.AddressModeV = RHI::AddressMode::ClampToEdge;
    desc.AddressModeW = RHI::AddressMode::ClampToEdge;
    desc.DebugName = "ClampToEdgeSampler";

    m_Sampler = RHI::RHISampler::Create(m_Device, desc);

    ASSERT_NE(m_Sampler, nullptr);
    EXPECT_NE(m_Sampler->GetHandle(), VK_NULL_HANDLE);
}

TEST_F(RHISamplerTest, CreateWithMirroredRepeatAddressMode) {
    ASSERT_NE(m_Device, nullptr);

    RHI::SamplerDesc desc;
    desc.AddressModeU = RHI::AddressMode::MirroredRepeat;
    desc.AddressModeV = RHI::AddressMode::MirroredRepeat;
    desc.AddressModeW = RHI::AddressMode::MirroredRepeat;
    desc.DebugName = "MirroredRepeatSampler";

    m_Sampler = RHI::RHISampler::Create(m_Device, desc);

    ASSERT_NE(m_Sampler, nullptr);
    EXPECT_NE(m_Sampler->GetHandle(), VK_NULL_HANDLE);
}

TEST_F(RHISamplerTest, CreateWithClampToBorderAddressMode) {
    ASSERT_NE(m_Device, nullptr);

    RHI::SamplerDesc desc;
    desc.AddressModeU = RHI::AddressMode::ClampToBorder;
    desc.AddressModeV = RHI::AddressMode::ClampToBorder;
    desc.AddressModeW = RHI::AddressMode::ClampToBorder;
    desc.Border = RHI::BorderColor::OpaqueBlack;
    desc.DebugName = "ClampToBorderSampler";

    m_Sampler = RHI::RHISampler::Create(m_Device, desc);

    ASSERT_NE(m_Sampler, nullptr);
    EXPECT_NE(m_Sampler->GetHandle(), VK_NULL_HANDLE);
}

TEST_F(RHISamplerTest, CreateWithMixedAddressModes) {
    ASSERT_NE(m_Device, nullptr);

    RHI::SamplerDesc desc;
    desc.AddressModeU = RHI::AddressMode::Repeat;
    desc.AddressModeV = RHI::AddressMode::ClampToEdge;
    desc.AddressModeW = RHI::AddressMode::MirroredRepeat;
    desc.DebugName = "MixedAddressModeSampler";

    m_Sampler = RHI::RHISampler::Create(m_Device, desc);

    ASSERT_NE(m_Sampler, nullptr);
    EXPECT_NE(m_Sampler->GetHandle(), VK_NULL_HANDLE);
}

// =============================================================================
// Border Color Tests
// =============================================================================

TEST_F(RHISamplerTest, CreateWithTransparentBlackBorder) {
    ASSERT_NE(m_Device, nullptr);

    RHI::SamplerDesc desc;
    desc.AddressModeU = RHI::AddressMode::ClampToBorder;
    desc.AddressModeV = RHI::AddressMode::ClampToBorder;
    desc.Border = RHI::BorderColor::TransparentBlack;
    desc.DebugName = "TransparentBlackBorderSampler";

    m_Sampler = RHI::RHISampler::Create(m_Device, desc);

    ASSERT_NE(m_Sampler, nullptr);
    EXPECT_NE(m_Sampler->GetHandle(), VK_NULL_HANDLE);
}

TEST_F(RHISamplerTest, CreateWithOpaqueWhiteBorder) {
    ASSERT_NE(m_Device, nullptr);

    RHI::SamplerDesc desc;
    desc.AddressModeU = RHI::AddressMode::ClampToBorder;
    desc.AddressModeV = RHI::AddressMode::ClampToBorder;
    desc.Border = RHI::BorderColor::OpaqueWhite;
    desc.DebugName = "OpaqueWhiteBorderSampler";

    m_Sampler = RHI::RHISampler::Create(m_Device, desc);

    ASSERT_NE(m_Sampler, nullptr);
    EXPECT_NE(m_Sampler->GetHandle(), VK_NULL_HANDLE);
}

// =============================================================================
// Anisotropic Filtering Tests
// =============================================================================

TEST_F(RHISamplerTest, CreateWithAnisotropy) {
    ASSERT_NE(m_Device, nullptr);

    RHI::SamplerDesc desc;
    desc.AnisotropyEnable = true;
    desc.MaxAnisotropy = 16.0f;
    desc.DebugName = "AnisotropySampler";

    m_Sampler = RHI::RHISampler::Create(m_Device, desc);

    ASSERT_NE(m_Sampler, nullptr);
    EXPECT_NE(m_Sampler->GetHandle(), VK_NULL_HANDLE);
}

TEST_F(RHISamplerTest, CreateWithoutAnisotropy) {
    ASSERT_NE(m_Device, nullptr);

    RHI::SamplerDesc desc;
    desc.AnisotropyEnable = false;
    desc.DebugName = "NoAnisotropySampler";

    m_Sampler = RHI::RHISampler::Create(m_Device, desc);

    ASSERT_NE(m_Sampler, nullptr);
    EXPECT_NE(m_Sampler->GetHandle(), VK_NULL_HANDLE);
}

TEST_F(RHISamplerTest, CreateWithLowAnisotropy) {
    ASSERT_NE(m_Device, nullptr);

    RHI::SamplerDesc desc;
    desc.AnisotropyEnable = true;
    desc.MaxAnisotropy = 4.0f;
    desc.DebugName = "LowAnisotropySampler";

    m_Sampler = RHI::RHISampler::Create(m_Device, desc);

    ASSERT_NE(m_Sampler, nullptr);
    EXPECT_NE(m_Sampler->GetHandle(), VK_NULL_HANDLE);
}

// =============================================================================
// Mipmap Settings Tests
// =============================================================================

TEST_F(RHISamplerTest, CreateWithMipLodBias) {
    ASSERT_NE(m_Device, nullptr);

    RHI::SamplerDesc desc;
    desc.MipLodBias = 0.5f;
    desc.DebugName = "MipLodBiasSampler";

    m_Sampler = RHI::RHISampler::Create(m_Device, desc);

    ASSERT_NE(m_Sampler, nullptr);
    EXPECT_NE(m_Sampler->GetHandle(), VK_NULL_HANDLE);
}

TEST_F(RHISamplerTest, CreateWithLodRange) {
    ASSERT_NE(m_Device, nullptr);

    RHI::SamplerDesc desc;
    desc.MinLod = 0.0f;
    desc.MaxLod = 10.0f;
    desc.DebugName = "LodRangeSampler";

    m_Sampler = RHI::RHISampler::Create(m_Device, desc);

    ASSERT_NE(m_Sampler, nullptr);
    EXPECT_NE(m_Sampler->GetHandle(), VK_NULL_HANDLE);
}

TEST_F(RHISamplerTest, CreateWithRestrictedLodRange) {
    ASSERT_NE(m_Device, nullptr);

    RHI::SamplerDesc desc;
    desc.MinLod = 2.0f;
    desc.MaxLod = 6.0f;
    desc.DebugName = "RestrictedLodSampler";

    m_Sampler = RHI::RHISampler::Create(m_Device, desc);

    ASSERT_NE(m_Sampler, nullptr);
    EXPECT_NE(m_Sampler->GetHandle(), VK_NULL_HANDLE);
}

// =============================================================================
// Comparison (Shadow Map) Sampler Tests
// =============================================================================

TEST_F(RHISamplerTest, CreateWithCompareEnable) {
    ASSERT_NE(m_Device, nullptr);

    RHI::SamplerDesc desc;
    desc.CompareEnable = true;
    desc.CompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    desc.DebugName = "CompareSampler";

    m_Sampler = RHI::RHISampler::Create(m_Device, desc);

    ASSERT_NE(m_Sampler, nullptr);
    EXPECT_NE(m_Sampler->GetHandle(), VK_NULL_HANDLE);
}

TEST_F(RHISamplerTest, CreateWithDifferentCompareOps) {
    ASSERT_NE(m_Device, nullptr);

    // Test with VK_COMPARE_OP_LESS
    {
        RHI::SamplerDesc desc;
        desc.CompareEnable = true;
        desc.CompareOp = VK_COMPARE_OP_LESS;
        desc.DebugName = "CompareLessSampler";

        auto sampler = RHI::RHISampler::Create(m_Device, desc);
        ASSERT_NE(sampler, nullptr);
        EXPECT_NE(sampler->GetHandle(), VK_NULL_HANDLE);
    }

    // Test with VK_COMPARE_OP_GREATER
    {
        RHI::SamplerDesc desc;
        desc.CompareEnable = true;
        desc.CompareOp = VK_COMPARE_OP_GREATER;
        desc.DebugName = "CompareGreaterSampler";

        auto sampler = RHI::RHISampler::Create(m_Device, desc);
        ASSERT_NE(sampler, nullptr);
        EXPECT_NE(sampler->GetHandle(), VK_NULL_HANDLE);
    }
}

// =============================================================================
// Sampler Destruction Tests
// =============================================================================

TEST_F(RHISamplerTest, SamplerDestructorCleansUp) {
    ASSERT_NE(m_Device, nullptr);

    VkSampler samplerHandle = VK_NULL_HANDLE;

    {
        RHI::SamplerDesc desc;
        desc.DebugName = "TempSampler";

        auto sampler = RHI::RHISampler::Create(m_Device, desc);
        ASSERT_NE(sampler, nullptr);
        samplerHandle = sampler->GetHandle();
        EXPECT_NE(samplerHandle, VK_NULL_HANDLE);

        // Sampler goes out of scope here
    }

    // After destruction, the VkSampler should be destroyed
    // We can't directly verify this without validation layers,
    // but we can verify the test doesn't crash
    SUCCEED();
}

// =============================================================================
// Multiple Sampler Creation Tests
// =============================================================================

TEST_F(RHISamplerTest, CreateMultipleSamplers) {
    ASSERT_NE(m_Device, nullptr);

    auto sampler1 = RHI::RHISampler::CreateLinear(m_Device);
    auto sampler2 = RHI::RHISampler::CreateNearest(m_Device);
    auto sampler3 = RHI::RHISampler::CreateLinearClamp(m_Device);
    auto sampler4 = RHI::RHISampler::CreateShadow(m_Device);

    ASSERT_NE(sampler1, nullptr);
    ASSERT_NE(sampler2, nullptr);
    ASSERT_NE(sampler3, nullptr);
    ASSERT_NE(sampler4, nullptr);

    // All handles should be unique
    EXPECT_NE(sampler1->GetHandle(), sampler2->GetHandle());
    EXPECT_NE(sampler1->GetHandle(), sampler3->GetHandle());
    EXPECT_NE(sampler1->GetHandle(), sampler4->GetHandle());
    EXPECT_NE(sampler2->GetHandle(), sampler3->GetHandle());
    EXPECT_NE(sampler2->GetHandle(), sampler4->GetHandle());
    EXPECT_NE(sampler3->GetHandle(), sampler4->GetHandle());
}
