cmake_minimum_required(VERSION 3.20)
project(VulkanRenderer VERSION 0.1.0 LANGUAGES CXX)

# =============================================================================
# C++ Standard
# =============================================================================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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

# Core library (header-only for now)
add_library(Core INTERFACE)
target_include_directories(Core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# =============================================================================
# Tests
# =============================================================================
if(BUILD_TESTS)
    enable_testing()

    # Assert test
    add_executable(TestAssert tests/TestAssert.cpp)
    target_link_libraries(TestAssert PRIVATE Core)

    add_test(NAME TestAssert COMMAND TestAssert)
endif()

# =============================================================================
# Installation (placeholder for future)
# =============================================================================
# install(...)
