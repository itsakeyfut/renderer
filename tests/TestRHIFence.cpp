/**
 * @file TestRHIFence.cpp
 * @brief Test file for RHI/RHIFence.h using GoogleTest.
 *
 * This file tests that the RHIFence class correctly creates
 * and manages Vulkan fences for CPU-GPU synchronization.
 *
 * Note: These tests require a Vulkan-capable system to pass.
 */

#include <gtest/gtest.h>
#include "RHI/RHIFence.h"
#include "RHI/RHIDevice.h"
#include "RHI/RHIPhysicalDevice.h"
#include "RHI/RHIInstance.h"
#include "Platform/Window.h"
#include "Core/Log.h"

// =============================================================================
// Test Fixture for RHIFence Tests
// =============================================================================

class RHIFenceTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!Core::Log::IsInitialized()) {
            Core::Log::Init();
        }

        // Create a window to initialize GLFW and get a surface
        Platform::WindowConfig windowConfig;
        windowConfig.Width = 100;
        windowConfig.Height = 100;
        windowConfig.Title = "RHI Fence Test Window";
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
        m_Fence.reset();
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
    Core::Ref<RHI::RHIFence> m_Fence;
    VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
};

// =============================================================================
// Creation Tests
// =============================================================================

TEST_F(RHIFenceTest, CreatesFenceUnsignaled) {
    ASSERT_NE(m_Device, nullptr);

    m_Fence = RHI::RHIFence::Create(m_Device, false);

    ASSERT_NE(m_Fence, nullptr);
    EXPECT_NE(m_Fence->GetHandle(), VK_NULL_HANDLE);
    EXPECT_FALSE(m_Fence->IsSignaled());
}

TEST_F(RHIFenceTest, CreatesFenceSignaled) {
    ASSERT_NE(m_Device, nullptr);

    m_Fence = RHI::RHIFence::Create(m_Device, true);

    ASSERT_NE(m_Fence, nullptr);
    EXPECT_NE(m_Fence->GetHandle(), VK_NULL_HANDLE);
    EXPECT_TRUE(m_Fence->IsSignaled());
}

TEST_F(RHIFenceTest, DefaultCreatesUnsignaledFence) {
    ASSERT_NE(m_Device, nullptr);

    m_Fence = RHI::RHIFence::Create(m_Device);

    ASSERT_NE(m_Fence, nullptr);
    EXPECT_FALSE(m_Fence->IsSignaled());
}

// =============================================================================
// Handle Validity Tests
// =============================================================================

TEST_F(RHIFenceTest, HandleIsValidVkFence) {
    ASSERT_NE(m_Device, nullptr);

    m_Fence = RHI::RHIFence::Create(m_Device);
    ASSERT_NE(m_Fence, nullptr);

    VkFence handle = m_Fence->GetHandle();
    EXPECT_NE(handle, VK_NULL_HANDLE);
}

// =============================================================================
// Wait Tests
// =============================================================================

TEST_F(RHIFenceTest, WaitOnSignaledFenceReturnsImmediately) {
    ASSERT_NE(m_Device, nullptr);

    m_Fence = RHI::RHIFence::Create(m_Device, true);  // Create signaled
    ASSERT_NE(m_Fence, nullptr);

    // Wait should return true immediately
    bool result = m_Fence->Wait(1000000000);  // 1 second timeout
    EXPECT_TRUE(result);
}

TEST_F(RHIFenceTest, WaitOnUnsignaledFenceTimesOut) {
    ASSERT_NE(m_Device, nullptr);

    m_Fence = RHI::RHIFence::Create(m_Device, false);  // Create unsignaled
    ASSERT_NE(m_Fence, nullptr);

    // Wait should timeout
    bool result = m_Fence->Wait(1000);  // 1 microsecond timeout
    EXPECT_FALSE(result);
}

// =============================================================================
// Reset Tests
// =============================================================================

TEST_F(RHIFenceTest, ResetSignaledFence) {
    ASSERT_NE(m_Device, nullptr);

    m_Fence = RHI::RHIFence::Create(m_Device, true);  // Create signaled
    ASSERT_NE(m_Fence, nullptr);
    EXPECT_TRUE(m_Fence->IsSignaled());

    m_Fence->Reset();

    EXPECT_FALSE(m_Fence->IsSignaled());
}

TEST_F(RHIFenceTest, ResetDoesNotCrash) {
    ASSERT_NE(m_Device, nullptr);

    m_Fence = RHI::RHIFence::Create(m_Device, true);
    ASSERT_NE(m_Fence, nullptr);

    EXPECT_NO_THROW(m_Fence->Reset());
}

// =============================================================================
// IsSignaled Tests
// =============================================================================

TEST_F(RHIFenceTest, IsSignaledReturnsTrueForSignaledFence) {
    ASSERT_NE(m_Device, nullptr);

    m_Fence = RHI::RHIFence::Create(m_Device, true);
    ASSERT_NE(m_Fence, nullptr);

    EXPECT_TRUE(m_Fence->IsSignaled());
}

TEST_F(RHIFenceTest, IsSignaledReturnsFalseForUnsignaledFence) {
    ASSERT_NE(m_Device, nullptr);

    m_Fence = RHI::RHIFence::Create(m_Device, false);
    ASSERT_NE(m_Fence, nullptr);

    EXPECT_FALSE(m_Fence->IsSignaled());
}

// =============================================================================
// Multiple Fence Tests
// =============================================================================

TEST_F(RHIFenceTest, CreatesMultipleFences) {
    ASSERT_NE(m_Device, nullptr);

    auto fence1 = RHI::RHIFence::Create(m_Device, true);
    auto fence2 = RHI::RHIFence::Create(m_Device, false);
    auto fence3 = RHI::RHIFence::Create(m_Device, true);

    ASSERT_NE(fence1, nullptr);
    ASSERT_NE(fence2, nullptr);
    ASSERT_NE(fence3, nullptr);

    // Each fence should have a unique handle
    EXPECT_NE(fence1->GetHandle(), fence2->GetHandle());
    EXPECT_NE(fence2->GetHandle(), fence3->GetHandle());
    EXPECT_NE(fence1->GetHandle(), fence3->GetHandle());

    // Verify signaled states
    EXPECT_TRUE(fence1->IsSignaled());
    EXPECT_FALSE(fence2->IsSignaled());
    EXPECT_TRUE(fence3->IsSignaled());
}

// =============================================================================
// Destructor Tests
// =============================================================================

TEST_F(RHIFenceTest, DestructorCleansUpResources) {
    ASSERT_NE(m_Device, nullptr);

    VkFence handle = VK_NULL_HANDLE;

    {
        auto fence = RHI::RHIFence::Create(m_Device, true);
        ASSERT_NE(fence, nullptr);
        handle = fence->GetHandle();
        EXPECT_NE(handle, VK_NULL_HANDLE);
        // Fence goes out of scope here
    }

    // After destruction, the code should not crash
    SUCCEED();
}

// =============================================================================
// Frame-in-Flight Pattern Tests
// =============================================================================

TEST_F(RHIFenceTest, FrameInFlightPattern) {
    ASSERT_NE(m_Device, nullptr);

    // Create fences for double-buffering (MAX_FRAMES_IN_FLIGHT = 2)
    const int framesInFlight = 2;

    std::vector<Core::Ref<RHI::RHIFence>> inFlightFences;

    for (int i = 0; i < framesInFlight; ++i) {
        // Create signaled so first frame doesn't block
        auto fence = RHI::RHIFence::Create(m_Device, true);
        ASSERT_NE(fence, nullptr);
        EXPECT_TRUE(fence->IsSignaled());
        inFlightFences.push_back(fence);
    }

    EXPECT_EQ(inFlightFences.size(), static_cast<size_t>(framesInFlight));

    // Test first frame wait and reset for each fence
    // Note: We can only test the first wait per fence since we don't have
    // actual GPU work to re-signal the fences after reset
    for (int i = 0; i < framesInFlight; ++i) {
        auto& fence = inFlightFences[i];

        // First wait should succeed (signaled initially)
        bool waited = fence->Wait(1000000000);
        EXPECT_TRUE(waited) << "First wait for fence " << i << " failed";

        // Reset for next use
        fence->Reset();
        EXPECT_FALSE(fence->IsSignaled());
    }

    // Verify pattern setup is correct
    // In real code, vkQueueSubmit would signal the fence after GPU work
}

// =============================================================================
// Stress Tests
// =============================================================================

TEST_F(RHIFenceTest, CreateManyFences) {
    ASSERT_NE(m_Device, nullptr);

    const int count = 100;
    std::vector<Core::Ref<RHI::RHIFence>> fences;
    fences.reserve(count);

    for (int i = 0; i < count; ++i) {
        auto fence = RHI::RHIFence::Create(m_Device, i % 2 == 0);  // Alternate signaled/unsignaled
        ASSERT_NE(fence, nullptr) << "Failed to create fence " << i;
        EXPECT_NE(fence->GetHandle(), VK_NULL_HANDLE);
        fences.push_back(fence);
    }

    // Verify all handles are unique
    std::set<VkFence> uniqueHandles;
    for (const auto& fence : fences) {
        uniqueHandles.insert(fence->GetHandle());
    }
    EXPECT_EQ(uniqueHandles.size(), static_cast<size_t>(count));
}
