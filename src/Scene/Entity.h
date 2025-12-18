/**
 * @file Entity.h
 * @brief Entity class representing game objects in the scene.
 *
 * Entities are the basic building blocks of the scene. Each entity has
 * an ID, transform, and can reference mesh and material resources.
 */

#pragma once

#include "Scene/Transform.h"
#include "Resources/ResourceHandle.h"
#include <string>
#include <cstdint>

namespace Scene {

/**
 * @brief Entity represents a game object in the scene.
 *
 * Each entity has:
 * - Unique ID for identification
 * - Active flag for enabling/disabling
 * - Transform for position/rotation/scale
 * - Optional mesh and material references
 * - Optional name for debugging/lookup
 */
class Entity {
public:
    /**
     * @brief Constructs an entity with a unique ID.
     * @param id Unique identifier for this entity.
     */
    explicit Entity(uint32_t id)
        : m_ID(id)
    {
    }

    /**
     * @brief Constructs a named entity with a unique ID.
     * @param id Unique identifier for this entity.
     * @param name Display name for debugging and lookup.
     */
    Entity(uint32_t id, const std::string& name)
        : m_ID(id)
        , m_Name(name)
    {
    }

    // Non-copyable, movable
    Entity(const Entity&) = delete;
    Entity& operator=(const Entity&) = delete;
    Entity(Entity&&) = default;
    Entity& operator=(Entity&&) = default;

    ~Entity() = default;

    // =========================================================================
    // Identity
    // =========================================================================

    /**
     * @brief Gets the unique identifier for this entity.
     * @return Entity ID.
     */
    uint32_t GetID() const { return m_ID; }

    /**
     * @brief Gets the entity's name.
     * @return Entity name string.
     */
    const std::string& GetName() const { return m_Name; }

    /**
     * @brief Sets the entity's name.
     * @param name New name for the entity.
     */
    void SetName(const std::string& name) { m_Name = name; }

    // =========================================================================
    // Active State
    // =========================================================================

    /**
     * @brief Checks if the entity is active.
     * @return true if the entity is active.
     */
    bool IsActive() const { return m_Active; }

    /**
     * @brief Sets the entity's active state.
     * @param active Whether the entity should be active.
     */
    void SetActive(bool active) { m_Active = active; }

    // =========================================================================
    // Transform
    // =========================================================================

    /**
     * @brief Gets the entity's transform.
     * @return Reference to the transform component.
     */
    Transform& GetTransform() { return m_Transform; }

    /**
     * @brief Gets the entity's transform (const).
     * @return Const reference to the transform component.
     */
    const Transform& GetTransform() const { return m_Transform; }

    // =========================================================================
    // Mesh
    // =========================================================================

    /**
     * @brief Sets the mesh for this entity.
     * @param mesh Handle to the mesh resource.
     */
    void SetMesh(Resources::ModelHandle mesh) { m_Mesh = mesh; }

    /**
     * @brief Gets the mesh handle.
     * @return Handle to the mesh resource.
     */
    Resources::ModelHandle GetMesh() const { return m_Mesh; }

    /**
     * @brief Checks if this entity has a mesh.
     * @return true if a valid mesh is set.
     */
    bool HasMesh() const { return m_Mesh.IsValid(); }

    // =========================================================================
    // Material
    // =========================================================================

    /**
     * @brief Sets the material for this entity.
     * @param material Handle to the material resource.
     */
    void SetMaterial(Resources::MaterialHandle material) { m_Material = material; }

    /**
     * @brief Gets the material handle.
     * @return Handle to the material resource.
     */
    Resources::MaterialHandle GetMaterial() const { return m_Material; }

    /**
     * @brief Checks if this entity has a material.
     * @return true if a valid material is set.
     */
    bool HasMaterial() const { return m_Material.IsValid(); }

    // =========================================================================
    // Visibility
    // =========================================================================

    /**
     * @brief Checks if the entity is visible.
     * @return true if the entity should be rendered.
     */
    bool IsVisible() const { return m_Visible; }

    /**
     * @brief Sets the entity's visibility.
     * @param visible Whether the entity should be rendered.
     */
    void SetVisible(bool visible) { m_Visible = visible; }

    // =========================================================================
    // Layer/Tags (for filtering)
    // =========================================================================

    /**
     * @brief Gets the entity's layer mask.
     * @return Layer bitmask for rendering/physics filtering.
     */
    uint32_t GetLayer() const { return m_Layer; }

    /**
     * @brief Sets the entity's layer mask.
     * @param layer Layer bitmask.
     */
    void SetLayer(uint32_t layer) { m_Layer = layer; }

private:
    uint32_t m_ID = 0;
    std::string m_Name;
    bool m_Active = true;
    bool m_Visible = true;
    uint32_t m_Layer = 1;  // Default layer

    Transform m_Transform;
    Resources::ModelHandle m_Mesh;
    Resources::MaterialHandle m_Material;
};

} // namespace Scene
