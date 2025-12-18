/**
 * @file ResourcePool.h
 * @brief Generic resource pool for handle-based resource management.
 *
 * Provides cache-friendly contiguous storage with generation counters
 * to detect stale handles. Used by ResourceManager for texture, model,
 * and material storage.
 */

#pragma once

#include "Core/Types.h"
#include "Resources/ResourceHandle.h"

#include <vector>
#include <queue>
#include <cstdint>

namespace Resources {

/**
 * @brief Entry in the resource pool containing resource data and metadata.
 * @tparam T The resource type stored in this entry.
 */
template<typename T>
struct ResourceEntry {
    Core::Ref<T> Resource;
    uint32_t Generation = 1;
    uint32_t RefCount = 0;
    bool InUse = false;
};

/**
 * @brief Generic resource pool with handle-based access.
 *
 * Features:
 * - Contiguous storage for cache-friendly iteration
 * - Generation counters to detect stale handles
 * - Reference counting for resource lifetime management
 * - Free list for efficient slot reuse
 *
 * @tparam T The resource type to store.
 */
template<typename T>
class ResourcePool {
public:
    /**
     * @brief Default constructor.
     */
    ResourcePool() = default;

    /**
     * @brief Reserves capacity for resources.
     * @param capacity Number of resources to reserve space for.
     */
    void Reserve(size_t capacity)
    {
        m_Entries.reserve(capacity);
    }

    /**
     * @brief Allocates a slot and stores a resource.
     * @param resource The resource to store.
     * @return Handle to the stored resource.
     */
    ResourceHandle<T> Add(Core::Ref<T> resource)
    {
        uint32_t index;

        if (!m_FreeList.empty()) {
            // Reuse a free slot
            index = m_FreeList.front();
            m_FreeList.pop();

            auto& entry = m_Entries[index];
            entry.Resource = std::move(resource);
            entry.RefCount = 1;
            entry.InUse = true;
            // Generation was already incremented when freed
        }
        else {
            // Allocate new slot
            index = static_cast<uint32_t>(m_Entries.size());
            ResourceEntry<T> entry;
            entry.Resource = std::move(resource);
            entry.Generation = 1;
            entry.RefCount = 1;
            entry.InUse = true;
            m_Entries.push_back(std::move(entry));
        }

        return ResourceHandle<T>(index, m_Entries[index].Generation);
    }

    /**
     * @brief Gets a resource by handle.
     * @param handle Handle to the resource.
     * @return Shared pointer to the resource, or nullptr if handle is invalid.
     */
    Core::Ref<T> Get(ResourceHandle<T> handle) const
    {
        if (!IsValid(handle)) {
            return nullptr;
        }
        return m_Entries[handle.GetIndex()].Resource;
    }

    /**
     * @brief Checks if a handle is valid and points to an active resource.
     * @param handle Handle to validate.
     * @return true if the handle is valid and the resource exists.
     */
    bool IsValid(ResourceHandle<T> handle) const
    {
        if (!handle.IsValid()) {
            return false;
        }

        uint32_t index = handle.GetIndex();
        if (index >= m_Entries.size()) {
            return false;
        }

        const auto& entry = m_Entries[index];
        return entry.InUse && entry.Generation == handle.GetGeneration();
    }

    /**
     * @brief Increments the reference count for a resource.
     * @param handle Handle to the resource.
     */
    void AddRef(ResourceHandle<T> handle)
    {
        if (IsValid(handle)) {
            m_Entries[handle.GetIndex()].RefCount++;
        }
    }

    /**
     * @brief Decrements the reference count for a resource.
     * @param handle Handle to the resource.
     * @return The new reference count, or 0 if handle was invalid.
     */
    uint32_t Release(ResourceHandle<T> handle)
    {
        if (!IsValid(handle)) {
            return 0;
        }

        auto& entry = m_Entries[handle.GetIndex()];
        if (entry.RefCount > 0) {
            entry.RefCount--;
        }
        return entry.RefCount;
    }

    /**
     * @brief Gets the reference count for a resource.
     * @param handle Handle to the resource.
     * @return Reference count, or 0 if handle is invalid.
     */
    uint32_t GetRefCount(ResourceHandle<T> handle) const
    {
        if (!IsValid(handle)) {
            return 0;
        }
        return m_Entries[handle.GetIndex()].RefCount;
    }

    /**
     * @brief Removes a resource from the pool.
     * @param handle Handle to the resource to remove.
     * @return true if the resource was removed successfully.
     */
    bool Remove(ResourceHandle<T> handle)
    {
        if (!IsValid(handle)) {
            return false;
        }

        uint32_t index = handle.GetIndex();
        auto& entry = m_Entries[index];

        entry.Resource.reset();
        entry.Generation++;  // Increment generation to invalidate existing handles
        entry.RefCount = 0;
        entry.InUse = false;

        m_FreeList.push(index);
        return true;
    }

    /**
     * @brief Removes all resources with zero reference count.
     * @return Number of resources released.
     */
    size_t ReleaseUnused()
    {
        size_t released = 0;

        for (uint32_t i = 0; i < m_Entries.size(); ++i) {
            auto& entry = m_Entries[i];
            if (entry.InUse && entry.RefCount == 0) {
                entry.Resource.reset();
                entry.Generation++;
                entry.InUse = false;
                m_FreeList.push(i);
                released++;
            }
        }

        return released;
    }

    /**
     * @brief Clears all resources from the pool.
     */
    void Clear()
    {
        m_Entries.clear();
        // Clear the free list
        std::queue<uint32_t> empty;
        std::swap(m_FreeList, empty);
    }

    /**
     * @brief Gets the number of active resources in the pool.
     * @return Number of resources currently in use.
     */
    size_t GetActiveCount() const
    {
        size_t count = 0;
        for (const auto& entry : m_Entries) {
            if (entry.InUse) {
                count++;
            }
        }
        return count;
    }

    /**
     * @brief Gets the total capacity of the pool.
     * @return Total number of slots (including free slots).
     */
    size_t GetCapacity() const
    {
        return m_Entries.size();
    }

    /**
     * @brief Iterates over all active resources.
     * @tparam Func Callback function type.
     * @param func Callback taking ResourceHandle<T> and const Core::Ref<T>&.
     */
    template<typename Func>
    void ForEach(Func&& func) const
    {
        for (uint32_t i = 0; i < m_Entries.size(); ++i) {
            const auto& entry = m_Entries[i];
            if (entry.InUse) {
                ResourceHandle<T> handle(i, entry.Generation);
                func(handle, entry.Resource);
            }
        }
    }

private:
    std::vector<ResourceEntry<T>> m_Entries;
    std::queue<uint32_t> m_FreeList;
};

} // namespace Resources
