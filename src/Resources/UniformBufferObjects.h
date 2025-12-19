/**
 * @file UniformBufferObjects.h
 * @brief Uniform Buffer Object structures for shader data.
 *
 * Defines standard UBO structures for camera and object transform data
 * that are passed to shaders via descriptor sets.
 */

#pragma once

#include <glm/glm.hpp>

namespace Resources {

/**
 * @brief Camera uniform buffer object.
 *
 * Contains camera matrices and position data for view/projection transforms.
 * Designed to be bound to shader register b0.
 *
 * HLSL equivalent:
 * @code
 * cbuffer CameraData : register(b0) {
 *     float4x4 view;
 *     float4x4 projection;
 *     float4x4 viewProjection;
 *     float3 cameraPosition;
 *     float padding;
 * };
 * @endcode
 */
struct CameraUBO {
    /**
     * @brief View matrix (world to view space).
     */
    glm::mat4 View = glm::mat4(1.0f);

    /**
     * @brief Projection matrix (view to clip space).
     */
    glm::mat4 Projection = glm::mat4(1.0f);

    /**
     * @brief Combined view-projection matrix.
     * Computed as Projection * View for efficiency.
     */
    glm::mat4 ViewProjection = glm::mat4(1.0f);

    /**
     * @brief Camera world position.
     */
    glm::vec3 CameraPosition = glm::vec3(0.0f);

    /**
     * @brief Padding for 16-byte alignment (std140 layout).
     */
    float Padding = 0.0f;
};

/**
 * @brief Object transform uniform buffer object.
 *
 * Contains per-object transformation data for model transforms.
 * Designed to be bound to shader register b1.
 *
 * HLSL equivalent:
 * @code
 * cbuffer ObjectData : register(b1) {
 *     float4x4 model;
 *     float4x4 normalMatrix;
 * };
 * @endcode
 */
struct ObjectUBO {
    /**
     * @brief Model matrix (local to world space).
     */
    glm::mat4 Model = glm::mat4(1.0f);

    /**
     * @brief Normal matrix for transforming normals.
     * Should be the transpose of the inverse of the upper-left 3x3 of Model.
     * Stored as mat4 for alignment purposes.
     */
    glm::mat4 NormalMatrix = glm::mat4(1.0f);
};

// Static assertions to verify UBO alignment (std140 layout)
static_assert(sizeof(CameraUBO) == 208, "CameraUBO size must be 208 bytes for std140 layout");
static_assert(sizeof(ObjectUBO) == 128, "ObjectUBO size must be 128 bytes for std140 layout");

static_assert(alignof(CameraUBO) <= 16, "CameraUBO alignment must be 16 bytes or less");
static_assert(alignof(ObjectUBO) <= 16, "ObjectUBO alignment must be 16 bytes or less");

} // namespace Resources
