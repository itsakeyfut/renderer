/**
 * @file stb_impl.cpp
 * @brief STB library implementations for the RHI layer.
 *
 * This file provides the implementation of stb_image and stb_image_write
 * for texture loading and writing. The headers are included by other
 * source files that need image loading/writing.
 */

// Suppress warnings from stb headers (third-party code)
#ifdef _MSC_VER
#pragma warning(push, 0)
#elif defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-compare"
#endif

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#ifdef _MSC_VER
#pragma warning(pop)
#elif defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
