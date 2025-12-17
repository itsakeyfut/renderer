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

# nlohmann/json - JSON for Modern C++
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.12.0
)
FetchContent_MakeAvailable(nlohmann_json)

# GLM - OpenGL Mathematics
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.2
)
FetchContent_MakeAvailable(glm)

# GLFW - Window and input library
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
)
FetchContent_MakeAvailable(glfw)

# VMA - Vulkan Memory Allocator
FetchContent_Declare(
    vma
    GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
    GIT_TAG        v3.3.0
)
FetchContent_MakeAvailable(vma)

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
# Vulkan SDK
# =============================================================================
# Try to find Vulkan SDK - check common Windows installation paths if not found
if(NOT DEFINED ENV{VULKAN_SDK})
    # Check common Windows paths for Vulkan SDK
    file(GLOB VULKAN_SDK_CANDIDATES "C:/VulkanSDK/*")
    if(VULKAN_SDK_CANDIDATES)
        list(SORT VULKAN_SDK_CANDIDATES ORDER DESCENDING)
        list(GET VULKAN_SDK_CANDIDATES 0 VULKAN_SDK_PATH)
        set(ENV{VULKAN_SDK} "${VULKAN_SDK_PATH}")
        message(STATUS "Auto-detected Vulkan SDK: ${VULKAN_SDK_PATH}")
    endif()
endif()

find_package(Vulkan QUIET)

if(Vulkan_FOUND)
    message(STATUS "Vulkan SDK found: ${Vulkan_INCLUDE_DIRS}")
    set(VULKAN_TARGET Vulkan::Vulkan)
    set(VULKAN_HEADERS_TARGET Vulkan::Headers)
else()
    message(STATUS "Vulkan SDK not found, using FetchContent for Vulkan headers")
    message(STATUS "Note: For full Vulkan development, install the SDK from https://vulkan.lunarg.com/")

    # Fetch Vulkan headers
    FetchContent_Declare(
        vulkan-headers
        GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers.git
        GIT_TAG        v1.4.306
    )
    FetchContent_MakeAvailable(vulkan-headers)

    # Create an interface library for Vulkan headers
    add_library(VulkanHeaders INTERFACE)
    target_include_directories(VulkanHeaders INTERFACE ${vulkan-headers_SOURCE_DIR}/include)

    # For now, use just the headers with GLFW's built-in Vulkan loading
    # volk will be added in RHI layer when we need direct Vulkan calls
    set(VULKAN_TARGET VulkanHeaders)
    set(VULKAN_HEADERS_TARGET VulkanHeaders)
endif()

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
    src/Core/Config.cpp
    src/Core/Log.cpp
    src/Core/Timer.cpp
)

set(CORE_HEADERS
    src/Core/Assert.h
    src/Core/Config.h
    src/Core/Log.h
    src/Core/Timer.h
    src/Core/Types.h
)

add_library(Core STATIC ${CORE_SOURCES} ${CORE_HEADERS})
target_include_directories(Core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(Core PUBLIC spdlog::spdlog nlohmann_json::nlohmann_json)

# Platform library
set(PLATFORM_SOURCES
    src/Platform/Input.cpp
    src/Platform/Window.cpp
)

set(PLATFORM_HEADERS
    src/Platform/Input.h
    src/Platform/Window.h
)

add_library(Platform STATIC ${PLATFORM_SOURCES} ${PLATFORM_HEADERS})
target_include_directories(Platform PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(Platform PUBLIC Core glfw ${VULKAN_TARGET})

# RHI library
set(RHI_SOURCES
    src/RHI/RHIDevice.cpp
    src/RHI/RHIInstance.cpp
    src/RHI/RHIPhysicalDevice.cpp
)

set(RHI_HEADERS
    src/RHI/RHIDevice.h
    src/RHI/RHIInstance.h
    src/RHI/RHIPhysicalDevice.h
)

add_library(RHI STATIC ${RHI_SOURCES} ${RHI_HEADERS})
target_include_directories(RHI PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(RHI PUBLIC Core Platform ${VULKAN_TARGET} GPUOpen::VulkanMemoryAllocator)

# =============================================================================
# Main Application
# =============================================================================
add_executable(VulkanRenderer src/main.cpp)
target_link_libraries(VulkanRenderer PRIVATE RHI Platform Core glm::glm GPUOpen::VulkanMemoryAllocator)

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

    # Timer test
    add_executable(TestTimer tests/TestTimer.cpp)
    target_link_libraries(TestTimer PRIVATE Core GTest::gtest_main)
    gtest_discover_tests(TestTimer)

    # Config test
    add_executable(TestConfig tests/TestConfig.cpp)
    target_link_libraries(TestConfig PRIVATE Core GTest::gtest_main)
    gtest_discover_tests(TestConfig)

    # Window test
    add_executable(TestWindow tests/TestWindow.cpp)
    target_link_libraries(TestWindow PRIVATE Platform Core GTest::gtest_main)
    gtest_discover_tests(TestWindow)

    # Input test
    add_executable(TestInput tests/TestInput.cpp)
    target_link_libraries(TestInput PRIVATE Platform Core GTest::gtest_main)
    gtest_discover_tests(TestInput)

    # RHI Instance test
    add_executable(TestRHIInstance tests/TestRHIInstance.cpp)
    target_link_libraries(TestRHIInstance PRIVATE RHI Platform Core GTest::gtest_main)
    gtest_discover_tests(TestRHIInstance)

    # RHI Physical Device test
    add_executable(TestRHIPhysicalDevice tests/TestRHIPhysicalDevice.cpp)
    target_link_libraries(TestRHIPhysicalDevice PRIVATE RHI Platform Core GTest::gtest_main)
    gtest_discover_tests(TestRHIPhysicalDevice)

    # RHI Device test
    add_executable(TestRHIDevice tests/TestRHIDevice.cpp)
    target_link_libraries(TestRHIDevice PRIVATE RHI Platform Core GTest::gtest_main)
    gtest_discover_tests(TestRHIDevice)
endif()

# =============================================================================
# Installation (placeholder for future)
# =============================================================================
# install(...)
