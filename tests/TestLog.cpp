/**
 * @file TestLog.cpp
 * @brief Test file for Log.h logging system.
 *
 * This file tests that the logging system initializes correctly
 * and all log levels work as expected.
 */

#include "Core/Log.h"
#include "Core/Assert.h"
#include <iostream>
#include <string>

void TestLogInitialization() {
    std::cout << "Testing log initialization..." << std::endl;

    // Logger should not be initialized yet
    ASSERT(!Core::Log::IsInitialized());

    // Initialize the logger
    Core::Log::Init();

    // Logger should now be initialized
    ASSERT(Core::Log::IsInitialized());
    ASSERT(Core::Log::GetLogger() != nullptr);

    std::cout << "  Log initialization OK" << std::endl;
}

void TestAllLogLevels() {
    std::cout << "Testing all log levels..." << std::endl;

    // Test each log level - these should all work without crashing
    LOG_TRACE("This is a TRACE message");
    LOG_DEBUG("This is a DEBUG message");
    LOG_INFO("This is an INFO message");
    LOG_WARN("This is a WARN message");
    LOG_ERROR("This is an ERROR message");
    // Note: LOG_FATAL is not tested as it aborts the program

    std::cout << "  All log levels OK" << std::endl;
}

void TestFormatStrings() {
    std::cout << "Testing format strings..." << std::endl;

    // Test various format string patterns
    int intValue = 42;
    float floatValue = 3.14f;
    std::string strValue = "test";
    const char* cstrValue = "c-string";

    LOG_INFO("Integer value: {}", intValue);
    LOG_INFO("Float value: {:.2f}", floatValue);
    LOG_INFO("String value: {}", strValue);
    LOG_INFO("C-string value: {}", cstrValue);
    LOG_INFO("Multiple values: int={}, float={:.1f}, str={}", intValue, floatValue, strValue);

    std::cout << "  Format strings OK" << std::endl;
}

void TestLogShutdown() {
    std::cout << "Testing log shutdown..." << std::endl;

    // Shutdown the logger
    Core::Log::Shutdown();

    // Logger should no longer be initialized
    ASSERT(!Core::Log::IsInitialized());

    // Using macros after shutdown should be safe (no crash)
    LOG_INFO("This should not crash even after shutdown");

    std::cout << "  Log shutdown OK" << std::endl;
}

void TestReinitialize() {
    std::cout << "Testing log re-initialization..." << std::endl;

    // Re-initialize the logger
    Core::Log::Init();
    ASSERT(Core::Log::IsInitialized());

    LOG_INFO("Logger re-initialized successfully");

    std::cout << "  Log re-initialization OK" << std::endl;
}

void TestDebugBuildBehavior() {
    std::cout << "Testing debug/release build behavior..." << std::endl;

#if RENDERER_LOG_DEBUG_ENABLED
    std::cout << "  RENDERER_LOG_DEBUG_ENABLED = 1 (Debug build)" << std::endl;
    std::cout << "  TRACE and DEBUG logs are enabled" << std::endl;
#else
    std::cout << "  RENDERER_LOG_DEBUG_ENABLED = 0 (Release build)" << std::endl;
    std::cout << "  TRACE and DEBUG logs are disabled" << std::endl;
#endif

    std::cout << "  Debug build behavior OK" << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Log.h Test Suite" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    TestLogInitialization();
    TestAllLogLevels();
    TestFormatStrings();
    TestLogShutdown();
    TestReinitialize();
    TestDebugBuildBehavior();

    // Final cleanup
    Core::Log::Shutdown();

    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "All tests passed!" << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}
