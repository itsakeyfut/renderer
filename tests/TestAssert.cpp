/**
 * @file TestAssert.cpp
 * @brief Test file for Assert.h macros using GoogleTest.
 *
 * This file tests that assertion macros compile and work correctly.
 * Run in debug mode to test assertion behavior.
 */

#include <gtest/gtest.h>
#include "Core/Assert.h"
#include "Core/Types.h"

// =============================================================================
// ASSERT Macro Tests
// =============================================================================

TEST(AssertTest, PassingAssertions) {
    // These should all pass without triggering assertions
    ASSERT(true);
    ASSERT(1 == 1);
    ASSERT(nullptr == nullptr);

    int* ptr = new int(42);
    ASSERT(ptr != nullptr);
    ASSERT(*ptr == 42);
    delete ptr;
}

TEST(AssertTest, AssertWithMessage) {
    ASSERT_MSG(true, "This should pass");
    ASSERT_MSG(2 + 2 == 4, "Math still works");
}

// =============================================================================
// VERIFY Macro Tests
// =============================================================================

TEST(VerifyTest, VerifyAlwaysEvaluates) {
    // VERIFY should always evaluate the condition
    int counter = 0;

    VERIFY(++counter == 1);  // Should increment counter
    EXPECT_EQ(counter, 1);   // Verify it was incremented

    VERIFY_MSG(++counter == 2, "Counter should be 2");
    EXPECT_EQ(counter, 2);
}

// =============================================================================
// Types.h Tests
// =============================================================================

TEST(TypesTest, UnsignedIntegerTypes) {
    Core::uint8  u8  = 255;
    Core::uint16 u16 = 65535;
    Core::uint32 u32 = 4294967295U;
    Core::uint64 u64 = 18446744073709551615ULL;

    EXPECT_EQ(u8, 255);
    EXPECT_EQ(u16, 65535);
    EXPECT_EQ(u32, 4294967295U);
    EXPECT_EQ(u64, 18446744073709551615ULL);
}

TEST(TypesTest, SignedIntegerTypes) {
    Core::int8  i8  = -128;
    Core::int16 i16 = -32768;
    Core::int32 i32 = -2147483647 - 1;
    Core::int64 i64 = -9223372036854775807LL - 1;

    EXPECT_EQ(i8, -128);
    EXPECT_EQ(i16, -32768);
    EXPECT_EQ(i32, -2147483647 - 1);
    EXPECT_EQ(i64, -9223372036854775807LL - 1);
}

TEST(TypesTest, SmartPointerHelpers) {
    auto refPtr = Core::CreateRef<int>(42);
    ASSERT_NE(refPtr, nullptr);
    EXPECT_EQ(*refPtr, 42);

    auto scopePtr = Core::CreateScope<int>(100);
    ASSERT_NE(scopePtr, nullptr);
    EXPECT_EQ(*scopePtr, 100);
}

// =============================================================================
// Debug Macro Tests
// =============================================================================

TEST(DebugTest, DebugMacroIsDefined) {
#if RENDERER_DEBUG
    // In debug builds, RENDERER_DEBUG should be 1
    EXPECT_EQ(RENDERER_DEBUG, 1);
#else
    // In release builds, RENDERER_DEBUG should be 0
    EXPECT_EQ(RENDERER_DEBUG, 0);
#endif
}
