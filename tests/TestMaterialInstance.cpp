/**
 * @file TestMaterialInstance.cpp
 * @brief Test file for MaterialInstance and MaterialGPUData using GoogleTest.
 *
 * This file tests the MaterialInstance class for proper creation and
 * parameter management. Tests requiring GPU resources verify resource
 * creation with a Vulkan device.
 *
 * Note: Tests marked with GPU require a Vulkan-capable system to pass.
 */

#include <gtest/gtest.h>
#include "Resources/MaterialInstance.h"
#include "RHI/RHIDescriptorSetLayout.h"
#include "RHI/RHIDescriptorPool.h"
#include "RHI/RHIDescriptorSet.h"
#include "RHI/RHIBuffer.h"
#include "RHI/RHITexture.h"
#include "RHI/RHISampler.h"
#include "RHI/RHIDevice.h"
#include "RHI/RHIPhysicalDevice.h"
#include "RHI/RHIInstance.h"
#include "Platform/Window.h"
#include "Core/Log.h"

#include <glm/glm.hpp>

// =============================================================================
// MaterialGPUData Tests (CPU-only)
// =============================================================================

class MaterialGPUDataTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!Core::Log::IsInitialized()) {
            Core::Log::Init();
        }
    }
};

TEST_F(MaterialGPUDataTest, DefaultValuesAreCorrect) {
    Resources::MaterialGPUData data;

    EXPECT_EQ(data.BaseColor, glm::vec4(1.0f));
    EXPECT_FLOAT_EQ(data.Metallic, 0.0f);
    EXPECT_FLOAT_EQ(data.Roughness, 0.5f);
    EXPECT_FLOAT_EQ(data.AmbientOcclusion, 1.0f);
    EXPECT_FLOAT_EQ(data.NormalScale, 1.0f);
    EXPECT_EQ(data.EmissiveFactor, glm::vec3(0.0f));
    EXPECT_FLOAT_EQ(data.AlphaCutoff, 0.5f);

    EXPECT_EQ(data.HasBaseColorTexture, 0);
    EXPECT_EQ(data.HasNormalTexture, 0);
    EXPECT_EQ(data.HasMetallicRoughnessTexture, 0);
    EXPECT_EQ(data.HasOcclusionTexture, 0);
    EXPECT_EQ(data.HasEmissiveTexture, 0);
}

TEST_F(MaterialGPUDataTest, SizeIs16ByteAligned) {
    // MaterialGPUData must be 16-byte aligned for UBO compatibility
    EXPECT_EQ(sizeof(Resources::MaterialGPUData) % 16, 0u);
}

TEST_F(MaterialGPUDataTest, SizeIsExpected) {
    // Expected layout:
    // vec4 BaseColor (16 bytes)
    // float Metallic, Roughness, AmbientOcclusion, NormalScale (16 bytes)
    // vec3 EmissiveFactor + float AlphaCutoff (16 bytes)
    // 5 int32_t flags + 3 int32_t padding (32 bytes)
    // Total: 80 bytes
    EXPECT_EQ(sizeof(Resources::MaterialGPUData), 80u);
}

// =============================================================================
// TextureSlot Tests
// =============================================================================

TEST_F(MaterialGPUDataTest, TextureSlotEnumValuesAreSequential) {
    EXPECT_EQ(static_cast<uint32_t>(Resources::TextureSlot::BaseColor), 0u);
    EXPECT_EQ(static_cast<uint32_t>(Resources::TextureSlot::Normal), 1u);
    EXPECT_EQ(static_cast<uint32_t>(Resources::TextureSlot::MetallicRoughness), 2u);
    EXPECT_EQ(static_cast<uint32_t>(Resources::TextureSlot::Occlusion), 3u);
    EXPECT_EQ(static_cast<uint32_t>(Resources::TextureSlot::Emissive), 4u);
    EXPECT_EQ(static_cast<uint32_t>(Resources::TextureSlot::Count), 5u);
}

// =============================================================================
// MaterialInstance GPU Tests
// =============================================================================

class MaterialInstanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!Core::Log::IsInitialized()) {
            Core::Log::Init();
        }

        // Create a window to initialize GLFW and get a surface
        Platform::WindowConfig windowConfig;
        windowConfig.Width = 100;
        windowConfig.Height = 100;
        windowConfig.Title = "MaterialInstance Test Window";
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

        // Create default resources for MaterialInstance
        if (m_Device) {
            // Create descriptor pool for materials
            RHI::DescriptorPoolDesc poolDesc;
            poolDesc.MaxSets = 10;
            poolDesc.PoolSizes = {
                {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10},
                {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 50}
            };
            m_DescriptorPool = RHI::RHIDescriptorPool::Create(m_Device, poolDesc);

            // Create material descriptor set layout
            m_DescriptorSetLayout = Resources::MaterialInstance::CreateDescriptorSetLayout(m_Device);

            // Create default sampler
            m_Sampler = RHI::RHISampler::CreateLinear(m_Device);

            // Create default 1x1 white texture
            CreateDefaultTexture();
        }
    }

    void TearDown() override {
        m_MaterialInstance.reset();
        m_DefaultTexture.reset();
        m_Sampler.reset();
        m_DescriptorPool.reset();
        m_DescriptorSetLayout.reset();
        m_Device.reset();
        m_PhysicalDevice.reset();

        if (m_Surface != VK_NULL_HANDLE && m_Instance) {
            vkDestroySurfaceKHR(m_Instance->GetHandle(), m_Surface, nullptr);
            m_Surface = VK_NULL_HANDLE;
        }

        m_Instance.reset();
        m_Window.reset();
    }

    void CreateDefaultTexture() {
        // Create a 1x1 white texture
        RHI::TextureDesc texDesc;
        texDesc.Width = 1;
        texDesc.Height = 1;
        texDesc.Format = VK_FORMAT_R8G8B8A8_UNORM;
        texDesc.MipLevels = 1;
        texDesc.DebugName = "Default White Texture";

        m_DefaultTexture = RHI::RHITexture::Create(m_Device, texDesc);
        if (m_DefaultTexture) {
            uint32_t whitePixel = 0xFFFFFFFF;
            m_DefaultTexture->UploadData(m_Device, &whitePixel, sizeof(whitePixel));
        }
    }

    std::unique_ptr<Platform::Window> m_Window;
    Core::Ref<RHI::RHIInstance> m_Instance;
    Core::Ref<RHI::RHIPhysicalDevice> m_PhysicalDevice;
    Core::Ref<RHI::RHIDevice> m_Device;
    Core::Ref<RHI::RHIDescriptorPool> m_DescriptorPool;
    Core::Ref<RHI::RHIDescriptorSetLayout> m_DescriptorSetLayout;
    Core::Ref<RHI::RHISampler> m_Sampler;
    Core::Ref<RHI::RHITexture> m_DefaultTexture;
    Core::Ref<Resources::MaterialInstance> m_MaterialInstance;
    VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
};

TEST_F(MaterialInstanceTest, CreateDescriptorSetLayout) {
    ASSERT_NE(m_Device, nullptr);
    ASSERT_NE(m_DescriptorSetLayout, nullptr);

    // Verify layout has correct number of bindings
    // Binding 0: UBO, Bindings 1-5: textures
    EXPECT_EQ(m_DescriptorSetLayout->GetBindingCount(), 6u);

    const auto& bindings = m_DescriptorSetLayout->GetBindings();
    EXPECT_EQ(bindings[0].Type, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    for (size_t i = 1; i < bindings.size(); ++i) {
        EXPECT_EQ(bindings[i].Type, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    }
}

TEST_F(MaterialInstanceTest, CreateMaterialInstance) {
    ASSERT_NE(m_Device, nullptr);
    ASSERT_NE(m_DescriptorPool, nullptr);
    ASSERT_NE(m_DescriptorSetLayout, nullptr);
    ASSERT_NE(m_Sampler, nullptr);
    ASSERT_NE(m_DefaultTexture, nullptr);

    m_MaterialInstance = Resources::MaterialInstance::Create(
        m_Device, m_DescriptorPool, m_DescriptorSetLayout, m_Sampler, m_DefaultTexture);

    ASSERT_NE(m_MaterialInstance, nullptr);
    EXPECT_NE(m_MaterialInstance->GetDescriptorSet(), nullptr);
    EXPECT_NE(m_MaterialInstance->GetUniformBuffer(), nullptr);
}

TEST_F(MaterialInstanceTest, DefaultParameterValues) {
    ASSERT_NE(m_Device, nullptr);

    m_MaterialInstance = Resources::MaterialInstance::Create(
        m_Device, m_DescriptorPool, m_DescriptorSetLayout, m_Sampler, m_DefaultTexture);
    ASSERT_NE(m_MaterialInstance, nullptr);

    EXPECT_EQ(m_MaterialInstance->GetBaseColor(), glm::vec4(1.0f));
    EXPECT_FLOAT_EQ(m_MaterialInstance->GetMetallic(), 0.0f);
    EXPECT_FLOAT_EQ(m_MaterialInstance->GetRoughness(), 0.5f);
    EXPECT_FLOAT_EQ(m_MaterialInstance->GetAmbientOcclusion(), 1.0f);
    EXPECT_FLOAT_EQ(m_MaterialInstance->GetNormalScale(), 1.0f);
    EXPECT_EQ(m_MaterialInstance->GetEmissiveFactor(), glm::vec3(0.0f));
    EXPECT_FLOAT_EQ(m_MaterialInstance->GetAlphaCutoff(), 0.5f);
}

TEST_F(MaterialInstanceTest, SetBaseColor) {
    ASSERT_NE(m_Device, nullptr);

    m_MaterialInstance = Resources::MaterialInstance::Create(
        m_Device, m_DescriptorPool, m_DescriptorSetLayout, m_Sampler, m_DefaultTexture);
    ASSERT_NE(m_MaterialInstance, nullptr);

    glm::vec4 newColor(0.5f, 0.3f, 0.8f, 1.0f);
    m_MaterialInstance->SetBaseColor(newColor);

    EXPECT_EQ(m_MaterialInstance->GetBaseColor(), newColor);
    EXPECT_TRUE(m_MaterialInstance->IsDirty());
}

TEST_F(MaterialInstanceTest, SetMetallic) {
    ASSERT_NE(m_Device, nullptr);

    m_MaterialInstance = Resources::MaterialInstance::Create(
        m_Device, m_DescriptorPool, m_DescriptorSetLayout, m_Sampler, m_DefaultTexture);
    ASSERT_NE(m_MaterialInstance, nullptr);

    m_MaterialInstance->SetMetallic(0.75f);
    EXPECT_FLOAT_EQ(m_MaterialInstance->GetMetallic(), 0.75f);
}

TEST_F(MaterialInstanceTest, SetMetallicClampedToRange) {
    ASSERT_NE(m_Device, nullptr);

    m_MaterialInstance = Resources::MaterialInstance::Create(
        m_Device, m_DescriptorPool, m_DescriptorSetLayout, m_Sampler, m_DefaultTexture);
    ASSERT_NE(m_MaterialInstance, nullptr);

    // Test clamping above 1.0
    m_MaterialInstance->SetMetallic(1.5f);
    EXPECT_FLOAT_EQ(m_MaterialInstance->GetMetallic(), 1.0f);

    // Test clamping below 0.0
    m_MaterialInstance->SetMetallic(-0.5f);
    EXPECT_FLOAT_EQ(m_MaterialInstance->GetMetallic(), 0.0f);
}

TEST_F(MaterialInstanceTest, SetRoughness) {
    ASSERT_NE(m_Device, nullptr);

    m_MaterialInstance = Resources::MaterialInstance::Create(
        m_Device, m_DescriptorPool, m_DescriptorSetLayout, m_Sampler, m_DefaultTexture);
    ASSERT_NE(m_MaterialInstance, nullptr);

    m_MaterialInstance->SetRoughness(0.3f);
    EXPECT_FLOAT_EQ(m_MaterialInstance->GetRoughness(), 0.3f);
}

TEST_F(MaterialInstanceTest, SetEmissiveFactor) {
    ASSERT_NE(m_Device, nullptr);

    m_MaterialInstance = Resources::MaterialInstance::Create(
        m_Device, m_DescriptorPool, m_DescriptorSetLayout, m_Sampler, m_DefaultTexture);
    ASSERT_NE(m_MaterialInstance, nullptr);

    glm::vec3 emissive(1.0f, 0.5f, 0.0f);
    m_MaterialInstance->SetEmissiveFactor(emissive);

    EXPECT_EQ(m_MaterialInstance->GetEmissiveFactor(), emissive);
}

TEST_F(MaterialInstanceTest, SetParameterByName) {
    ASSERT_NE(m_Device, nullptr);

    m_MaterialInstance = Resources::MaterialInstance::Create(
        m_Device, m_DescriptorPool, m_DescriptorSetLayout, m_Sampler, m_DefaultTexture);
    ASSERT_NE(m_MaterialInstance, nullptr);

    EXPECT_TRUE(m_MaterialInstance->SetParameter("metallic", 0.8f));
    EXPECT_FLOAT_EQ(m_MaterialInstance->GetMetallic(), 0.8f);

    EXPECT_TRUE(m_MaterialInstance->SetParameter("roughness", 0.2f));
    EXPECT_FLOAT_EQ(m_MaterialInstance->GetRoughness(), 0.2f);

    EXPECT_TRUE(m_MaterialInstance->SetParameter("ao", 0.9f));
    EXPECT_FLOAT_EQ(m_MaterialInstance->GetAmbientOcclusion(), 0.9f);
}

TEST_F(MaterialInstanceTest, SetParameterByNameInvalidReturnsFalse) {
    ASSERT_NE(m_Device, nullptr);

    m_MaterialInstance = Resources::MaterialInstance::Create(
        m_Device, m_DescriptorPool, m_DescriptorSetLayout, m_Sampler, m_DefaultTexture);
    ASSERT_NE(m_MaterialInstance, nullptr);

    EXPECT_FALSE(m_MaterialInstance->SetParameter("invalidParam", 0.5f));
}

TEST_F(MaterialInstanceTest, SetVec4ParameterByName) {
    ASSERT_NE(m_Device, nullptr);

    m_MaterialInstance = Resources::MaterialInstance::Create(
        m_Device, m_DescriptorPool, m_DescriptorSetLayout, m_Sampler, m_DefaultTexture);
    ASSERT_NE(m_MaterialInstance, nullptr);

    glm::vec4 color(1.0f, 0.0f, 0.0f, 1.0f);
    EXPECT_TRUE(m_MaterialInstance->SetParameter("baseColor", color));
    EXPECT_EQ(m_MaterialInstance->GetBaseColor(), color);
}

TEST_F(MaterialInstanceTest, NoTexturesBoundByDefault) {
    ASSERT_NE(m_Device, nullptr);

    m_MaterialInstance = Resources::MaterialInstance::Create(
        m_Device, m_DescriptorPool, m_DescriptorSetLayout, m_Sampler, m_DefaultTexture);
    ASSERT_NE(m_MaterialInstance, nullptr);

    EXPECT_FALSE(m_MaterialInstance->HasTexture(Resources::TextureSlot::BaseColor));
    EXPECT_FALSE(m_MaterialInstance->HasTexture(Resources::TextureSlot::Normal));
    EXPECT_FALSE(m_MaterialInstance->HasTexture(Resources::TextureSlot::MetallicRoughness));
    EXPECT_FALSE(m_MaterialInstance->HasTexture(Resources::TextureSlot::Occlusion));
    EXPECT_FALSE(m_MaterialInstance->HasTexture(Resources::TextureSlot::Emissive));
}

TEST_F(MaterialInstanceTest, SetTextureBySlot) {
    ASSERT_NE(m_Device, nullptr);

    m_MaterialInstance = Resources::MaterialInstance::Create(
        m_Device, m_DescriptorPool, m_DescriptorSetLayout, m_Sampler, m_DefaultTexture);
    ASSERT_NE(m_MaterialInstance, nullptr);

    // Use default texture as a test texture
    m_MaterialInstance->SetTexture(Resources::TextureSlot::BaseColor, m_DefaultTexture);

    EXPECT_TRUE(m_MaterialInstance->HasTexture(Resources::TextureSlot::BaseColor));
    EXPECT_EQ(m_MaterialInstance->GetTexture(Resources::TextureSlot::BaseColor), m_DefaultTexture);

    // Check that material data flags are updated
    const auto& data = m_MaterialInstance->GetMaterialData();
    EXPECT_EQ(data.HasBaseColorTexture, 1);
}

TEST_F(MaterialInstanceTest, SetTextureByName) {
    ASSERT_NE(m_Device, nullptr);

    m_MaterialInstance = Resources::MaterialInstance::Create(
        m_Device, m_DescriptorPool, m_DescriptorSetLayout, m_Sampler, m_DefaultTexture);
    ASSERT_NE(m_MaterialInstance, nullptr);

    EXPECT_TRUE(m_MaterialInstance->SetTexture("normal", m_DefaultTexture));
    EXPECT_TRUE(m_MaterialInstance->HasTexture(Resources::TextureSlot::Normal));

    EXPECT_TRUE(m_MaterialInstance->SetTexture("albedo", m_DefaultTexture));
    EXPECT_TRUE(m_MaterialInstance->HasTexture(Resources::TextureSlot::BaseColor));

    // Invalid name should return false
    EXPECT_FALSE(m_MaterialInstance->SetTexture("invalidSlot", m_DefaultTexture));
}

TEST_F(MaterialInstanceTest, ClearTextureBySettingNull) {
    ASSERT_NE(m_Device, nullptr);

    m_MaterialInstance = Resources::MaterialInstance::Create(
        m_Device, m_DescriptorPool, m_DescriptorSetLayout, m_Sampler, m_DefaultTexture);
    ASSERT_NE(m_MaterialInstance, nullptr);

    // Set a texture
    m_MaterialInstance->SetTexture(Resources::TextureSlot::BaseColor, m_DefaultTexture);
    EXPECT_TRUE(m_MaterialInstance->HasTexture(Resources::TextureSlot::BaseColor));

    // Clear it
    m_MaterialInstance->SetTexture(Resources::TextureSlot::BaseColor, nullptr);
    EXPECT_FALSE(m_MaterialInstance->HasTexture(Resources::TextureSlot::BaseColor));

    // Check that material data flags are updated
    const auto& data = m_MaterialInstance->GetMaterialData();
    EXPECT_EQ(data.HasBaseColorTexture, 0);
}

TEST_F(MaterialInstanceTest, UploadToGPUClearsDirtyFlag) {
    ASSERT_NE(m_Device, nullptr);

    m_MaterialInstance = Resources::MaterialInstance::Create(
        m_Device, m_DescriptorPool, m_DescriptorSetLayout, m_Sampler, m_DefaultTexture);
    ASSERT_NE(m_MaterialInstance, nullptr);

    // Modify a parameter to mark as dirty
    m_MaterialInstance->SetMetallic(0.9f);
    EXPECT_TRUE(m_MaterialInstance->IsDirty());

    // Upload should clear dirty flag
    m_MaterialInstance->UploadToGPU();
    EXPECT_FALSE(m_MaterialInstance->IsDirty());
}

// =============================================================================
// Error Handling Tests
// =============================================================================

TEST_F(MaterialInstanceTest, CreateWithNullDeviceFails) {
    ASSERT_NE(m_DescriptorPool, nullptr);
    ASSERT_NE(m_DescriptorSetLayout, nullptr);
    ASSERT_NE(m_Sampler, nullptr);
    ASSERT_NE(m_DefaultTexture, nullptr);

    auto instance = Resources::MaterialInstance::Create(
        nullptr, m_DescriptorPool, m_DescriptorSetLayout, m_Sampler, m_DefaultTexture);

    EXPECT_EQ(instance, nullptr);
}

TEST_F(MaterialInstanceTest, CreateWithNullPoolFails) {
    ASSERT_NE(m_Device, nullptr);
    ASSERT_NE(m_DescriptorSetLayout, nullptr);
    ASSERT_NE(m_Sampler, nullptr);
    ASSERT_NE(m_DefaultTexture, nullptr);

    auto instance = Resources::MaterialInstance::Create(
        m_Device, nullptr, m_DescriptorSetLayout, m_Sampler, m_DefaultTexture);

    EXPECT_EQ(instance, nullptr);
}

TEST_F(MaterialInstanceTest, CreateWithNullLayoutFails) {
    ASSERT_NE(m_Device, nullptr);
    ASSERT_NE(m_DescriptorPool, nullptr);
    ASSERT_NE(m_Sampler, nullptr);
    ASSERT_NE(m_DefaultTexture, nullptr);

    auto instance = Resources::MaterialInstance::Create(
        m_Device, m_DescriptorPool, nullptr, m_Sampler, m_DefaultTexture);

    EXPECT_EQ(instance, nullptr);
}

TEST_F(MaterialInstanceTest, CreateWithNullSamplerFails) {
    ASSERT_NE(m_Device, nullptr);
    ASSERT_NE(m_DescriptorPool, nullptr);
    ASSERT_NE(m_DescriptorSetLayout, nullptr);
    ASSERT_NE(m_DefaultTexture, nullptr);

    auto instance = Resources::MaterialInstance::Create(
        m_Device, m_DescriptorPool, m_DescriptorSetLayout, nullptr, m_DefaultTexture);

    EXPECT_EQ(instance, nullptr);
}

TEST_F(MaterialInstanceTest, CreateWithNullDefaultTextureFails) {
    ASSERT_NE(m_Device, nullptr);
    ASSERT_NE(m_DescriptorPool, nullptr);
    ASSERT_NE(m_DescriptorSetLayout, nullptr);
    ASSERT_NE(m_Sampler, nullptr);

    auto instance = Resources::MaterialInstance::Create(
        m_Device, m_DescriptorPool, m_DescriptorSetLayout, m_Sampler, nullptr);

    EXPECT_EQ(instance, nullptr);
}

TEST_F(MaterialInstanceTest, CreateDescriptorSetLayoutWithNullDeviceFails) {
    auto layout = Resources::MaterialInstance::CreateDescriptorSetLayout(nullptr);
    EXPECT_EQ(layout, nullptr);
}
