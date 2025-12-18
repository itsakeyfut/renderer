/**
 * @file TestWindow.cpp
 * @brief Test file for Platform/Window.h using GoogleTest.
 *
 * This file tests that the Window class correctly manages GLFW windows
 * and provides Vulkan integration functionality.
 */

#include <gtest/gtest.h>
#include "Platform/Window.h"
#include "Core/Log.h"

// =============================================================================
// Test Fixture for Window Tests
// =============================================================================

class WindowTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!Core::Log::IsInitialized()) {
            Core::Log::Init();
        }
    }

    void TearDown() override {
        // Logging cleanup is handled by the test suite
    }
};

// =============================================================================
// Window Creation Tests
// =============================================================================

TEST_F(WindowTest, CreatesWindowWithDefaultConfig) {
    Platform::WindowConfig config;
    config.Visible = false;  // Hide window during tests
    Platform::Window window(config);

    EXPECT_NE(window.GetHandle(), nullptr);
    EXPECT_EQ(window.GetWidth(), config.Width);
    EXPECT_EQ(window.GetHeight(), config.Height);
}

TEST_F(WindowTest, CreatesWindowWithCustomDimensions) {
    Platform::WindowConfig config;
    config.Width = 800;
    config.Height = 600;
    config.Title = "Test Window";
    config.Visible = false;  // Hide window during tests

    Platform::Window window(config);

    EXPECT_NE(window.GetHandle(), nullptr);
    EXPECT_EQ(window.GetWidth(), 800);
    EXPECT_EQ(window.GetHeight(), 600);
}

TEST_F(WindowTest, ShouldCloseReturnsFalseInitially) {
    Platform::WindowConfig config;
    config.Visible = false;  // Hide window during tests
    Platform::Window window(config);

    EXPECT_FALSE(window.ShouldClose());
}

TEST_F(WindowTest, GetFramebufferSizeReturnsValidDimensions) {
    Platform::WindowConfig config;
    config.Width = 640;
    config.Height = 480;
    config.Visible = false;  // Hide window during tests

    Platform::Window window(config);

    uint32_t fbWidth = 0;
    uint32_t fbHeight = 0;
    window.GetFramebufferSize(fbWidth, fbHeight);

    // Framebuffer size should be non-zero for a valid window
    // Note: On HiDPI displays, this may differ from window size
    EXPECT_GT(fbWidth, 0u);
    EXPECT_GT(fbHeight, 0u);
}

TEST_F(WindowTest, WasResizedReturnsFalseInitially) {
    Platform::WindowConfig config;
    config.Visible = false;  // Hide window during tests
    Platform::Window window(config);

    EXPECT_FALSE(window.WasResized());
}

TEST_F(WindowTest, ResetResizedFlagWorks) {
    Platform::WindowConfig config;
    config.Visible = false;  // Hide window during tests
    Platform::Window window(config);

    // Initially not resized
    EXPECT_FALSE(window.WasResized());

    window.ResetResizedFlag();

    EXPECT_FALSE(window.WasResized());
}

// =============================================================================
// Move Semantics Tests
// =============================================================================

TEST_F(WindowTest, MoveConstructorTransfersOwnership) {
    Platform::WindowConfig config;
    config.Width = 320;
    config.Height = 240;
    config.Visible = false;  // Hide window during tests

    Platform::Window original(config);
    GLFWwindow* originalHandle = original.GetHandle();
    EXPECT_NE(originalHandle, nullptr);

    Platform::Window moved(std::move(original));

    // Moved-from window should have null handle
    EXPECT_EQ(original.GetHandle(), nullptr);

    // New window should have the original handle
    EXPECT_EQ(moved.GetHandle(), originalHandle);
    EXPECT_EQ(moved.GetWidth(), 320);
    EXPECT_EQ(moved.GetHeight(), 240);
}

TEST_F(WindowTest, MoveAssignmentTransfersOwnership) {
    Platform::WindowConfig config1;
    config1.Width = 400;
    config1.Height = 300;
    config1.Visible = false;  // Hide window during tests

    Platform::WindowConfig config2;
    config2.Width = 200;
    config2.Height = 150;
    config2.Visible = false;  // Hide window during tests

    Platform::Window window1(config1);
    Platform::Window window2(config2);

    GLFWwindow* handle1 = window1.GetHandle();

    window2 = std::move(window1);

    // window1 should have null handle
    EXPECT_EQ(window1.GetHandle(), nullptr);

    // window2 should have window1's original handle and properties
    EXPECT_EQ(window2.GetHandle(), handle1);
    EXPECT_EQ(window2.GetWidth(), 400);
    EXPECT_EQ(window2.GetHeight(), 300);
}

// =============================================================================
// Vulkan Extension Tests
// =============================================================================

TEST_F(WindowTest, GetRequiredVulkanExtensionsReturnsNonNull) {
    // GLFW must be initialized before calling GetRequiredVulkanExtensions
    // Creating a window initializes GLFW
    Platform::WindowConfig config;
    config.Visible = false;  // Hide window during tests
    Platform::Window window(config);

    uint32_t count = 0;
    const char** extensions = Platform::Window::GetRequiredVulkanExtensions(count);

    // Should return at least one extension (VK_KHR_surface typically)
    EXPECT_GT(count, 0u);
    EXPECT_NE(extensions, nullptr);
}

TEST_F(WindowTest, VulkanExtensionsAreValidStrings) {
    // GLFW must be initialized before calling GetRequiredVulkanExtensions
    Platform::WindowConfig config;
    config.Visible = false;  // Hide window during tests
    Platform::Window window(config);

    uint32_t count = 0;
    const char** extensions = Platform::Window::GetRequiredVulkanExtensions(count);

    for (uint32_t i = 0; i < count; ++i) {
        EXPECT_NE(extensions[i], nullptr);
        // Each extension name should start with "VK_"
        EXPECT_EQ(std::string(extensions[i]).substr(0, 3), "VK_");
    }
}

// =============================================================================
// Window Title Tests
// =============================================================================

TEST_F(WindowTest, SetTitleDoesNotCrash) {
    Platform::WindowConfig config;
    config.Visible = false;  // Hide window during tests
    Platform::Window window(config);

    // Setting title should not crash
    EXPECT_NO_THROW({
        window.SetTitle("New Title");
        window.SetTitle("");
        window.SetTitle("Another Title With Special Chars: !@#$%");
    });
}

// =============================================================================
// Resize Callback Tests
// =============================================================================

TEST_F(WindowTest, ResizeCallbackCanBeSet) {
    Platform::WindowConfig config;
    config.Visible = false;  // Hide window during tests
    Platform::Window window(config);

    bool callbackInvoked = false;
    window.SetResizeCallback([&callbackInvoked](uint32_t, uint32_t) {
        callbackInvoked = true;
    });

    // Just verify setting the callback doesn't crash
    // Actually triggering the callback would require window resize events
    EXPECT_FALSE(callbackInvoked); // Not invoked yet, just set
}

// =============================================================================
// PollEvents Tests
// =============================================================================

TEST_F(WindowTest, PollEventsDoesNotCrash) {
    Platform::WindowConfig config;
    config.Visible = false;  // Hide window during tests
    Platform::Window window(config);

    // Polling events should not crash
    EXPECT_NO_THROW({
        for (int i = 0; i < 10; ++i) {
            window.PollEvents();
        }
    });
}

// =============================================================================
// CreateSurface Tests
// Note: Full CreateSurface testing requires a valid VkInstance, which requires
// the RHI layer to be implemented. Integration tests will be added when
// RHIInstance is implemented.
//
// The CreateSurface method cannot be unit tested without a valid VkInstance
// because GLFW's glfwCreateWindowSurface asserts on VK_NULL_HANDLE.
// =============================================================================
