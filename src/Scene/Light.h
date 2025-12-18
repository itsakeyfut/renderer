/**
 * @file Light.h
 * @brief Light type definitions for the scene system.
 *
 * Defines the three main light types: Directional, Point, and Spot lights.
 * These structures are designed to be GPU-compatible for uniform buffer usage.
 */

#pragma once

#include <glm/glm.hpp>

namespace Scene {

/**
 * @brief Directional light representing infinitely distant light source.
 *
 * Used for sun/moon lighting. Has no position, only direction.
 * Memory layout is aligned for GPU uniform buffer usage.
 */
struct DirectionalLight {
    glm::vec3 Direction = glm::vec3(0.0f, -1.0f, 0.0f);
    float Intensity = 1.0f;
    glm::vec3 Color = glm::vec3(1.0f);
    float Padding = 0.0f;

    /**
     * @brief Creates a directional light with specified parameters.
     * @param direction Normalized direction vector (from light to surface).
     * @param color RGB color of the light.
     * @param intensity Light intensity multiplier.
     */
    static DirectionalLight Create(
        const glm::vec3& direction,
        const glm::vec3& color = glm::vec3(1.0f),
        float intensity = 1.0f)
    {
        DirectionalLight light;
        light.Direction = glm::normalize(direction);
        light.Color = color;
        light.Intensity = intensity;
        return light;
    }
};

/**
 * @brief Point light emitting light in all directions from a position.
 *
 * Attenuation is based on distance from the light position.
 * Memory layout is aligned for GPU uniform buffer usage.
 */
struct PointLight {
    glm::vec3 Position = glm::vec3(0.0f);
    float Radius = 10.0f;
    glm::vec3 Color = glm::vec3(1.0f);
    float Intensity = 1.0f;

    /**
     * @brief Creates a point light with specified parameters.
     * @param position World position of the light.
     * @param color RGB color of the light.
     * @param intensity Light intensity multiplier.
     * @param radius Maximum influence radius.
     */
    static PointLight Create(
        const glm::vec3& position,
        const glm::vec3& color = glm::vec3(1.0f),
        float intensity = 1.0f,
        float radius = 10.0f)
    {
        PointLight light;
        light.Position = position;
        light.Color = color;
        light.Intensity = intensity;
        light.Radius = radius;
        return light;
    }
};

/**
 * @brief Spot light emitting light in a cone from a position.
 *
 * Combines point light position with directional falloff.
 * Memory layout is aligned for GPU uniform buffer usage.
 */
struct SpotLight {
    glm::vec3 Position = glm::vec3(0.0f);
    float InnerConeAngle = 0.9f;  // cos(angle), ~25 degrees
    glm::vec3 Direction = glm::vec3(0.0f, -1.0f, 0.0f);
    float OuterConeAngle = 0.8f;  // cos(angle), ~36 degrees
    glm::vec3 Color = glm::vec3(1.0f);
    float Intensity = 1.0f;

    /**
     * @brief Creates a spot light with specified parameters.
     * @param position World position of the light.
     * @param direction Normalized direction the light points.
     * @param color RGB color of the light.
     * @param intensity Light intensity multiplier.
     * @param innerAngleDegrees Inner cone angle in degrees (full intensity).
     * @param outerAngleDegrees Outer cone angle in degrees (falloff to zero).
     */
    static SpotLight Create(
        const glm::vec3& position,
        const glm::vec3& direction,
        const glm::vec3& color = glm::vec3(1.0f),
        float intensity = 1.0f,
        float innerAngleDegrees = 25.0f,
        float outerAngleDegrees = 35.0f)
    {
        SpotLight light;
        light.Position = position;
        light.Direction = glm::normalize(direction);
        light.Color = color;
        light.Intensity = intensity;
        light.InnerConeAngle = glm::cos(glm::radians(innerAngleDegrees));
        light.OuterConeAngle = glm::cos(glm::radians(outerAngleDegrees));
        return light;
    }
};

/**
 * @brief Light uniform buffer data for GPU.
 *
 * Contains the directional light and counts for dynamic lights.
 * Used in conjunction with StructuredBuffers for point/spot lights.
 */
struct LightUBO {
    DirectionalLight DirectionalLightData;
    uint32_t NumPointLights = 0;
    uint32_t NumSpotLights = 0;
    glm::vec2 Padding = glm::vec2(0.0f);
};

} // namespace Scene
