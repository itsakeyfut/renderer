/**
 * @file Types.h
 * @brief Basic type definitions and aliases for the renderer.
 *
 * This file provides standard type aliases and smart pointer helpers
 * used throughout the codebase.
 */

#pragma once

#include <cstdint>
#include <memory>

namespace Core {

// =============================================================================
// Integer Type Aliases
// =============================================================================

using int8   = std::int8_t;
using int16  = std::int16_t;
using int32  = std::int32_t;
using int64  = std::int64_t;

using uint8  = std::uint8_t;
using uint16 = std::uint16_t;
using uint32 = std::uint32_t;
using uint64 = std::uint64_t;

// =============================================================================
// Smart Pointer Aliases
// =============================================================================

/**
 * @brief Shared pointer alias for reference-counted ownership.
 */
template<typename T>
using Ref = std::shared_ptr<T>;

/**
 * @brief Unique pointer alias for exclusive ownership.
 */
template<typename T>
using Scope = std::unique_ptr<T>;

// =============================================================================
// Smart Pointer Factory Functions
// =============================================================================

/**
 * @brief Creates a Ref<T> (shared_ptr) with the given arguments.
 * @tparam T The type to create.
 * @tparam Args The constructor argument types.
 * @param args The constructor arguments.
 * @return A Ref<T> pointing to the newly created object.
 */
template<typename T, typename... Args>
constexpr Ref<T> CreateRef(Args&&... args) {
    return std::make_shared<T>(std::forward<Args>(args)...);
}

/**
 * @brief Creates a Scope<T> (unique_ptr) with the given arguments.
 * @tparam T The type to create.
 * @tparam Args The constructor argument types.
 * @param args The constructor arguments.
 * @return A Scope<T> pointing to the newly created object.
 */
template<typename T, typename... Args>
constexpr Scope<T> CreateScope(Args&&... args) {
    return std::make_unique<T>(std::forward<Args>(args)...);
}

} // namespace Core
