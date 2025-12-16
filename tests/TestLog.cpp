/**
 * @file TestLog.cpp
 * @brief Test file for Log.h logging system using GoogleTest.
 *
 * This file tests that the logging system initializes correctly
 * and all log levels work as expected.
 */

#include <gtest/gtest.h>
#include "Core/Log.h"
#include "Core/Assert.h"
#include <string>

// =============================================================================
// Test Fixture for Log Tests
// =============================================================================

class LogTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Ensure logger is not initialized before each test
        if (Core::Log::IsInitialized()) {
            Core::Log::Shutdown();
        }
    }

    void TearDown() override {
        // Clean up after each test
        if (Core::Log::IsInitialized()) {
            Core::Log::Shutdown();
        }
    }
};

// =============================================================================
// Initialization Tests
// =============================================================================

TEST_F(LogTest, InitiallyNotInitialized) {
    EXPECT_FALSE(Core::Log::IsInitialized());
}

TEST_F(LogTest, InitializesSuccessfully) {
    Core::Log::Init();

    EXPECT_TRUE(Core::Log::IsInitialized());
    EXPECT_NE(Core::Log::GetLogger(), nullptr);
}

TEST_F(LogTest, ShutdownWorks) {
    Core::Log::Init();
    ASSERT_TRUE(Core::Log::IsInitialized());

    Core::Log::Shutdown();

    EXPECT_FALSE(Core::Log::IsInitialized());
}

TEST_F(LogTest, CanReinitialize) {
    Core::Log::Init();
    Core::Log::Shutdown();

    // Re-initialize
    Core::Log::Init();

    EXPECT_TRUE(Core::Log::IsInitialized());
    EXPECT_NE(Core::Log::GetLogger(), nullptr);
}

// =============================================================================
// Log Level Tests
// =============================================================================

TEST_F(LogTest, AllLogLevelsWork) {
    Core::Log::Init();

    // These should all execute without crashing
    // Note: We can't easily verify output in unit tests,
    // but we verify they don't throw or crash
    EXPECT_NO_THROW({
        LOG_TRACE("This is a TRACE message");
        LOG_DEBUG("This is a DEBUG message");
        LOG_INFO("This is an INFO message");
        LOG_WARN("This is a WARN message");
        LOG_ERROR("This is an ERROR message");
    });
}

TEST_F(LogTest, FormatStringsWork) {
    Core::Log::Init();

    int intValue = 42;
    float floatValue = 3.14f;
    std::string strValue = "test";
    const char* cstrValue = "c-string";

    EXPECT_NO_THROW({
        LOG_INFO("Integer value: {}", intValue);
        LOG_INFO("Float value: {:.2f}", floatValue);
        LOG_INFO("String value: {}", strValue);
        LOG_INFO("C-string value: {}", cstrValue);
        LOG_INFO("Multiple values: int={}, float={:.1f}, str={}", intValue, floatValue, strValue);
    });
}

// =============================================================================
// Safety Tests
// =============================================================================

TEST_F(LogTest, SafeToLogAfterShutdown) {
    Core::Log::Init();
    Core::Log::Shutdown();

    // Using macros after shutdown should be safe (no crash)
    EXPECT_NO_THROW({
        LOG_INFO("This should not crash even after shutdown");
    });
}

TEST_F(LogTest, SafeToLogBeforeInit) {
    // Logging before initialization should be safe
    EXPECT_NO_THROW({
        LOG_INFO("This should not crash before initialization");
    });
}

// =============================================================================
// Debug Build Behavior Tests
// =============================================================================

TEST(LogDebugTest, DebugLoggingMacroIsDefined) {
#if RENDERER_LOG_DEBUG_ENABLED
    // In debug builds, trace and debug logs are enabled
    EXPECT_EQ(RENDERER_LOG_DEBUG_ENABLED, 1);
#else
    // In release builds, trace and debug logs are disabled
    EXPECT_EQ(RENDERER_LOG_DEBUG_ENABLED, 0);
#endif
}
