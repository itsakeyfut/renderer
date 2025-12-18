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
#include <string>
#include <vector>
#include <cstdint>

namespace Resources {

/**
 * @brief Vertex data structure.
 */
struct Vertex {
    float Position[3] = {0.0f, 0.0f, 0.0f};
    float Normal[3] = {0.0f, 1.0f, 0.0f};
    float TexCoord[2] = {0.0f, 0.0f};
    float Tangent[4] = {1.0f, 0.0f, 0.0f, 1.0f};  // w = handedness
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
                    m_Bounds.Min[0] = m_Bounds.Max[0] = vertex.Position[0];
                    m_Bounds.Min[1] = m_Bounds.Max[1] = vertex.Position[1];
                    m_Bounds.Min[2] = m_Bounds.Max[2] = vertex.Position[2];
                    first = false;
                }
                else {
                    for (int i = 0; i < 3; ++i) {
                        if (vertex.Position[i] < m_Bounds.Min[i]) {
                            m_Bounds.Min[i] = vertex.Position[i];
                        }
                        if (vertex.Position[i] > m_Bounds.Max[i]) {
                            m_Bounds.Max[i] = vertex.Position[i];
                        }
                    }
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
    BoundingBox m_Bounds;
};

} // namespace Resources
