/**
 * @file ResourceManager.cpp
 * @brief Implementation of the centralized resource management system.
 */

#include "Resources/ResourceManager.h"
#include "Resources/Texture.h"
#include "Resources/Model.h"
#include "Resources/Material.h"
#include "Resources/ModelLoader.h"
#include "Core/Log.h"

#include <filesystem>

namespace Resources {

ResourceManager& ResourceManager::Instance()
{
    static ResourceManager instance;
    if (!instance.m_Initialized) {
        instance.Initialize();
    }
    return instance;
}

void ResourceManager::Initialize()
{
    if (m_Initialized) {
        return;
    }

    LOG_INFO("ResourceManager initialized");
    m_Initialized = true;
}

void ResourceManager::Shutdown()
{
    if (!m_Initialized) {
        return;
    }

    Clear();
    LOG_INFO("ResourceManager shutdown");
    m_Initialized = false;
}

// =============================================================================
// Texture Management
// =============================================================================

TextureHandle ResourceManager::LoadTexture(const std::string& path, const TextureDesc& desc)
{
    // Check cache first
    auto it = m_TextureCache.find(path);
    if (it != m_TextureCache.end()) {
        // Found in cache - increment ref count and return
        if (m_TexturePool.IsValid(it->second)) {
            m_TexturePool.AddRef(it->second);
            m_Stats.TextureCacheHits++;
            LOG_DEBUG("Texture cache hit: {}", path);
            return it->second;
        }
        // Handle was invalidated, remove from cache
        m_TextureCache.erase(it);
    }

    m_Stats.TextureCacheMisses++;

    // Create new texture
    // Note: This is a stub implementation. Real texture loading
    // would load the file and create GPU resources here.
    auto texture = Core::CreateRef<Texture>(path);
    texture->SetFormat(desc.sRGB ? TextureFormat::RGBA8_SRGB : TextureFormat::RGBA8_UNORM);
    if (desc.GenerateMipmaps) {
        texture->SetMipLevels(1);  // Placeholder - would calculate based on dimensions
    }

    TextureHandle handle = m_TexturePool.Add(texture);
    m_TextureCache[path] = handle;

    LOG_DEBUG("Loaded texture: {} (handle index={})", path, handle.GetIndex());
    return handle;
}

TextureHandle ResourceManager::GetTexture(const std::string& path) const
{
    auto it = m_TextureCache.find(path);
    if (it != m_TextureCache.end() && m_TexturePool.IsValid(it->second)) {
        return it->second;
    }
    return TextureHandle();  // Invalid handle
}

Core::Ref<Texture> ResourceManager::GetTextureResource(TextureHandle handle) const
{
    return m_TexturePool.Get(handle);
}

void ResourceManager::ReleaseTexture(TextureHandle handle)
{
    m_TexturePool.Release(handle);
}

bool ResourceManager::IsTextureValid(TextureHandle handle) const
{
    return m_TexturePool.IsValid(handle);
}

// =============================================================================
// Model Management
// =============================================================================

ModelHandle ResourceManager::LoadModel(const std::string& path, const ModelDesc& desc)
{
    // Check cache first
    auto it = m_ModelCache.find(path);
    if (it != m_ModelCache.end()) {
        if (m_ModelPool.IsValid(it->second)) {
            m_ModelPool.AddRef(it->second);
            m_Stats.ModelCacheHits++;
            LOG_DEBUG("Model cache hit: {}", path);
            return it->second;
        }
        m_ModelCache.erase(it);
    }

    m_Stats.ModelCacheMisses++;

    // Determine file extension
    std::string extension = std::filesystem::path(path).extension().string();

    Core::Ref<Model> model;

    // Load model based on file format
    if (ModelLoader::IsFormatSupported(extension)) {
        ModelLoadOptions options;
        options.CalculateNormals = desc.CalculateNormals;
        options.CalculateTangents = desc.CalculateTangents;

        model = ModelLoader::LoadGLTF(path, options);
    }
    else {
        LOG_ERROR("Unsupported model format: {}", extension);
        return ModelHandle();
    }

    if (!model) {
        LOG_ERROR("Failed to load model: {}", path);
        return ModelHandle();
    }

    ModelHandle handle = m_ModelPool.Add(model);
    m_ModelCache[path] = handle;

    LOG_DEBUG("Loaded model: {} (handle index={})", path, handle.GetIndex());
    return handle;
}

ModelHandle ResourceManager::GetModel(const std::string& path) const
{
    auto it = m_ModelCache.find(path);
    if (it != m_ModelCache.end() && m_ModelPool.IsValid(it->second)) {
        return it->second;
    }
    return ModelHandle();
}

Core::Ref<Model> ResourceManager::GetModelResource(ModelHandle handle) const
{
    return m_ModelPool.Get(handle);
}

void ResourceManager::ReleaseModel(ModelHandle handle)
{
    m_ModelPool.Release(handle);
}

bool ResourceManager::IsModelValid(ModelHandle handle) const
{
    return m_ModelPool.IsValid(handle);
}

// =============================================================================
// Material Management
// =============================================================================

MaterialHandle ResourceManager::CreateMaterial(const std::string& name, const MaterialDesc& desc)
{
    // Check if material already exists
    auto it = m_MaterialLookup.find(name);
    if (it != m_MaterialLookup.end() && m_MaterialPool.IsValid(it->second)) {
        m_MaterialPool.AddRef(it->second);
        LOG_DEBUG("Material already exists: {}", name);
        return it->second;
    }

    auto material = Core::CreateRef<Material>(name);

    // Apply descriptor properties
    if (desc.DiffuseTexture.IsValid()) {
        material->SetDiffuseTexture(desc.DiffuseTexture);
    }
    if (desc.NormalTexture.IsValid()) {
        material->SetNormalTexture(desc.NormalTexture);
    }
    if (desc.MetallicRoughnessTexture.IsValid()) {
        material->SetMetallicRoughnessTexture(desc.MetallicRoughnessTexture);
    }

    MaterialHandle handle = m_MaterialPool.Add(material);
    m_MaterialLookup[name] = handle;

    LOG_DEBUG("Created material: {} (handle index={})", name, handle.GetIndex());
    return handle;
}

MaterialHandle ResourceManager::GetMaterial(const std::string& name) const
{
    auto it = m_MaterialLookup.find(name);
    if (it != m_MaterialLookup.end() && m_MaterialPool.IsValid(it->second)) {
        return it->second;
    }
    return MaterialHandle();
}

Core::Ref<Material> ResourceManager::GetMaterialResource(MaterialHandle handle) const
{
    return m_MaterialPool.Get(handle);
}

void ResourceManager::ReleaseMaterial(MaterialHandle handle)
{
    m_MaterialPool.Release(handle);
}

bool ResourceManager::IsMaterialValid(MaterialHandle handle) const
{
    return m_MaterialPool.IsValid(handle);
}

// =============================================================================
// Resource Cleanup
// =============================================================================

size_t ResourceManager::ReleaseUnused()
{
    size_t released = 0;

    // Clean up unused textures and update cache
    m_TexturePool.ForEach([this](TextureHandle handle, const Core::Ref<Texture>& texture) {
        if (m_TexturePool.GetRefCount(handle) == 0 && texture) {
            // Remove from cache
            auto it = m_TextureCache.find(texture->GetPath());
            if (it != m_TextureCache.end() && it->second == handle) {
                m_TextureCache.erase(it);
            }
        }
    });
    released += m_TexturePool.ReleaseUnused();

    // Clean up unused models and update cache
    m_ModelPool.ForEach([this](ModelHandle handle, const Core::Ref<Model>& model) {
        if (m_ModelPool.GetRefCount(handle) == 0 && model) {
            auto it = m_ModelCache.find(model->GetPath());
            if (it != m_ModelCache.end() && it->second == handle) {
                m_ModelCache.erase(it);
            }
        }
    });
    released += m_ModelPool.ReleaseUnused();

    // Clean up unused materials and update lookup
    m_MaterialPool.ForEach([this](MaterialHandle handle, const Core::Ref<Material>& material) {
        if (m_MaterialPool.GetRefCount(handle) == 0 && material) {
            auto it = m_MaterialLookup.find(material->GetName());
            if (it != m_MaterialLookup.end() && it->second == handle) {
                m_MaterialLookup.erase(it);
            }
        }
    });
    released += m_MaterialPool.ReleaseUnused();

    if (released > 0) {
        LOG_DEBUG("Released {} unused resources", released);
    }

    return released;
}

void ResourceManager::Clear()
{
    m_TexturePool.Clear();
    m_ModelPool.Clear();
    m_MaterialPool.Clear();

    m_TextureCache.clear();
    m_ModelCache.clear();
    m_MaterialLookup.clear();

    ResetStats();

    LOG_DEBUG("Cleared all resources");
}

// =============================================================================
// Statistics
// =============================================================================

size_t ResourceManager::GetTextureCount() const
{
    return m_TexturePool.GetActiveCount();
}

size_t ResourceManager::GetModelCount() const
{
    return m_ModelPool.GetActiveCount();
}

size_t ResourceManager::GetMaterialCount() const
{
    return m_MaterialPool.GetActiveCount();
}

size_t ResourceManager::GetMemoryUsage() const
{
    size_t total = 0;

    m_TexturePool.ForEach([&total](TextureHandle, const Core::Ref<Texture>& texture) {
        if (texture) {
            total += texture->GetMemorySize();
        }
    });

    m_ModelPool.ForEach([&total](ModelHandle, const Core::Ref<Model>& model) {
        if (model) {
            total += model->GetMemorySize();
        }
    });

    m_MaterialPool.ForEach([&total](MaterialHandle, const Core::Ref<Material>& material) {
        if (material) {
            total += material->GetMemorySize();
        }
    });

    return total;
}

ResourceStats ResourceManager::GetStats() const
{
    m_Stats.TextureCount = GetTextureCount();
    m_Stats.ModelCount = GetModelCount();
    m_Stats.MaterialCount = GetMaterialCount();
    m_Stats.TotalMemoryBytes = GetMemoryUsage();
    return m_Stats;
}

void ResourceManager::ResetStats()
{
    m_Stats = ResourceStats();
}

} // namespace Resources
