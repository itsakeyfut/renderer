/**
 * @file TestInput.cpp
 * @brief Test file for Platform/Input.h using GoogleTest.
 *
 * This file tests the Input class for keyboard and mouse input handling.
 * Note: Actual input events cannot be simulated in unit tests, so these
 * tests focus on initialization, state queries, and method signatures.
 */

#include <gtest/gtest.h>
#include "Platform/Input.h"
#include "Platform/Window.h"
#include "Core/Log.h"

// =============================================================================
// Test Fixture for Input Tests
// =============================================================================

class InputTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!Core::Log::IsInitialized()) {
            Core::Log::Init();
        }
    }

    void TearDown() override {
        // Ensure Input is shutdown after each test
        Platform::Input::Shutdown();
    }
};

// =============================================================================
// Initialization Tests
// =============================================================================

TEST_F(InputTest, InitializesWithWindow) {
    Platform::WindowConfig config;
    config.Visible = false;  // Hide window during tests
    Platform::Window window(config);

    EXPECT_NO_THROW({
        Platform::Input::Init(window);
    });
}

TEST_F(InputTest, ShutdownWithoutInitDoesNotCrash) {
    EXPECT_NO_THROW({
        Platform::Input::Shutdown();
    });
}

TEST_F(InputTest, CanReinitializeAfterShutdown) {
    Platform::WindowConfig config;
    config.Visible = false;  // Hide window during tests
    Platform::Window window(config);

    Platform::Input::Init(window);
    Platform::Input::Shutdown();

    EXPECT_NO_THROW({
        Platform::Input::Init(window);
    });
}

// =============================================================================
// Keyboard Input Tests
// =============================================================================

TEST_F(InputTest, IsKeyDownReturnsFalseBeforeInit) {
    // Without initialization, all keys should return false
    EXPECT_FALSE(Platform::Input::IsKeyDown(Platform::KeyCode::W));
    EXPECT_FALSE(Platform::Input::IsKeyDown(Platform::KeyCode::Space));
    EXPECT_FALSE(Platform::Input::IsKeyDown(Platform::KeyCode::Escape));
}

TEST_F(InputTest, IsKeyDownReturnsFalseAfterInit) {
    Platform::WindowConfig config;
    config.Visible = false;  // Hide window during tests
    Platform::Window window(config);
    Platform::Input::Init(window);

    // No keys pressed yet
    EXPECT_FALSE(Platform::Input::IsKeyDown(Platform::KeyCode::W));
    EXPECT_FALSE(Platform::Input::IsKeyDown(Platform::KeyCode::A));
    EXPECT_FALSE(Platform::Input::IsKeyDown(Platform::KeyCode::S));
    EXPECT_FALSE(Platform::Input::IsKeyDown(Platform::KeyCode::D));
}

TEST_F(InputTest, IsKeyPressedReturnsFalseInitially) {
    Platform::WindowConfig config;
    config.Visible = false;  // Hide window during tests
    Platform::Window window(config);
    Platform::Input::Init(window);

    EXPECT_FALSE(Platform::Input::IsKeyPressed(Platform::KeyCode::Space));
    EXPECT_FALSE(Platform::Input::IsKeyPressed(Platform::KeyCode::Enter));
}

TEST_F(InputTest, IsKeyReleasedReturnsFalseInitially) {
    Platform::WindowConfig config;
    config.Visible = false;  // Hide window during tests
    Platform::Window window(config);
    Platform::Input::Init(window);

    EXPECT_FALSE(Platform::Input::IsKeyReleased(Platform::KeyCode::Space));
    EXPECT_FALSE(Platform::Input::IsKeyReleased(Platform::KeyCode::Escape));
}

// =============================================================================
// Mouse Button Tests
// =============================================================================

TEST_F(InputTest, IsMouseButtonDownReturnsFalseInitially) {
    Platform::WindowConfig config;
    config.Visible = false;  // Hide window during tests
    Platform::Window window(config);
    Platform::Input::Init(window);

    EXPECT_FALSE(Platform::Input::IsMouseButtonDown(Platform::MouseButton::Left));
    EXPECT_FALSE(Platform::Input::IsMouseButtonDown(Platform::MouseButton::Right));
    EXPECT_FALSE(Platform::Input::IsMouseButtonDown(Platform::MouseButton::Middle));
}

TEST_F(InputTest, IsMouseButtonPressedReturnsFalseInitially) {
    Platform::WindowConfig config;
    config.Visible = false;  // Hide window during tests
    Platform::Window window(config);
    Platform::Input::Init(window);

    EXPECT_FALSE(Platform::Input::IsMouseButtonPressed(Platform::MouseButton::Left));
    EXPECT_FALSE(Platform::Input::IsMouseButtonPressed(Platform::MouseButton::Right));
}

TEST_F(InputTest, IsMouseButtonReleasedReturnsFalseInitially) {
    Platform::WindowConfig config;
    config.Visible = false;  // Hide window during tests
    Platform::Window window(config);
    Platform::Input::Init(window);

    EXPECT_FALSE(Platform::Input::IsMouseButtonReleased(Platform::MouseButton::Left));
    EXPECT_FALSE(Platform::Input::IsMouseButtonReleased(Platform::MouseButton::Right));
}

// =============================================================================
// Mouse Position Tests
// =============================================================================

TEST_F(InputTest, GetMousePositionReturnsValidValues) {
    Platform::WindowConfig config;
    config.Visible = false;  // Hide window during tests
    Platform::Window window(config);
    Platform::Input::Init(window);

    // Mouse position should be within reasonable bounds
    // (could be outside window if cursor is outside)
    double mouseX = Platform::Input::GetMouseX();
    double mouseY = Platform::Input::GetMouseY();

    // Just verify no crash and reasonable values (not NaN)
    EXPECT_FALSE(std::isnan(mouseX));
    EXPECT_FALSE(std::isnan(mouseY));
}

TEST_F(InputTest, GetMouseDeltaReturnsZeroInitially) {
    Platform::WindowConfig config;
    config.Visible = false;  // Hide window during tests
    Platform::Window window(config);
    Platform::Input::Init(window);

    // After init but before first Update, delta should be zero
    EXPECT_DOUBLE_EQ(Platform::Input::GetMouseDeltaX(), 0.0);
    EXPECT_DOUBLE_EQ(Platform::Input::GetMouseDeltaY(), 0.0);
}

TEST_F(InputTest, GetScrollDeltaReturnsZeroInitially) {
    Platform::WindowConfig config;
    config.Visible = false;  // Hide window during tests
    Platform::Window window(config);
    Platform::Input::Init(window);

    EXPECT_DOUBLE_EQ(Platform::Input::GetScrollDelta(), 0.0);
}

// =============================================================================
// Update Tests
// =============================================================================

TEST_F(InputTest, UpdateDoesNotCrashBeforeInit) {
    EXPECT_NO_THROW({
        Platform::Input::Update();
    });
}

TEST_F(InputTest, UpdateDoesNotCrashAfterInit) {
    Platform::WindowConfig config;
    config.Visible = false;  // Hide window during tests
    Platform::Window window(config);
    Platform::Input::Init(window);

    EXPECT_NO_THROW({
        for (int i = 0; i < 10; ++i) {
            Platform::Input::Update();
        }
    });
}

TEST_F(InputTest, UpdateResetsScrollDelta) {
    Platform::WindowConfig config;
    config.Visible = false;  // Hide window during tests
    Platform::Window window(config);
    Platform::Input::Init(window);

    // Update should reset scroll delta
    Platform::Input::Update();
    EXPECT_DOUBLE_EQ(Platform::Input::GetScrollDelta(), 0.0);
}

// =============================================================================
// Cursor Mode Tests
// =============================================================================

TEST_F(InputTest, GetCursorModeReturnsNormalByDefault) {
    Platform::WindowConfig config;
    config.Visible = false;  // Hide window during tests
    Platform::Window window(config);
    Platform::Input::Init(window);

    EXPECT_EQ(Platform::Input::GetCursorMode(), Platform::CursorMode::Normal);
}

TEST_F(InputTest, SetCursorModeDoesNotCrash) {
    Platform::WindowConfig config;
    config.Visible = false;  // Hide window during tests
    Platform::Window window(config);
    Platform::Input::Init(window);

    EXPECT_NO_THROW({
        Platform::Input::SetCursorMode(Platform::CursorMode::Normal);
        Platform::Input::SetCursorMode(Platform::CursorMode::Hidden);
        Platform::Input::SetCursorMode(Platform::CursorMode::Disabled);
        Platform::Input::SetCursorMode(Platform::CursorMode::Normal);
    });
}

TEST_F(InputTest, SetCursorModeUpdatesState) {
    Platform::WindowConfig config;
    config.Visible = false;  // Hide window during tests
    Platform::Window window(config);
    Platform::Input::Init(window);

    Platform::Input::SetCursorMode(Platform::CursorMode::Hidden);
    EXPECT_EQ(Platform::Input::GetCursorMode(), Platform::CursorMode::Hidden);

    Platform::Input::SetCursorMode(Platform::CursorMode::Disabled);
    EXPECT_EQ(Platform::Input::GetCursorMode(), Platform::CursorMode::Disabled);

    Platform::Input::SetCursorMode(Platform::CursorMode::Normal);
    EXPECT_EQ(Platform::Input::GetCursorMode(), Platform::CursorMode::Normal);
}

// =============================================================================
// KeyCode Enum Tests
// =============================================================================

TEST_F(InputTest, KeyCodeValuesMatchGLFW) {
    // Verify key code values match expected GLFW values
    EXPECT_EQ(static_cast<int>(Platform::KeyCode::Space), 32);
    EXPECT_EQ(static_cast<int>(Platform::KeyCode::W), 87);
    EXPECT_EQ(static_cast<int>(Platform::KeyCode::A), 65);
    EXPECT_EQ(static_cast<int>(Platform::KeyCode::S), 83);
    EXPECT_EQ(static_cast<int>(Platform::KeyCode::D), 68);
    EXPECT_EQ(static_cast<int>(Platform::KeyCode::Escape), 256);
    EXPECT_EQ(static_cast<int>(Platform::KeyCode::Enter), 257);
}

// =============================================================================
// MouseButton Enum Tests
// =============================================================================

TEST_F(InputTest, MouseButtonValuesMatchGLFW) {
    // Verify mouse button values match expected GLFW values
    EXPECT_EQ(static_cast<int>(Platform::MouseButton::Left), 0);
    EXPECT_EQ(static_cast<int>(Platform::MouseButton::Right), 1);
    EXPECT_EQ(static_cast<int>(Platform::MouseButton::Middle), 2);
}

// =============================================================================
// Edge Case Tests
// =============================================================================

TEST_F(InputTest, HandlesInvalidKeyCodeGracefully) {
    Platform::WindowConfig config;
    config.Visible = false;  // Hide window during tests
    Platform::Window window(config);
    Platform::Input::Init(window);

    // Casting an invalid key code should not crash
    auto invalidKey = static_cast<Platform::KeyCode>(9999);
    EXPECT_FALSE(Platform::Input::IsKeyDown(invalidKey));
    EXPECT_FALSE(Platform::Input::IsKeyPressed(invalidKey));
    EXPECT_FALSE(Platform::Input::IsKeyReleased(invalidKey));
}

TEST_F(InputTest, HandlesInvalidMouseButtonGracefully) {
    Platform::WindowConfig config;
    config.Visible = false;  // Hide window during tests
    Platform::Window window(config);
    Platform::Input::Init(window);

    // Casting an invalid mouse button should not crash
    auto invalidButton = static_cast<Platform::MouseButton>(99);
    EXPECT_FALSE(Platform::Input::IsMouseButtonDown(invalidButton));
    EXPECT_FALSE(Platform::Input::IsMouseButtonPressed(invalidButton));
    EXPECT_FALSE(Platform::Input::IsMouseButtonReleased(invalidButton));
}
