/**
 * @file TestRHISemaphore.cpp
 * @brief Test file for RHI/RHISemaphore.h using GoogleTest.
 *
 * This file tests that the RHISemaphore class correctly creates
 * and manages Vulkan semaphores for GPU-GPU synchronization.
 *
 * Note: These tests require a Vulkan-capable system to pass.
 */

#include <gtest/gtest.h>
#include "RHI/RHISemaphore.h"
#include "RHI/RHIDevice.h"
#include "RHI/RHIPhysicalDevice.h"
#include "RHI/RHIInstance.h"
#include "Platform/Window.h"
#include "Core/Log.h"

// =============================================================================
// Test Fixture for RHISemaphore Tests
// =============================================================================

class RHISemaphoreTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!Core::Log::IsInitialized()) {
            Core::Log::Init();
        }

        // Create a window to initialize GLFW and get a surface
        Platform::WindowConfig windowConfig;
        windowConfig.Width = 100;
        windowConfig.Height = 100;
        windowConfig.Title = "RHI Semaphore Test Window";
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
        m_Semaphore.reset();
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
    Core::Ref<RHI::RHISemaphore> m_Semaphore;
    VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
};

// =============================================================================
// Creation Tests
// =============================================================================

TEST_F(RHISemaphoreTest, CreatesSemaphore) {
    ASSERT_NE(m_Device, nullptr);

    m_Semaphore = RHI::RHISemaphore::Create(m_Device);

    ASSERT_NE(m_Semaphore, nullptr);
    EXPECT_NE(m_Semaphore->GetHandle(), VK_NULL_HANDLE);
}

TEST_F(RHISemaphoreTest, CreatesMultipleSemaphores) {
    ASSERT_NE(m_Device, nullptr);

    auto sem1 = RHI::RHISemaphore::Create(m_Device);
    auto sem2 = RHI::RHISemaphore::Create(m_Device);
    auto sem3 = RHI::RHISemaphore::Create(m_Device);

    ASSERT_NE(sem1, nullptr);
    ASSERT_NE(sem2, nullptr);
    ASSERT_NE(sem3, nullptr);

    // Each semaphore should have a unique handle
    EXPECT_NE(sem1->GetHandle(), sem2->GetHandle());
    EXPECT_NE(sem2->GetHandle(), sem3->GetHandle());
    EXPECT_NE(sem1->GetHandle(), sem3->GetHandle());
}

// =============================================================================
// Handle Validity Tests
// =============================================================================

TEST_F(RHISemaphoreTest, HandleIsValidVkSemaphore) {
    ASSERT_NE(m_Device, nullptr);

    m_Semaphore = RHI::RHISemaphore::Create(m_Device);
    ASSERT_NE(m_Semaphore, nullptr);

    VkSemaphore handle = m_Semaphore->GetHandle();
    EXPECT_NE(handle, VK_NULL_HANDLE);
}

// =============================================================================
// Destructor Tests
// =============================================================================

TEST_F(RHISemaphoreTest, DestructorCleansUpResources) {
    ASSERT_NE(m_Device, nullptr);

    VkSemaphore handle = VK_NULL_HANDLE;

    {
        auto semaphore = RHI::RHISemaphore::Create(m_Device);
        ASSERT_NE(semaphore, nullptr);
        handle = semaphore->GetHandle();
        EXPECT_NE(handle, VK_NULL_HANDLE);
        // Semaphore goes out of scope here
    }

    // After destruction, the code should not crash
    SUCCEED();
}

// =============================================================================
// Stress Tests
// =============================================================================

TEST_F(RHISemaphoreTest, CreateManySemaphores) {
    ASSERT_NE(m_Device, nullptr);

    const int count = 100;
    std::vector<Core::Ref<RHI::RHISemaphore>> semaphores;
    semaphores.reserve(count);

    for (int i = 0; i < count; ++i) {
        auto sem = RHI::RHISemaphore::Create(m_Device);
        ASSERT_NE(sem, nullptr) << "Failed to create semaphore " << i;
        EXPECT_NE(sem->GetHandle(), VK_NULL_HANDLE);
        semaphores.push_back(sem);
    }

    // Verify all handles are unique
    std::set<VkSemaphore> uniqueHandles;
    for (const auto& sem : semaphores) {
        uniqueHandles.insert(sem->GetHandle());
    }
    EXPECT_EQ(uniqueHandles.size(), static_cast<size_t>(count));
}

// =============================================================================
// Usage Pattern Tests
// =============================================================================

TEST_F(RHISemaphoreTest, SemaphoreCanBeUsedForFrameSync) {
    ASSERT_NE(m_Device, nullptr);

    // Simulate typical frame synchronization pattern
    // Create image available and render finished semaphores
    auto imageAvailable = RHI::RHISemaphore::Create(m_Device);
    auto renderFinished = RHI::RHISemaphore::Create(m_Device);

    ASSERT_NE(imageAvailable, nullptr);
    ASSERT_NE(renderFinished, nullptr);
    EXPECT_NE(imageAvailable->GetHandle(), renderFinished->GetHandle());
}

TEST_F(RHISemaphoreTest, MultipleSemaphoresPerFrame) {
    ASSERT_NE(m_Device, nullptr);

    // Create semaphores for double-buffering (MAX_FRAMES_IN_FLIGHT = 2)
    const int framesInFlight = 2;

    std::vector<Core::Ref<RHI::RHISemaphore>> imageAvailableSemaphores;
    std::vector<Core::Ref<RHI::RHISemaphore>> renderFinishedSemaphores;

    for (int i = 0; i < framesInFlight; ++i) {
        auto imgAvail = RHI::RHISemaphore::Create(m_Device);
        auto renderFinish = RHI::RHISemaphore::Create(m_Device);

        ASSERT_NE(imgAvail, nullptr);
        ASSERT_NE(renderFinish, nullptr);

        imageAvailableSemaphores.push_back(imgAvail);
        renderFinishedSemaphores.push_back(renderFinish);
    }

    EXPECT_EQ(imageAvailableSemaphores.size(), static_cast<size_t>(framesInFlight));
    EXPECT_EQ(renderFinishedSemaphores.size(), static_cast<size_t>(framesInFlight));
}
