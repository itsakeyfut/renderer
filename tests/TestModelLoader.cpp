/**
 * @file TestModelLoader.cpp
 * @brief Unit tests for ModelLoader class.
 */

#include "Resources/ModelLoader.h"
#include "Resources/Model.h"
#include "Resources/Vertex.h"
#include "Core/Log.h"

#include <gtest/gtest.h>

#include <cmath>
#include <filesystem>
#include <fstream>

// Initialize logging for tests
class ModelLoaderTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override
    {
        Core::Log::Init();
    }

    void TearDown() override
    {
        Core::Log::Shutdown();
    }
};

testing::Environment* const modelLoaderEnv =
    testing::AddGlobalTestEnvironment(new ModelLoaderTestEnvironment);

class ModelLoaderTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // Create test directory
        m_TestDir = std::filesystem::temp_directory_path() / "modelloader_test";
        std::filesystem::create_directories(m_TestDir);
    }

    void TearDown() override
    {
        // Cleanup test directory
        std::error_code ec;
        std::filesystem::remove_all(m_TestDir, ec);
    }

    std::filesystem::path m_TestDir;
};

// =============================================================================
// Format Support Tests
// =============================================================================

TEST_F(ModelLoaderTest, IsFormatSupported_GLTF)
{
    EXPECT_TRUE(Resources::ModelLoader::IsFormatSupported(".gltf"));
    EXPECT_TRUE(Resources::ModelLoader::IsFormatSupported(".GLTF"));
    EXPECT_TRUE(Resources::ModelLoader::IsFormatSupported(".GlTf"));
}

TEST_F(ModelLoaderTest, IsFormatSupported_GLB)
{
    EXPECT_TRUE(Resources::ModelLoader::IsFormatSupported(".glb"));
    EXPECT_TRUE(Resources::ModelLoader::IsFormatSupported(".GLB"));
}

TEST_F(ModelLoaderTest, IsFormatSupported_Unsupported)
{
    EXPECT_FALSE(Resources::ModelLoader::IsFormatSupported(".obj"));
    EXPECT_FALSE(Resources::ModelLoader::IsFormatSupported(".fbx"));
    EXPECT_FALSE(Resources::ModelLoader::IsFormatSupported(".stl"));
    EXPECT_FALSE(Resources::ModelLoader::IsFormatSupported(""));
}

TEST_F(ModelLoaderTest, GetSupportedExtensions)
{
    auto extensions = Resources::ModelLoader::GetSupportedExtensions();

    ASSERT_EQ(extensions.size(), 2u);
    EXPECT_EQ(extensions[0], ".gltf");
    EXPECT_EQ(extensions[1], ".glb");
}

// =============================================================================
// Error Handling Tests
// =============================================================================

TEST_F(ModelLoaderTest, LoadNonExistentFile)
{
    auto model = Resources::ModelLoader::LoadGLTF("nonexistent.gltf");
    EXPECT_EQ(model, nullptr);
}

TEST_F(ModelLoaderTest, LoadInvalidFile)
{
    // Create an invalid glTF file
    auto invalidPath = m_TestDir / "invalid.gltf";
    std::ofstream file(invalidPath);
    file << "this is not valid json or gltf";
    file.close();

    auto model = Resources::ModelLoader::LoadGLTF(invalidPath.string());
    EXPECT_EQ(model, nullptr);
}

// =============================================================================
// Model Structure Tests
// =============================================================================

TEST_F(ModelLoaderTest, ModelBasicStructure)
{
    // Create a minimal valid glTF file with separate binary buffer
    auto gltfPath = m_TestDir / "minimal.gltf";
    auto binPath = m_TestDir / "minimal.bin";

    // Write binary buffer: 3 positions + 3 indices
    // Positions: (0,0,0), (1,0,0), (0,1,0) - 36 bytes
    // Indices: 0, 1, 2 - 6 bytes (uint16)
    std::ofstream binFile(binPath, std::ios::binary);
    float positions[] = {0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};
    uint16_t indices[] = {0, 1, 2};
    binFile.write(reinterpret_cast<const char*>(positions), sizeof(positions));
    binFile.write(reinterpret_cast<const char*>(indices), sizeof(indices));
    binFile.close();

    std::ofstream file(gltfPath);
    file << R"({
        "asset": { "version": "2.0" },
        "scenes": [{ "nodes": [0] }],
        "nodes": [{ "mesh": 0 }],
        "meshes": [{
            "primitives": [{
                "attributes": { "POSITION": 0 },
                "indices": 1
            }]
        }],
        "accessors": [
            {
                "bufferView": 0,
                "componentType": 5126,
                "count": 3,
                "type": "VEC3"
            },
            {
                "bufferView": 1,
                "componentType": 5123,
                "count": 3,
                "type": "SCALAR"
            }
        ],
        "bufferViews": [
            { "buffer": 0, "byteOffset": 0, "byteLength": 36 },
            { "buffer": 0, "byteOffset": 36, "byteLength": 6 }
        ],
        "buffers": [{
            "uri": "minimal.bin",
            "byteLength": 42
        }]
    })";
    file.close();

    auto model = Resources::ModelLoader::LoadGLTF(gltfPath.string());

    ASSERT_NE(model, nullptr);
    EXPECT_EQ(model->GetName(), "minimal");
    EXPECT_EQ(model->GetMeshCount(), 1u);
}

TEST_F(ModelLoaderTest, ModelWithTriangle)
{
    // Create a glTF with a single triangle
    auto gltfPath = m_TestDir / "triangle.gltf";
    auto binPath = m_TestDir / "triangle.bin";

    // Write binary buffer: 3 positions + 3 indices
    std::ofstream binFile(binPath, std::ios::binary);
    float positions[] = {0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};
    uint16_t indices[] = {0, 1, 2};
    binFile.write(reinterpret_cast<const char*>(positions), sizeof(positions));
    binFile.write(reinterpret_cast<const char*>(indices), sizeof(indices));
    binFile.close();

    std::ofstream file(gltfPath);
    file << R"({
        "asset": { "version": "2.0" },
        "scenes": [{ "nodes": [0] }],
        "nodes": [{ "mesh": 0 }],
        "meshes": [{
            "primitives": [{
                "attributes": { "POSITION": 0 },
                "indices": 1
            }]
        }],
        "accessors": [
            {
                "bufferView": 0,
                "componentType": 5126,
                "count": 3,
                "type": "VEC3"
            },
            {
                "bufferView": 1,
                "componentType": 5123,
                "count": 3,
                "type": "SCALAR"
            }
        ],
        "bufferViews": [
            { "buffer": 0, "byteOffset": 0, "byteLength": 36 },
            { "buffer": 0, "byteOffset": 36, "byteLength": 6 }
        ],
        "buffers": [{
            "uri": "triangle.bin",
            "byteLength": 42
        }]
    })";
    file.close();

    auto model = Resources::ModelLoader::LoadGLTF(gltfPath.string());

    ASSERT_NE(model, nullptr);
    EXPECT_EQ(model->GetMeshCount(), 1u);

    const auto& mesh = model->GetMesh(0);
    EXPECT_EQ(mesh.Vertices.size(), 3u);
    EXPECT_EQ(mesh.Indices.size(), 3u);
}

TEST_F(ModelLoaderTest, ModelWithNormals)
{
    // Create a glTF with positions and normals
    auto gltfPath = m_TestDir / "normals.gltf";
    auto binPath = m_TestDir / "normals.bin";

    // Write binary buffer: 3 positions + 3 normals + 3 indices
    std::ofstream binFile(binPath, std::ios::binary);
    float positions[] = {0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};
    float normals[] = {0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f};
    uint16_t indices[] = {0, 1, 2};
    binFile.write(reinterpret_cast<const char*>(positions), sizeof(positions));
    binFile.write(reinterpret_cast<const char*>(normals), sizeof(normals));
    binFile.write(reinterpret_cast<const char*>(indices), sizeof(indices));
    binFile.close();

    std::ofstream file(gltfPath);
    file << R"({
        "asset": { "version": "2.0" },
        "scenes": [{ "nodes": [0] }],
        "nodes": [{ "mesh": 0 }],
        "meshes": [{
            "primitives": [{
                "attributes": {
                    "POSITION": 0,
                    "NORMAL": 1
                },
                "indices": 2
            }]
        }],
        "accessors": [
            {
                "bufferView": 0,
                "componentType": 5126,
                "count": 3,
                "type": "VEC3"
            },
            {
                "bufferView": 1,
                "componentType": 5126,
                "count": 3,
                "type": "VEC3"
            },
            {
                "bufferView": 2,
                "componentType": 5123,
                "count": 3,
                "type": "SCALAR"
            }
        ],
        "bufferViews": [
            { "buffer": 0, "byteOffset": 0, "byteLength": 36 },
            { "buffer": 0, "byteOffset": 36, "byteLength": 36 },
            { "buffer": 0, "byteOffset": 72, "byteLength": 6 }
        ],
        "buffers": [{
            "uri": "normals.bin",
            "byteLength": 78
        }]
    })";
    file.close();

    auto model = Resources::ModelLoader::LoadGLTF(gltfPath.string());

    ASSERT_NE(model, nullptr);
    const auto& mesh = model->GetMesh(0);

    // Check normals are loaded
    for (const auto& vertex : mesh.Vertices) {
        EXPECT_FLOAT_EQ(vertex.Normal.x, 0.0f);
        EXPECT_FLOAT_EQ(vertex.Normal.y, 1.0f);
        EXPECT_FLOAT_EQ(vertex.Normal.z, 0.0f);
    }
}

TEST_F(ModelLoaderTest, ModelWithTexCoords)
{
    // Create a glTF with texture coordinates
    auto gltfPath = m_TestDir / "texcoords.gltf";
    auto binPath = m_TestDir / "texcoords.bin";

    // Write binary buffer: 3 positions + 3 texcoords + 3 indices
    std::ofstream binFile(binPath, std::ios::binary);
    float positions[] = {0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};
    float texcoords[] = {0.0f, 0.0f, 1.0f, 0.0f, 0.5f, 1.0f};
    uint16_t indices[] = {0, 1, 2};
    binFile.write(reinterpret_cast<const char*>(positions), sizeof(positions));
    binFile.write(reinterpret_cast<const char*>(texcoords), sizeof(texcoords));
    binFile.write(reinterpret_cast<const char*>(indices), sizeof(indices));
    binFile.close();

    std::ofstream file(gltfPath);
    file << R"({
        "asset": { "version": "2.0" },
        "scenes": [{ "nodes": [0] }],
        "nodes": [{ "mesh": 0 }],
        "meshes": [{
            "primitives": [{
                "attributes": {
                    "POSITION": 0,
                    "TEXCOORD_0": 1
                },
                "indices": 2
            }]
        }],
        "accessors": [
            {
                "bufferView": 0,
                "componentType": 5126,
                "count": 3,
                "type": "VEC3"
            },
            {
                "bufferView": 1,
                "componentType": 5126,
                "count": 3,
                "type": "VEC2"
            },
            {
                "bufferView": 2,
                "componentType": 5123,
                "count": 3,
                "type": "SCALAR"
            }
        ],
        "bufferViews": [
            { "buffer": 0, "byteOffset": 0, "byteLength": 36 },
            { "buffer": 0, "byteOffset": 36, "byteLength": 24 },
            { "buffer": 0, "byteOffset": 60, "byteLength": 6 }
        ],
        "buffers": [{
            "uri": "texcoords.bin",
            "byteLength": 66
        }]
    })";
    file.close();

    auto model = Resources::ModelLoader::LoadGLTF(gltfPath.string());

    ASSERT_NE(model, nullptr);
    const auto& mesh = model->GetMesh(0);

    // Verify texture coordinates are loaded (not default zeros)
    bool hasNonZeroTexCoords = false;
    for (const auto& vertex : mesh.Vertices) {
        if (vertex.TexCoord.x != 0.0f || vertex.TexCoord.y != 0.0f) {
            hasNonZeroTexCoords = true;
            break;
        }
    }
    // At least one vertex should have non-zero tex coords based on test data
    EXPECT_TRUE(hasNonZeroTexCoords);
}

// =============================================================================
// Bounding Box Tests
// =============================================================================

TEST_F(ModelLoaderTest, BoundingBoxCalculation)
{
    // Create a glTF with known vertex positions
    auto gltfPath = m_TestDir / "bounds.gltf";
    auto binPath = m_TestDir / "bounds.bin";

    // Write binary buffer: 3 positions + 3 indices
    // Vertices: (-1, 0, 0), (1, 0, 0), (0, 2, 0)
    std::ofstream binFile(binPath, std::ios::binary);
    float positions[] = {-1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 2.0f, 0.0f};
    uint16_t indices[] = {0, 1, 2};
    binFile.write(reinterpret_cast<const char*>(positions), sizeof(positions));
    binFile.write(reinterpret_cast<const char*>(indices), sizeof(indices));
    binFile.close();

    std::ofstream file(gltfPath);
    file << R"({
        "asset": { "version": "2.0" },
        "scenes": [{ "nodes": [0] }],
        "nodes": [{ "mesh": 0 }],
        "meshes": [{
            "primitives": [{
                "attributes": { "POSITION": 0 },
                "indices": 1
            }]
        }],
        "accessors": [
            {
                "bufferView": 0,
                "componentType": 5126,
                "count": 3,
                "type": "VEC3"
            },
            {
                "bufferView": 1,
                "componentType": 5123,
                "count": 3,
                "type": "SCALAR"
            }
        ],
        "bufferViews": [
            { "buffer": 0, "byteOffset": 0, "byteLength": 36 },
            { "buffer": 0, "byteOffset": 36, "byteLength": 6 }
        ],
        "buffers": [{
            "uri": "bounds.bin",
            "byteLength": 42
        }]
    })";
    file.close();

    auto model = Resources::ModelLoader::LoadGLTF(gltfPath.string());

    ASSERT_NE(model, nullptr);

    const auto& bounds = model->GetBounds();

    // Bounds should be calculated from vertices: (-1,0,0) to (1,2,0)
    EXPECT_FLOAT_EQ(bounds.Min[0], -1.0f);
    EXPECT_FLOAT_EQ(bounds.Min[1], 0.0f);
    EXPECT_FLOAT_EQ(bounds.Min[2], 0.0f);
    EXPECT_FLOAT_EQ(bounds.Max[0], 1.0f);
    EXPECT_FLOAT_EQ(bounds.Max[1], 2.0f);
    EXPECT_FLOAT_EQ(bounds.Max[2], 0.0f);
}

// =============================================================================
// Options Tests
// =============================================================================

TEST_F(ModelLoaderTest, CalculateTangentsOption)
{
    // Create a glTF without tangents
    auto gltfPath = m_TestDir / "notangents.gltf";
    auto binPath = m_TestDir / "notangents.bin";

    // Write binary buffer: 3 positions + 3 normals + 3 texcoords + 3 indices
    std::ofstream binFile(binPath, std::ios::binary);
    float positions[] = {0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};
    float normals[] = {0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f};
    float texcoords[] = {0.0f, 0.0f, 1.0f, 0.0f, 0.5f, 1.0f};
    uint16_t indices[] = {0, 1, 2};
    binFile.write(reinterpret_cast<const char*>(positions), sizeof(positions));
    binFile.write(reinterpret_cast<const char*>(normals), sizeof(normals));
    binFile.write(reinterpret_cast<const char*>(texcoords), sizeof(texcoords));
    binFile.write(reinterpret_cast<const char*>(indices), sizeof(indices));
    binFile.close();

    std::ofstream file(gltfPath);
    file << R"({
        "asset": { "version": "2.0" },
        "scenes": [{ "nodes": [0] }],
        "nodes": [{ "mesh": 0 }],
        "meshes": [{
            "primitives": [{
                "attributes": {
                    "POSITION": 0,
                    "NORMAL": 1,
                    "TEXCOORD_0": 2
                },
                "indices": 3
            }]
        }],
        "accessors": [
            {
                "bufferView": 0,
                "componentType": 5126,
                "count": 3,
                "type": "VEC3"
            },
            {
                "bufferView": 1,
                "componentType": 5126,
                "count": 3,
                "type": "VEC3"
            },
            {
                "bufferView": 2,
                "componentType": 5126,
                "count": 3,
                "type": "VEC2"
            },
            {
                "bufferView": 3,
                "componentType": 5123,
                "count": 3,
                "type": "SCALAR"
            }
        ],
        "bufferViews": [
            { "buffer": 0, "byteOffset": 0, "byteLength": 36 },
            { "buffer": 0, "byteOffset": 36, "byteLength": 36 },
            { "buffer": 0, "byteOffset": 72, "byteLength": 24 },
            { "buffer": 0, "byteOffset": 96, "byteLength": 6 }
        ],
        "buffers": [{
            "uri": "notangents.bin",
            "byteLength": 102
        }]
    })";
    file.close();

    Resources::ModelLoadOptions options;
    options.CalculateTangents = true;

    auto model = Resources::ModelLoader::LoadGLTF(gltfPath.string(), options);

    ASSERT_NE(model, nullptr);
    const auto& mesh = model->GetMesh(0);

    // Verify tangents are calculated and have valid handedness
    for (const auto& vertex : mesh.Vertices) {
        float tangentLength = std::sqrt(
            vertex.Tangent.x * vertex.Tangent.x +
            vertex.Tangent.y * vertex.Tangent.y +
            vertex.Tangent.z * vertex.Tangent.z);

        // Tangent xyz should be normalized (length ~= 1)
        EXPECT_NEAR(tangentLength, 1.0f, 0.1f);

        // Handedness should be -1 or 1
        EXPECT_TRUE(vertex.Tangent.w == 1.0f || vertex.Tangent.w == -1.0f);
    }
}

// =============================================================================
// Vertex Structure Tests
// =============================================================================

TEST_F(ModelLoaderTest, VertexBindingDescription)
{
    auto binding = Resources::Vertex::GetBindingDescription();

    EXPECT_EQ(binding.binding, 0u);
    EXPECT_EQ(binding.stride, sizeof(Resources::Vertex));
    EXPECT_EQ(binding.inputRate, VK_VERTEX_INPUT_RATE_VERTEX);
}

TEST_F(ModelLoaderTest, VertexAttributeDescriptions)
{
    auto attributes = Resources::Vertex::GetAttributeDescriptions();

    EXPECT_EQ(attributes.size(), 4u);

    // Position - location 0
    EXPECT_EQ(attributes[0].location, 0u);
    EXPECT_EQ(attributes[0].format, VK_FORMAT_R32G32B32_SFLOAT);

    // Normal - location 1
    EXPECT_EQ(attributes[1].location, 1u);
    EXPECT_EQ(attributes[1].format, VK_FORMAT_R32G32B32_SFLOAT);

    // TexCoord - location 2
    EXPECT_EQ(attributes[2].location, 2u);
    EXPECT_EQ(attributes[2].format, VK_FORMAT_R32G32_SFLOAT);

    // Tangent - location 3
    EXPECT_EQ(attributes[3].location, 3u);
    EXPECT_EQ(attributes[3].format, VK_FORMAT_R32G32B32A32_SFLOAT);
}
