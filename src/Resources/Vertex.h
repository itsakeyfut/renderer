/**
 * @file Vertex.h
 * @brief Vertex data structures with Vulkan vertex input descriptions.
 *
 * Defines standard vertex formats used throughout the renderer and provides
 * static methods to generate Vulkan vertex input binding and attribute descriptions
 * for pipeline creation.
 */

#pragma once

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

#include <array>
#include <cstddef>

namespace Resources {

/**
 * @brief Standard vertex structure for 3D meshes.
 *
 * Contains position, normal, texture coordinates, and tangent data.
 * The tangent's w component stores handedness for TBN space calculations.
 *
 * Memory layout matches shader input:
 *   - location 0: position (vec3)
 *   - location 1: normal (vec3)
 *   - location 2: texCoord (vec2)
 *   - location 3: tangent (vec4, w = handedness)
 */
struct Vertex {
    glm::vec3 Position = glm::vec3(0.0f);
    glm::vec3 Normal = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec2 TexCoord = glm::vec2(0.0f);
    glm::vec4 Tangent = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);  // w = handedness

    /**
     * @brief Gets the vertex input binding description for Vulkan pipelines.
     *
     * Describes how to interpret vertex data from a vertex buffer:
     *   - Binding 0
     *   - Stride = sizeof(Vertex)
     *   - Input rate = per-vertex
     *
     * @return VkVertexInputBindingDescription for this vertex format.
     */
    static VkVertexInputBindingDescription GetBindingDescription()
    {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindingDescription;
    }

    /**
     * @brief Gets the vertex attribute descriptions for Vulkan pipelines.
     *
     * Describes the layout of each vertex attribute:
     *   - location 0: position (R32G32B32_SFLOAT)
     *   - location 1: normal (R32G32B32_SFLOAT)
     *   - location 2: texCoord (R32G32_SFLOAT)
     *   - location 3: tangent (R32G32B32A32_SFLOAT)
     *
     * @return Array of VkVertexInputAttributeDescription for this vertex format.
     */
    static std::array<VkVertexInputAttributeDescription, 4> GetAttributeDescriptions()
    {
        std::array<VkVertexInputAttributeDescription, 4> attributeDescriptions{};

        // Position - location 0
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, Position);

        // Normal - location 1
        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, Normal);

        // TexCoord - location 2
        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(Vertex, TexCoord);

        // Tangent - location 3
        attributeDescriptions[3].binding = 0;
        attributeDescriptions[3].location = 3;
        attributeDescriptions[3].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attributeDescriptions[3].offset = offsetof(Vertex, Tangent);

        return attributeDescriptions;
    }

    /**
     * @brief Equality operator for comparing vertices.
     * @param other The vertex to compare against.
     * @return true if all components are equal.
     */
    bool operator==(const Vertex& other) const
    {
        return Position == other.Position &&
               Normal == other.Normal &&
               TexCoord == other.TexCoord &&
               Tangent == other.Tangent;
    }

    /**
     * @brief Inequality operator for comparing vertices.
     * @param other The vertex to compare against.
     * @return true if any component differs.
     */
    bool operator!=(const Vertex& other) const
    {
        return !(*this == other);
    }
};

} // namespace Resources
