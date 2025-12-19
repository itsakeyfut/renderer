/**
 * @file TestScene.cpp
 * @brief Unit tests for the Scene module (Transform, Entity, Scene, Light).
 */

#include <gtest/gtest.h>
#include <glm/glm.hpp>
#include <glm/gtc/epsilon.hpp>
#include <glm/gtc/constants.hpp>

#include "Scene/Transform.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Scene/Light.h"
#include "Resources/ResourceHandle.h"

// =============================================================================
// Transform Tests
// =============================================================================

class TransformTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}

    // Helper to compare vectors with epsilon
    static bool VecEqual(const glm::vec3& a, const glm::vec3& b, float epsilon = 0.0001f)
    {
        return glm::all(glm::epsilonEqual(a, b, epsilon));
    }

    // Helper to compare matrices with epsilon
    static bool MatEqual(const glm::mat4& a, const glm::mat4& b, float epsilon = 0.0001f)
    {
        for (int i = 0; i < 4; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                if (std::abs(a[i][j] - b[i][j]) > epsilon)
                {
                    return false;
                }
            }
        }
        return true;
    }
};

TEST_F(TransformTest, DefaultConstructor)
{
    Scene::Transform transform;

    EXPECT_TRUE(VecEqual(transform.GetPosition(), glm::vec3(0.0f)));
    EXPECT_TRUE(VecEqual(transform.GetScale(), glm::vec3(1.0f)));
    EXPECT_EQ(transform.GetRotation(), glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    EXPECT_TRUE(MatEqual(transform.GetLocalMatrix(), glm::mat4(1.0f)));
}

TEST_F(TransformTest, PositionConstructor)
{
    glm::vec3 pos(1.0f, 2.0f, 3.0f);
    Scene::Transform transform(pos);

    EXPECT_TRUE(VecEqual(transform.GetPosition(), pos));
}

TEST_F(TransformTest, SetPosition)
{
    Scene::Transform transform;
    glm::vec3 pos(5.0f, 10.0f, 15.0f);
    transform.SetPosition(pos);

    EXPECT_TRUE(VecEqual(transform.GetPosition(), pos));
    EXPECT_TRUE(VecEqual(transform.GetWorldPosition(), pos));
}

TEST_F(TransformTest, Translate)
{
    Scene::Transform transform;
    transform.SetPosition(glm::vec3(1.0f, 0.0f, 0.0f));
    transform.Translate(glm::vec3(0.0f, 2.0f, 0.0f));

    EXPECT_TRUE(VecEqual(transform.GetPosition(), glm::vec3(1.0f, 2.0f, 0.0f)));
}

TEST_F(TransformTest, SetScale)
{
    Scene::Transform transform;
    transform.SetScale(glm::vec3(2.0f, 3.0f, 4.0f));

    EXPECT_TRUE(VecEqual(transform.GetScale(), glm::vec3(2.0f, 3.0f, 4.0f)));
}

TEST_F(TransformTest, UniformScale)
{
    Scene::Transform transform;
    transform.SetScale(5.0f);

    EXPECT_TRUE(VecEqual(transform.GetScale(), glm::vec3(5.0f)));
}

TEST_F(TransformTest, SetRotation)
{
    Scene::Transform transform;
    glm::quat rotation = glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    transform.SetRotation(rotation);

    // Verify forward vector changed
    // Initial forward is (0, 0, -1), rotating 90 degrees around Y gives (-1, 0, 0)
    glm::vec3 forward = transform.GetForward();
    EXPECT_TRUE(VecEqual(forward, glm::vec3(-1.0f, 0.0f, 0.0f), 0.001f));
}

TEST_F(TransformTest, DirectionVectors)
{
    Scene::Transform transform;

    // Default orientation: looking down negative Z
    EXPECT_TRUE(VecEqual(transform.GetForward(), glm::vec3(0.0f, 0.0f, -1.0f)));
    EXPECT_TRUE(VecEqual(transform.GetRight(), glm::vec3(1.0f, 0.0f, 0.0f)));
    EXPECT_TRUE(VecEqual(transform.GetUp(), glm::vec3(0.0f, 1.0f, 0.0f)));
}

TEST_F(TransformTest, LocalMatrix)
{
    Scene::Transform transform;
    transform.SetPosition(glm::vec3(1.0f, 2.0f, 3.0f));
    transform.SetScale(2.0f);

    glm::mat4 matrix = transform.GetLocalMatrix();

    // Translation is in column 3
    EXPECT_FLOAT_EQ(matrix[3][0], 1.0f);
    EXPECT_FLOAT_EQ(matrix[3][1], 2.0f);
    EXPECT_FLOAT_EQ(matrix[3][2], 3.0f);

    // Scale is on diagonal (with identity rotation)
    EXPECT_FLOAT_EQ(matrix[0][0], 2.0f);
    EXPECT_FLOAT_EQ(matrix[1][1], 2.0f);
    EXPECT_FLOAT_EQ(matrix[2][2], 2.0f);
}

TEST_F(TransformTest, ParentChildHierarchy)
{
    Scene::Transform parent;
    Scene::Transform child;

    parent.SetPosition(glm::vec3(10.0f, 0.0f, 0.0f));
    child.SetPosition(glm::vec3(5.0f, 0.0f, 0.0f));

    child.SetParent(&parent);

    EXPECT_EQ(child.GetParent(), &parent);
    EXPECT_EQ(parent.GetChildCount(), 1u);
    EXPECT_TRUE(parent.HasChildren());

    // World position should be parent + local
    glm::vec3 worldPos = child.GetWorldPosition();
    EXPECT_TRUE(VecEqual(worldPos, glm::vec3(15.0f, 0.0f, 0.0f)));
}

TEST_F(TransformTest, UnparentChild)
{
    Scene::Transform parent;
    Scene::Transform child;

    child.SetParent(&parent);
    EXPECT_EQ(parent.GetChildCount(), 1u);

    child.SetParent(nullptr);
    EXPECT_EQ(parent.GetChildCount(), 0u);
    EXPECT_EQ(child.GetParent(), nullptr);
}

TEST_F(TransformTest, CircularReferencePreventionSelf)
{
    Scene::Transform transform;

    // Attempting to set self as parent should be silently rejected
    transform.SetParent(&transform);

    EXPECT_EQ(transform.GetParent(), nullptr);
}

TEST_F(TransformTest, CircularReferencePreventionDescendant)
{
    Scene::Transform grandparent;
    Scene::Transform parent;
    Scene::Transform child;

    parent.SetParent(&grandparent);
    child.SetParent(&parent);

    // Attempting to set grandparent's parent to child would create a cycle
    grandparent.SetParent(&child);

    // Should be rejected - grandparent should remain without parent
    EXPECT_EQ(grandparent.GetParent(), nullptr);

    // Hierarchy should remain intact
    EXPECT_EQ(parent.GetParent(), &grandparent);
    EXPECT_EQ(child.GetParent(), &parent);
}

TEST_F(TransformTest, NestedHierarchy)
{
    Scene::Transform grandparent;
    Scene::Transform parent;
    Scene::Transform child;

    grandparent.SetPosition(glm::vec3(100.0f, 0.0f, 0.0f));
    parent.SetPosition(glm::vec3(10.0f, 0.0f, 0.0f));
    child.SetPosition(glm::vec3(1.0f, 0.0f, 0.0f));

    parent.SetParent(&grandparent);
    child.SetParent(&parent);

    glm::vec3 worldPos = child.GetWorldPosition();
    EXPECT_TRUE(VecEqual(worldPos, glm::vec3(111.0f, 0.0f, 0.0f)));
}

TEST_F(TransformTest, NormalMatrixIdentity)
{
    Scene::Transform transform;

    // With identity transform, normal matrix should also be identity
    glm::mat4 normalMatrix = transform.GetNormalMatrix();
    EXPECT_TRUE(MatEqual(normalMatrix, glm::mat4(1.0f)));
}

TEST_F(TransformTest, NormalMatrixWithUniformScale)
{
    Scene::Transform transform;
    transform.SetScale(2.0f);

    glm::mat4 normalMatrix = transform.GetNormalMatrix();

    // With uniform scale of 2, the normal matrix should have 0.5 on the diagonal
    // (inverse of scale, since normal matrix = transpose(inverse(M)))
    EXPECT_FLOAT_EQ(normalMatrix[0][0], 0.5f);
    EXPECT_FLOAT_EQ(normalMatrix[1][1], 0.5f);
    EXPECT_FLOAT_EQ(normalMatrix[2][2], 0.5f);
}

TEST_F(TransformTest, NormalMatrixWithNonUniformScale)
{
    Scene::Transform transform;
    transform.SetScale(glm::vec3(1.0f, 2.0f, 1.0f));

    // A normal pointing in Y direction
    glm::vec3 normal(0.0f, 1.0f, 0.0f);

    glm::mat4 normalMatrix = transform.GetNormalMatrix();
    glm::vec3 transformedNormal = glm::normalize(glm::vec3(normalMatrix * glm::vec4(normal, 0.0f)));

    // Normal should still point in Y direction after transformation
    // The normal matrix correctly handles non-uniform scaling
    EXPECT_TRUE(VecEqual(transformedNormal, glm::vec3(0.0f, 1.0f, 0.0f)));
}

TEST_F(TransformTest, NormalMatrixWithRotation)
{
    Scene::Transform transform;
    // Rotate 90 degrees around Y axis
    transform.SetRotation(glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)));

    // A normal pointing in Z direction
    glm::vec3 normal(0.0f, 0.0f, 1.0f);

    glm::mat4 normalMatrix = transform.GetNormalMatrix();
    glm::vec3 transformedNormal = glm::normalize(glm::vec3(normalMatrix * glm::vec4(normal, 0.0f)));

    // After 90 degree rotation around Y (right-hand rule), (0,0,1) becomes (1,0,0)
    EXPECT_TRUE(VecEqual(transformedNormal, glm::vec3(1.0f, 0.0f, 0.0f), 0.001f));
}

TEST_F(TransformTest, NormalMatrixWithHierarchy)
{
    Scene::Transform parent;
    Scene::Transform child;

    parent.SetScale(glm::vec3(2.0f, 1.0f, 1.0f));
    child.SetParent(&parent);

    glm::mat4 normalMatrix = child.GetNormalMatrix();

    // With parent having non-uniform scale, normal matrix should account for it
    EXPECT_FLOAT_EQ(normalMatrix[0][0], 0.5f);
    EXPECT_FLOAT_EQ(normalMatrix[1][1], 1.0f);
    EXPECT_FLOAT_EQ(normalMatrix[2][2], 1.0f);
}

// =============================================================================
// Entity Tests
// =============================================================================

class EntityTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(EntityTest, Construction)
{
    Scene::Entity entity(42);

    EXPECT_EQ(entity.GetID(), 42u);
    EXPECT_TRUE(entity.IsActive());
    EXPECT_TRUE(entity.IsVisible());
    EXPECT_TRUE(entity.GetName().empty());
}

TEST_F(EntityTest, NamedConstruction)
{
    Scene::Entity entity(1, "TestEntity");

    EXPECT_EQ(entity.GetID(), 1u);
    EXPECT_EQ(entity.GetName(), "TestEntity");
}

TEST_F(EntityTest, SetActive)
{
    Scene::Entity entity(1);

    entity.SetActive(false);
    EXPECT_FALSE(entity.IsActive());

    entity.SetActive(true);
    EXPECT_TRUE(entity.IsActive());
}

TEST_F(EntityTest, SetVisible)
{
    Scene::Entity entity(1);

    entity.SetVisible(false);
    EXPECT_FALSE(entity.IsVisible());

    entity.SetVisible(true);
    EXPECT_TRUE(entity.IsVisible());
}

TEST_F(EntityTest, Transform)
{
    Scene::Entity entity(1);

    entity.GetTransform().SetPosition(glm::vec3(1.0f, 2.0f, 3.0f));

    const Scene::Entity& constEntity = entity;
    EXPECT_EQ(constEntity.GetTransform().GetPosition(), glm::vec3(1.0f, 2.0f, 3.0f));
}

TEST_F(EntityTest, MeshHandle)
{
    Scene::Entity entity(1);

    EXPECT_FALSE(entity.HasMesh());

    Resources::ModelHandle mesh(0, 1);
    entity.SetMesh(mesh);

    EXPECT_TRUE(entity.HasMesh());
    EXPECT_EQ(entity.GetMesh(), mesh);
}

TEST_F(EntityTest, MaterialHandle)
{
    Scene::Entity entity(1);

    EXPECT_FALSE(entity.HasMaterial());

    Resources::MaterialHandle material(5, 2);
    entity.SetMaterial(material);

    EXPECT_TRUE(entity.HasMaterial());
    EXPECT_EQ(entity.GetMaterial(), material);
}

TEST_F(EntityTest, Layer)
{
    Scene::Entity entity(1);

    EXPECT_EQ(entity.GetLayer(), 1u);  // Default layer

    entity.SetLayer(0xFF);
    EXPECT_EQ(entity.GetLayer(), 0xFFu);
}

// =============================================================================
// Scene Tests
// =============================================================================

class SceneTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(SceneTest, DefaultConstruction)
{
    Scene::Scene scene;

    EXPECT_TRUE(scene.GetName().empty());
    EXPECT_EQ(scene.GetEntityCount(), 0u);
    EXPECT_EQ(scene.GetPointLightCount(), 0u);
    EXPECT_EQ(scene.GetSpotLightCount(), 0u);
}

TEST_F(SceneTest, NamedConstruction)
{
    Scene::Scene scene("TestScene");

    EXPECT_EQ(scene.GetName(), "TestScene");
}

TEST_F(SceneTest, CreateEntity)
{
    Scene::Scene scene;

    Scene::Entity* entity = scene.CreateEntity();

    EXPECT_NE(entity, nullptr);
    EXPECT_EQ(scene.GetEntityCount(), 1u);
    EXPECT_EQ(entity->GetID(), 0u);
}

TEST_F(SceneTest, CreateNamedEntity)
{
    Scene::Scene scene;

    Scene::Entity* entity = scene.CreateEntity("Player");

    EXPECT_NE(entity, nullptr);
    EXPECT_EQ(entity->GetName(), "Player");
}

TEST_F(SceneTest, FindEntityByID)
{
    Scene::Scene scene;

    Scene::Entity* entity1 = scene.CreateEntity();
    Scene::Entity* entity2 = scene.CreateEntity();

    EXPECT_EQ(scene.FindEntity(entity1->GetID()), entity1);
    EXPECT_EQ(scene.FindEntity(entity2->GetID()), entity2);
    EXPECT_EQ(scene.FindEntity(9999), nullptr);
}

TEST_F(SceneTest, FindEntityByName)
{
    Scene::Scene scene;

    Scene::Entity* player = scene.CreateEntity("Player");
    Scene::Entity* enemy = scene.CreateEntity("Enemy");

    EXPECT_EQ(scene.FindEntity("Player"), player);
    EXPECT_EQ(scene.FindEntity("Enemy"), enemy);
    EXPECT_EQ(scene.FindEntity("NonExistent"), nullptr);
}

TEST_F(SceneTest, DestroyEntityByPointer)
{
    Scene::Scene scene;

    Scene::Entity* entity = scene.CreateEntity("ToDelete");
    EXPECT_EQ(scene.GetEntityCount(), 1u);

    scene.DestroyEntity(entity);
    EXPECT_EQ(scene.GetEntityCount(), 0u);
    EXPECT_EQ(scene.FindEntity("ToDelete"), nullptr);
}

TEST_F(SceneTest, DestroyEntityByID)
{
    Scene::Scene scene;

    Scene::Entity* entity = scene.CreateEntity();
    uint32_t id = entity->GetID();

    scene.DestroyEntity(id);
    EXPECT_EQ(scene.GetEntityCount(), 0u);
    EXPECT_EQ(scene.FindEntity(id), nullptr);
}

TEST_F(SceneTest, ForEachActiveEntity)
{
    Scene::Scene scene;

    scene.CreateEntity()->SetActive(true);
    scene.CreateEntity()->SetActive(false);
    scene.CreateEntity()->SetActive(true);

    int activeCount = 0;
    scene.ForEachActiveEntity([&activeCount](const Scene::Entity&) {
        activeCount++;
    });

    EXPECT_EQ(activeCount, 2);
}

TEST_F(SceneTest, ForEachVisibleEntity)
{
    Scene::Scene scene;

    auto* e1 = scene.CreateEntity();
    e1->SetActive(true);
    e1->SetVisible(true);

    auto* e2 = scene.CreateEntity();
    e2->SetActive(true);
    e2->SetVisible(false);

    auto* e3 = scene.CreateEntity();
    e3->SetActive(false);
    e3->SetVisible(true);

    int visibleCount = 0;
    scene.ForEachVisibleEntity([&visibleCount](Scene::Entity&) {
        visibleCount++;
    });

    EXPECT_EQ(visibleCount, 1);
}

TEST_F(SceneTest, ClearEntities)
{
    Scene::Scene scene;

    scene.CreateEntity("A");
    scene.CreateEntity("B");
    scene.CreateEntity("C");

    EXPECT_EQ(scene.GetEntityCount(), 3u);

    scene.ClearEntities();

    EXPECT_EQ(scene.GetEntityCount(), 0u);
}

// =============================================================================
// Light Tests
// =============================================================================

class LightTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}

    static bool VecEqual(const glm::vec3& a, const glm::vec3& b, float epsilon = 0.0001f)
    {
        return glm::all(glm::epsilonEqual(a, b, epsilon));
    }
};

TEST_F(LightTest, DirectionalLightDefaults)
{
    Scene::DirectionalLight light;

    EXPECT_TRUE(VecEqual(light.Direction, glm::vec3(0.0f, -1.0f, 0.0f)));
    EXPECT_FLOAT_EQ(light.Intensity, 1.0f);
    EXPECT_TRUE(VecEqual(light.Color, glm::vec3(1.0f)));
}

TEST_F(LightTest, DirectionalLightCreate)
{
    auto light = Scene::DirectionalLight::Create(
        glm::vec3(1.0f, -1.0f, 0.0f),
        glm::vec3(1.0f, 0.9f, 0.8f),
        2.0f
    );

    // Direction should be normalized
    float len = glm::length(light.Direction);
    EXPECT_FLOAT_EQ(len, 1.0f);

    EXPECT_FLOAT_EQ(light.Intensity, 2.0f);
    EXPECT_TRUE(VecEqual(light.Color, glm::vec3(1.0f, 0.9f, 0.8f)));
}

TEST_F(LightTest, PointLightDefaults)
{
    Scene::PointLight light;

    EXPECT_TRUE(VecEqual(light.Position, glm::vec3(0.0f)));
    EXPECT_FLOAT_EQ(light.Radius, 10.0f);
    EXPECT_FLOAT_EQ(light.Intensity, 1.0f);
    EXPECT_TRUE(VecEqual(light.Color, glm::vec3(1.0f)));
}

TEST_F(LightTest, PointLightCreate)
{
    auto light = Scene::PointLight::Create(
        glm::vec3(5.0f, 10.0f, 15.0f),
        glm::vec3(1.0f, 0.0f, 0.0f),
        5.0f,
        20.0f
    );

    EXPECT_TRUE(VecEqual(light.Position, glm::vec3(5.0f, 10.0f, 15.0f)));
    EXPECT_TRUE(VecEqual(light.Color, glm::vec3(1.0f, 0.0f, 0.0f)));
    EXPECT_FLOAT_EQ(light.Intensity, 5.0f);
    EXPECT_FLOAT_EQ(light.Radius, 20.0f);
}

TEST_F(LightTest, SpotLightDefaults)
{
    Scene::SpotLight light;

    EXPECT_TRUE(VecEqual(light.Position, glm::vec3(0.0f)));
    EXPECT_TRUE(VecEqual(light.Direction, glm::vec3(0.0f, -1.0f, 0.0f)));
    EXPECT_FLOAT_EQ(light.Intensity, 1.0f);
}

TEST_F(LightTest, SpotLightCreate)
{
    auto light = Scene::SpotLight::Create(
        glm::vec3(0.0f, 10.0f, 0.0f),
        glm::vec3(0.0f, -1.0f, 0.0f),
        glm::vec3(1.0f),
        2.0f,
        20.0f,
        30.0f
    );

    EXPECT_TRUE(VecEqual(light.Position, glm::vec3(0.0f, 10.0f, 0.0f)));
    EXPECT_FLOAT_EQ(light.Intensity, 2.0f);

    // Check cone angles are cosines
    EXPECT_FLOAT_EQ(light.InnerConeAngle, glm::cos(glm::radians(20.0f)));
    EXPECT_FLOAT_EQ(light.OuterConeAngle, glm::cos(glm::radians(30.0f)));
}

TEST_F(LightTest, SceneAddPointLight)
{
    Scene::Scene scene;

    auto light = Scene::PointLight::Create(
        glm::vec3(0.0f, 5.0f, 0.0f),
        glm::vec3(1.0f, 0.5f, 0.0f),
        3.0f,
        15.0f
    );

    size_t index = scene.AddPointLight(light);

    EXPECT_EQ(index, 0u);
    EXPECT_EQ(scene.GetPointLightCount(), 1u);
    EXPECT_TRUE(VecEqual(scene.GetPointLight(0).Position, glm::vec3(0.0f, 5.0f, 0.0f)));
}

TEST_F(LightTest, SceneAddSpotLight)
{
    Scene::Scene scene;

    auto light = Scene::SpotLight::Create(
        glm::vec3(0.0f, 10.0f, 0.0f),
        glm::vec3(0.0f, -1.0f, 0.0f)
    );

    size_t index = scene.AddSpotLight(light);

    EXPECT_EQ(index, 0u);
    EXPECT_EQ(scene.GetSpotLightCount(), 1u);
}

TEST_F(LightTest, SceneRemovePointLight)
{
    Scene::Scene scene;

    scene.AddPointLight(Scene::PointLight::Create(glm::vec3(0.0f)));
    scene.AddPointLight(Scene::PointLight::Create(glm::vec3(1.0f)));

    EXPECT_EQ(scene.GetPointLightCount(), 2u);

    scene.RemovePointLight(0);

    EXPECT_EQ(scene.GetPointLightCount(), 1u);
    EXPECT_TRUE(VecEqual(scene.GetPointLight(0).Position, glm::vec3(1.0f)));
}

TEST_F(LightTest, SceneSetDirectionalLight)
{
    Scene::Scene scene;

    auto light = Scene::DirectionalLight::Create(
        glm::vec3(0.5f, -1.0f, 0.5f),
        glm::vec3(1.0f, 0.95f, 0.9f),
        1.5f
    );

    scene.SetDirectionalLight(light);

    EXPECT_FLOAT_EQ(scene.GetDirectionalLight().Intensity, 1.5f);
}

TEST_F(LightTest, SceneBuildLightUBO)
{
    Scene::Scene scene;

    scene.SetDirectionalLight(Scene::DirectionalLight::Create(
        glm::vec3(0.0f, -1.0f, 0.0f),
        glm::vec3(1.0f),
        2.0f
    ));

    scene.AddPointLight(Scene::PointLight::Create(glm::vec3(0.0f)));
    scene.AddPointLight(Scene::PointLight::Create(glm::vec3(1.0f)));
    scene.AddSpotLight(Scene::SpotLight::Create(glm::vec3(0.0f), glm::vec3(0.0f, -1.0f, 0.0f)));

    auto ubo = scene.BuildLightUBO();

    EXPECT_FLOAT_EQ(ubo.DirectionalLightData.Intensity, 2.0f);
    EXPECT_EQ(ubo.NumPointLights, 2u);
    EXPECT_EQ(ubo.NumSpotLights, 1u);
}

TEST_F(LightTest, SceneClearLights)
{
    Scene::Scene scene;

    scene.AddPointLight(Scene::PointLight::Create(glm::vec3(0.0f)));
    scene.AddSpotLight(Scene::SpotLight::Create(glm::vec3(0.0f), glm::vec3(0.0f, -1.0f, 0.0f)));

    scene.ClearLights();

    EXPECT_EQ(scene.GetPointLightCount(), 0u);
    EXPECT_EQ(scene.GetSpotLightCount(), 0u);
}

// =============================================================================
// ResourceHandle Tests
// =============================================================================

class ResourceHandleTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ResourceHandleTest, DefaultInvalid)
{
    Resources::ModelHandle handle;

    EXPECT_FALSE(handle.IsValid());
    EXPECT_FALSE(static_cast<bool>(handle));
}

TEST_F(ResourceHandleTest, ValidHandle)
{
    Resources::ModelHandle handle(5, 1);

    EXPECT_TRUE(handle.IsValid());
    EXPECT_TRUE(static_cast<bool>(handle));
    EXPECT_EQ(handle.GetIndex(), 5u);
    EXPECT_EQ(handle.GetGeneration(), 1u);
}

TEST_F(ResourceHandleTest, Invalidate)
{
    Resources::ModelHandle handle(5, 1);

    handle.Invalidate();

    EXPECT_FALSE(handle.IsValid());
}

TEST_F(ResourceHandleTest, Equality)
{
    Resources::ModelHandle a(1, 1);
    Resources::ModelHandle b(1, 1);
    Resources::ModelHandle c(1, 2);
    Resources::ModelHandle d(2, 1);

    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
    EXPECT_NE(a, d);
}
