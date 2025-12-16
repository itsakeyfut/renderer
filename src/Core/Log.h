/**
 * @file Log.h
 * @brief Unified logging system for the renderer.
 *
 * Provides logging macros with different severity levels:
 * - LOG_TRACE: Detailed tracing information (debug builds only)
 * - LOG_DEBUG: Debug information (debug builds only)
 * - LOG_INFO: General information
 * - LOG_WARN: Warning messages
 * - LOG_ERROR: Error messages
 * - LOG_FATAL: Fatal errors (followed by abort)
 *
 * In release builds, TRACE and DEBUG logs are completely removed
 * at compile time for optimal performance.
 *
 * Usage:
 * @code
 * LOG_INFO("Vulkan Instance created successfully");
 * LOG_ERROR("Failed to load texture: {}", filepath);
 * LOG_WARN("Frame rate dropped to {} FPS", fps);
 * @endcode
 */

#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>
#include <memory>

namespace Core {

/**
 * @class Log
 * @brief Static logging class that manages the application logger.
 *
 * The logging system must be initialized before use by calling Init().
 * Provides formatted logging with support for various output sinks.
 */
class Log {
public:
    /**
     * @brief Initializes the logging system.
     *
     * Sets up the logger with console and optional file output.
     * Must be called before any logging macros are used.
     *
     * @param enableFileOutput If true, logs are also written to a file.
     * @param logFilePath Path to the log file (only used if enableFileOutput is true).
     */
    static void Init(bool enableFileOutput = false, const char* logFilePath = "renderer.log");

    /**
     * @brief Shuts down the logging system.
     *
     * Flushes all pending logs and releases resources.
     * Call this before application exit for clean shutdown.
     */
    static void Shutdown();

    /**
     * @brief Gets the application logger instance.
     * @return Shared pointer to the spdlog logger.
     */
    static std::shared_ptr<spdlog::logger>& GetLogger() { return s_Logger; }

    /**
     * @brief Checks if the logging system has been initialized.
     * @return true if initialized, false otherwise.
     */
    static bool IsInitialized() { return s_Logger != nullptr; }

    // Logging methods for use by macros

    /**
     * @brief Logs a trace-level message.
     * @tparam Args Format argument types.
     * @param fmt Format string.
     * @param args Format arguments.
     */
    template<typename... Args>
    static void Trace(spdlog::format_string_t<Args...> fmt, Args&&... args) {
        if (s_Logger) {
            s_Logger->trace(fmt, std::forward<Args>(args)...);
        }
    }

    /**
     * @brief Logs a debug-level message.
     * @tparam Args Format argument types.
     * @param fmt Format string.
     * @param args Format arguments.
     */
    template<typename... Args>
    static void Debug(spdlog::format_string_t<Args...> fmt, Args&&... args) {
        if (s_Logger) {
            s_Logger->debug(fmt, std::forward<Args>(args)...);
        }
    }

    /**
     * @brief Logs an info-level message.
     * @tparam Args Format argument types.
     * @param fmt Format string.
     * @param args Format arguments.
     */
    template<typename... Args>
    static void Info(spdlog::format_string_t<Args...> fmt, Args&&... args) {
        if (s_Logger) {
            s_Logger->info(fmt, std::forward<Args>(args)...);
        }
    }

    /**
     * @brief Logs a warning-level message.
     * @tparam Args Format argument types.
     * @param fmt Format string.
     * @param args Format arguments.
     */
    template<typename... Args>
    static void Warn(spdlog::format_string_t<Args...> fmt, Args&&... args) {
        if (s_Logger) {
            s_Logger->warn(fmt, std::forward<Args>(args)...);
        }
    }

    /**
     * @brief Logs an error-level message.
     * @tparam Args Format argument types.
     * @param fmt Format string.
     * @param args Format arguments.
     */
    template<typename... Args>
    static void Error(spdlog::format_string_t<Args...> fmt, Args&&... args) {
        if (s_Logger) {
            s_Logger->error(fmt, std::forward<Args>(args)...);
        }
    }

    /**
     * @brief Logs a fatal error message and aborts the program.
     * @tparam Args Format argument types.
     * @param fmt Format string.
     * @param args Format arguments.
     */
    template<typename... Args>
    [[noreturn]] static void Fatal(spdlog::format_string_t<Args...> fmt, Args&&... args) {
        if (s_Logger) {
            s_Logger->critical(fmt, std::forward<Args>(args)...);
            s_Logger->flush();
        }
        std::abort();
    }

private:
    static std::shared_ptr<spdlog::logger> s_Logger;
};

} // namespace Core

// =============================================================================
// Build configuration detection
// =============================================================================

#if !defined(NDEBUG) || defined(_DEBUG)
    #define RENDERER_LOG_DEBUG_ENABLED 1
#else
    #define RENDERER_LOG_DEBUG_ENABLED 0
#endif

// =============================================================================
// Logging Macros
// =============================================================================

#if RENDERER_LOG_DEBUG_ENABLED

/**
 * @brief Logs trace-level message. Only active in debug builds.
 *
 * Use for detailed tracing during development. Completely removed in release.
 */
#define LOG_TRACE(...) ::Core::Log::Trace(__VA_ARGS__)

/**
 * @brief Logs debug-level message. Only active in debug builds.
 *
 * Use for debugging information. Completely removed in release.
 */
#define LOG_DEBUG(...) ::Core::Log::Debug(__VA_ARGS__)

#else

// In release builds, trace and debug logs are completely removed
#define LOG_TRACE(...) ((void)0)
#define LOG_DEBUG(...) ((void)0)

#endif // RENDERER_LOG_DEBUG_ENABLED

/**
 * @brief Logs info-level message.
 *
 * Use for general operational information.
 */
#define LOG_INFO(...)  ::Core::Log::Info(__VA_ARGS__)

/**
 * @brief Logs warning-level message.
 *
 * Use for potentially problematic situations.
 */
#define LOG_WARN(...)  ::Core::Log::Warn(__VA_ARGS__)

/**
 * @brief Logs error-level message.
 *
 * Use for error conditions that allow recovery.
 */
#define LOG_ERROR(...) ::Core::Log::Error(__VA_ARGS__)

/**
 * @brief Logs fatal error message and aborts.
 *
 * Use for unrecoverable errors. Program terminates after this.
 */
#define LOG_FATAL(...) ::Core::Log::Fatal(__VA_ARGS__)
