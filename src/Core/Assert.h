/**
 * @file Assert.h
 * @brief Assertion macros for debugging and development.
 *
 * Provides ASSERT and VERIFY macros similar to Unreal Engine's
 * check, ensure, and verify macros.
 *
 * - ASSERT(cond): Only evaluated in debug builds. Removed entirely in release.
 * - VERIFY(cond): Always evaluated, but only asserts in debug builds.
 * - ASSERT_MSG(cond, msg): ASSERT with a custom message.
 * - VERIFY_MSG(cond, msg): VERIFY with a custom message.
 *
 * On assertion failure:
 * 1. Outputs error message with file, line, and condition
 * 2. Triggers debugger breakpoint if attached
 * 3. Aborts the program
 */

#pragma once

#include <cstdio>
#include <cstdlib>

// =============================================================================
// Platform-specific debugger break
// =============================================================================

#if defined(_MSC_VER)
    // Microsoft Visual C++
    #define RENDERER_DEBUG_BREAK() __debugbreak()
#elif defined(__clang__) || defined(__GNUC__)
    #if defined(__has_builtin)
        #if __has_builtin(__builtin_debugtrap)
            #define RENDERER_DEBUG_BREAK() __builtin_debugtrap()
        #else
            #define RENDERER_DEBUG_BREAK() __builtin_trap()
        #endif
    #else
        // Fallback for older compilers
        #include <csignal>
        #define RENDERER_DEBUG_BREAK() std::raise(SIGTRAP)
    #endif
#else
    // Unknown compiler fallback
    #include <csignal>
    #define RENDERER_DEBUG_BREAK() std::raise(SIGABRT)
#endif

// =============================================================================
// Build configuration detection
// =============================================================================

// Use NDEBUG (standard C++) or _DEBUG (MSVC) to detect debug builds
#if !defined(NDEBUG) || defined(_DEBUG)
    #define RENDERER_DEBUG 1
#else
    #define RENDERER_DEBUG 0
#endif

// =============================================================================
// Internal assertion handler
// =============================================================================

namespace Core {
namespace Internal {

/**
 * @brief Handles assertion failure by printing error and triggering debugger.
 * @param condition The condition string that failed.
 * @param message Optional custom message (can be nullptr).
 * @param file The source file where assertion failed.
 * @param line The line number where assertion failed.
 *
 * This function is marked [[noreturn]] as it always terminates the program.
 */
[[noreturn]] inline void AssertionFailed(
    const char* condition,
    const char* message,
    const char* file,
    int line
) {
    // Print to stderr for immediate visibility
    std::fprintf(stderr, "\n");
    std::fprintf(stderr, "========================================\n");
    std::fprintf(stderr, "ASSERTION FAILED\n");
    std::fprintf(stderr, "========================================\n");
    std::fprintf(stderr, "Condition: %s\n", condition);
    if (message && message[0] != '\0') {
        std::fprintf(stderr, "Message:   %s\n", message);
    }
    std::fprintf(stderr, "Location:  %s:%d\n", file, line);
    std::fprintf(stderr, "========================================\n");
    std::fprintf(stderr, "\n");
    std::fflush(stderr);

    // Trigger debugger breakpoint
    RENDERER_DEBUG_BREAK();

    // Abort if debugger doesn't handle it
    std::abort();
}

} // namespace Internal
} // namespace Core

// =============================================================================
// Assertion Macros
// =============================================================================

#if RENDERER_DEBUG

/**
 * @brief Asserts that a condition is true. Only active in debug builds.
 *
 * In release builds, this macro expands to nothing, so the condition
 * is not evaluated at all. Use VERIFY if you need the condition to
 * always be evaluated.
 *
 * @param cond The condition to check.
 *
 * Example:
 * @code
 * ASSERT(ptr != nullptr);
 * ASSERT(index < array.size());
 * @endcode
 */
#define ASSERT(cond) \
    do { \
        if (!(cond)) { \
            ::Core::Internal::AssertionFailed(#cond, nullptr, __FILE__, __LINE__); \
        } \
    } while (0)

/**
 * @brief Asserts that a condition is true with a custom message.
 *
 * @param cond The condition to check.
 * @param msg The message to display on failure.
 *
 * Example:
 * @code
 * ASSERT_MSG(device != nullptr, "Vulkan device must be initialized first");
 * @endcode
 */
#define ASSERT_MSG(cond, msg) \
    do { \
        if (!(cond)) { \
            ::Core::Internal::AssertionFailed(#cond, msg, __FILE__, __LINE__); \
        } \
    } while (0)

/**
 * @brief Always evaluates the condition, asserts in debug builds only.
 *
 * Unlike ASSERT, the condition is always evaluated even in release builds.
 * This is useful for function calls with side effects that must happen,
 * but you want to check the result in debug builds.
 *
 * @param cond The condition to evaluate and check.
 *
 * Example:
 * @code
 * VERIFY(vkCreateInstance(&info, nullptr, &instance) == VK_SUCCESS);
 * @endcode
 */
#define VERIFY(cond) \
    do { \
        if (!(cond)) { \
            ::Core::Internal::AssertionFailed(#cond, nullptr, __FILE__, __LINE__); \
        } \
    } while (0)

/**
 * @brief VERIFY with a custom message.
 *
 * @param cond The condition to evaluate and check.
 * @param msg The message to display on failure.
 */
#define VERIFY_MSG(cond, msg) \
    do { \
        if (!(cond)) { \
            ::Core::Internal::AssertionFailed(#cond, msg, __FILE__, __LINE__); \
        } \
    } while (0)

#else // Release build

// In release builds, ASSERT is completely removed
#define ASSERT(cond)          ((void)0)
#define ASSERT_MSG(cond, msg) ((void)0)

// In release builds, VERIFY still evaluates but doesn't check
#define VERIFY(cond)          ((void)(cond))
#define VERIFY_MSG(cond, msg) ((void)(cond))

#endif // RENDERER_DEBUG

// =============================================================================
// Additional utility macros
// =============================================================================

/**
 * @brief Marks a code path that should never be reached.
 *
 * Use this for default cases in switches that handle all enum values,
 * or other "impossible" code paths.
 *
 * Example:
 * @code
 * switch (type) {
 *     case Type::A: return handleA();
 *     case Type::B: return handleB();
 *     default: ASSERT_UNREACHABLE();
 * }
 * @endcode
 */
#define ASSERT_UNREACHABLE() \
    ASSERT_MSG(false, "Unreachable code path executed")

/**
 * @brief Marks code that is not yet implemented.
 *
 * Example:
 * @code
 * void SomeFunction() {
 *     ASSERT_NOT_IMPLEMENTED();
 * }
 * @endcode
 */
#define ASSERT_NOT_IMPLEMENTED() \
    ASSERT_MSG(false, "Not implemented")
