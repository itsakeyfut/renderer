/**
 * @file Timer.cpp
 * @brief Implementation of the high-precision timer system.
 */

#include "Timer.h"

namespace Core {

void Timer::Start() {
    m_StartTime = Clock::now();
    m_LastFrameTime = m_StartTime;
    m_DeltaTime = 0.0f;
    m_TotalTime = 0.0f;
    m_FrameCount = 0;
    m_AverageFPS = 0.0f;
    m_IsRunning = true;
}

void Timer::Tick() {
    if (!m_IsRunning) {
        return;
    }

    TimePoint currentTime = Clock::now();

    // Calculate delta time in seconds
    Duration deltaDuration = currentTime - m_LastFrameTime;
    m_DeltaTime = deltaDuration.count();

    // Clamp delta time to avoid large jumps (e.g., after breakpoints)
    // Maximum delta of 0.25 seconds (4 FPS minimum)
    constexpr float MAX_DELTA_TIME = 0.25f;
    if (m_DeltaTime > MAX_DELTA_TIME) {
        m_DeltaTime = MAX_DELTA_TIME;
    }

    // Calculate total time since start
    Duration totalDuration = currentTime - m_StartTime;
    m_TotalTime = totalDuration.count();

    // Update frame count
    ++m_FrameCount;

    // Update average FPS using exponential moving average
    if (m_DeltaTime > 0.0f) {
        float instantFPS = 1.0f / m_DeltaTime;
        if (m_FrameCount == 1) {
            // First frame, initialize average FPS
            m_AverageFPS = instantFPS;
        } else {
            // Exponential moving average for smooth FPS
            m_AverageFPS = m_AverageFPS + FPS_SMOOTHING_FACTOR * (instantFPS - m_AverageFPS);
        }
    }

    // Store current time for next frame's delta calculation
    m_LastFrameTime = currentTime;
}

float Timer::GetInstantFPS() const {
    if (m_DeltaTime > 0.0f) {
        return 1.0f / m_DeltaTime;
    }
    return 0.0f;
}

// =============================================================================
// Global Timer Implementation
// =============================================================================

namespace {
    Timer s_GlobalTimer;
}

Timer& GetGlobalTimer() {
    return s_GlobalTimer;
}

float DeltaTime() {
    return s_GlobalTimer.GetDeltaTime();
}

float TotalTime() {
    return s_GlobalTimer.GetTotalTime();
}

} // namespace Core
