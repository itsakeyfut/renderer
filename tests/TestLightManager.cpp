/**
 * @file TestLightManager.cpp
 * @brief Test file for Renderer/LightManager.h using GoogleTest.
 *
 * This file tests that the LightManager class correctly manages
 * scene lights and their GPU resources.
 *
 * Note: These tests require a Vulkan-capable system to pass.
 */

#include <gtest/gtest.h>
#include "Renderer/LightManager.h"
#include "RHI/RHIDevice.h"
#include "RHI/RHIPhysicalDevice.h"
#include "RHI/RHIInstance.h"
#include "Platform/Window.h"
#include "Core/Log.h"

#include <glm/glm.hpp>

// =============================================================================
// Test Fixture for LightManager Tests
// =============================================================================

class LightManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!Core::Log::IsInitialized()) {
            Core::Log::Init();
        }

        // Create a window to initialize GLFW and get a surface
        Platform::WindowConfig windowConfig;
        windowConfig.Width = 100;
        windowConfig.Height = 100;
        windowConfig.Title = "LightManager Test Window";
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
        m_LightManager.reset();
        m_Device.reset();
        m_PhysicalDevice.reset();

        if (m_Surface != VK_NULL_HANDLE && m_Instance) {
            vkDestroySurfaceKHR(m_Instance->GetHandle(), m_Surface, nullptr);
            m_Surface = VK_NULL_HANDLE;
        }

        m_Instance.reset();
        m_Window.reset();
    }

    static bool VecEqual(const glm::vec3& a, const glm::vec3& b, float epsilon = 0.0001f) {
        return glm::all(glm::lessThan(glm::abs(a - b), glm::vec3(epsilon)));
    }

    Core::Scope<Platform::Window> m_Window;
    Core::Ref<RHI::RHIInstance> m_Instance;
    Core::Ref<RHI::RHIPhysicalDevice> m_PhysicalDevice;
    Core::Ref<RHI::RHIDevice> m_Device;
    Core::Ref<Renderer::LightManager> m_LightManager;
    VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
};

// =============================================================================
// Creation Tests
// =============================================================================

TEST_F(LightManagerTest, CreatesLightManager) {
    ASSERT_NE(m_Device, nullptr);

    m_LightManager = Renderer::LightManager::Create(m_Device);

    ASSERT_NE(m_LightManager, nullptr);
}

TEST_F(LightManagerTest, CreatesWithCustomConfig) {
    ASSERT_NE(m_Device, nullptr);

    Renderer::LightManagerConfig config;
    config.MaxPointLights = 32;
    config.MaxSpotLights = 16;

    m_LightManager = Renderer::LightManager::Create(m_Device, config);

    ASSERT_NE(m_LightManager, nullptr);
}

// =============================================================================
// Directional Light Tests
// =============================================================================

TEST_F(LightManagerTest, SetDirectionalLight) {
    ASSERT_NE(m_Device, nullptr);
    m_LightManager = Renderer::LightManager::Create(m_Device);
    ASSERT_NE(m_LightManager, nullptr);

    Scene::DirectionalLight light;
    light.Direction = glm::normalize(glm::vec3(-1.0f, -1.0f, -1.0f));
    light.Color = glm::vec3(1.0f, 0.9f, 0.8f);
    light.Intensity = 2.0f;

    m_LightManager->SetDirectionalLight(light);

    const auto& retrieved = m_LightManager->GetDirectionalLight();
    EXPECT_TRUE(VecEqual(retrieved.Direction, light.Direction));
    EXPECT_TRUE(VecEqual(retrieved.Color, light.Color));
    EXPECT_FLOAT_EQ(retrieved.Intensity, 2.0f);
}

TEST_F(LightManagerTest, SetDirectionalLightMarksDirty) {
    ASSERT_NE(m_Device, nullptr);
    m_LightManager = Renderer::LightManager::Create(m_Device);
    ASSERT_NE(m_LightManager, nullptr);

    // Initial state is dirty (all frames need update)
    EXPECT_TRUE(m_LightManager->IsDirty());

    // Update all frames
    m_LightManager->UpdateGPUBuffers(0);
    m_LightManager->UpdateGPUBuffers(1);

    // After updating all frames, not dirty
    EXPECT_FALSE(m_LightManager->IsDirty());

    // Set light should mark dirty again
    Scene::DirectionalLight light;
    light.Direction = glm::vec3(0.0f, -1.0f, 0.0f);
    m_LightManager->SetDirectionalLight(light);

    EXPECT_TRUE(m_LightManager->IsDirty());
}

// =============================================================================
// Point Light Tests
// =============================================================================

TEST_F(LightManagerTest, AddPointLight) {
    ASSERT_NE(m_Device, nullptr);
    m_LightManager = Renderer::LightManager::Create(m_Device);
    ASSERT_NE(m_LightManager, nullptr);

    Scene::PointLight light;
    light.Position = glm::vec3(0.0f, 5.0f, 0.0f);
    light.Color = glm::vec3(1.0f, 1.0f, 1.0f);
    light.Intensity = 10.0f;
    light.Radius = 20.0f;

    int32_t index = m_LightManager->AddPointLight(light);

    EXPECT_EQ(index, 0);
    EXPECT_EQ(m_LightManager->GetPointLightCount(), 1u);
}

TEST_F(LightManagerTest, AddMultiplePointLights) {
    ASSERT_NE(m_Device, nullptr);
    m_LightManager = Renderer::LightManager::Create(m_Device);
    ASSERT_NE(m_LightManager, nullptr);

    for (int i = 0; i < 10; ++i) {
        Scene::PointLight light;
        light.Position = glm::vec3(static_cast<float>(i), 5.0f, 0.0f);
        light.Intensity = 10.0f;

        int32_t index = m_LightManager->AddPointLight(light);
        EXPECT_EQ(index, i);
    }

    EXPECT_EQ(m_LightManager->GetPointLightCount(), 10u);
}

TEST_F(LightManagerTest, GetPointLight) {
    ASSERT_NE(m_Device, nullptr);
    m_LightManager = Renderer::LightManager::Create(m_Device);
    ASSERT_NE(m_LightManager, nullptr);

    Scene::PointLight light;
    light.Position = glm::vec3(1.0f, 2.0f, 3.0f);
    light.Color = glm::vec3(0.5f, 0.6f, 0.7f);
    light.Intensity = 15.0f;
    light.Radius = 25.0f;

    m_LightManager->AddPointLight(light);

    const Scene::PointLight* retrieved = m_LightManager->GetPointLight(0);
    ASSERT_NE(retrieved, nullptr);
    EXPECT_TRUE(VecEqual(retrieved->Position, light.Position));
    EXPECT_TRUE(VecEqual(retrieved->Color, light.Color));
    EXPECT_FLOAT_EQ(retrieved->Intensity, 15.0f);
    EXPECT_FLOAT_EQ(retrieved->Radius, 25.0f);
}

TEST_F(LightManagerTest, GetPointLightInvalidIndex) {
    ASSERT_NE(m_Device, nullptr);
    m_LightManager = Renderer::LightManager::Create(m_Device);
    ASSERT_NE(m_LightManager, nullptr);

    const Scene::PointLight* light = m_LightManager->GetPointLight(999);
    EXPECT_EQ(light, nullptr);
}

TEST_F(LightManagerTest, UpdatePointLight) {
    ASSERT_NE(m_Device, nullptr);
    m_LightManager = Renderer::LightManager::Create(m_Device);
    ASSERT_NE(m_LightManager, nullptr);

    Scene::PointLight light;
    light.Position = glm::vec3(0.0f, 0.0f, 0.0f);
    light.Intensity = 10.0f;
    m_LightManager->AddPointLight(light);

    Scene::PointLight updated;
    updated.Position = glm::vec3(5.0f, 5.0f, 5.0f);
    updated.Intensity = 20.0f;

    bool result = m_LightManager->UpdatePointLight(0, updated);
    EXPECT_TRUE(result);

    const Scene::PointLight* retrieved = m_LightManager->GetPointLight(0);
    ASSERT_NE(retrieved, nullptr);
    EXPECT_TRUE(VecEqual(retrieved->Position, updated.Position));
    EXPECT_FLOAT_EQ(retrieved->Intensity, 20.0f);
}

TEST_F(LightManagerTest, UpdatePointLightInvalidIndex) {
    ASSERT_NE(m_Device, nullptr);
    m_LightManager = Renderer::LightManager::Create(m_Device);
    ASSERT_NE(m_LightManager, nullptr);

    Scene::PointLight light;
    bool result = m_LightManager->UpdatePointLight(999, light);
    EXPECT_FALSE(result);
}

TEST_F(LightManagerTest, ClearPointLights) {
    ASSERT_NE(m_Device, nullptr);
    m_LightManager = Renderer::LightManager::Create(m_Device);
    ASSERT_NE(m_LightManager, nullptr);

    for (int i = 0; i < 5; ++i) {
        Scene::PointLight light;
        m_LightManager->AddPointLight(light);
    }
    EXPECT_EQ(m_LightManager->GetPointLightCount(), 5u);

    m_LightManager->ClearPointLights();

    EXPECT_EQ(m_LightManager->GetPointLightCount(), 0u);
}

// =============================================================================
// Spot Light Tests
// =============================================================================

TEST_F(LightManagerTest, AddSpotLight) {
    ASSERT_NE(m_Device, nullptr);
    m_LightManager = Renderer::LightManager::Create(m_Device);
    ASSERT_NE(m_LightManager, nullptr);

    Scene::SpotLight light;
    light.Position = glm::vec3(0.0f, 10.0f, 0.0f);
    light.Direction = glm::vec3(0.0f, -1.0f, 0.0f);
    light.Color = glm::vec3(1.0f, 1.0f, 1.0f);
    light.Intensity = 50.0f;
    light.InnerConeAngle = glm::cos(glm::radians(15.0f));
    light.OuterConeAngle = glm::cos(glm::radians(30.0f));

    int32_t index = m_LightManager->AddSpotLight(light);

    EXPECT_EQ(index, 0);
    EXPECT_EQ(m_LightManager->GetSpotLightCount(), 1u);
}

TEST_F(LightManagerTest, AddMultipleSpotLights) {
    ASSERT_NE(m_Device, nullptr);
    m_LightManager = Renderer::LightManager::Create(m_Device);
    ASSERT_NE(m_LightManager, nullptr);

    for (int i = 0; i < 8; ++i) {
        Scene::SpotLight light;
        light.Position = glm::vec3(static_cast<float>(i), 10.0f, 0.0f);
        light.Direction = glm::vec3(0.0f, -1.0f, 0.0f);

        int32_t index = m_LightManager->AddSpotLight(light);
        EXPECT_EQ(index, i);
    }

    EXPECT_EQ(m_LightManager->GetSpotLightCount(), 8u);
}

TEST_F(LightManagerTest, GetSpotLight) {
    ASSERT_NE(m_Device, nullptr);
    m_LightManager = Renderer::LightManager::Create(m_Device);
    ASSERT_NE(m_LightManager, nullptr);

    Scene::SpotLight light;
    light.Position = glm::vec3(1.0f, 2.0f, 3.0f);
    light.Direction = glm::normalize(glm::vec3(1.0f, -1.0f, 0.0f));
    light.InnerConeAngle = 20.0f;
    light.OuterConeAngle = 40.0f;

    m_LightManager->AddSpotLight(light);

    const Scene::SpotLight* retrieved = m_LightManager->GetSpotLight(0);
    ASSERT_NE(retrieved, nullptr);
    EXPECT_TRUE(VecEqual(retrieved->Position, light.Position));
    EXPECT_FLOAT_EQ(retrieved->InnerConeAngle, 20.0f);
    EXPECT_FLOAT_EQ(retrieved->OuterConeAngle, 40.0f);
}

TEST_F(LightManagerTest, GetSpotLightInvalidIndex) {
    ASSERT_NE(m_Device, nullptr);
    m_LightManager = Renderer::LightManager::Create(m_Device);
    ASSERT_NE(m_LightManager, nullptr);

    const Scene::SpotLight* light = m_LightManager->GetSpotLight(999);
    EXPECT_EQ(light, nullptr);
}

TEST_F(LightManagerTest, UpdateSpotLight) {
    ASSERT_NE(m_Device, nullptr);
    m_LightManager = Renderer::LightManager::Create(m_Device);
    ASSERT_NE(m_LightManager, nullptr);

    Scene::SpotLight light;
    light.Position = glm::vec3(0.0f, 0.0f, 0.0f);
    light.InnerConeAngle = 15.0f;
    m_LightManager->AddSpotLight(light);

    Scene::SpotLight updated;
    updated.Position = glm::vec3(10.0f, 10.0f, 10.0f);
    updated.InnerConeAngle = 25.0f;

    bool result = m_LightManager->UpdateSpotLight(0, updated);
    EXPECT_TRUE(result);

    const Scene::SpotLight* retrieved = m_LightManager->GetSpotLight(0);
    ASSERT_NE(retrieved, nullptr);
    EXPECT_TRUE(VecEqual(retrieved->Position, updated.Position));
    EXPECT_FLOAT_EQ(retrieved->InnerConeAngle, 25.0f);
}

TEST_F(LightManagerTest, UpdateSpotLightInvalidIndex) {
    ASSERT_NE(m_Device, nullptr);
    m_LightManager = Renderer::LightManager::Create(m_Device);
    ASSERT_NE(m_LightManager, nullptr);

    Scene::SpotLight light;
    bool result = m_LightManager->UpdateSpotLight(999, light);
    EXPECT_FALSE(result);
}

TEST_F(LightManagerTest, ClearSpotLights) {
    ASSERT_NE(m_Device, nullptr);
    m_LightManager = Renderer::LightManager::Create(m_Device);
    ASSERT_NE(m_LightManager, nullptr);

    for (int i = 0; i < 5; ++i) {
        Scene::SpotLight light;
        m_LightManager->AddSpotLight(light);
    }
    EXPECT_EQ(m_LightManager->GetSpotLightCount(), 5u);

    m_LightManager->ClearSpotLights();

    EXPECT_EQ(m_LightManager->GetSpotLightCount(), 0u);
}

// =============================================================================
// Descriptor Tests
// =============================================================================

TEST_F(LightManagerTest, HasValidDescriptorSetLayout) {
    ASSERT_NE(m_Device, nullptr);
    m_LightManager = Renderer::LightManager::Create(m_Device);
    ASSERT_NE(m_LightManager, nullptr);

    const auto& layout = m_LightManager->GetDescriptorSetLayout();
    EXPECT_NE(layout, nullptr);
    EXPECT_NE(layout->GetHandle(), VK_NULL_HANDLE);
}

TEST_F(LightManagerTest, HasValidDescriptorSets) {
    ASSERT_NE(m_Device, nullptr);
    m_LightManager = Renderer::LightManager::Create(m_Device);
    ASSERT_NE(m_LightManager, nullptr);

    // Check descriptor sets for all frames in flight
    for (uint32_t i = 0; i < Renderer::MAX_FRAMES_IN_FLIGHT; ++i) {
        const auto& descriptorSet = m_LightManager->GetDescriptorSet(i);
        EXPECT_NE(descriptorSet, nullptr);
        EXPECT_NE(descriptorSet->GetHandle(), VK_NULL_HANDLE);
    }
}

// =============================================================================
// GPU Buffer Update Tests
// =============================================================================

TEST_F(LightManagerTest, UpdateGPUBuffersDoesNotCrash) {
    ASSERT_NE(m_Device, nullptr);
    m_LightManager = Renderer::LightManager::Create(m_Device);
    ASSERT_NE(m_LightManager, nullptr);

    // Add some lights
    Scene::DirectionalLight dirLight;
    dirLight.Direction = glm::vec3(0.0f, -1.0f, 0.0f);
    m_LightManager->SetDirectionalLight(dirLight);

    Scene::PointLight pointLight;
    pointLight.Position = glm::vec3(0.0f, 5.0f, 0.0f);
    m_LightManager->AddPointLight(pointLight);

    Scene::SpotLight spotLight;
    spotLight.Position = glm::vec3(0.0f, 10.0f, 0.0f);
    spotLight.Direction = glm::vec3(0.0f, -1.0f, 0.0f);
    m_LightManager->AddSpotLight(spotLight);

    // Update GPU buffers
    EXPECT_NO_THROW(m_LightManager->UpdateGPUBuffers(0));
    EXPECT_NO_THROW(m_LightManager->UpdateGPUBuffers(1));
}

TEST_F(LightManagerTest, UpdateGPUBuffersClearsDirtyFlag) {
    ASSERT_NE(m_Device, nullptr);
    m_LightManager = Renderer::LightManager::Create(m_Device);
    ASSERT_NE(m_LightManager, nullptr);

    EXPECT_TRUE(m_LightManager->IsDirty());

    // Update all frames
    for (uint32_t i = 0; i < Renderer::MAX_FRAMES_IN_FLIGHT; ++i) {
        m_LightManager->UpdateGPUBuffers(i);
    }

    EXPECT_FALSE(m_LightManager->IsDirty());
}

// =============================================================================
// Dirty Flag Tests
// =============================================================================

TEST_F(LightManagerTest, MarkDirtySetsFlag) {
    ASSERT_NE(m_Device, nullptr);
    m_LightManager = Renderer::LightManager::Create(m_Device);
    ASSERT_NE(m_LightManager, nullptr);

    // Clear dirty by updating all frames
    for (uint32_t i = 0; i < Renderer::MAX_FRAMES_IN_FLIGHT; ++i) {
        m_LightManager->UpdateGPUBuffers(i);
    }
    EXPECT_FALSE(m_LightManager->IsDirty());

    m_LightManager->MarkDirty();

    EXPECT_TRUE(m_LightManager->IsDirty());
}

TEST_F(LightManagerTest, AddLightMarksDirty) {
    ASSERT_NE(m_Device, nullptr);
    m_LightManager = Renderer::LightManager::Create(m_Device);
    ASSERT_NE(m_LightManager, nullptr);

    // Clear dirty
    for (uint32_t i = 0; i < Renderer::MAX_FRAMES_IN_FLIGHT; ++i) {
        m_LightManager->UpdateGPUBuffers(i);
    }
    EXPECT_FALSE(m_LightManager->IsDirty());

    // Adding a point light should mark dirty
    Scene::PointLight light;
    m_LightManager->AddPointLight(light);

    EXPECT_TRUE(m_LightManager->IsDirty());
}

// =============================================================================
// Config Tests
// =============================================================================

TEST_F(LightManagerTest, ConfigDefaultValues) {
    Renderer::LightManagerConfig config;

    EXPECT_EQ(config.MaxPointLights, Renderer::MAX_POINT_LIGHTS);
    EXPECT_EQ(config.MaxSpotLights, Renderer::MAX_SPOT_LIGHTS);
}

TEST_F(LightManagerTest, ConstantsHaveReasonableValues) {
    EXPECT_GE(Renderer::MAX_POINT_LIGHTS, 32u);
    EXPECT_GE(Renderer::MAX_SPOT_LIGHTS, 16u);
}
