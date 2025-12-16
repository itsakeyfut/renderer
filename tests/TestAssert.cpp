/**
 * @file TestAssert.cpp
 * @brief Test file for Assert.h macros.
 *
 * This file tests that assertion macros compile and work correctly.
 * Run in debug mode to test assertion behavior.
 */

#include "../src/Core/Assert.h"
#include "../src/Core/Types.h"
#include <iostream>

void TestPassingAssertions() {
    std::cout << "Testing passing assertions..." << std::endl;

    // These should all pass
    ASSERT(true);
    ASSERT(1 == 1);
    ASSERT(nullptr == nullptr);

    int* ptr = new int(42);
    ASSERT(ptr != nullptr);
    ASSERT(*ptr == 42);
    delete ptr;

    ASSERT_MSG(true, "This should pass");
    ASSERT_MSG(2 + 2 == 4, "Math still works");

    std::cout << "  All passing assertions OK" << std::endl;
}

void TestVerifyMacro() {
    std::cout << "Testing VERIFY macro..." << std::endl;

    // VERIFY should always evaluate the condition
    int counter = 0;

    VERIFY(++counter == 1);  // Should increment counter
    ASSERT(counter == 1);    // Verify it was incremented

    VERIFY_MSG(++counter == 2, "Counter should be 2");
    ASSERT(counter == 2);

    std::cout << "  VERIFY macro OK (counter = " << counter << ")" << std::endl;
}

void TestTypesHeader() {
    std::cout << "Testing Types.h..." << std::endl;

    // Test type aliases
    Core::uint8  u8  = 255;
    Core::uint16 u16 = 65535;
    Core::uint32 u32 = 4294967295U;
    Core::uint64 u64 = 18446744073709551615ULL;

    Core::int8  i8  = -128;
    Core::int16 i16 = -32768;
    Core::int32 i32 = -2147483647 - 1;
    Core::int64 i64 = -9223372036854775807LL - 1;

    ASSERT(u8 == 255);
    ASSERT(u16 == 65535);
    ASSERT(u32 == 4294967295U);
    ASSERT(u64 == 18446744073709551615ULL);
    ASSERT(i8 == -128);
    ASSERT(i16 == -32768);

    // Suppress unused variable warnings (needed for release builds where ASSERT is removed)
    (void)u8; (void)u16; (void)u32; (void)u64;
    (void)i8; (void)i16; (void)i32; (void)i64;

    // Test smart pointer helpers
    auto refPtr = Core::CreateRef<int>(42);
    ASSERT(refPtr != nullptr);
    ASSERT(*refPtr == 42);

    auto scopePtr = Core::CreateScope<int>(100);
    ASSERT(scopePtr != nullptr);
    ASSERT(*scopePtr == 100);

    std::cout << "  Types.h OK" << std::endl;
}

void TestDebugMacroValue() {
    std::cout << "Testing RENDERER_DEBUG macro..." << std::endl;

#if RENDERER_DEBUG
    std::cout << "  RENDERER_DEBUG = 1 (Debug build)" << std::endl;
#else
    std::cout << "  RENDERER_DEBUG = 0 (Release build)" << std::endl;
#endif

    std::cout << "  Debug macro OK" << std::endl;
}

// Uncomment to test assertion failure (will abort the program)
// void TestFailingAssertion() {
//     std::cout << "Testing failing assertion (will abort)..." << std::endl;
//     ASSERT(false);  // This will trigger the assertion
// }

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Assert.h Test Suite" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    TestDebugMacroValue();
    TestPassingAssertions();
    TestVerifyMacro();
    TestTypesHeader();

    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "All tests passed!" << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}
