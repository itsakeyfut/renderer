/**
 * @file TestAsyncResourceLoader.cpp
 * @brief Unit tests for Resources/AsyncResourceLoader.
 *
 * Tests the async resource loading system for correct behavior,
 * callbacks, and thread safety. Some tests require a valid RHI device
 * and are skipped in minimal test environments.
 */

#include <gtest/gtest.h>
#include "Resources/AsyncResourceLoader.h"
#include "Resources/ResourceManager.h"
#include "Core/Log.h"

#include <atomic>
#include <chrono>
#include <thread>

// =============================================================================
// Test Fixture
// =============================================================================

class AsyncResourceLoaderTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!Core::Log::IsInitialized()) {
            Core::Log::Init();
        }
        // Note: We don't have a device in this test environment
        // Tests that require a device will be skipped or use mocked behavior
    }

    void TearDown() override {
        // Ensure ResourceManager is clean
        Resources::ResourceManager::Instance().ShutdownAsync();
        Resources::ResourceManager::Instance().Clear();
    }
};

// =============================================================================
// Creation Tests (without device)
// =============================================================================

TEST_F(AsyncResourceLoaderTest, ResourceManagerAsyncNotAvailableByDefault) {
    auto& rm = Resources::ResourceManager::Instance();

    EXPECT_FALSE(rm.IsAsyncAvailable());
    EXPECT_EQ(rm.GetAsyncLoader(), nullptr);
}

TEST_F(AsyncResourceLoaderTest, ProcessAsyncLoadsReturnsZeroWhenNotInitialized) {
    auto& rm = Resources::ResourceManager::Instance();

    // Should not crash, should return 0
    EXPECT_EQ(rm.ProcessAsyncLoads(), 0u);
}

TEST_F(AsyncResourceLoaderTest, GetPendingAsyncLoadCountReturnsZeroWhenNotInitialized) {
    auto& rm = Resources::ResourceManager::Instance();

    EXPECT_EQ(rm.GetPendingAsyncLoadCount(), 0u);
}

TEST_F(AsyncResourceLoaderTest, IsLoadingAsyncReturnsFalseWhenNotInitialized) {
    auto& rm = Resources::ResourceManager::Instance();

    EXPECT_FALSE(rm.IsLoadingAsync("nonexistent.png"));
}

TEST_F(AsyncResourceLoaderTest, LoadTextureAsyncCallsCallbackWithFailureWhenNotInitialized) {
    auto& rm = Resources::ResourceManager::Instance();
    std::atomic<bool> callbackCalled{false};
    std::atomic<bool> callbackSuccess{true};

    rm.LoadTextureAsync("test.png", [&](Resources::TextureHandle handle, bool success) {
        (void)handle;  // Suppress unused parameter warning
        callbackCalled = true;
        callbackSuccess = success;
    });

    EXPECT_TRUE(callbackCalled);
    EXPECT_FALSE(callbackSuccess);
}

TEST_F(AsyncResourceLoaderTest, LoadModelAsyncCallsCallbackWithFailureWhenNotInitialized) {
    auto& rm = Resources::ResourceManager::Instance();
    std::atomic<bool> callbackCalled{false};
    std::atomic<bool> callbackSuccess{true};

    rm.LoadModelAsync("test.gltf", [&](Resources::ModelHandle handle, bool success) {
        (void)handle;  // Suppress unused parameter warning
        callbackCalled = true;
        callbackSuccess = success;
    });

    EXPECT_TRUE(callbackCalled);
    EXPECT_FALSE(callbackSuccess);
}

TEST_F(AsyncResourceLoaderTest, LoadTextureAsyncFutureReturnsInvalidHandleWhenNotInitialized) {
    auto& rm = Resources::ResourceManager::Instance();

    auto future = rm.LoadTextureAsync("test.png");
    auto handle = future.get();

    EXPECT_FALSE(handle.IsValid());
}

TEST_F(AsyncResourceLoaderTest, LoadModelAsyncFutureReturnsInvalidHandleWhenNotInitialized) {
    auto& rm = Resources::ResourceManager::Instance();

    auto future = rm.LoadModelAsync("test.gltf");
    auto handle = future.get();

    EXPECT_FALSE(handle.IsValid());
}

// =============================================================================
// LoadStatus Enum Tests
// =============================================================================

TEST_F(AsyncResourceLoaderTest, LoadStatusEnumValues) {
    // Verify enum values exist and are distinct
    EXPECT_NE(static_cast<int>(Resources::LoadStatus::Pending),
              static_cast<int>(Resources::LoadStatus::Loading));
    EXPECT_NE(static_cast<int>(Resources::LoadStatus::Loading),
              static_cast<int>(Resources::LoadStatus::Completed));
    EXPECT_NE(static_cast<int>(Resources::LoadStatus::Completed),
              static_cast<int>(Resources::LoadStatus::Failed));
}

TEST_F(AsyncResourceLoaderTest, LoadPriorityEnumValues) {
    // Verify priority ordering
    EXPECT_LT(static_cast<int>(Resources::LoadPriority::Low),
              static_cast<int>(Resources::LoadPriority::Normal));
    EXPECT_LT(static_cast<int>(Resources::LoadPriority::Normal),
              static_cast<int>(Resources::LoadPriority::High));
    EXPECT_LT(static_cast<int>(Resources::LoadPriority::High),
              static_cast<int>(Resources::LoadPriority::Immediate));
}

// =============================================================================
// LoadRequestInfo Tests
// =============================================================================

TEST_F(AsyncResourceLoaderTest, LoadRequestInfoDefaults) {
    Resources::LoadRequestInfo info;

    EXPECT_TRUE(info.Path.empty());
    EXPECT_EQ(info.Status, Resources::LoadStatus::Pending);
    EXPECT_EQ(info.Priority, Resources::LoadPriority::Normal);
    EXPECT_FLOAT_EQ(info.Progress, 0.0f);
}

// =============================================================================
// ResourceManager Shutdown Tests
// =============================================================================

TEST_F(AsyncResourceLoaderTest, ShutdownAsyncMultipleTimes) {
    auto& rm = Resources::ResourceManager::Instance();

    // Should not crash
    rm.ShutdownAsync();
    rm.ShutdownAsync();
    rm.ShutdownAsync();

    EXPECT_FALSE(rm.IsAsyncAvailable());
}

// =============================================================================
// Notes for Integration Tests
// =============================================================================

// The following tests require a valid RHI device and should be run
// in an integration test environment:
//
// - AsyncResourceLoaderTest_CreatesWithDevice
// - AsyncResourceLoaderTest_LoadsTextureAsyncWithCallback
// - AsyncResourceLoaderTest_LoadsTextureAsyncWithFuture
// - AsyncResourceLoaderTest_LoadsModelAsyncWithCallback
// - AsyncResourceLoaderTest_LoadsModelAsyncWithFuture
// - AsyncResourceLoaderTest_ProcessCompletedLoads
// - AsyncResourceLoaderTest_CancelPendingLoad
// - AsyncResourceLoaderTest_ProgressCallbackInvoked
// - AsyncResourceLoaderTest_ConcurrentLoads
// - AsyncResourceLoaderTest_PriorityLoading

// Example of what an integration test would look like:
/*
TEST_F(AsyncResourceLoaderIntegrationTest, LoadsTextureAsyncWithCallback) {
    // This test requires a valid RHI device
    auto device = CreateTestDevice();
    auto loader = Resources::AsyncResourceLoader::Create(device, 2);

    std::atomic<bool> callbackCalled{false};
    std::atomic<bool> success{false};
    Resources::TextureHandle resultHandle;

    loader->LoadTextureAsync(
        "assets/textures/test.png",
        [&](Resources::TextureHandle handle, bool loadSuccess) {
            callbackCalled = true;
            success = loadSuccess;
            resultHandle = handle;
        }
    );

    // Wait for the load to complete
    loader->WaitForAll();

    // Process on main thread
    loader->ProcessCompletedLoads();

    EXPECT_TRUE(callbackCalled);
    EXPECT_TRUE(success);
    EXPECT_TRUE(resultHandle.IsValid());
}
*/
