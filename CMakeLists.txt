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

# tinygltf - glTF 2.0 loader
set(TINYGLTF_HEADER_ONLY ON CACHE INTERNAL "" FORCE)
set(TINYGLTF_INSTALL OFF CACHE INTERNAL "" FORCE)
FetchContent_Declare(
    tinygltf
    GIT_REPOSITORY https://github.com/syoyo/tinygltf.git
    GIT_TAG        v2.9.7
)
FetchContent_MakeAvailable(tinygltf)

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
# Dear ImGui - Immediate Mode GUI Library
# =============================================================================
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.92.5
)
FetchContent_MakeAvailable(imgui)

# Create ImGui library target (ImGui doesn't provide CMakeLists.txt)
add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)
target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui PUBLIC glfw ${VULKAN_TARGET})
# Disable warnings for ImGui (third-party code)
if(MSVC)
    target_compile_options(imgui PRIVATE /W0)
else()
    target_compile_options(imgui PRIVATE -w)
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
    src/Core/ThreadPool.cpp
    src/Core/Timer.cpp
)

set(CORE_HEADERS
    src/Core/Assert.h
    src/Core/Config.h
    src/Core/Event.h
    src/Core/EventDispatcher.h
    src/Core/Log.h
    src/Core/ThreadPool.h
    src/Core/Timer.h
    src/Core/Types.h
)

add_library(Core STATIC ${CORE_SOURCES} ${CORE_HEADERS})
target_include_directories(Core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(Core PUBLIC spdlog::spdlog nlohmann_json::nlohmann_json)

# Platform library
set(PLATFORM_SOURCES
    src/Platform/FileDialog.cpp
    src/Platform/Input.cpp
    src/Platform/Window.cpp
)

set(PLATFORM_HEADERS
    src/Platform/FileDialog.h
    src/Platform/Input.h
    src/Platform/Window.h
)

add_library(Platform STATIC ${PLATFORM_SOURCES} ${PLATFORM_HEADERS})
target_include_directories(Platform PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(Platform PUBLIC Core glfw ${VULKAN_TARGET})

# RHI library
set(RHI_SOURCES
    src/RHI/MipmapGenerator.cpp
    src/RHI/RHIBuffer.cpp
    src/RHI/RHICommandBuffer.cpp
    src/RHI/RHICommandPool.cpp
    src/RHI/RHIDeletionQueue.cpp
    src/RHI/RHIDescriptorPool.cpp
    src/RHI/RHIDescriptorSet.cpp
    src/RHI/RHIDescriptorSetLayout.cpp
    src/RHI/RHIDevice.cpp
    src/RHI/RHIFence.cpp
    src/RHI/RHIImage.cpp
    src/RHI/RHIInstance.cpp
    src/RHI/RHIPhysicalDevice.cpp
    src/RHI/RHIPipeline.cpp
    src/RHI/RHISampler.cpp
    src/RHI/RHISemaphore.cpp
    src/RHI/RHIShader.cpp
    src/RHI/RHISwapchain.cpp
    src/RHI/RHITexture.cpp
    src/RHI/stb_impl.cpp
)

set(RHI_HEADERS
    src/RHI/MipmapGenerator.h
    src/RHI/RHIBuffer.h
    src/RHI/RHICommandBuffer.h
    src/RHI/RHICommandPool.h
    src/RHI/RHIDeletionQueue.h
    src/RHI/RHIDescriptorPool.h
    src/RHI/RHIDescriptorSet.h
    src/RHI/RHIDescriptorSetLayout.h
    src/RHI/RHIDevice.h
    src/RHI/RHIFence.h
    src/RHI/RHIImage.h
    src/RHI/RHIInstance.h
    src/RHI/RHIPhysicalDevice.h
    src/RHI/RHIPipeline.h
    src/RHI/RHISampler.h
    src/RHI/RHISemaphore.h
    src/RHI/RHIShader.h
    src/RHI/RHISwapchain.h
    src/RHI/RHITexture.h
)

add_library(RHI STATIC ${RHI_SOURCES} ${RHI_HEADERS})
target_include_directories(RHI PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(RHI PRIVATE ${tinygltf_SOURCE_DIR})  # For stb_image.h
target_link_libraries(RHI PUBLIC Core Platform ${VULKAN_TARGET} GPUOpen::VulkanMemoryAllocator)

# Resources library
set(RESOURCES_SOURCES
    src/Resources/AsyncResourceLoader.cpp
    src/Resources/EnvironmentMap.cpp
    src/Resources/MaterialInstance.cpp
    src/Resources/ModelLoader.cpp
    src/Resources/ResourceManager.cpp
    src/Resources/TextureLoader.cpp
)

set(RESOURCES_HEADERS
    src/Resources/AsyncResourceLoader.h
    src/Resources/EnvironmentMap.h
    src/Resources/Material.h
    src/Resources/MaterialInstance.h
    src/Resources/Model.h
    src/Resources/ModelLoader.h
    src/Resources/ResourceHandle.h
    src/Resources/ResourceManager.h
    src/Resources/ResourcePool.h
    src/Resources/Texture.h
    src/Resources/TextureLoader.h
    src/Resources/UniformBufferObjects.h
    src/Resources/Vertex.h
)

add_library(Resources STATIC ${RESOURCES_SOURCES} ${RESOURCES_HEADERS})
target_include_directories(Resources PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(Resources PRIVATE ${tinygltf_SOURCE_DIR})
target_link_libraries(Resources PUBLIC Core RHI glm::glm ${VULKAN_HEADERS_TARGET})

# Scene library (header-only for now)
set(SCENE_HEADERS
    src/Scene/Camera.h
    src/Scene/Entity.h
    src/Scene/FPSCameraController.h
    src/Scene/Light.h
    src/Scene/OrbitCameraController.h
    src/Scene/Scene.h
    src/Scene/Transform.h
)

add_library(Scene INTERFACE)
target_sources(Scene INTERFACE ${SCENE_HEADERS})
target_include_directories(Scene INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(Scene INTERFACE Core Platform Resources glm::glm)

# Renderer library
set(RENDERER_SOURCES
    src/Renderer/CascadedShadowMap.cpp
    src/Renderer/DepthBuffer.cpp
    src/Renderer/FrameManager.cpp
    src/Renderer/LightManager.cpp
    src/Renderer/ShadowMap.cpp
    src/Renderer/SkyboxRenderer.cpp
    src/Renderer/Debug/ImGuiRenderer.cpp
)

set(RENDERER_HEADERS
    src/Renderer/CascadedShadowMap.h
    src/Renderer/DepthBuffer.h
    src/Renderer/FrameManager.h
    src/Renderer/LightManager.h
    src/Renderer/ShadowMap.h
    src/Renderer/SkyboxRenderer.h
    src/Renderer/Debug/ImGuiRenderer.h
)

add_library(Renderer STATIC ${RENDERER_SOURCES} ${RENDERER_HEADERS})
target_include_directories(Renderer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(Renderer PUBLIC RHI Scene imgui)

# =============================================================================
# Main Application
# =============================================================================
add_executable(VulkanRenderer src/main.cpp)
target_link_libraries(VulkanRenderer PRIVATE Renderer RHI Platform Core glm::glm GPUOpen::VulkanMemoryAllocator)

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

    # Event test
    add_executable(TestEvent tests/TestEvent.cpp)
    target_link_libraries(TestEvent PRIVATE Core GTest::gtest_main)
    gtest_discover_tests(TestEvent)

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

    # RHI Buffer test
    add_executable(TestRHIBuffer tests/TestRHIBuffer.cpp)
    target_link_libraries(TestRHIBuffer PRIVATE RHI Platform Core GTest::gtest_main)
    gtest_discover_tests(TestRHIBuffer)

    # RHI Swapchain test
    add_executable(TestRHISwapchain tests/TestRHISwapchain.cpp)
    target_link_libraries(TestRHISwapchain PRIVATE RHI Platform Core GTest::gtest_main)
    gtest_discover_tests(TestRHISwapchain)

    # RHI Command Buffer test
    add_executable(TestRHICommandBuffer tests/TestRHICommandBuffer.cpp)
    target_link_libraries(TestRHICommandBuffer PRIVATE RHI Platform Core GTest::gtest_main)
    gtest_discover_tests(TestRHICommandBuffer)

    # RHI Shader test
    add_executable(TestRHIShader tests/TestRHIShader.cpp)
    target_link_libraries(TestRHIShader PRIVATE RHI Platform Core GTest::gtest_main)
    gtest_discover_tests(TestRHIShader)

    # RHI Pipeline test
    add_executable(TestRHIPipeline tests/TestRHIPipeline.cpp)
    target_link_libraries(TestRHIPipeline PRIVATE RHI Platform Core GTest::gtest_main)
    gtest_discover_tests(TestRHIPipeline)

    # FrameManager test
    add_executable(TestFrameManager tests/TestFrameManager.cpp)
    target_link_libraries(TestFrameManager PRIVATE Renderer RHI Platform Core GTest::gtest_main)
    gtest_discover_tests(TestFrameManager)

    # RHI DeletionQueue test
    add_executable(TestRHIDeletionQueue tests/TestRHIDeletionQueue.cpp)
    target_link_libraries(TestRHIDeletionQueue PRIVATE RHI Platform Core GTest::gtest_main)
    gtest_discover_tests(TestRHIDeletionQueue)

    # Scene test
    add_executable(TestScene tests/TestScene.cpp)
    target_link_libraries(TestScene PRIVATE Scene Resources Core glm::glm GTest::gtest_main)
    gtest_discover_tests(TestScene)

    # Camera test
    add_executable(TestCamera tests/TestCamera.cpp)
    target_link_libraries(TestCamera PRIVATE Scene Resources Core glm::glm GTest::gtest_main)
    gtest_discover_tests(TestCamera)

    # ResourceManager test
    add_executable(TestResourceManager tests/TestResourceManager.cpp)
    target_link_libraries(TestResourceManager PRIVATE Resources Core GTest::gtest_main)
    gtest_discover_tests(TestResourceManager)

    # Vertex test
    add_executable(TestVertex tests/TestVertex.cpp)
    target_link_libraries(TestVertex PRIVATE Resources Core GTest::gtest_main)
    gtest_discover_tests(TestVertex)

    # ModelLoader test
    add_executable(TestModelLoader tests/TestModelLoader.cpp)
    target_include_directories(TestModelLoader PRIVATE ${tinygltf_SOURCE_DIR})
    target_link_libraries(TestModelLoader PRIVATE Resources Core GTest::gtest_main)
    gtest_discover_tests(TestModelLoader)

    # RHI Descriptor test
    add_executable(TestRHIDescriptor tests/TestRHIDescriptor.cpp)
    target_link_libraries(TestRHIDescriptor PRIVATE RHI Resources Platform Core GTest::gtest_main)
    gtest_discover_tests(TestRHIDescriptor)

    # RHI Texture test
    add_executable(TestRHITexture tests/TestRHITexture.cpp)
    target_link_libraries(TestRHITexture PRIVATE RHI Platform Core GTest::gtest_main)
    gtest_discover_tests(TestRHITexture)

    # RHI Sampler test
    add_executable(TestRHISampler tests/TestRHISampler.cpp)
    target_link_libraries(TestRHISampler PRIVATE RHI Platform Core GTest::gtest_main)
    gtest_discover_tests(TestRHISampler)

    # ThreadPool test
    add_executable(TestThreadPool tests/TestThreadPool.cpp)
    target_link_libraries(TestThreadPool PRIVATE Core GTest::gtest_main)
    gtest_discover_tests(TestThreadPool)

    # AsyncResourceLoader test
    add_executable(TestAsyncResourceLoader tests/TestAsyncResourceLoader.cpp)
    target_link_libraries(TestAsyncResourceLoader PRIVATE Resources Core GTest::gtest_main)
    gtest_discover_tests(TestAsyncResourceLoader)

    # MipmapGenerator test
    add_executable(TestMipmapGenerator tests/TestMipmapGenerator.cpp)
    target_link_libraries(TestMipmapGenerator PRIVATE RHI Platform Core GTest::gtest_main)
    gtest_discover_tests(TestMipmapGenerator)

    # MaterialInstance test
    add_executable(TestMaterialInstance tests/TestMaterialInstance.cpp)
    target_link_libraries(TestMaterialInstance PRIVATE Resources RHI Platform Core glm::glm GTest::gtest_main)
    gtest_discover_tests(TestMaterialInstance)

    # RHI CommandPool test
    add_executable(TestRHICommandPool tests/TestRHICommandPool.cpp)
    target_link_libraries(TestRHICommandPool PRIVATE RHI Platform Core GTest::gtest_main)
    gtest_discover_tests(TestRHICommandPool)

    # RHI Semaphore test
    add_executable(TestRHISemaphore tests/TestRHISemaphore.cpp)
    target_link_libraries(TestRHISemaphore PRIVATE RHI Platform Core GTest::gtest_main)
    gtest_discover_tests(TestRHISemaphore)

    # RHI Fence test
    add_executable(TestRHIFence tests/TestRHIFence.cpp)
    target_link_libraries(TestRHIFence PRIVATE RHI Platform Core GTest::gtest_main)
    gtest_discover_tests(TestRHIFence)

    # RHI Image test
    add_executable(TestRHIImage tests/TestRHIImage.cpp)
    target_link_libraries(TestRHIImage PRIVATE RHI Platform Core GTest::gtest_main)
    gtest_discover_tests(TestRHIImage)

    # ResourceHandle test
    add_executable(TestResourceHandle tests/TestResourceHandle.cpp)
    target_link_libraries(TestResourceHandle PRIVATE Resources Core GTest::gtest_main)
    gtest_discover_tests(TestResourceHandle)

    # TextureLoader test
    add_executable(TestTextureLoader tests/TestTextureLoader.cpp)
    target_link_libraries(TestTextureLoader PRIVATE Resources RHI Platform Core GTest::gtest_main)
    gtest_discover_tests(TestTextureLoader)

    # DepthBuffer test
    add_executable(TestDepthBuffer tests/TestDepthBuffer.cpp)
    target_link_libraries(TestDepthBuffer PRIVATE Renderer RHI Platform Core GTest::gtest_main)
    gtest_discover_tests(TestDepthBuffer)

    # LightManager test
    add_executable(TestLightManager tests/TestLightManager.cpp)
    target_link_libraries(TestLightManager PRIVATE Renderer RHI Platform Core glm::glm GTest::gtest_main)
    gtest_discover_tests(TestLightManager)
endif()

# =============================================================================
# Installation (placeholder for future)
# =============================================================================
# install(...)
