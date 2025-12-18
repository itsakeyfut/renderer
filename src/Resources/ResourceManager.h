/**
 * @file ResourceManager.h
 * @brief Centralized resource management with path-based caching.
 *
 * Provides a singleton manager for textures, models, and materials with:
 * - Path-based caching to prevent duplicate loads
 * - Reference counting for resource lifetime management
 * - Statistics tracking for debugging
 */

#pragma once

#include "Core/Types.h"
#include "Resources/ResourceHandle.h"
#include "Resources/ResourcePool.h"

#include <string>
#include <unordered_map>
#include <functional>

namespace Resources {

// Forward declarations (will be fully implemented in separate issues)
class Texture;
class Model;
class Material;

/**
 * @brief Texture resource descriptor for creation.
 */
struct TextureDesc {
    std::string Path;
    bool GenerateMipmaps = true;
    bool sRGB = true;
};

/**
 * @brief Model resource descriptor for creation.
 */
struct ModelDesc {
    std::string Path;
    bool CalculateNormals = false;
    bool CalculateTangents = true;
};

/**
 * @brief Material resource descriptor for creation.
 */
struct MaterialDesc {
    std::string Name;
    TextureHandle DiffuseTexture;
    TextureHandle NormalTexture;
    TextureHandle MetallicRoughnessTexture;
};

/**
 * @brief Resource usage statistics.
 */
struct ResourceStats {
    size_t TextureCount = 0;
    size_t ModelCount = 0;
    size_t MaterialCount = 0;
    size_t TotalMemoryBytes = 0;
    size_t TextureCacheHits = 0;
    size_t TextureCacheMisses = 0;
    size_t ModelCacheHits = 0;
    size_t ModelCacheMisses = 0;
};

/**
 * @brief Callback type for async resource loading notification.
 * @tparam HandleType The type of handle returned.
 */
template<typename HandleType>
using LoadCallback = std::function<void(HandleType, bool success)>;

/**
 * @brief Centralized manager for GPU resources.
 *
 * Features:
 * - Singleton pattern for global access
 * - Path-based caching to prevent duplicate loads
 * - Reference counting for automatic cleanup
 * - Statistics tracking for debugging
 *
 * Usage:
 * @code
 * auto& rm = ResourceManager::Instance();
 *
 * // Load a texture (cached if already loaded)
 * TextureHandle tex = rm.LoadTexture("textures/diffuse.png");
 *
 * // Load same texture returns cached handle
 * TextureHandle sameTex = rm.LoadTexture("textures/diffuse.png");
 * assert(tex == sameTex);
 *
 * // Release when done
 * rm.ReleaseTexture(tex);
 *
 * // Cleanup unused resources
 * rm.ReleaseUnused();
 * @endcode
 */
class ResourceManager {
public:
    /**
     * @brief Gets the singleton instance.
     * @return Reference to the ResourceManager instance.
     */
    static ResourceManager& Instance();

    /**
     * @brief Initializes the resource manager.
     *
     * Must be called before using any resource loading functions.
     * Called automatically on first Instance() access, but can be
     * called explicitly for initialization timing control.
     */
    void Initialize();

    /**
     * @brief Shuts down the resource manager and releases all resources.
     *
     * Should be called before application shutdown to ensure proper
     * resource cleanup order.
     */
    void Shutdown();

    // =========================================================================
    // Texture Management
    // =========================================================================

    /**
     * @brief Loads a texture from file path.
     *
     * If the texture is already loaded, returns the cached handle.
     * Increments reference count in either case.
     *
     * @param path Path to the texture file.
     * @param desc Optional texture loading parameters.
     * @return Handle to the loaded texture.
     */
    TextureHandle LoadTexture(const std::string& path, const TextureDesc& desc = {});

    /**
     * @brief Gets an already-loaded texture handle without incrementing ref count.
     * @param path Path to the texture file.
     * @return Handle to the texture, or invalid handle if not loaded.
     */
    TextureHandle GetTexture(const std::string& path) const;

    /**
     * @brief Gets the texture resource from a handle.
     * @param handle Handle to the texture.
     * @return Shared pointer to the texture, or nullptr if invalid.
     */
    Core::Ref<Texture> GetTextureResource(TextureHandle handle) const;

    /**
     * @brief Releases a reference to a texture.
     *
     * Decrements reference count. Texture is not immediately destroyed
     * but marked for cleanup by ReleaseUnused().
     *
     * @param handle Handle to release.
     */
    void ReleaseTexture(TextureHandle handle);

    /**
     * @brief Checks if a texture handle is valid.
     * @param handle Handle to check.
     * @return true if the handle refers to a valid texture.
     */
    bool IsTextureValid(TextureHandle handle) const;

    // =========================================================================
    // Model Management
    // =========================================================================

    /**
     * @brief Loads a model from file path.
     *
     * If the model is already loaded, returns the cached handle.
     * Increments reference count in either case.
     *
     * @param path Path to the model file (glTF, OBJ, etc.).
     * @param desc Optional model loading parameters.
     * @return Handle to the loaded model.
     */
    ModelHandle LoadModel(const std::string& path, const ModelDesc& desc = {});

    /**
     * @brief Gets an already-loaded model handle without incrementing ref count.
     * @param path Path to the model file.
     * @return Handle to the model, or invalid handle if not loaded.
     */
    ModelHandle GetModel(const std::string& path) const;

    /**
     * @brief Gets the model resource from a handle.
     * @param handle Handle to the model.
     * @return Shared pointer to the model, or nullptr if invalid.
     */
    Core::Ref<Model> GetModelResource(ModelHandle handle) const;

    /**
     * @brief Releases a reference to a model.
     *
     * Decrements reference count. Model is not immediately destroyed
     * but marked for cleanup by ReleaseUnused().
     *
     * @param handle Handle to release.
     */
    void ReleaseModel(ModelHandle handle);

    /**
     * @brief Checks if a model handle is valid.
     * @param handle Handle to check.
     * @return true if the handle refers to a valid model.
     */
    bool IsModelValid(ModelHandle handle) const;

    // =========================================================================
    // Material Management
    // =========================================================================

    /**
     * @brief Creates a new material with the given name.
     *
     * Materials are not file-based, so there's no caching by path.
     * Each call creates a new material.
     *
     * @param name Name for the material (for debugging).
     * @param desc Material properties.
     * @return Handle to the created material.
     */
    MaterialHandle CreateMaterial(const std::string& name, const MaterialDesc& desc = {});

    /**
     * @brief Gets a material by name.
     * @param name Name of the material.
     * @return Handle to the material, or invalid handle if not found.
     */
    MaterialHandle GetMaterial(const std::string& name) const;

    /**
     * @brief Gets the material resource from a handle.
     * @param handle Handle to the material.
     * @return Shared pointer to the material, or nullptr if invalid.
     */
    Core::Ref<Material> GetMaterialResource(MaterialHandle handle) const;

    /**
     * @brief Releases a reference to a material.
     * @param handle Handle to release.
     */
    void ReleaseMaterial(MaterialHandle handle);

    /**
     * @brief Checks if a material handle is valid.
     * @param handle Handle to check.
     * @return true if the handle refers to a valid material.
     */
    bool IsMaterialValid(MaterialHandle handle) const;

    // =========================================================================
    // Resource Cleanup
    // =========================================================================

    /**
     * @brief Releases all resources with zero reference count.
     *
     * Call periodically to free unused resources and reclaim memory.
     *
     * @return Total number of resources released.
     */
    size_t ReleaseUnused();

    /**
     * @brief Releases all resources immediately.
     *
     * Use with caution - invalidates all existing handles.
     * Typically called only during shutdown.
     */
    void Clear();

    // =========================================================================
    // Statistics
    // =========================================================================

    /**
     * @brief Gets the number of loaded textures.
     * @return Active texture count.
     */
    size_t GetTextureCount() const;

    /**
     * @brief Gets the number of loaded models.
     * @return Active model count.
     */
    size_t GetModelCount() const;

    /**
     * @brief Gets the number of created materials.
     * @return Active material count.
     */
    size_t GetMaterialCount() const;

    /**
     * @brief Gets estimated total memory usage.
     * @return Approximate memory usage in bytes.
     */
    size_t GetMemoryUsage() const;

    /**
     * @brief Gets detailed resource statistics.
     * @return ResourceStats structure with all statistics.
     */
    ResourceStats GetStats() const;

    /**
     * @brief Resets cache hit/miss statistics.
     */
    void ResetStats();

    // Non-copyable, non-movable (singleton)
    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;
    ResourceManager(ResourceManager&&) = delete;
    ResourceManager& operator=(ResourceManager&&) = delete;

private:
    ResourceManager() = default;
    ~ResourceManager() = default;

    // Resource pools
    ResourcePool<Texture> m_TexturePool;
    ResourcePool<Model> m_ModelPool;
    ResourcePool<Material> m_MaterialPool;

    // Path-based caches (path -> handle)
    std::unordered_map<std::string, TextureHandle> m_TextureCache;
    std::unordered_map<std::string, ModelHandle> m_ModelCache;

    // Material name lookup
    std::unordered_map<std::string, MaterialHandle> m_MaterialLookup;

    // Statistics
    mutable ResourceStats m_Stats;
    bool m_Initialized = false;
};

} // namespace Resources
