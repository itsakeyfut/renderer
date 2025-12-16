/**
 * @file Timer.h
 * @brief High-precision timer for frame timing and performance measurement.
 *
 * Provides frame delta time calculation, total elapsed time tracking,
 * frame counting, and FPS calculation. Uses std::chrono::high_resolution_clock
 * for maximum precision.
 *
 * Usage:
 * @code
 * Core::Timer timer;
 * timer.Start();
 *
 * // In game loop
 * while (running) {
 *     timer.Tick();
 *     float dt = timer.GetDeltaTime();
 *     // Update game logic with dt
 * }
 * @endcode
 */

#pragma once

#include "Types.h"
#include <chrono>

namespace Core {

/**
 * @class Timer
 * @brief High-precision timer for measuring frame time and calculating FPS.
 *
 * The timer must be started with Start() before use. Call Tick() at the
 * beginning of each frame to update timing values.
 */
class Timer {
public:
    /**
     * @brief Default constructor. Timer is not started until Start() is called.
     */
    Timer() = default;

    /**
     * @brief Starts or resets the timer.
     *
     * Initializes the start time and last frame time to the current time.
     * Resets frame count and delta time. Call this before entering the main loop.
     */
    void Start();

    /**
     * @brief Updates timing values. Call at the beginning of each frame.
     *
     * Calculates delta time since the last Tick() call, updates total elapsed
     * time, increments frame count, and updates FPS calculations.
     */
    void Tick();

    /**
     * @brief Gets the time elapsed since the last frame in seconds.
     * @return Delta time in seconds.
     */
    [[nodiscard]] float GetDeltaTime() const { return m_DeltaTime; }

    /**
     * @brief Gets the total time elapsed since Start() was called in seconds.
     * @return Total elapsed time in seconds.
     */
    [[nodiscard]] float GetTotalTime() const { return m_TotalTime; }

    /**
     * @brief Gets the total number of frames since Start() was called.
     * @return Frame count.
     */
    [[nodiscard]] uint64 GetFrameCount() const { return m_FrameCount; }

    /**
     * @brief Gets the smoothed average frames per second.
     *
     * Uses an exponential moving average for smooth FPS display.
     * @return Average FPS.
     */
    [[nodiscard]] float GetFPS() const { return m_AverageFPS; }

    /**
     * @brief Gets the instantaneous FPS based on the last frame.
     * @return Instantaneous FPS (1 / deltaTime).
     */
    [[nodiscard]] float GetInstantFPS() const;

    /**
     * @brief Gets the last frame time in milliseconds.
     * @return Frame time in milliseconds.
     */
    [[nodiscard]] float GetFrameTimeMs() const { return m_DeltaTime * 1000.0f; }

    /**
     * @brief Checks if the timer has been started.
     * @return true if Start() has been called, false otherwise.
     */
    [[nodiscard]] bool IsRunning() const { return m_IsRunning; }

private:
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = Clock::time_point;
    using Duration = std::chrono::duration<float>;

    TimePoint m_StartTime{};
    TimePoint m_LastFrameTime{};

    float m_DeltaTime = 0.0f;
    float m_TotalTime = 0.0f;
    float m_AverageFPS = 0.0f;

    uint64 m_FrameCount = 0;
    bool m_IsRunning = false;

    // Smoothing factor for exponential moving average FPS calculation
    // Lower values = smoother but slower to respond
    static constexpr float FPS_SMOOTHING_FACTOR = 0.1f;
};

// =============================================================================
// Global Timer Access
// =============================================================================

/**
 * @brief Gets the global timer instance.
 *
 * Provides a singleton-like access to a global timer for convenience.
 * The global timer must be started and ticked manually.
 *
 * @return Reference to the global Timer instance.
 */
Timer& GetGlobalTimer();

/**
 * @brief Gets the global delta time in seconds.
 *
 * Convenience function that returns GetGlobalTimer().GetDeltaTime().
 * @return Delta time from the global timer.
 */
float DeltaTime();

/**
 * @brief Gets the global total time in seconds.
 *
 * Convenience function that returns GetGlobalTimer().GetTotalTime().
 * @return Total time from the global timer.
 */
float TotalTime();

} // namespace Core
