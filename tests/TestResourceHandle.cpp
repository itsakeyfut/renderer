/**
 * @file TestResourceHandle.cpp
 * @brief Test file for Resources/ResourceHandle.h using GoogleTest.
 *
 * This file tests that the ResourceHandle template class correctly
 * manages handle-based resource references with generation counters.
 */

#include <gtest/gtest.h>
#include "Resources/ResourceHandle.h"

// =============================================================================
// Test Type Definitions
// =============================================================================

// Dummy resource types for testing
struct TestResourceA {};
struct TestResourceB {};

using TestHandleA = Resources::ResourceHandle<TestResourceA>;
using TestHandleB = Resources::ResourceHandle<TestResourceB>;

// =============================================================================
// Default Construction Tests
// =============================================================================

TEST(ResourceHandleTest, DefaultConstructorCreatesInvalidHandle) {
    TestHandleA handle;

    EXPECT_FALSE(handle.IsValid());
    EXPECT_EQ(handle.GetIndex(), TestHandleA::INVALID_INDEX);
    EXPECT_EQ(handle.GetGeneration(), TestHandleA::INVALID_GENERATION);
}

TEST(ResourceHandleTest, DefaultHandleBoolConversionIsFalse) {
    TestHandleA handle;

    EXPECT_FALSE(static_cast<bool>(handle));
}

// =============================================================================
// Parameterized Construction Tests
// =============================================================================

TEST(ResourceHandleTest, ConstructorWithValidParameters) {
    TestHandleA handle(5, 1);

    EXPECT_TRUE(handle.IsValid());
    EXPECT_EQ(handle.GetIndex(), 5u);
    EXPECT_EQ(handle.GetGeneration(), 1u);
}

TEST(ResourceHandleTest, ConstructorWithZeroIndex) {
    TestHandleA handle(0, 1);

    EXPECT_TRUE(handle.IsValid());
    EXPECT_EQ(handle.GetIndex(), 0u);
}

TEST(ResourceHandleTest, ConstructorWithLargeValues) {
    uint32_t largeIndex = 1000000;
    uint32_t largeGeneration = 999999;

    TestHandleA handle(largeIndex, largeGeneration);

    EXPECT_TRUE(handle.IsValid());
    EXPECT_EQ(handle.GetIndex(), largeIndex);
    EXPECT_EQ(handle.GetGeneration(), largeGeneration);
}

// =============================================================================
// Validity Tests
// =============================================================================

TEST(ResourceHandleTest, IsValidReturnsTrueForValidHandle) {
    TestHandleA handle(10, 5);

    EXPECT_TRUE(handle.IsValid());
}

TEST(ResourceHandleTest, IsValidReturnsFalseForInvalidIndex) {
    TestHandleA handle(TestHandleA::INVALID_INDEX, 5);

    EXPECT_FALSE(handle.IsValid());
}

TEST(ResourceHandleTest, IsValidReturnsFalseForInvalidGeneration) {
    TestHandleA handle(10, TestHandleA::INVALID_GENERATION);

    EXPECT_FALSE(handle.IsValid());
}

TEST(ResourceHandleTest, IsValidReturnsFalseForBothInvalid) {
    TestHandleA handle(TestHandleA::INVALID_INDEX, TestHandleA::INVALID_GENERATION);

    EXPECT_FALSE(handle.IsValid());
}

// =============================================================================
// Bool Conversion Tests
// =============================================================================

TEST(ResourceHandleTest, BoolConversionTrueForValidHandle) {
    TestHandleA handle(5, 3);

    EXPECT_TRUE(static_cast<bool>(handle));
    if (handle) {
        SUCCEED();
    } else {
        FAIL() << "Bool conversion should be true for valid handle";
    }
}

TEST(ResourceHandleTest, BoolConversionFalseForInvalidHandle) {
    TestHandleA handle;

    EXPECT_FALSE(static_cast<bool>(handle));
    if (!handle) {
        SUCCEED();
    } else {
        FAIL() << "Bool conversion should be false for invalid handle";
    }
}

// =============================================================================
// Invalidate Tests
// =============================================================================

TEST(ResourceHandleTest, InvalidateMakesHandleInvalid) {
    TestHandleA handle(10, 5);
    ASSERT_TRUE(handle.IsValid());

    handle.Invalidate();

    EXPECT_FALSE(handle.IsValid());
    EXPECT_EQ(handle.GetIndex(), TestHandleA::INVALID_INDEX);
    EXPECT_EQ(handle.GetGeneration(), TestHandleA::INVALID_GENERATION);
}

TEST(ResourceHandleTest, InvalidateOnAlreadyInvalidHandle) {
    TestHandleA handle;
    ASSERT_FALSE(handle.IsValid());

    handle.Invalidate();

    EXPECT_FALSE(handle.IsValid());
}

// =============================================================================
// Equality Tests
// =============================================================================

TEST(ResourceHandleTest, EqualityOperatorSameHandles) {
    TestHandleA handle1(10, 5);
    TestHandleA handle2(10, 5);

    EXPECT_TRUE(handle1 == handle2);
    EXPECT_FALSE(handle1 != handle2);
}

TEST(ResourceHandleTest, EqualityOperatorDifferentIndex) {
    TestHandleA handle1(10, 5);
    TestHandleA handle2(11, 5);

    EXPECT_FALSE(handle1 == handle2);
    EXPECT_TRUE(handle1 != handle2);
}

TEST(ResourceHandleTest, EqualityOperatorDifferentGeneration) {
    TestHandleA handle1(10, 5);
    TestHandleA handle2(10, 6);

    EXPECT_FALSE(handle1 == handle2);
    EXPECT_TRUE(handle1 != handle2);
}

TEST(ResourceHandleTest, EqualityOperatorBothInvalid) {
    TestHandleA handle1;
    TestHandleA handle2;

    EXPECT_TRUE(handle1 == handle2);
    EXPECT_FALSE(handle1 != handle2);
}

TEST(ResourceHandleTest, EqualityOperatorSameObject) {
    TestHandleA handle(10, 5);

    EXPECT_TRUE(handle == handle);
    EXPECT_FALSE(handle != handle);
}

// =============================================================================
// Copy Tests
// =============================================================================

TEST(ResourceHandleTest, CopyConstructor) {
    TestHandleA original(10, 5);
    TestHandleA copy(original);

    EXPECT_EQ(copy.GetIndex(), original.GetIndex());
    EXPECT_EQ(copy.GetGeneration(), original.GetGeneration());
    EXPECT_TRUE(copy == original);
}

TEST(ResourceHandleTest, CopyAssignment) {
    TestHandleA original(10, 5);
    TestHandleA copy;

    copy = original;

    EXPECT_EQ(copy.GetIndex(), original.GetIndex());
    EXPECT_EQ(copy.GetGeneration(), original.GetGeneration());
    EXPECT_TRUE(copy == original);
}

TEST(ResourceHandleTest, CopyDoesNotAffectOriginal) {
    TestHandleA original(10, 5);
    TestHandleA copy(original);

    copy.Invalidate();

    EXPECT_TRUE(original.IsValid());
    EXPECT_FALSE(copy.IsValid());
}

// =============================================================================
// Type Safety Tests
// =============================================================================

TEST(ResourceHandleTest, DifferentTypesAreDistinct) {
    // This test verifies that handles for different types are separate
    // The type system prevents direct comparison, so we just verify
    // that both types can be created independently
    TestHandleA handleA(10, 5);
    TestHandleB handleB(10, 5);

    EXPECT_TRUE(handleA.IsValid());
    EXPECT_TRUE(handleB.IsValid());
    EXPECT_EQ(handleA.GetIndex(), handleB.GetIndex());
    EXPECT_EQ(handleA.GetGeneration(), handleB.GetGeneration());

    // Note: handleA == handleB would be a compile error (type safety)
}

// =============================================================================
// Constant Values Tests
// =============================================================================

TEST(ResourceHandleTest, InvalidIndexIsMaxUint32) {
    EXPECT_EQ(TestHandleA::INVALID_INDEX, ~0u);
    EXPECT_EQ(TestHandleA::INVALID_INDEX, UINT32_MAX);
}

TEST(ResourceHandleTest, InvalidGenerationIsZero) {
    EXPECT_EQ(TestHandleA::INVALID_GENERATION, 0u);
}

// =============================================================================
// Type Alias Tests
// =============================================================================

TEST(ResourceHandleTest, ModelHandleTypeAlias) {
    Resources::ModelHandle handle(1, 1);

    EXPECT_TRUE(handle.IsValid());
    EXPECT_EQ(handle.GetIndex(), 1u);
}

TEST(ResourceHandleTest, MaterialHandleTypeAlias) {
    Resources::MaterialHandle handle(2, 3);

    EXPECT_TRUE(handle.IsValid());
    EXPECT_EQ(handle.GetIndex(), 2u);
    EXPECT_EQ(handle.GetGeneration(), 3u);
}

TEST(ResourceHandleTest, TextureHandleTypeAlias) {
    Resources::TextureHandle handle(5, 10);

    EXPECT_TRUE(handle.IsValid());
    EXPECT_EQ(handle.GetIndex(), 5u);
    EXPECT_EQ(handle.GetGeneration(), 10u);
}

// =============================================================================
// Generation Counter Use Case Tests
// =============================================================================

TEST(ResourceHandleTest, GenerationDetectsStaleHandle) {
    // Simulate a resource pool scenario:
    // 1. Create handle with index 0, generation 1
    // 2. "Free" the resource (generation would increment)
    // 3. Create new resource at same index with generation 2
    // 4. Old handle should not match new resource

    TestHandleA oldHandle(0, 1);
    TestHandleA newHandle(0, 2);

    // Same index, different generation
    EXPECT_EQ(oldHandle.GetIndex(), newHandle.GetIndex());
    EXPECT_NE(oldHandle.GetGeneration(), newHandle.GetGeneration());
    EXPECT_FALSE(oldHandle == newHandle);
}

TEST(ResourceHandleTest, MultipleGenerationIncrements) {
    std::vector<TestHandleA> handles;

    // Simulate multiple allocate/free cycles at same index
    for (uint32_t gen = 1; gen <= 100; ++gen) {
        handles.emplace_back(0, gen);
    }

    // All handles should be valid but different
    for (size_t i = 0; i < handles.size(); ++i) {
        EXPECT_TRUE(handles[i].IsValid());
        EXPECT_EQ(handles[i].GetIndex(), 0u);
        EXPECT_EQ(handles[i].GetGeneration(), static_cast<uint32_t>(i + 1));

        // Each handle should be different from all others
        for (size_t j = i + 1; j < handles.size(); ++j) {
            EXPECT_FALSE(handles[i] == handles[j]);
        }
    }
}

// =============================================================================
// Container Usage Tests
// =============================================================================

TEST(ResourceHandleTest, HandleInVector) {
    std::vector<TestHandleA> handles;

    for (uint32_t i = 0; i < 100; ++i) {
        handles.emplace_back(i, 1);
    }

    EXPECT_EQ(handles.size(), 100u);

    for (uint32_t i = 0; i < 100; ++i) {
        EXPECT_TRUE(handles[i].IsValid());
        EXPECT_EQ(handles[i].GetIndex(), i);
    }
}

TEST(ResourceHandleTest, HandleInSet) {
    // Handles don't have a hash function by default, but we can verify
    // that they work with equality comparisons
    std::vector<TestHandleA> handles;
    handles.emplace_back(0, 1);
    handles.emplace_back(0, 2);
    handles.emplace_back(1, 1);

    // Manual uniqueness check
    for (size_t i = 0; i < handles.size(); ++i) {
        for (size_t j = i + 1; j < handles.size(); ++j) {
            EXPECT_FALSE(handles[i] == handles[j]) << "Handles at " << i << " and " << j << " should be different";
        }
    }
}
