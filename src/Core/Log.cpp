/**
 * @file Log.cpp
 * @brief Implementation of the logging system.
 */

#include "Log.h"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <vector>

namespace Core {

Ref<spdlog::logger> Log::s_Logger = nullptr;

void Log::Init(bool enableFileOutput, const char* logFilePath) {
    // Create sinks
    std::vector<spdlog::sink_ptr> sinks;

    // Console sink with color support
    auto consoleSink = CreateRef<spdlog::sinks::stdout_color_sink_mt>();
    consoleSink->set_pattern("%^[%T] [%l] %v%$");
    sinks.push_back(consoleSink);

    // Optional file sink
    if (enableFileOutput && logFilePath != nullptr) {
        auto fileSink = CreateRef<spdlog::sinks::basic_file_sink_mt>(logFilePath, true);
        fileSink->set_pattern("[%Y-%m-%d %T.%e] [%l] %v");
        sinks.push_back(fileSink);
    }

    // Create logger with all sinks
    s_Logger = CreateRef<spdlog::logger>("RENDERER", sinks.begin(), sinks.end());
    spdlog::register_logger(s_Logger);

    // Set log level based on build configuration
#if RENDERER_LOG_DEBUG_ENABLED
    s_Logger->set_level(spdlog::level::trace);
#else
    s_Logger->set_level(spdlog::level::info);
#endif

    // Flush on error or higher
    s_Logger->flush_on(spdlog::level::err);

    LOG_INFO("Logging system initialized");
}

void Log::Shutdown() {
    if (s_Logger) {
        LOG_INFO("Logging system shutting down");
        s_Logger->flush();
        spdlog::drop("RENDERER");
        s_Logger.reset();
    }
}

} // namespace Core
