/**
 * @file TestRHIDescriptor.cpp
 * @brief Test file for RHI descriptor classes using GoogleTest.
 *
 * This file tests RHIDescriptorSetLayout, RHIDescriptorPool, and RHIDescriptorSet
 * classes for proper creation and resource binding.
 *
 * Note: These tests require a Vulkan-capable system to pass.
 */

#include <gtest/gtest.h>
#include "RHI/RHIDescriptorSetLayout.h"
#include "RHI/RHIDescriptorPool.h"
#include "RHI/RHIDescriptorSet.h"
#include "RHI/RHIBuffer.h"
#include "RHI/RHIDevice.h"
#include "RHI/RHIPhysicalDevice.h"
#include "RHI/RHIInstance.h"
#include "Platform/Window.h"
#include "Core/Log.h"
#include "Resources/UniformBufferObjects.h"

#include <vector>

// =============================================================================
// Test Fixture for RHI Descriptor Tests
// =============================================================================

class RHIDescriptorTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!Core::Log::IsInitialized()) {
            Core::Log::Init();
        }

        // Create a window to initialize GLFW and get a surface
        Platform::WindowConfig windowConfig;
        windowConfig.Width = 100;
        windowConfig.Height = 100;
        windowConfig.Title = "RHI Descriptor Test Window";
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
        m_DescriptorSet.reset();
        m_DescriptorPool.reset();
        m_DescriptorSetLayout.reset();
        m_UniformBuffer.reset();
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
    Core::Ref<RHI::RHIDescriptorSetLayout> m_DescriptorSetLayout;
    Core::Ref<RHI::RHIDescriptorPool> m_DescriptorPool;
    Core::Ref<RHI::RHIDescriptorSet> m_DescriptorSet;
    Core::Ref<RHI::RHIBuffer> m_UniformBuffer;
    VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
};

// =============================================================================
// RHIDescriptorSetLayout Tests
// =============================================================================

TEST_F(RHIDescriptorTest, CreateDescriptorSetLayoutWithSingleBinding) {
    ASSERT_NE(m_Device, nullptr);

    RHI::DescriptorSetLayoutDesc desc;
    desc.Bindings = {
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr}
    };

    m_DescriptorSetLayout = RHI::RHIDescriptorSetLayout::Create(m_Device, desc);

    ASSERT_NE(m_DescriptorSetLayout, nullptr);
    EXPECT_NE(m_DescriptorSetLayout->GetHandle(), VK_NULL_HANDLE);
    EXPECT_EQ(m_DescriptorSetLayout->GetBindingCount(), 1u);
}

TEST_F(RHIDescriptorTest, CreateDescriptorSetLayoutWithMultipleBindings) {
    ASSERT_NE(m_Device, nullptr);

    RHI::DescriptorSetLayoutDesc desc;
    desc.Bindings = {
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
        {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}
    };

    m_DescriptorSetLayout = RHI::RHIDescriptorSetLayout::Create(m_Device, desc);

    ASSERT_NE(m_DescriptorSetLayout, nullptr);
    EXPECT_NE(m_DescriptorSetLayout->GetHandle(), VK_NULL_HANDLE);
    EXPECT_EQ(m_DescriptorSetLayout->GetBindingCount(), 3u);
}

TEST_F(RHIDescriptorTest, CreateDescriptorSetLayoutWithBindingsVector) {
    ASSERT_NE(m_Device, nullptr);

    std::vector<RHI::DescriptorBinding> bindings = {
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr}
    };

    m_DescriptorSetLayout = RHI::RHIDescriptorSetLayout::Create(m_Device, bindings);

    ASSERT_NE(m_DescriptorSetLayout, nullptr);
    EXPECT_NE(m_DescriptorSetLayout->GetHandle(), VK_NULL_HANDLE);
}

TEST_F(RHIDescriptorTest, CreateDescriptorSetLayoutForCameraUBO) {
    ASSERT_NE(m_Device, nullptr);

    // Create layout matching CameraUBO (binding 0)
    RHI::DescriptorSetLayoutDesc desc;
    desc.Bindings = {
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}
    };

    m_DescriptorSetLayout = RHI::RHIDescriptorSetLayout::Create(m_Device, desc);

    ASSERT_NE(m_DescriptorSetLayout, nullptr);
    EXPECT_NE(m_DescriptorSetLayout->GetHandle(), VK_NULL_HANDLE);

    // Verify bindings
    const auto& bindings = m_DescriptorSetLayout->GetBindings();
    ASSERT_EQ(bindings.size(), 1u);
    EXPECT_EQ(bindings[0].Binding, 0u);
    EXPECT_EQ(bindings[0].Type, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
}

// =============================================================================
// RHIDescriptorPool Tests
// =============================================================================

TEST_F(RHIDescriptorTest, CreateDescriptorPoolWithSingleType) {
    ASSERT_NE(m_Device, nullptr);

    RHI::DescriptorPoolDesc poolDesc;
    poolDesc.MaxSets = 10;
    poolDesc.PoolSizes = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10}
    };

    m_DescriptorPool = RHI::RHIDescriptorPool::Create(m_Device, poolDesc);

    ASSERT_NE(m_DescriptorPool, nullptr);
    EXPECT_NE(m_DescriptorPool->GetHandle(), VK_NULL_HANDLE);
    EXPECT_EQ(m_DescriptorPool->GetMaxSets(), 10u);
}

TEST_F(RHIDescriptorTest, CreateDescriptorPoolWithMultipleTypes) {
    ASSERT_NE(m_Device, nullptr);

    RHI::DescriptorPoolDesc poolDesc;
    poolDesc.MaxSets = 100;
    poolDesc.PoolSizes = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 100},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 50}
    };

    m_DescriptorPool = RHI::RHIDescriptorPool::Create(m_Device, poolDesc);

    ASSERT_NE(m_DescriptorPool, nullptr);
    EXPECT_NE(m_DescriptorPool->GetHandle(), VK_NULL_HANDLE);
    EXPECT_EQ(m_DescriptorPool->GetMaxSets(), 100u);
}

TEST_F(RHIDescriptorTest, CreateDescriptorPoolWithConvenienceMethod) {
    ASSERT_NE(m_Device, nullptr);

    std::vector<RHI::DescriptorPoolSize> poolSizes = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10}
    };

    m_DescriptorPool = RHI::RHIDescriptorPool::Create(m_Device, 10, poolSizes);

    ASSERT_NE(m_DescriptorPool, nullptr);
    EXPECT_NE(m_DescriptorPool->GetHandle(), VK_NULL_HANDLE);
}

TEST_F(RHIDescriptorTest, AllocateDescriptorSetFromPool) {
    ASSERT_NE(m_Device, nullptr);

    // Create layout
    RHI::DescriptorSetLayoutDesc layoutDesc;
    layoutDesc.Bindings = {
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr}
    };
    m_DescriptorSetLayout = RHI::RHIDescriptorSetLayout::Create(m_Device, layoutDesc);
    ASSERT_NE(m_DescriptorSetLayout, nullptr);

    // Create pool
    RHI::DescriptorPoolDesc poolDesc;
    poolDesc.MaxSets = 10;
    poolDesc.PoolSizes = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10}
    };
    m_DescriptorPool = RHI::RHIDescriptorPool::Create(m_Device, poolDesc);
    ASSERT_NE(m_DescriptorPool, nullptr);

    // Allocate set
    VkDescriptorSet set = m_DescriptorPool->Allocate(m_DescriptorSetLayout);
    EXPECT_NE(set, VK_NULL_HANDLE);
}

TEST_F(RHIDescriptorTest, AllocateMultipleDescriptorSets) {
    ASSERT_NE(m_Device, nullptr);

    // Create layout
    RHI::DescriptorSetLayoutDesc layoutDesc;
    layoutDesc.Bindings = {
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr}
    };
    m_DescriptorSetLayout = RHI::RHIDescriptorSetLayout::Create(m_Device, layoutDesc);
    ASSERT_NE(m_DescriptorSetLayout, nullptr);

    // Create pool
    RHI::DescriptorPoolDesc poolDesc;
    poolDesc.MaxSets = 10;
    poolDesc.PoolSizes = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10}
    };
    m_DescriptorPool = RHI::RHIDescriptorPool::Create(m_Device, poolDesc);
    ASSERT_NE(m_DescriptorPool, nullptr);

    // Allocate multiple sets
    auto sets = m_DescriptorPool->AllocateMultiple(m_DescriptorSetLayout->GetHandle(), 5);
    EXPECT_EQ(sets.size(), 5u);
    for (const auto& set : sets) {
        EXPECT_NE(set, VK_NULL_HANDLE);
    }
}

TEST_F(RHIDescriptorTest, ResetDescriptorPool) {
    ASSERT_NE(m_Device, nullptr);

    // Create layout
    RHI::DescriptorSetLayoutDesc layoutDesc;
    layoutDesc.Bindings = {
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr}
    };
    m_DescriptorSetLayout = RHI::RHIDescriptorSetLayout::Create(m_Device, layoutDesc);
    ASSERT_NE(m_DescriptorSetLayout, nullptr);

    // Create pool
    RHI::DescriptorPoolDesc poolDesc;
    poolDesc.MaxSets = 10;
    poolDesc.PoolSizes = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10}
    };
    m_DescriptorPool = RHI::RHIDescriptorPool::Create(m_Device, poolDesc);
    ASSERT_NE(m_DescriptorPool, nullptr);

    // Allocate all sets
    auto sets = m_DescriptorPool->AllocateMultiple(m_DescriptorSetLayout->GetHandle(), 10);
    EXPECT_EQ(sets.size(), 10u);

    // Reset pool
    bool resetResult = m_DescriptorPool->Reset();
    EXPECT_TRUE(resetResult);

    // Allocate again after reset
    auto newSets = m_DescriptorPool->AllocateMultiple(m_DescriptorSetLayout->GetHandle(), 5);
    EXPECT_EQ(newSets.size(), 5u);
}

// =============================================================================
// RHIDescriptorSet Tests
// =============================================================================

TEST_F(RHIDescriptorTest, CreateDescriptorSet) {
    ASSERT_NE(m_Device, nullptr);

    // Create layout
    RHI::DescriptorSetLayoutDesc layoutDesc;
    layoutDesc.Bindings = {
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr}
    };
    m_DescriptorSetLayout = RHI::RHIDescriptorSetLayout::Create(m_Device, layoutDesc);
    ASSERT_NE(m_DescriptorSetLayout, nullptr);

    // Create pool
    RHI::DescriptorPoolDesc poolDesc;
    poolDesc.MaxSets = 10;
    poolDesc.PoolSizes = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10}
    };
    m_DescriptorPool = RHI::RHIDescriptorPool::Create(m_Device, poolDesc);
    ASSERT_NE(m_DescriptorPool, nullptr);

    // Create descriptor set
    m_DescriptorSet = RHI::RHIDescriptorSet::Create(m_Device, m_DescriptorPool, m_DescriptorSetLayout);

    ASSERT_NE(m_DescriptorSet, nullptr);
    EXPECT_NE(m_DescriptorSet->GetHandle(), VK_NULL_HANDLE);
    EXPECT_TRUE(m_DescriptorSet->IsValid());
}

TEST_F(RHIDescriptorTest, UpdateDescriptorSetWithUniformBuffer) {
    ASSERT_NE(m_Device, nullptr);

    // Create layout
    RHI::DescriptorSetLayoutDesc layoutDesc;
    layoutDesc.Bindings = {
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr}
    };
    m_DescriptorSetLayout = RHI::RHIDescriptorSetLayout::Create(m_Device, layoutDesc);
    ASSERT_NE(m_DescriptorSetLayout, nullptr);

    // Create pool
    RHI::DescriptorPoolDesc poolDesc;
    poolDesc.MaxSets = 10;
    poolDesc.PoolSizes = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10}
    };
    m_DescriptorPool = RHI::RHIDescriptorPool::Create(m_Device, poolDesc);
    ASSERT_NE(m_DescriptorPool, nullptr);

    // Create uniform buffer
    RHI::BufferDesc bufferDesc;
    bufferDesc.Size = sizeof(Resources::CameraUBO);
    bufferDesc.Usage = RHI::BufferUsage::Uniform;
    bufferDesc.Memory = RHI::MemoryUsage::CpuToGpu;
    m_UniformBuffer = RHI::RHIBuffer::Create(m_Device, bufferDesc);
    ASSERT_NE(m_UniformBuffer, nullptr);

    // Create descriptor set
    m_DescriptorSet = RHI::RHIDescriptorSet::Create(m_Device, m_DescriptorPool, m_DescriptorSetLayout);
    ASSERT_NE(m_DescriptorSet, nullptr);

    // Update descriptor set with buffer - should not crash
    EXPECT_NO_THROW(m_DescriptorSet->UpdateBuffer(
        0,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        m_UniformBuffer));
}

TEST_F(RHIDescriptorTest, UpdateDescriptorSetWithRawBufferHandle) {
    ASSERT_NE(m_Device, nullptr);

    // Create layout
    RHI::DescriptorSetLayoutDesc layoutDesc;
    layoutDesc.Bindings = {
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr}
    };
    m_DescriptorSetLayout = RHI::RHIDescriptorSetLayout::Create(m_Device, layoutDesc);
    ASSERT_NE(m_DescriptorSetLayout, nullptr);

    // Create pool
    RHI::DescriptorPoolDesc poolDesc;
    poolDesc.MaxSets = 10;
    poolDesc.PoolSizes = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10}
    };
    m_DescriptorPool = RHI::RHIDescriptorPool::Create(m_Device, poolDesc);
    ASSERT_NE(m_DescriptorPool, nullptr);

    // Create uniform buffer
    RHI::BufferDesc bufferDesc;
    bufferDesc.Size = sizeof(Resources::ObjectUBO);
    bufferDesc.Usage = RHI::BufferUsage::Uniform;
    bufferDesc.Memory = RHI::MemoryUsage::CpuToGpu;
    m_UniformBuffer = RHI::RHIBuffer::Create(m_Device, bufferDesc);
    ASSERT_NE(m_UniformBuffer, nullptr);

    // Create descriptor set
    m_DescriptorSet = RHI::RHIDescriptorSet::Create(m_Device, m_DescriptorPool, m_DescriptorSetLayout);
    ASSERT_NE(m_DescriptorSet, nullptr);

    // Update with raw handle
    EXPECT_NO_THROW(m_DescriptorSet->UpdateBuffer(
        0,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        m_UniformBuffer->GetHandle(),
        0,
        sizeof(Resources::ObjectUBO)));
}

TEST_F(RHIDescriptorTest, UpdateDescriptorSetWithMultipleBuffers) {
    ASSERT_NE(m_Device, nullptr);

    // Create layout with multiple bindings
    RHI::DescriptorSetLayoutDesc layoutDesc;
    layoutDesc.Bindings = {
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr}
    };
    m_DescriptorSetLayout = RHI::RHIDescriptorSetLayout::Create(m_Device, layoutDesc);
    ASSERT_NE(m_DescriptorSetLayout, nullptr);

    // Create pool
    RHI::DescriptorPoolDesc poolDesc;
    poolDesc.MaxSets = 10;
    poolDesc.PoolSizes = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 20}
    };
    m_DescriptorPool = RHI::RHIDescriptorPool::Create(m_Device, poolDesc);
    ASSERT_NE(m_DescriptorPool, nullptr);

    // Create uniform buffers
    RHI::BufferDesc bufferDesc1;
    bufferDesc1.Size = sizeof(Resources::CameraUBO);
    bufferDesc1.Usage = RHI::BufferUsage::Uniform;
    bufferDesc1.Memory = RHI::MemoryUsage::CpuToGpu;
    auto cameraBuffer = RHI::RHIBuffer::Create(m_Device, bufferDesc1);
    ASSERT_NE(cameraBuffer, nullptr);

    RHI::BufferDesc bufferDesc2;
    bufferDesc2.Size = sizeof(Resources::ObjectUBO);
    bufferDesc2.Usage = RHI::BufferUsage::Uniform;
    bufferDesc2.Memory = RHI::MemoryUsage::CpuToGpu;
    auto objectBuffer = RHI::RHIBuffer::Create(m_Device, bufferDesc2);
    ASSERT_NE(objectBuffer, nullptr);

    // Create descriptor set
    m_DescriptorSet = RHI::RHIDescriptorSet::Create(m_Device, m_DescriptorPool, m_DescriptorSetLayout);
    ASSERT_NE(m_DescriptorSet, nullptr);

    // Update with multiple buffers
    std::vector<RHI::DescriptorBufferWrite> writes = {
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, cameraBuffer->GetHandle(), 0, sizeof(Resources::CameraUBO), 0},
        {1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, objectBuffer->GetHandle(), 0, sizeof(Resources::ObjectUBO), 0}
    };

    EXPECT_NO_THROW(m_DescriptorSet->UpdateBuffers(writes));
}

TEST_F(RHIDescriptorTest, CreateDescriptorSetFromExistingHandle) {
    ASSERT_NE(m_Device, nullptr);

    // Create layout
    RHI::DescriptorSetLayoutDesc layoutDesc;
    layoutDesc.Bindings = {
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr}
    };
    m_DescriptorSetLayout = RHI::RHIDescriptorSetLayout::Create(m_Device, layoutDesc);
    ASSERT_NE(m_DescriptorSetLayout, nullptr);

    // Create pool
    RHI::DescriptorPoolDesc poolDesc;
    poolDesc.MaxSets = 10;
    poolDesc.PoolSizes = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10}
    };
    m_DescriptorPool = RHI::RHIDescriptorPool::Create(m_Device, poolDesc);
    ASSERT_NE(m_DescriptorPool, nullptr);

    // Allocate raw handle
    VkDescriptorSet rawHandle = m_DescriptorPool->Allocate(m_DescriptorSetLayout);
    ASSERT_NE(rawHandle, VK_NULL_HANDLE);

    // Create wrapper from handle
    m_DescriptorSet = RHI::RHIDescriptorSet::CreateFromHandle(m_Device, rawHandle);

    ASSERT_NE(m_DescriptorSet, nullptr);
    EXPECT_EQ(m_DescriptorSet->GetHandle(), rawHandle);
    EXPECT_TRUE(m_DescriptorSet->IsValid());
}

// =============================================================================
// UBO Structure Tests
// =============================================================================

TEST_F(RHIDescriptorTest, CameraUBOHasCorrectSize) {
    // Verify CameraUBO size matches expected std140 layout
    EXPECT_EQ(sizeof(Resources::CameraUBO), 208u);
}

TEST_F(RHIDescriptorTest, ObjectUBOHasCorrectSize) {
    // Verify ObjectUBO size matches expected std140 layout
    EXPECT_EQ(sizeof(Resources::ObjectUBO), 128u);
}

TEST_F(RHIDescriptorTest, WriteDataToCameraUBO) {
    ASSERT_NE(m_Device, nullptr);

    // Create uniform buffer
    RHI::BufferDesc bufferDesc;
    bufferDesc.Size = sizeof(Resources::CameraUBO);
    bufferDesc.Usage = RHI::BufferUsage::Uniform;
    bufferDesc.Memory = RHI::MemoryUsage::CpuToGpu;
    m_UniformBuffer = RHI::RHIBuffer::Create(m_Device, bufferDesc);
    ASSERT_NE(m_UniformBuffer, nullptr);

    // Prepare camera data
    Resources::CameraUBO cameraData;
    cameraData.View = glm::mat4(1.0f);
    cameraData.Projection = glm::mat4(1.0f);
    cameraData.ViewProjection = glm::mat4(1.0f);
    cameraData.CameraPosition = glm::vec3(0.0f, 0.0f, 5.0f);

    // Write to buffer
    bool result = m_UniformBuffer->SetData(&cameraData, sizeof(Resources::CameraUBO));
    EXPECT_TRUE(result);
}

TEST_F(RHIDescriptorTest, WriteDataToObjectUBO) {
    ASSERT_NE(m_Device, nullptr);

    // Create uniform buffer
    RHI::BufferDesc bufferDesc;
    bufferDesc.Size = sizeof(Resources::ObjectUBO);
    bufferDesc.Usage = RHI::BufferUsage::Uniform;
    bufferDesc.Memory = RHI::MemoryUsage::CpuToGpu;
    m_UniformBuffer = RHI::RHIBuffer::Create(m_Device, bufferDesc);
    ASSERT_NE(m_UniformBuffer, nullptr);

    // Prepare object data
    Resources::ObjectUBO objectData;
    objectData.Model = glm::mat4(1.0f);
    objectData.NormalMatrix = glm::mat4(1.0f);

    // Write to buffer
    bool result = m_UniformBuffer->SetData(&objectData, sizeof(Resources::ObjectUBO));
    EXPECT_TRUE(result);
}

// =============================================================================
// Error Handling Tests
// =============================================================================

TEST_F(RHIDescriptorTest, CreateDescriptorSetLayoutWithNullDeviceFails) {
    RHI::DescriptorSetLayoutDesc desc;
    desc.Bindings = {
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr}
    };

    auto layout = RHI::RHIDescriptorSetLayout::Create(nullptr, desc);
    EXPECT_EQ(layout, nullptr);
}

TEST_F(RHIDescriptorTest, CreateDescriptorPoolWithZeroMaxSetsFails) {
    ASSERT_NE(m_Device, nullptr);

    RHI::DescriptorPoolDesc poolDesc;
    poolDesc.MaxSets = 0;  // Invalid
    poolDesc.PoolSizes = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10}
    };

    auto pool = RHI::RHIDescriptorPool::Create(m_Device, poolDesc);
    EXPECT_EQ(pool, nullptr);
}

TEST_F(RHIDescriptorTest, CreateDescriptorPoolWithEmptyPoolSizesFails) {
    ASSERT_NE(m_Device, nullptr);

    RHI::DescriptorPoolDesc poolDesc;
    poolDesc.MaxSets = 10;
    poolDesc.PoolSizes = {};  // Empty

    auto pool = RHI::RHIDescriptorPool::Create(m_Device, poolDesc);
    EXPECT_EQ(pool, nullptr);
}

TEST_F(RHIDescriptorTest, AllocateFromPoolWithNullLayoutFails) {
    ASSERT_NE(m_Device, nullptr);

    // Create pool
    RHI::DescriptorPoolDesc poolDesc;
    poolDesc.MaxSets = 10;
    poolDesc.PoolSizes = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10}
    };
    m_DescriptorPool = RHI::RHIDescriptorPool::Create(m_Device, poolDesc);
    ASSERT_NE(m_DescriptorPool, nullptr);

    VkDescriptorSet set = m_DescriptorPool->Allocate(VK_NULL_HANDLE);
    EXPECT_EQ(set, VK_NULL_HANDLE);
}

TEST_F(RHIDescriptorTest, CreateDescriptorSetWithNullPoolFails) {
    ASSERT_NE(m_Device, nullptr);

    // Create layout
    RHI::DescriptorSetLayoutDesc layoutDesc;
    layoutDesc.Bindings = {
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr}
    };
    m_DescriptorSetLayout = RHI::RHIDescriptorSetLayout::Create(m_Device, layoutDesc);
    ASSERT_NE(m_DescriptorSetLayout, nullptr);

    auto descriptorSet = RHI::RHIDescriptorSet::Create(m_Device, nullptr, m_DescriptorSetLayout);
    EXPECT_EQ(descriptorSet, nullptr);
}

TEST_F(RHIDescriptorTest, CreateDescriptorSetWithNullLayoutFails) {
    ASSERT_NE(m_Device, nullptr);

    // Create pool
    RHI::DescriptorPoolDesc poolDesc;
    poolDesc.MaxSets = 10;
    poolDesc.PoolSizes = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10}
    };
    m_DescriptorPool = RHI::RHIDescriptorPool::Create(m_Device, poolDesc);
    ASSERT_NE(m_DescriptorPool, nullptr);

    auto descriptorSet = RHI::RHIDescriptorSet::Create(m_Device, m_DescriptorPool, nullptr);
    EXPECT_EQ(descriptorSet, nullptr);
}
