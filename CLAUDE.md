# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

A Vulkan-based renderer written in C++20. Uses Dynamic Rendering (VK_KHR_dynamic_rendering) instead of traditional render passes.

## Build Commands

```bash
# Configure (Release)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# Configure (Debug)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build build

# Run application
./build/VulkanRenderer.exe

# Run all tests
ctest --test-dir build --output-on-failure

# Run specific test by name
ctest --test-dir build -R LogTest
ctest --test-dir build -R RHIDeviceTest
```

## Architecture

**Layer Structure** (upper depends only on lower):
```
Application → Scene → Renderer → Resources → RHI → Platform → Core
```

**Current Implementation Status:**
- `Core/` - Foundation (Types, Assert, Log, Timer, Config, Event, ThreadPool)
- `Platform/` - Window (GLFW), Input handling, FileDialog
- `RHI/` - Vulkan abstraction (Instance, Device, Swapchain, CommandBuffer, Pipeline, Buffer, Texture, Sampler, Descriptors, MipmapGenerator)
- `Resources/` - ModelLoader (glTF via tinygltf), TextureLoader, ResourceManager, AsyncResourceLoader
- `Renderer/` - FrameManager, DepthBuffer, ImGuiRenderer
- `Scene/` - Camera, FPS/Orbit controllers, Entity, Transform, Light, Scene

## Coding Conventions

| Element | Convention | Example |
|---------|------------|---------|
| Class | PascalCase | `RHIDevice` |
| Function | PascalCase | `CreateBuffer()` |
| Member variable | m_PascalCase | `m_Device` |
| Local variable | camelCase | `vertexCount` |
| Constant | ALL_CAPS | `MAX_FRAMES_IN_FLIGHT` |
| Namespace | PascalCase | `Core`, `RHI` |

## Core Macros

**Assertions** (`Core/Assert.h`):
- `ASSERT(cond)` - Debug-only, removed in release
- `VERIFY(cond)` - Always evaluated, asserts in debug only
- `ASSERT_MSG(cond, msg)` / `VERIFY_MSG(cond, msg)` - With custom message

**Logging** (`Core/Log.h`):
- `LOG_TRACE/LOG_DEBUG` - Debug builds only
- `LOG_INFO/LOG_WARN/LOG_ERROR/LOG_FATAL` - Always available

## Smart Pointers (`Core/Types.h`)

- `Core::Ref<T>` → `std::shared_ptr<T>`
- `Core::Scope<T>` → `std::unique_ptr<T>`
- `Core::CreateRef<T>(args...)` / `Core::CreateScope<T>(args...)`

## Key Design Patterns

**Handle-Based Resources**: Use `ResourceHandle<T>` with generation counter for safe GPU resource references.

**Deferred Deletion**: `RHIDeletionQueue` queues resource deletion until GPU is done (frame overlap = 2).

**Frame-in-Flight**: `FrameManager` manages per-frame command buffers, semaphores, and fences (MAX_FRAMES_IN_FLIGHT = 2).

## Language Requirements

- All code comments in English
- All documentation in English
- Commit messages in English
- PR titles and bodies in Japanese (per `/finish` command)

## Dependencies (via CMake FetchContent)

- spdlog v1.16.0 - Logging
- GoogleTest v1.17.0 - Testing
- GLFW 3.4 - Windowing
- GLM 1.0.2 - Math
- VMA v3.3.0 - Vulkan Memory Allocator
- tinygltf v2.9.7 - glTF loading
- nlohmann_json v3.12.0 - JSON parsing
- Dear ImGui v1.92.5 - Debug UI

## Reference Documents

- `specs/architecture.md` - Full architecture design
- `specs/implementation-phases.md` - Implementation phases and detailed tasks
- `specs/checklist.md` - Progress tracking (Phase 1-2 complete)
- `specs/vulkan-concepts.md` - Core Vulkan concepts
