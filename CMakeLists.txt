cmake_minimum_required(VERSION 3.20)
project(VulkanRenderer VERSION 0.1.0 LANGUAGES CXX)

# =============================================================================
# C++ Standard
# =============================================================================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# =============================================================================
# Dependencies via FetchContent
# =============================================================================
include(FetchContent)

# spdlog - Fast C++ logging library
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.16.0
)
FetchContent_MakeAvailable(spdlog)

# GoogleTest - C++ Testing Framework
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.17.0
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# =============================================================================
# Build Options
# =============================================================================
option(BUILD_TESTS "Build test executables" ON)
option(ENABLE_VALIDATION "Enable Vulkan validation layers in debug builds" ON)

# =============================================================================
# Compiler Warnings
# =============================================================================
if(MSVC)
    add_compile_options(/W4 /WX)
    # Disable specific warnings if needed
    # add_compile_options(/wd4100)  # unreferenced formal parameter
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# =============================================================================
# Debug/Release Configuration
# =============================================================================
if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "")
    add_compile_definitions(_DEBUG)
    message(STATUS "Build type: Debug")
else()
    add_compile_definitions(NDEBUG)
    message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
endif()

# =============================================================================
# Source Files
# =============================================================================

# Core library
set(CORE_SOURCES
    src/Core/Log.cpp
)

set(CORE_HEADERS
    src/Core/Assert.h
    src/Core/Log.h
    src/Core/Types.h
)

add_library(Core STATIC ${CORE_SOURCES} ${CORE_HEADERS})
target_include_directories(Core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(Core PUBLIC spdlog::spdlog)

# =============================================================================
# Tests
# =============================================================================
if(BUILD_TESTS)
    enable_testing()
    include(GoogleTest)

    # Assert test
    add_executable(TestAssert tests/TestAssert.cpp)
    target_link_libraries(TestAssert PRIVATE Core GTest::gtest_main)
    gtest_discover_tests(TestAssert)

    # Log test
    add_executable(TestLog tests/TestLog.cpp)
    target_link_libraries(TestLog PRIVATE Core GTest::gtest_main)
    gtest_discover_tests(TestLog)
endif()

# =============================================================================
# Installation (placeholder for future)
# =============================================================================
# install(...)
