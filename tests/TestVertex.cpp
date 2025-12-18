/**
 * @file TestVertex.cpp
 * @brief Unit tests for the Vertex struct and its Vulkan input descriptions.
 */

#include <gtest/gtest.h>

#include "Resources/Vertex.h"

#include <glm/glm.hpp>

namespace Resources {
namespace Tests {

class VertexTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// =============================================================================
// Default Constructor Tests
// =============================================================================

TEST_F(VertexTest, DefaultConstructor_InitializesPosition)
{
    Vertex vertex;
    EXPECT_EQ(vertex.Position, glm::vec3(0.0f, 0.0f, 0.0f));
}

TEST_F(VertexTest, DefaultConstructor_InitializesNormal)
{
    Vertex vertex;
    EXPECT_EQ(vertex.Normal, glm::vec3(0.0f, 1.0f, 0.0f));
}

TEST_F(VertexTest, DefaultConstructor_InitializesTexCoord)
{
    Vertex vertex;
    EXPECT_EQ(vertex.TexCoord, glm::vec2(0.0f, 0.0f));
}

TEST_F(VertexTest, DefaultConstructor_InitializesTangent)
{
    Vertex vertex;
    EXPECT_EQ(vertex.Tangent, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
}

// =============================================================================
// Struct Size Tests
// =============================================================================

TEST_F(VertexTest, StructSize_IsCorrect)
{
    // vec3 (12) + vec3 (12) + vec2 (8) + vec4 (16) = 48 bytes
    EXPECT_EQ(sizeof(Vertex), 48u);
}

// =============================================================================
// Binding Description Tests
// =============================================================================

TEST_F(VertexTest, GetBindingDescription_BindingIsZero)
{
    auto binding = Vertex::GetBindingDescription();
    EXPECT_EQ(binding.binding, 0u);
}

TEST_F(VertexTest, GetBindingDescription_StrideMatchesStructSize)
{
    auto binding = Vertex::GetBindingDescription();
    EXPECT_EQ(binding.stride, sizeof(Vertex));
}

TEST_F(VertexTest, GetBindingDescription_InputRateIsVertex)
{
    auto binding = Vertex::GetBindingDescription();
    EXPECT_EQ(binding.inputRate, VK_VERTEX_INPUT_RATE_VERTEX);
}

// =============================================================================
// Attribute Description Tests
// =============================================================================

TEST_F(VertexTest, GetAttributeDescriptions_ReturnsCorrectCount)
{
    auto attributes = Vertex::GetAttributeDescriptions();
    EXPECT_EQ(attributes.size(), 4u);
}

TEST_F(VertexTest, GetAttributeDescriptions_Position_LocationIsZero)
{
    auto attributes = Vertex::GetAttributeDescriptions();
    EXPECT_EQ(attributes[0].location, 0u);
}

TEST_F(VertexTest, GetAttributeDescriptions_Position_FormatIsVec3)
{
    auto attributes = Vertex::GetAttributeDescriptions();
    EXPECT_EQ(attributes[0].format, VK_FORMAT_R32G32B32_SFLOAT);
}

TEST_F(VertexTest, GetAttributeDescriptions_Position_OffsetIsCorrect)
{
    auto attributes = Vertex::GetAttributeDescriptions();
    EXPECT_EQ(attributes[0].offset, offsetof(Vertex, Position));
    EXPECT_EQ(attributes[0].offset, 0u);
}

TEST_F(VertexTest, GetAttributeDescriptions_Normal_LocationIsOne)
{
    auto attributes = Vertex::GetAttributeDescriptions();
    EXPECT_EQ(attributes[1].location, 1u);
}

TEST_F(VertexTest, GetAttributeDescriptions_Normal_FormatIsVec3)
{
    auto attributes = Vertex::GetAttributeDescriptions();
    EXPECT_EQ(attributes[1].format, VK_FORMAT_R32G32B32_SFLOAT);
}

TEST_F(VertexTest, GetAttributeDescriptions_Normal_OffsetIsCorrect)
{
    auto attributes = Vertex::GetAttributeDescriptions();
    EXPECT_EQ(attributes[1].offset, offsetof(Vertex, Normal));
    EXPECT_EQ(attributes[1].offset, 12u);  // After vec3 Position
}

TEST_F(VertexTest, GetAttributeDescriptions_TexCoord_LocationIsTwo)
{
    auto attributes = Vertex::GetAttributeDescriptions();
    EXPECT_EQ(attributes[2].location, 2u);
}

TEST_F(VertexTest, GetAttributeDescriptions_TexCoord_FormatIsVec2)
{
    auto attributes = Vertex::GetAttributeDescriptions();
    EXPECT_EQ(attributes[2].format, VK_FORMAT_R32G32_SFLOAT);
}

TEST_F(VertexTest, GetAttributeDescriptions_TexCoord_OffsetIsCorrect)
{
    auto attributes = Vertex::GetAttributeDescriptions();
    EXPECT_EQ(attributes[2].offset, offsetof(Vertex, TexCoord));
    EXPECT_EQ(attributes[2].offset, 24u);  // After vec3 Position + vec3 Normal
}

TEST_F(VertexTest, GetAttributeDescriptions_Tangent_LocationIsThree)
{
    auto attributes = Vertex::GetAttributeDescriptions();
    EXPECT_EQ(attributes[3].location, 3u);
}

TEST_F(VertexTest, GetAttributeDescriptions_Tangent_FormatIsVec4)
{
    auto attributes = Vertex::GetAttributeDescriptions();
    EXPECT_EQ(attributes[3].format, VK_FORMAT_R32G32B32A32_SFLOAT);
}

TEST_F(VertexTest, GetAttributeDescriptions_Tangent_OffsetIsCorrect)
{
    auto attributes = Vertex::GetAttributeDescriptions();
    EXPECT_EQ(attributes[3].offset, offsetof(Vertex, Tangent));
    EXPECT_EQ(attributes[3].offset, 32u);  // After vec3 + vec3 + vec2
}

TEST_F(VertexTest, GetAttributeDescriptions_AllBindingsAreZero)
{
    auto attributes = Vertex::GetAttributeDescriptions();
    for (const auto& attr : attributes) {
        EXPECT_EQ(attr.binding, 0u);
    }
}

// =============================================================================
// Equality Operator Tests
// =============================================================================

TEST_F(VertexTest, EqualityOperator_SameVerticesAreEqual)
{
    Vertex a;
    Vertex b;
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a != b);
}

TEST_F(VertexTest, EqualityOperator_DifferentPositionsAreNotEqual)
{
    Vertex a;
    Vertex b;
    b.Position = glm::vec3(1.0f, 0.0f, 0.0f);
    EXPECT_FALSE(a == b);
    EXPECT_TRUE(a != b);
}

TEST_F(VertexTest, EqualityOperator_DifferentNormalsAreNotEqual)
{
    Vertex a;
    Vertex b;
    b.Normal = glm::vec3(1.0f, 0.0f, 0.0f);
    EXPECT_FALSE(a == b);
    EXPECT_TRUE(a != b);
}

TEST_F(VertexTest, EqualityOperator_DifferentTexCoordsAreNotEqual)
{
    Vertex a;
    Vertex b;
    b.TexCoord = glm::vec2(1.0f, 0.0f);
    EXPECT_FALSE(a == b);
    EXPECT_TRUE(a != b);
}

TEST_F(VertexTest, EqualityOperator_DifferentTangentsAreNotEqual)
{
    Vertex a;
    Vertex b;
    b.Tangent = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
    EXPECT_FALSE(a == b);
    EXPECT_TRUE(a != b);
}

// =============================================================================
// Custom Value Tests
// =============================================================================

TEST_F(VertexTest, CustomValues_CanSetAndRetrievePosition)
{
    Vertex vertex;
    vertex.Position = glm::vec3(1.0f, 2.0f, 3.0f);
    EXPECT_EQ(vertex.Position.x, 1.0f);
    EXPECT_EQ(vertex.Position.y, 2.0f);
    EXPECT_EQ(vertex.Position.z, 3.0f);
}

TEST_F(VertexTest, CustomValues_CanSetAndRetrieveNormal)
{
    Vertex vertex;
    vertex.Normal = glm::vec3(0.0f, 0.0f, 1.0f);
    EXPECT_EQ(vertex.Normal.x, 0.0f);
    EXPECT_EQ(vertex.Normal.y, 0.0f);
    EXPECT_EQ(vertex.Normal.z, 1.0f);
}

TEST_F(VertexTest, CustomValues_CanSetAndRetrieveTexCoord)
{
    Vertex vertex;
    vertex.TexCoord = glm::vec2(0.5f, 0.5f);
    EXPECT_EQ(vertex.TexCoord.x, 0.5f);
    EXPECT_EQ(vertex.TexCoord.y, 0.5f);
}

TEST_F(VertexTest, CustomValues_TangentWIsHandedness)
{
    Vertex vertex;
    vertex.Tangent = glm::vec4(0.0f, 1.0f, 0.0f, -1.0f);  // Left-handed TBN
    EXPECT_EQ(vertex.Tangent.w, -1.0f);
}

} // namespace Tests
} // namespace Resources
