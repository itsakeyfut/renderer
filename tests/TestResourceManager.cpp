/**
 * @file TestResourceManager.cpp
 * @brief Unit tests for ResourceManager and ResourcePool.
 */

#include "Resources/ResourceManager.h"
#include "Resources/ResourcePool.h"
#include "Resources/Texture.h"
#include "Resources/Model.h"
#include "Resources/Material.h"
#include "Core/Log.h"

#include <gtest/gtest.h>

// Initialize logging for tests
class ResourceManagerTestEnvironment : public ::testing::Environment {
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

// Register environment
testing::Environment* const resourceEnv =
    testing::AddGlobalTestEnvironment(new ResourceManagerTestEnvironment);

// =============================================================================
// ResourcePool Tests
// =============================================================================

class ResourcePoolTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        pool = std::make_unique<Resources::ResourcePool<Resources::Texture>>();
    }

    void TearDown() override
    {
        pool->Clear();
    }

    std::unique_ptr<Resources::ResourcePool<Resources::Texture>> pool;
};

TEST_F(ResourcePoolTest, AddAndGet)
{
    auto texture = Core::CreateRef<Resources::Texture>("test/texture.png");
    texture->SetDimensions(256, 256);

    auto handle = pool->Add(texture);

    EXPECT_TRUE(handle.IsValid());
    EXPECT_TRUE(pool->IsValid(handle));

    auto retrieved = pool->Get(handle);
    EXPECT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->GetPath(), "test/texture.png");
    EXPECT_EQ(retrieved->GetWidth(), 256u);
    EXPECT_EQ(retrieved->GetHeight(), 256u);
}

TEST_F(ResourcePoolTest, InvalidHandle)
{
    Resources::TextureHandle invalidHandle;

    EXPECT_FALSE(invalidHandle.IsValid());
    EXPECT_FALSE(pool->IsValid(invalidHandle));
    EXPECT_EQ(pool->Get(invalidHandle), nullptr);
}

TEST_F(ResourcePoolTest, ReferenceCount)
{
    auto texture = Core::CreateRef<Resources::Texture>("test/refcount.png");
    auto handle = pool->Add(texture);

    EXPECT_EQ(pool->GetRefCount(handle), 1u);

    pool->AddRef(handle);
    EXPECT_EQ(pool->GetRefCount(handle), 2u);

    pool->Release(handle);
    EXPECT_EQ(pool->GetRefCount(handle), 1u);

    pool->Release(handle);
    EXPECT_EQ(pool->GetRefCount(handle), 0u);
}

TEST_F(ResourcePoolTest, Remove)
{
    auto texture = Core::CreateRef<Resources::Texture>("test/remove.png");
    auto handle = pool->Add(texture);

    EXPECT_TRUE(pool->IsValid(handle));
    EXPECT_TRUE(pool->Remove(handle));
    EXPECT_FALSE(pool->IsValid(handle));
    EXPECT_EQ(pool->Get(handle), nullptr);
}

TEST_F(ResourcePoolTest, ReleaseUnused)
{
    auto tex1 = Core::CreateRef<Resources::Texture>("test/tex1.png");
    auto tex2 = Core::CreateRef<Resources::Texture>("test/tex2.png");

    auto handle1 = pool->Add(tex1);  // RefCount = 1
    auto handle2 = pool->Add(tex2);  // RefCount = 1

    EXPECT_EQ(pool->GetActiveCount(), 2u);

    // Release one texture completely
    pool->Release(handle1);  // RefCount = 0

    size_t released = pool->ReleaseUnused();
    EXPECT_EQ(released, 1u);
    EXPECT_EQ(pool->GetActiveCount(), 1u);
    EXPECT_FALSE(pool->IsValid(handle1));
    EXPECT_TRUE(pool->IsValid(handle2));
}

TEST_F(ResourcePoolTest, SlotReuse)
{
    auto tex1 = Core::CreateRef<Resources::Texture>("test/tex1.png");
    auto handle1 = pool->Add(tex1);
    uint32_t originalIndex = handle1.GetIndex();
    uint32_t originalGeneration = handle1.GetGeneration();

    pool->Remove(handle1);

    auto tex2 = Core::CreateRef<Resources::Texture>("test/tex2.png");
    auto handle2 = pool->Add(tex2);

    // Same index but different generation
    EXPECT_EQ(handle2.GetIndex(), originalIndex);
    EXPECT_NE(handle2.GetGeneration(), originalGeneration);

    // Original handle should be invalid
    EXPECT_FALSE(pool->IsValid(handle1));
    EXPECT_TRUE(pool->IsValid(handle2));
}

TEST_F(ResourcePoolTest, ForEach)
{
    pool->Add(Core::CreateRef<Resources::Texture>("test/tex1.png"));
    pool->Add(Core::CreateRef<Resources::Texture>("test/tex2.png"));
    pool->Add(Core::CreateRef<Resources::Texture>("test/tex3.png"));

    int count = 0;
    pool->ForEach([&count](Resources::TextureHandle, const Core::Ref<Resources::Texture>&) {
        count++;
    });

    EXPECT_EQ(count, 3);
}

// =============================================================================
// ResourceManager Singleton Tests
// =============================================================================

class ResourceManagerTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // Clear resources before each test
        Resources::ResourceManager::Instance().Clear();
    }

    void TearDown() override
    {
        Resources::ResourceManager::Instance().Clear();
    }
};

TEST_F(ResourceManagerTest, SingletonAccess)
{
    auto& rm1 = Resources::ResourceManager::Instance();
    auto& rm2 = Resources::ResourceManager::Instance();
    EXPECT_EQ(&rm1, &rm2);
}

// =============================================================================
// Texture Management Tests
// =============================================================================

TEST_F(ResourceManagerTest, LoadTexture)
{
    auto& rm = Resources::ResourceManager::Instance();

    auto handle = rm.LoadTexture("textures/diffuse.png");
    EXPECT_TRUE(handle.IsValid());
    EXPECT_TRUE(rm.IsTextureValid(handle));
    EXPECT_EQ(rm.GetTextureCount(), 1u);
}

TEST_F(ResourceManagerTest, TextureCaching)
{
    auto& rm = Resources::ResourceManager::Instance();

    auto handle1 = rm.LoadTexture("textures/cached.png");
    auto handle2 = rm.LoadTexture("textures/cached.png");

    // Same handle should be returned
    EXPECT_EQ(handle1, handle2);
    EXPECT_EQ(rm.GetTextureCount(), 1u);
}

TEST_F(ResourceManagerTest, TextureCacheStats)
{
    auto& rm = Resources::ResourceManager::Instance();
    rm.ResetStats();

    rm.LoadTexture("textures/test.png");  // Miss
    rm.LoadTexture("textures/test.png");  // Hit
    rm.LoadTexture("textures/test.png");  // Hit
    rm.LoadTexture("textures/other.png"); // Miss

    auto stats = rm.GetStats();
    EXPECT_EQ(stats.TextureCacheHits, 2u);
    EXPECT_EQ(stats.TextureCacheMisses, 2u);
}

TEST_F(ResourceManagerTest, GetTexture)
{
    auto& rm = Resources::ResourceManager::Instance();

    auto loaded = rm.LoadTexture("textures/lookup.png");
    auto found = rm.GetTexture("textures/lookup.png");
    auto notFound = rm.GetTexture("textures/nonexistent.png");

    EXPECT_EQ(loaded.GetIndex(), found.GetIndex());
    EXPECT_FALSE(notFound.IsValid());
}

TEST_F(ResourceManagerTest, GetTextureResource)
{
    auto& rm = Resources::ResourceManager::Instance();

    auto handle = rm.LoadTexture("textures/resource.png");
    auto resource = rm.GetTextureResource(handle);

    EXPECT_NE(resource, nullptr);
    EXPECT_EQ(resource->GetPath(), "textures/resource.png");
}

TEST_F(ResourceManagerTest, ReleaseTexture)
{
    auto& rm = Resources::ResourceManager::Instance();

    auto handle = rm.LoadTexture("textures/release.png");
    EXPECT_TRUE(rm.IsTextureValid(handle));

    rm.ReleaseTexture(handle);
    rm.ReleaseUnused();

    EXPECT_FALSE(rm.IsTextureValid(handle));
    EXPECT_EQ(rm.GetTextureCount(), 0u);
}

// =============================================================================
// Model Management Tests
// =============================================================================

TEST_F(ResourceManagerTest, LoadModel_NonExistent)
{
    auto& rm = Resources::ResourceManager::Instance();

    // Loading a non-existent model should return an invalid handle
    auto handle = rm.LoadModel("models/nonexistent.gltf");
    EXPECT_FALSE(handle.IsValid());
    EXPECT_EQ(rm.GetModelCount(), 0u);
}

TEST_F(ResourceManagerTest, LoadModel_UnsupportedFormat)
{
    auto& rm = Resources::ResourceManager::Instance();

    // Loading an unsupported format should return an invalid handle
    auto handle = rm.LoadModel("models/model.obj");
    EXPECT_FALSE(handle.IsValid());
    EXPECT_EQ(rm.GetModelCount(), 0u);
}

TEST_F(ResourceManagerTest, GetModelResource_InvalidHandle)
{
    auto& rm = Resources::ResourceManager::Instance();

    Resources::ModelHandle invalidHandle;
    auto resource = rm.GetModelResource(invalidHandle);

    EXPECT_EQ(resource, nullptr);
}

// =============================================================================
// Material Management Tests
// =============================================================================

TEST_F(ResourceManagerTest, CreateMaterial)
{
    auto& rm = Resources::ResourceManager::Instance();

    auto handle = rm.CreateMaterial("TestMaterial");
    EXPECT_TRUE(handle.IsValid());
    EXPECT_TRUE(rm.IsMaterialValid(handle));
    EXPECT_EQ(rm.GetMaterialCount(), 1u);
}

TEST_F(ResourceManagerTest, MaterialCaching)
{
    auto& rm = Resources::ResourceManager::Instance();

    auto handle1 = rm.CreateMaterial("CachedMaterial");
    auto handle2 = rm.CreateMaterial("CachedMaterial");

    // Same handle should be returned
    EXPECT_EQ(handle1, handle2);
    EXPECT_EQ(rm.GetMaterialCount(), 1u);
}

TEST_F(ResourceManagerTest, MaterialLookup)
{
    auto& rm = Resources::ResourceManager::Instance();

    auto created = rm.CreateMaterial("LookupMaterial");
    auto found = rm.GetMaterial("LookupMaterial");
    auto notFound = rm.GetMaterial("NonexistentMaterial");

    EXPECT_EQ(created.GetIndex(), found.GetIndex());
    EXPECT_FALSE(notFound.IsValid());
}

TEST_F(ResourceManagerTest, MaterialWithTextures)
{
    auto& rm = Resources::ResourceManager::Instance();

    auto diffuse = rm.LoadTexture("textures/diffuse.png");
    auto normal = rm.LoadTexture("textures/normal.png");

    Resources::MaterialDesc desc;
    desc.Name = "TexturedMaterial";
    desc.DiffuseTexture = diffuse;
    desc.NormalTexture = normal;

    auto handle = rm.CreateMaterial("TexturedMaterial", desc);
    auto material = rm.GetMaterialResource(handle);

    EXPECT_NE(material, nullptr);
    EXPECT_EQ(material->GetDiffuseTexture(), diffuse);
    EXPECT_EQ(material->GetNormalTexture(), normal);
}

// =============================================================================
// Resource Cleanup Tests
// =============================================================================

TEST_F(ResourceManagerTest, ReleaseUnused)
{
    auto& rm = Resources::ResourceManager::Instance();

    auto tex1 = rm.LoadTexture("textures/keep.png");
    auto tex2 = rm.LoadTexture("textures/release.png");

    // Release tex2
    rm.ReleaseTexture(tex2);

    size_t released = rm.ReleaseUnused();
    EXPECT_EQ(released, 1u);
    EXPECT_EQ(rm.GetTextureCount(), 1u);
    EXPECT_TRUE(rm.IsTextureValid(tex1));
}

TEST_F(ResourceManagerTest, Clear)
{
    auto& rm = Resources::ResourceManager::Instance();

    rm.LoadTexture("textures/clear1.png");
    rm.LoadTexture("textures/clear2.png");
    rm.CreateMaterial("ClearMaterial");

    rm.Clear();

    EXPECT_EQ(rm.GetTextureCount(), 0u);
    EXPECT_EQ(rm.GetModelCount(), 0u);
    EXPECT_EQ(rm.GetMaterialCount(), 0u);
}

// =============================================================================
// Statistics Tests
// =============================================================================

TEST_F(ResourceManagerTest, GetStats)
{
    auto& rm = Resources::ResourceManager::Instance();

    rm.LoadTexture("textures/stats.png");
    rm.CreateMaterial("StatsMaterial");

    auto stats = rm.GetStats();

    EXPECT_EQ(stats.TextureCount, 1u);
    EXPECT_EQ(stats.MaterialCount, 1u);
    EXPECT_GT(stats.TotalMemoryBytes, 0u);
}

TEST_F(ResourceManagerTest, ResetStats)
{
    auto& rm = Resources::ResourceManager::Instance();

    rm.LoadTexture("textures/reset1.png");
    rm.LoadTexture("textures/reset1.png");  // Cache hit

    auto stats = rm.GetStats();
    EXPECT_EQ(stats.TextureCacheHits, 1u);

    rm.ResetStats();
    stats = rm.GetStats();
    EXPECT_EQ(stats.TextureCacheHits, 0u);
}

// =============================================================================
// Handle Generation Tests
// =============================================================================

TEST_F(ResourceManagerTest, HandleGeneration)
{
    auto& rm = Resources::ResourceManager::Instance();

    // Load and release a texture
    auto handle1 = rm.LoadTexture("textures/generation.png");
    uint32_t originalGeneration = handle1.GetGeneration();

    rm.ReleaseTexture(handle1);
    rm.ReleaseUnused();

    // Load same texture again - should get different generation
    auto handle2 = rm.LoadTexture("textures/generation.png");

    EXPECT_NE(originalGeneration, handle2.GetGeneration());
    EXPECT_FALSE(rm.IsTextureValid(handle1));  // Old handle invalid
    EXPECT_TRUE(rm.IsTextureValid(handle2));   // New handle valid
}

// =============================================================================
// Multiple Resource Types Tests
// =============================================================================

TEST_F(ResourceManagerTest, MultipleResourceTypes)
{
    auto& rm = Resources::ResourceManager::Instance();

    auto tex = rm.LoadTexture("textures/multi.png");
    auto mat = rm.CreateMaterial("MultiMaterial");

    EXPECT_TRUE(rm.IsTextureValid(tex));
    EXPECT_TRUE(rm.IsMaterialValid(mat));

    // Indices can overlap between different resource types
    // This should not cause issues
    EXPECT_EQ(rm.GetTextureCount(), 1u);
    EXPECT_EQ(rm.GetMaterialCount(), 1u);
}
