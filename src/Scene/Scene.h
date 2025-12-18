/**
 * @file Scene.h
 * @brief Scene container for managing entities and lights.
 *
 * The Scene class provides a centralized container for all game objects
 * (entities) and lights in the current level/scene.
 */

#pragma once

#include "Scene/Entity.h"
#include "Scene/Light.h"
#include "Core/Types.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Scene {

/**
 * @brief Scene container for entities and lights.
 *
 * Manages the lifecycle of entities, provides lookup by ID or name,
 * and maintains light data for rendering.
 */
class Scene {
public:
    /**
     * @brief Default constructor.
     */
    Scene() = default;

    /**
     * @brief Constructs a scene with a name.
     * @param name Display name for the scene.
     */
    explicit Scene(const std::string& name)
        : m_Name(name)
    {
    }

    // Non-copyable
    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;

    ~Scene() = default;

    // =========================================================================
    // Scene Properties
    // =========================================================================

    /**
     * @brief Gets the scene name.
     * @return Scene name string.
     */
    const std::string& GetName() const { return m_Name; }

    /**
     * @brief Sets the scene name.
     * @param name New name for the scene.
     */
    void SetName(const std::string& name) { m_Name = name; }

    // =========================================================================
    // Entity Management
    // =========================================================================

    /**
     * @brief Creates a new entity in the scene.
     * @return Pointer to the newly created entity.
     */
    Entity* CreateEntity()
    {
        uint32_t id = m_NextEntityID++;
        auto entity = std::make_unique<Entity>(id);
        Entity* ptr = entity.get();
        m_Entities.push_back(std::move(entity));
        return ptr;
    }

    /**
     * @brief Creates a new named entity in the scene.
     * @param name Name for the entity.
     * @return Pointer to the newly created entity.
     */
    Entity* CreateEntity(const std::string& name)
    {
        uint32_t id = m_NextEntityID++;
        auto entity = std::make_unique<Entity>(id, name);
        Entity* ptr = entity.get();

        if (!name.empty())
        {
            m_NamedEntities[name] = ptr;
        }

        m_Entities.push_back(std::move(entity));
        return ptr;
    }

    /**
     * @brief Destroys an entity by pointer.
     * @param entity Pointer to the entity to destroy.
     */
    void DestroyEntity(Entity* entity)
    {
        if (entity == nullptr)
        {
            return;
        }

        // Remove from named map if present
        if (!entity->GetName().empty())
        {
            m_NamedEntities.erase(entity->GetName());
        }

        // Remove from vector
        auto it = std::find_if(m_Entities.begin(), m_Entities.end(),
            [entity](const std::unique_ptr<Entity>& e) {
                return e.get() == entity;
            });

        if (it != m_Entities.end())
        {
            m_Entities.erase(it);
        }
    }

    /**
     * @brief Destroys an entity by ID.
     * @param id ID of the entity to destroy.
     */
    void DestroyEntity(uint32_t id)
    {
        DestroyEntity(FindEntity(id));
    }

    /**
     * @brief Finds an entity by ID.
     * @param id Entity ID to search for.
     * @return Pointer to the entity, or nullptr if not found.
     */
    Entity* FindEntity(uint32_t id)
    {
        for (auto& entity : m_Entities)
        {
            if (entity->GetID() == id)
            {
                return entity.get();
            }
        }
        return nullptr;
    }

    /**
     * @brief Finds an entity by ID (const).
     * @param id Entity ID to search for.
     * @return Const pointer to the entity, or nullptr if not found.
     */
    const Entity* FindEntity(uint32_t id) const
    {
        for (const auto& entity : m_Entities)
        {
            if (entity->GetID() == id)
            {
                return entity.get();
            }
        }
        return nullptr;
    }

    /**
     * @brief Finds an entity by name.
     * @param name Entity name to search for.
     * @return Pointer to the entity, or nullptr if not found.
     */
    Entity* FindEntity(const std::string& name)
    {
        auto it = m_NamedEntities.find(name);
        if (it != m_NamedEntities.end())
        {
            return it->second;
        }
        return nullptr;
    }

    /**
     * @brief Finds an entity by name (const).
     * @param name Entity name to search for.
     * @return Const pointer to the entity, or nullptr if not found.
     */
    const Entity* FindEntity(const std::string& name) const
    {
        auto it = m_NamedEntities.find(name);
        if (it != m_NamedEntities.end())
        {
            return it->second;
        }
        return nullptr;
    }

    /**
     * @brief Gets all entities in the scene.
     * @return Reference to the entity container.
     */
    const std::vector<std::unique_ptr<Entity>>& GetEntities() const
    {
        return m_Entities;
    }

    /**
     * @brief Gets the number of entities in the scene.
     * @return Entity count.
     */
    size_t GetEntityCount() const { return m_Entities.size(); }

    /**
     * @brief Iterates over all active entities.
     * @param callback Function to call for each active entity.
     */
    void ForEachActiveEntity(const std::function<void(Entity&)>& callback)
    {
        for (auto& entity : m_Entities)
        {
            if (entity->IsActive())
            {
                callback(*entity);
            }
        }
    }

    /**
     * @brief Iterates over all active entities (const).
     * @param callback Function to call for each active entity.
     */
    void ForEachActiveEntity(const std::function<void(const Entity&)>& callback) const
    {
        for (const auto& entity : m_Entities)
        {
            if (entity->IsActive())
            {
                callback(*entity);
            }
        }
    }

    /**
     * @brief Iterates over all visible entities.
     * @param callback Function to call for each visible entity.
     */
    void ForEachVisibleEntity(const std::function<void(Entity&)>& callback)
    {
        for (auto& entity : m_Entities)
        {
            if (entity->IsActive() && entity->IsVisible())
            {
                callback(*entity);
            }
        }
    }

    // =========================================================================
    // Update
    // =========================================================================

    /**
     * @brief Updates the scene.
     * @param deltaTime Time elapsed since last update in seconds.
     */
    void Update([[maybe_unused]] float deltaTime)
    {
        // Future: Update animations, physics, etc.
        // For now, just a placeholder for the scene update loop
    }

    // =========================================================================
    // Directional Light
    // =========================================================================

    /**
     * @brief Sets the main directional light.
     * @param light Directional light parameters.
     */
    void SetDirectionalLight(const DirectionalLight& light)
    {
        m_DirectionalLight = light;
    }

    /**
     * @brief Gets the main directional light.
     * @return Reference to the directional light.
     */
    DirectionalLight& GetDirectionalLight() { return m_DirectionalLight; }

    /**
     * @brief Gets the main directional light (const).
     * @return Const reference to the directional light.
     */
    const DirectionalLight& GetDirectionalLight() const { return m_DirectionalLight; }

    // =========================================================================
    // Point Lights
    // =========================================================================

    /**
     * @brief Adds a point light to the scene.
     * @param light Point light to add.
     * @return Index of the added light.
     */
    size_t AddPointLight(const PointLight& light)
    {
        m_PointLights.push_back(light);
        return m_PointLights.size() - 1;
    }

    /**
     * @brief Removes a point light by index.
     * @param index Index of the light to remove.
     */
    void RemovePointLight(size_t index)
    {
        if (index < m_PointLights.size())
        {
            m_PointLights.erase(m_PointLights.begin() + static_cast<ptrdiff_t>(index));
        }
    }

    /**
     * @brief Gets a point light by index.
     * @param index Index of the light.
     * @return Reference to the point light.
     */
    PointLight& GetPointLight(size_t index) { return m_PointLights[index]; }

    /**
     * @brief Gets a point light by index (const).
     * @param index Index of the light.
     * @return Const reference to the point light.
     */
    const PointLight& GetPointLight(size_t index) const { return m_PointLights[index]; }

    /**
     * @brief Gets all point lights.
     * @return Reference to the point light container.
     */
    std::vector<PointLight>& GetPointLights() { return m_PointLights; }

    /**
     * @brief Gets all point lights (const).
     * @return Const reference to the point light container.
     */
    const std::vector<PointLight>& GetPointLights() const { return m_PointLights; }

    /**
     * @brief Gets the number of point lights.
     * @return Point light count.
     */
    size_t GetPointLightCount() const { return m_PointLights.size(); }

    // =========================================================================
    // Spot Lights
    // =========================================================================

    /**
     * @brief Adds a spot light to the scene.
     * @param light Spot light to add.
     * @return Index of the added light.
     */
    size_t AddSpotLight(const SpotLight& light)
    {
        m_SpotLights.push_back(light);
        return m_SpotLights.size() - 1;
    }

    /**
     * @brief Removes a spot light by index.
     * @param index Index of the light to remove.
     */
    void RemoveSpotLight(size_t index)
    {
        if (index < m_SpotLights.size())
        {
            m_SpotLights.erase(m_SpotLights.begin() + static_cast<ptrdiff_t>(index));
        }
    }

    /**
     * @brief Gets a spot light by index.
     * @param index Index of the light.
     * @return Reference to the spot light.
     */
    SpotLight& GetSpotLight(size_t index) { return m_SpotLights[index]; }

    /**
     * @brief Gets a spot light by index (const).
     * @param index Index of the light.
     * @return Const reference to the spot light.
     */
    const SpotLight& GetSpotLight(size_t index) const { return m_SpotLights[index]; }

    /**
     * @brief Gets all spot lights.
     * @return Reference to the spot light container.
     */
    std::vector<SpotLight>& GetSpotLights() { return m_SpotLights; }

    /**
     * @brief Gets all spot lights (const).
     * @return Const reference to the spot light container.
     */
    const std::vector<SpotLight>& GetSpotLights() const { return m_SpotLights; }

    /**
     * @brief Gets the number of spot lights.
     * @return Spot light count.
     */
    size_t GetSpotLightCount() const { return m_SpotLights.size(); }

    // =========================================================================
    // Light UBO
    // =========================================================================

    /**
     * @brief Builds the light uniform buffer object data.
     * @return LightUBO structure ready for GPU upload.
     */
    LightUBO BuildLightUBO() const
    {
        LightUBO ubo;
        ubo.DirectionalLightData = m_DirectionalLight;
        ubo.NumPointLights = static_cast<uint32_t>(m_PointLights.size());
        ubo.NumSpotLights = static_cast<uint32_t>(m_SpotLights.size());
        return ubo;
    }

    // =========================================================================
    // Clear
    // =========================================================================

    /**
     * @brief Removes all entities from the scene.
     */
    void ClearEntities()
    {
        m_Entities.clear();
        m_NamedEntities.clear();
        m_NextEntityID = 0;
    }

    /**
     * @brief Removes all lights from the scene.
     */
    void ClearLights()
    {
        m_DirectionalLight = DirectionalLight();
        m_PointLights.clear();
        m_SpotLights.clear();
    }

    /**
     * @brief Clears the entire scene (entities and lights).
     */
    void Clear()
    {
        ClearEntities();
        ClearLights();
    }

private:
    std::string m_Name;

    // Entity storage
    std::vector<std::unique_ptr<Entity>> m_Entities;
    std::unordered_map<std::string, Entity*> m_NamedEntities;
    uint32_t m_NextEntityID = 0;

    // Lights
    DirectionalLight m_DirectionalLight;
    std::vector<PointLight> m_PointLights;
    std::vector<SpotLight> m_SpotLights;
};

} // namespace Scene
