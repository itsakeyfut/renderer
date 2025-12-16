/**
 * @file TestTimer.cpp
 * @brief Test file for Timer.h time management system using GoogleTest.
 *
 * This file tests the high-precision timer functionality including
 * delta time calculation, total time tracking, frame counting, and FPS calculation.
 */

#include <gtest/gtest.h>
#include "Core/Timer.h"
#include <thread>
#include <chrono>

// =============================================================================
// Basic Timer Tests
// =============================================================================

TEST(TimerTest, InitialState) {
    Core::Timer timer;

    EXPECT_FALSE(timer.IsRunning());
    EXPECT_EQ(timer.GetFrameCount(), 0);
    EXPECT_FLOAT_EQ(timer.GetDeltaTime(), 0.0f);
    EXPECT_FLOAT_EQ(timer.GetTotalTime(), 0.0f);
}

TEST(TimerTest, StartSetsRunning) {
    Core::Timer timer;

    timer.Start();

    EXPECT_TRUE(timer.IsRunning());
}

TEST(TimerTest, StartResetsValues) {
    Core::Timer timer;
    timer.Start();
    timer.Tick();
    timer.Tick();

    // Start again should reset
    timer.Start();

    EXPECT_EQ(timer.GetFrameCount(), 0);
    EXPECT_FLOAT_EQ(timer.GetDeltaTime(), 0.0f);
    EXPECT_FLOAT_EQ(timer.GetTotalTime(), 0.0f);
}

// =============================================================================
// Tick and Delta Time Tests
// =============================================================================

TEST(TimerTest, TickIncreasesFrameCount) {
    Core::Timer timer;
    timer.Start();

    timer.Tick();
    EXPECT_EQ(timer.GetFrameCount(), 1);

    timer.Tick();
    EXPECT_EQ(timer.GetFrameCount(), 2);

    timer.Tick();
    EXPECT_EQ(timer.GetFrameCount(), 3);
}

TEST(TimerTest, TickWithoutStartDoesNothing) {
    Core::Timer timer;

    timer.Tick();

    EXPECT_FALSE(timer.IsRunning());
    EXPECT_EQ(timer.GetFrameCount(), 0);
}

TEST(TimerTest, DeltaTimeIsPositive) {
    Core::Timer timer;
    timer.Start();

    // Small delay to ensure measurable delta
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    timer.Tick();

    EXPECT_GT(timer.GetDeltaTime(), 0.0f);
}

TEST(TimerTest, DeltaTimeIsReasonable) {
    Core::Timer timer;
    timer.Start();

    // Sleep for approximately 50ms
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    timer.Tick();

    float dt = timer.GetDeltaTime();
    // Allow some tolerance for timing variance (40ms to 100ms)
    EXPECT_GT(dt, 0.04f);
    EXPECT_LT(dt, 0.1f);
}

TEST(TimerTest, DeltaTimeIsClamped) {
    Core::Timer timer;
    timer.Start();

    // Sleep for longer than max delta (250ms+)
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    timer.Tick();

    // Delta time should be clamped to MAX_DELTA_TIME (0.25s)
    EXPECT_LE(timer.GetDeltaTime(), 0.25f);
}

// =============================================================================
// Total Time Tests
// =============================================================================

TEST(TimerTest, TotalTimeIncreases) {
    Core::Timer timer;
    timer.Start();

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    timer.Tick();
    float time1 = timer.GetTotalTime();

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    timer.Tick();
    float time2 = timer.GetTotalTime();

    EXPECT_GT(time2, time1);
}

TEST(TimerTest, TotalTimeIsReasonable) {
    Core::Timer timer;
    timer.Start();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    timer.Tick();

    float total = timer.GetTotalTime();
    // Allow tolerance (80ms to 200ms)
    EXPECT_GT(total, 0.08f);
    EXPECT_LT(total, 0.2f);
}

// =============================================================================
// FPS Tests
// =============================================================================

TEST(TimerTest, InstantFPSCalculation) {
    Core::Timer timer;
    timer.Start();

    // Simulate ~60 FPS (16.67ms per frame)
    std::this_thread::sleep_for(std::chrono::milliseconds(16));
    timer.Tick();

    float instantFPS = timer.GetInstantFPS();
    // Should be roughly 60 FPS (allow wide range due to timing variance: 30-100)
    EXPECT_GT(instantFPS, 30.0f);
    EXPECT_LT(instantFPS, 100.0f);
}

TEST(TimerTest, AverageFPSIsInitializedOnFirstFrame) {
    Core::Timer timer;
    timer.Start();

    std::this_thread::sleep_for(std::chrono::milliseconds(16));
    timer.Tick();

    float avgFPS = timer.GetFPS();
    // Should be non-zero after first frame
    EXPECT_GT(avgFPS, 0.0f);
}

TEST(TimerTest, InstantFPSZeroWhenNotStarted) {
    Core::Timer timer;

    EXPECT_FLOAT_EQ(timer.GetInstantFPS(), 0.0f);
}

// =============================================================================
// Frame Time Tests
// =============================================================================

TEST(TimerTest, FrameTimeMsConversion) {
    Core::Timer timer;
    timer.Start();

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    timer.Tick();

    float frameTimeMs = timer.GetFrameTimeMs();
    float deltaTime = timer.GetDeltaTime();

    // Frame time in ms should be delta time * 1000
    EXPECT_FLOAT_EQ(frameTimeMs, deltaTime * 1000.0f);
}

// =============================================================================
// Global Timer Tests
// =============================================================================

TEST(GlobalTimerTest, GetGlobalTimerReturnsSameInstance) {
    Core::Timer& timer1 = Core::GetGlobalTimer();
    Core::Timer& timer2 = Core::GetGlobalTimer();

    EXPECT_EQ(&timer1, &timer2);
}

TEST(GlobalTimerTest, DeltaTimeFunction) {
    Core::Timer& globalTimer = Core::GetGlobalTimer();
    globalTimer.Start();

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    globalTimer.Tick();

    float dt1 = Core::DeltaTime();
    float dt2 = globalTimer.GetDeltaTime();

    EXPECT_FLOAT_EQ(dt1, dt2);
}

TEST(GlobalTimerTest, TotalTimeFunction) {
    Core::Timer& globalTimer = Core::GetGlobalTimer();
    globalTimer.Start();

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    globalTimer.Tick();

    float total1 = Core::TotalTime();
    float total2 = globalTimer.GetTotalTime();

    EXPECT_FLOAT_EQ(total1, total2);
}

// =============================================================================
// Multiple Frame Simulation Tests
// =============================================================================

TEST(TimerTest, MultipleFrameSimulation) {
    Core::Timer timer;
    timer.Start();

    constexpr int FRAME_COUNT = 10;
    constexpr int FRAME_DELAY_MS = 10;

    for (int i = 0; i < FRAME_COUNT; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(FRAME_DELAY_MS));
        timer.Tick();
    }

    EXPECT_EQ(timer.GetFrameCount(), FRAME_COUNT);
    // Total time should be roughly FRAME_COUNT * FRAME_DELAY_MS milliseconds
    // Allow tolerance (50ms to 200ms for 10 frames at 10ms each = 100ms expected)
    EXPECT_GT(timer.GetTotalTime(), 0.05f);
    EXPECT_LT(timer.GetTotalTime(), 0.2f);
}
