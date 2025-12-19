/**
 * @file Model.h
 * @brief Model resource placeholder.
 *
 * This is a stub implementation. Full mesh/model support with GPU buffers
 * will be added in a separate issue when model loading is implemented.
 */

#pragma once

#include "Core/Assert.h"
#include "Core/Types.h"
#include "Resources/Vertex.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace Resources {

/**
 * @brief Material data loaded from model file (glTF).
 *
 * Contains texture paths and PBR parameters as loaded from
 * the model file. Used by TextureLoader to create GPU resources.
 */
struct MaterialData {
    std::string Name;

    // PBR Metallic-Roughness workflow
    std::string BaseColorTexturePath;       ///< albedo/diffuse texture
    std::string MetallicRoughnessTexturePath; ///< metallic (B) + roughness (G)
    std::string NormalTexturePath;          ///< normal map
    std::string OcclusionTexturePath;       ///< ambient occlusion
    std::string EmissiveTexturePath;        ///< emissive texture

    // Base factors (used when texture is not available)
    float BaseColorFactor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    float MetallicFactor = 1.0f;
    float RoughnessFactor = 1.0f;
    float EmissiveFactor[3] = {0.0f, 0.0f, 0.0f};

    // Alpha mode
    enum class AlphaMode { Opaque, Mask, Blend };
    AlphaMode Alpha = AlphaMode::Opaque;
    float AlphaCutoff = 0.5f;

    // Double-sided rendering
    bool DoubleSided = false;
};

/**
 * @brief Mesh primitive within a model.
 */
struct Mesh {
    std::vector<Vertex> Vertices;
    std::vector<uint32_t> Indices;
    int32_t MaterialIndex = -1;
    std::string Name;
};

/**
 * @brief Bounding box for spatial queries.
 */
struct BoundingBox {
    float Min[3] = {0.0f, 0.0f, 0.0f};
    float Max[3] = {0.0f, 0.0f, 0.0f};

    /**
     * @brief Gets the center of the bounding box.
     * @param outCenter Output array for center coordinates.
     */
    void GetCenter(float outCenter[3]) const
    {
        outCenter[0] = (Min[0] + Max[0]) * 0.5f;
        outCenter[1] = (Min[1] + Max[1]) * 0.5f;
        outCenter[2] = (Min[2] + Max[2]) * 0.5f;
    }

    /**
     * @brief Gets the extents (half-size) of the bounding box.
     * @param outExtents Output array for extent values.
     */
    void GetExtents(float outExtents[3]) const
    {
        outExtents[0] = (Max[0] - Min[0]) * 0.5f;
        outExtents[1] = (Max[1] - Min[1]) * 0.5f;
        outExtents[2] = (Max[2] - Min[2]) * 0.5f;
    }
};

/**
 * @brief Model resource class.
 *
 * Represents a 3D model with one or more meshes. Currently a stub
 * implementation that stores mesh data on the CPU side. GPU buffer
 * creation will be added when integrating with the RHI layer.
 */
class Model {
public:
    /**
     * @brief Constructs a model with the given path.
     * @param path File path this model was loaded from.
     */
    explicit Model(const std::string& path)
        : m_Path(path)
    {
    }

    /**
     * @brief Gets the file path.
     * @return Path this model was loaded from.
     */
    const std::string& GetPath() const { return m_Path; }

    /**
     * @brief Gets the model name.
     * @return Model name (usually derived from filename).
     */
    const std::string& GetName() const { return m_Name; }

    /**
     * @brief Sets the model name.
     * @param name New name for the model.
     */
    void SetName(const std::string& name) { m_Name = name; }

    /**
     * @brief Gets the number of meshes in this model.
     * @return Mesh count.
     */
    size_t GetMeshCount() const { return m_Meshes.size(); }

    /**
     * @brief Gets a mesh by index.
     * @param index Mesh index.
     * @return Reference to the mesh.
     */
    const Mesh& GetMesh(size_t index) const
    {
        ASSERT(index < m_Meshes.size());
        return m_Meshes[index];
    }

    /**
     * @brief Gets mutable access to a mesh.
     * @param index Mesh index.
     * @return Mutable reference to the mesh.
     */
    Mesh& GetMesh(size_t index)
    {
        ASSERT(index < m_Meshes.size());
        return m_Meshes[index];
    }

    /**
     * @brief Gets all meshes.
     * @return Reference to the mesh vector.
     */
    const std::vector<Mesh>& GetMeshes() const { return m_Meshes; }

    /**
     * @brief Adds a mesh to the model.
     * @param mesh Mesh to add.
     */
    void AddMesh(Mesh mesh) { m_Meshes.push_back(std::move(mesh)); }

    // ========== Material data accessors ==========

    /**
     * @brief Gets the number of material data entries in this model.
     * @return Material data count.
     */
    size_t GetMaterialDataCount() const { return m_MaterialData.size(); }

    /**
     * @brief Gets material data by index.
     * @param index Material data index.
     * @return Reference to the material data.
     */
    const MaterialData& GetMaterialData(size_t index) const
    {
        ASSERT(index < m_MaterialData.size());
        return m_MaterialData[index];
    }

    /**
     * @brief Gets all material data.
     * @return Reference to the material data vector.
     */
    const std::vector<MaterialData>& GetMaterialDataList() const { return m_MaterialData; }

    /**
     * @brief Adds material data to the model.
     * @param materialData Material data to add.
     */
    void AddMaterialData(MaterialData materialData) { m_MaterialData.push_back(std::move(materialData)); }

    /**
     * @brief Gets the bounding box.
     * @return Reference to the model's bounding box.
     */
    const BoundingBox& GetBounds() const { return m_Bounds; }

    /**
     * @brief Sets the bounding box.
     * @param bounds New bounding box.
     */
    void SetBounds(const BoundingBox& bounds) { m_Bounds = bounds; }

    /**
     * @brief Calculates and sets the bounding box from mesh data.
     */
    void CalculateBounds()
    {
        bool first = true;
        for (const auto& mesh : m_Meshes) {
            for (const auto& vertex : mesh.Vertices) {
                if (first) {
                    m_Bounds.Min[0] = m_Bounds.Max[0] = vertex.Position.x;
                    m_Bounds.Min[1] = m_Bounds.Max[1] = vertex.Position.y;
                    m_Bounds.Min[2] = m_Bounds.Max[2] = vertex.Position.z;
                    first = false;
                }
                else {
                    m_Bounds.Min[0] = std::min(m_Bounds.Min[0], vertex.Position.x);
                    m_Bounds.Min[1] = std::min(m_Bounds.Min[1], vertex.Position.y);
                    m_Bounds.Min[2] = std::min(m_Bounds.Min[2], vertex.Position.z);
                    m_Bounds.Max[0] = std::max(m_Bounds.Max[0], vertex.Position.x);
                    m_Bounds.Max[1] = std::max(m_Bounds.Max[1], vertex.Position.y);
                    m_Bounds.Max[2] = std::max(m_Bounds.Max[2], vertex.Position.z);
                }
            }
        }
    }

    /**
     * @brief Gets the total vertex count across all meshes.
     * @return Total number of vertices.
     */
    size_t GetTotalVertexCount() const
    {
        size_t count = 0;
        for (const auto& mesh : m_Meshes) {
            count += mesh.Vertices.size();
        }
        return count;
    }

    /**
     * @brief Gets the total index count across all meshes.
     * @return Total number of indices.
     */
    size_t GetTotalIndexCount() const
    {
        size_t count = 0;
        for (const auto& mesh : m_Meshes) {
            count += mesh.Indices.size();
        }
        return count;
    }

    /**
     * @brief Estimates memory usage of this model.
     * @return Approximate memory usage in bytes.
     */
    size_t GetMemorySize() const
    {
        size_t vertexSize = GetTotalVertexCount() * sizeof(Vertex);
        size_t indexSize = GetTotalIndexCount() * sizeof(uint32_t);
        return vertexSize + indexSize;
    }

private:
    std::string m_Path;
    std::string m_Name;
    std::vector<Mesh> m_Meshes;
    std::vector<MaterialData> m_MaterialData;
    BoundingBox m_Bounds;
};

} // namespace Resources
