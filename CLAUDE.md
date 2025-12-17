# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

A Vulkan-based renderer written in C++20. Currently in early development with Core utilities implemented.

## Build Commands

```bash
# Configure (Release)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# Configure (Debug)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build build

# Run all tests
ctest --test-dir build --output-on-failure

# Run specific test by name
ctest --test-dir build -R LogTest
ctest --test-dir build -R AssertTest
```

## Architecture

**Layer Structure** (upper depends only on lower):
```
Application → Scene → Renderer → Resources → RHI → Platform → Core
```

**Current Implementation:**
- `src/Core/` - Foundation (Types, Assert, Log) - uses spdlog for logging

**Planned Layers:**
- `src/Platform/` - Window, Input, FileSystem
- `src/RHI/` - Vulkan abstraction (Device, CommandBuffer, Pipeline, etc.)
- `src/Resources/` - Asset loading (models, textures)
- `src/Renderer/` - Render passes, post-processing
- `src/Scene/` - Entity/component management

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

## Language Requirements

- All code comments in English
- All documentation in English
- All commit messages in English (but PR content in Japanese per `/finish` command)

## Dependencies

Managed via CMake FetchContent:
- spdlog v1.16.0 - Logging
- GoogleTest v1.17.0 - Testing

## Reference Documents

- `specs/architecture.md` - Full architecture design
- `specs/overview.md` - Project overview and tech stack
- `specs/vulkan-concepts.md` - Core Vulkan concepts
- `specs/implementation-phases.md` - Implementation phases and detailed tasks
- `specs/checklist.md` - Progress tracking
