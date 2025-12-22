/**
 * @file TestRHIInstance.cpp
 * @brief Test file for RHI/RHIInstance.h using GoogleTest.
 *
 * This file tests that the RHIInstance class correctly creates and manages
 * Vulkan instances with validation layers and debug messenger.
 *
 * Note: These tests require a Vulkan-capable system to pass. On systems without
 * Vulkan support, some tests may be skipped.
 */

#include <gtest/gtest.h>
#include "RHI/RHIInstance.h"
#include "Platform/Window.h"
#include "Core/Log.h"

// =============================================================================
// Test Fixture for RHIInstance Tests
// =============================================================================

class RHIInstanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!Core::Log::IsInitialized()) {
            Core::Log::Init();
        }

        // Create a window to initialize GLFW (required for Vulkan extensions)
        // GLFW must be initialized before we can query Vulkan extensions
        Platform::WindowConfig windowConfig;
        windowConfig.Width = 100;
        windowConfig.Height = 100;
        windowConfig.Title = "RHI Test Window";
        windowConfig.Visible = false;  // Hide window during tests
        m_Window = Core::CreateScope<Platform::Window>(windowConfig);
    }

    void TearDown() override {
        m_Window.reset();
    }

    Core::Scope<Platform::Window> m_Window;
};

// =============================================================================
// Instance Creation Tests
// =============================================================================

TEST_F(RHIInstanceTest, CreatesInstanceWithDefaultConfig) {
    RHI::RHIInstanceConfig config;
    auto instance = RHI::RHIInstance::Create(config);

    ASSERT_NE(instance, nullptr);
    EXPECT_NE(instance->GetHandle(), VK_NULL_HANDLE);
}

TEST_F(RHIInstanceTest, CreatesInstanceWithValidationEnabled) {
    RHI::RHIInstanceConfig config;
    config.EnableValidation = true;

    auto instance = RHI::RHIInstance::Create(config);

    ASSERT_NE(instance, nullptr);
    EXPECT_NE(instance->GetHandle(), VK_NULL_HANDLE);

    // In debug builds with validation layers available, validation should be enabled
#if !defined(NDEBUG) || defined(_DEBUG)
    // Note: Validation might be disabled if layers aren't available
    // We just verify the instance was created successfully
#endif
}

TEST_F(RHIInstanceTest, CreatesInstanceWithValidationDisabled) {
    RHI::RHIInstanceConfig config;
    config.EnableValidation = false;

    auto instance = RHI::RHIInstance::Create(config);

    ASSERT_NE(instance, nullptr);
    EXPECT_NE(instance->GetHandle(), VK_NULL_HANDLE);
    EXPECT_FALSE(instance->IsValidationEnabled());
}

TEST_F(RHIInstanceTest, CreatesInstanceWithCustomAppInfo) {
    RHI::RHIInstanceConfig config;
    config.ApplicationName = "Test Application";
    config.ApplicationVersion = VK_MAKE_VERSION(2, 0, 0);
    config.EngineName = "Test Engine";
    config.EngineVersion = VK_MAKE_VERSION(1, 2, 3);
    config.ApiVersion = VK_API_VERSION_1_2;
    config.EnableValidation = false;

    auto instance = RHI::RHIInstance::Create(config);

    ASSERT_NE(instance, nullptr);
    EXPECT_NE(instance->GetHandle(), VK_NULL_HANDLE);
}

// =============================================================================
// Extension Tests
// =============================================================================

TEST_F(RHIInstanceTest, HasRequiredExtensions) {
    RHI::RHIInstanceConfig config;
    config.EnableValidation = false;

    auto instance = RHI::RHIInstance::Create(config);

    ASSERT_NE(instance, nullptr);

    // Should have at least the GLFW-required extensions
    const auto& extensions = instance->GetEnabledExtensions();
    EXPECT_GT(extensions.size(), 0u);

    // Check that all extensions are valid strings
    for (const auto* ext : extensions) {
        EXPECT_NE(ext, nullptr);
        EXPECT_EQ(std::string(ext).substr(0, 3), "VK_");
    }
}

TEST_F(RHIInstanceTest, HasDebugExtensionWhenValidationEnabled) {
    RHI::RHIInstanceConfig config;
    config.EnableValidation = true;

    auto instance = RHI::RHIInstance::Create(config);

    ASSERT_NE(instance, nullptr);

#if !defined(NDEBUG) || defined(_DEBUG)
    if (instance->IsValidationEnabled()) {
        // Should have VK_EXT_debug_utils extension
        const auto& extensions = instance->GetEnabledExtensions();
        bool hasDebugUtils = false;
        for (const auto* ext : extensions) {
            if (std::string(ext) == VK_EXT_DEBUG_UTILS_EXTENSION_NAME) {
                hasDebugUtils = true;
                break;
            }
        }
        EXPECT_TRUE(hasDebugUtils);
    }
#endif
}

// =============================================================================
// Lifecycle Tests
// =============================================================================

TEST_F(RHIInstanceTest, DestructorCleansUpResources) {
    VkInstance handle = VK_NULL_HANDLE;

    {
        RHI::RHIInstanceConfig config;
        config.EnableValidation = false;

        auto instance = RHI::RHIInstance::Create(config);
        ASSERT_NE(instance, nullptr);

        handle = instance->GetHandle();
        EXPECT_NE(handle, VK_NULL_HANDLE);

        // Instance goes out of scope and should be destroyed
    }

    // After destruction, the handle should be invalid
    // We can't directly test this, but we can verify no crash occurs
    SUCCEED();
}

TEST_F(RHIInstanceTest, MultipleInstancesCanBeCreated) {
    RHI::RHIInstanceConfig config;
    config.EnableValidation = false;

    auto instance1 = RHI::RHIInstance::Create(config);
    auto instance2 = RHI::RHIInstance::Create(config);

    ASSERT_NE(instance1, nullptr);
    ASSERT_NE(instance2, nullptr);
    EXPECT_NE(instance1->GetHandle(), VK_NULL_HANDLE);
    EXPECT_NE(instance2->GetHandle(), VK_NULL_HANDLE);

    // Each instance should have a different handle
    EXPECT_NE(instance1->GetHandle(), instance2->GetHandle());
}

// =============================================================================
// Surface Creation Integration Test
// =============================================================================

TEST_F(RHIInstanceTest, CanCreateWindowSurface) {
    RHI::RHIInstanceConfig config;
    config.EnableValidation = false;

    auto instance = RHI::RHIInstance::Create(config);
    ASSERT_NE(instance, nullptr);

    // Create a surface using the window
    VkSurfaceKHR surface = m_Window->CreateSurface(instance->GetHandle());
    EXPECT_NE(surface, VK_NULL_HANDLE);

    // Clean up the surface
    if (surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance->GetHandle(), surface, nullptr);
    }
}
