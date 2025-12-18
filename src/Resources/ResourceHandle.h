/**
 * @file ResourceHandle.h
 * @brief Handle-based references for GPU resources.
 *
 * Provides a safe, cache-friendly way to reference GPU resources like textures,
 * models, and materials. Uses a generation counter to detect stale handles.
 */

#pragma once

#include "Core/Types.h"
#include <cstdint>

namespace Resources {

/**
 * @brief Handle to a GPU resource.
 *
 * Instead of raw pointers, resources are referenced through handles that
 * contain an index into a resource pool and a generation counter to detect
 * stale references.
 *
 * @tparam T The resource type this handle references.
 */
template<typename T>
class ResourceHandle {
public:
    static constexpr uint32_t INVALID_INDEX = ~0u;
    static constexpr uint32_t INVALID_GENERATION = 0u;

    /**
     * @brief Default constructor creates an invalid handle.
     */
    ResourceHandle() = default;

    /**
     * @brief Constructs a handle with specific index and generation.
     * @param index Index into the resource pool.
     * @param generation Generation counter for stale detection.
     */
    ResourceHandle(uint32_t index, uint32_t generation)
        : m_Index(index)
        , m_Generation(generation)
    {
    }

    /**
     * @brief Checks if this handle is valid.
     * @return true if the handle has been assigned a valid resource.
     */
    bool IsValid() const
    {
        return m_Index != INVALID_INDEX && m_Generation != INVALID_GENERATION;
    }

    /**
     * @brief Gets the index into the resource pool.
     * @return The resource index.
     */
    uint32_t GetIndex() const { return m_Index; }

    /**
     * @brief Gets the generation counter.
     * @return The generation value.
     */
    uint32_t GetGeneration() const { return m_Generation; }

    /**
     * @brief Invalidates this handle.
     */
    void Invalidate()
    {
        m_Index = INVALID_INDEX;
        m_Generation = INVALID_GENERATION;
    }

    /**
     * @brief Equality comparison operator.
     */
    bool operator==(const ResourceHandle& other) const
    {
        return m_Index == other.m_Index && m_Generation == other.m_Generation;
    }

    /**
     * @brief Inequality comparison operator.
     */
    bool operator!=(const ResourceHandle& other) const
    {
        return !(*this == other);
    }

    /**
     * @brief Boolean conversion operator.
     * @return true if the handle is valid.
     */
    explicit operator bool() const { return IsValid(); }

private:
    uint32_t m_Index = INVALID_INDEX;
    uint32_t m_Generation = INVALID_GENERATION;
};

// Forward declarations for resource types
class Model;
class Material;
class Texture;

// Type aliases for common resource handles
using ModelHandle = ResourceHandle<Model>;
using MaterialHandle = ResourceHandle<Material>;
using TextureHandle = ResourceHandle<Texture>;

} // namespace Resources
