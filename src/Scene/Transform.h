/**
 * @file Transform.h
 * @brief Transform component for positioning entities in 3D space.
 *
 * Provides position, rotation, and scale transformations with support
 * for parent-child hierarchies and cached matrix calculations.
 */

#pragma once

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include <algorithm>
#include <vector>

namespace Scene {

// Forward declaration
class Entity;

/**
 * @brief Transform component for 3D positioning and orientation.
 *
 * Manages position, rotation (quaternion), and scale with support for
 * parent-child hierarchies. Uses lazy evaluation with dirty flags for
 * efficient matrix calculations.
 */
class Transform {
public:
    /**
     * @brief Default constructor creates identity transform.
     */
    Transform() = default;

    /**
     * @brief Constructs transform with initial position.
     * @param position Initial world position.
     */
    explicit Transform(const glm::vec3& position)
        : m_Position(position)
    {
    }

    /**
     * @brief Constructs transform with position, rotation, and scale.
     * @param position Initial position.
     * @param rotation Initial rotation as quaternion.
     * @param scale Initial scale.
     */
    Transform(const glm::vec3& position, const glm::quat& rotation, const glm::vec3& scale)
        : m_Position(position)
        , m_Rotation(rotation)
        , m_Scale(scale)
    {
    }

    // =========================================================================
    // Position
    // =========================================================================

    /**
     * @brief Gets the local position.
     * @return Local position vector.
     */
    const glm::vec3& GetPosition() const { return m_Position; }

    /**
     * @brief Sets the local position.
     * @param position New position vector.
     */
    void SetPosition(const glm::vec3& position)
    {
        m_Position = position;
        MarkDirty();
    }

    /**
     * @brief Translates the position by an offset.
     * @param offset Translation offset.
     */
    void Translate(const glm::vec3& offset)
    {
        m_Position += offset;
        MarkDirty();
    }

    // =========================================================================
    // Rotation
    // =========================================================================

    /**
     * @brief Gets the local rotation as quaternion.
     * @return Local rotation quaternion.
     */
    const glm::quat& GetRotation() const { return m_Rotation; }

    /**
     * @brief Sets the local rotation.
     * @param rotation New rotation quaternion.
     */
    void SetRotation(const glm::quat& rotation)
    {
        m_Rotation = rotation;
        MarkDirty();
    }

    /**
     * @brief Gets the rotation as Euler angles (pitch, yaw, roll) in radians.
     * @return Euler angles in radians.
     */
    glm::vec3 GetEulerAngles() const
    {
        return glm::eulerAngles(m_Rotation);
    }

    /**
     * @brief Sets rotation from Euler angles.
     * @param pitchYawRoll Euler angles (pitch, yaw, roll) in radians.
     */
    void SetEulerAngles(const glm::vec3& pitchYawRoll)
    {
        m_Rotation = glm::quat(pitchYawRoll);
        MarkDirty();
    }

    /**
     * @brief Rotates by an additional quaternion.
     * @param rotation Rotation to apply.
     */
    void Rotate(const glm::quat& rotation)
    {
        m_Rotation = rotation * m_Rotation;
        MarkDirty();
    }

    /**
     * @brief Rotates around an axis by an angle.
     * @param axis Rotation axis (should be normalized).
     * @param angleRadians Rotation angle in radians.
     */
    void RotateAround(const glm::vec3& axis, float angleRadians)
    {
        Rotate(glm::angleAxis(angleRadians, axis));
    }

    // =========================================================================
    // Scale
    // =========================================================================

    /**
     * @brief Gets the local scale.
     * @return Local scale vector.
     */
    const glm::vec3& GetScale() const { return m_Scale; }

    /**
     * @brief Sets the local scale.
     * @param scale New scale vector.
     */
    void SetScale(const glm::vec3& scale)
    {
        m_Scale = scale;
        MarkDirty();
    }

    /**
     * @brief Sets uniform scale on all axes.
     * @param uniformScale Scale value for all axes.
     */
    void SetScale(float uniformScale)
    {
        m_Scale = glm::vec3(uniformScale);
        MarkDirty();
    }

    // =========================================================================
    // Direction Vectors
    // =========================================================================

    /**
     * @brief Gets the forward direction vector (negative Z in local space).
     * @return Normalized forward direction.
     */
    glm::vec3 GetForward() const
    {
        return glm::normalize(m_Rotation * glm::vec3(0.0f, 0.0f, -1.0f));
    }

    /**
     * @brief Gets the right direction vector (positive X in local space).
     * @return Normalized right direction.
     */
    glm::vec3 GetRight() const
    {
        return glm::normalize(m_Rotation * glm::vec3(1.0f, 0.0f, 0.0f));
    }

    /**
     * @brief Gets the up direction vector (positive Y in local space).
     * @return Normalized up direction.
     */
    glm::vec3 GetUp() const
    {
        return glm::normalize(m_Rotation * glm::vec3(0.0f, 1.0f, 0.0f));
    }

    /**
     * @brief Makes the transform look at a target position.
     * @param target World position to look at.
     * @param up Up direction reference.
     */
    void LookAt(const glm::vec3& target, const glm::vec3& up = glm::vec3(0.0f, 1.0f, 0.0f))
    {
        glm::vec3 direction = glm::normalize(target - m_Position);
        m_Rotation = glm::quatLookAt(direction, up);
        MarkDirty();
    }

    // =========================================================================
    // Matrix Calculations
    // =========================================================================

    /**
     * @brief Gets the local transformation matrix.
     * @return 4x4 transformation matrix (Translation * Rotation * Scale).
     */
    glm::mat4 GetLocalMatrix() const
    {
        if (m_LocalDirty)
        {
            UpdateLocalMatrix();
        }
        return m_LocalMatrix;
    }

    /**
     * @brief Gets the world transformation matrix.
     *
     * If this transform has a parent, the world matrix is the parent's
     * world matrix multiplied by this local matrix.
     *
     * @return 4x4 world transformation matrix.
     */
    glm::mat4 GetWorldMatrix() const
    {
        if (m_WorldDirty)
        {
            UpdateWorldMatrix();
        }
        return m_WorldMatrix;
    }

    /**
     * @brief Gets the normal matrix for lighting calculations.
     *
     * This is the transpose of the inverse of the upper-left 3x3 of
     * the world matrix. Used to transform normals correctly.
     *
     * @return 4x4 normal matrix (only upper-left 3x3 is meaningful).
     */
    glm::mat4 GetNormalMatrix() const
    {
        return glm::transpose(glm::inverse(GetWorldMatrix()));
    }

    /**
     * @brief Gets the world position.
     * @return World position extracted from world matrix.
     */
    glm::vec3 GetWorldPosition() const
    {
        return glm::vec3(GetWorldMatrix()[3]);
    }

    // =========================================================================
    // Parent-Child Hierarchy
    // =========================================================================

    /**
     * @brief Gets the parent transform.
     * @return Pointer to parent transform, or nullptr if no parent.
     */
    Transform* GetParent() const { return m_Parent; }

    /**
     * @brief Sets the parent transform.
     *
     * Setting a parent makes this transform's world matrix relative to
     * the parent's world matrix.
     *
     * @param parent Pointer to parent transform, or nullptr to unparent.
     */
    void SetParent(Transform* parent)
    {
        if (m_Parent == parent)
        {
            return;
        }

        // Prevent circular references
        if (parent != nullptr)
        {
            Transform* ancestor = parent;
            while (ancestor != nullptr)
            {
                if (ancestor == this)
                {
                    // Would create circular reference, silently reject
                    return;
                }
                ancestor = ancestor->m_Parent;
            }
        }

        // Remove from old parent's children list
        if (m_Parent != nullptr)
        {
            auto& siblings = m_Parent->m_Children;
            siblings.erase(
                std::remove(siblings.begin(), siblings.end(), this),
                siblings.end());
        }

        m_Parent = parent;

        // Add to new parent's children list
        if (m_Parent != nullptr)
        {
            m_Parent->m_Children.push_back(this);
        }

        MarkDirty();
    }

    /**
     * @brief Gets the list of child transforms.
     * @return Vector of child transform pointers.
     */
    const std::vector<Transform*>& GetChildren() const { return m_Children; }

    /**
     * @brief Checks if this transform has any children.
     * @return true if there are child transforms.
     */
    bool HasChildren() const { return !m_Children.empty(); }

    /**
     * @brief Gets the number of children.
     * @return Number of child transforms.
     */
    size_t GetChildCount() const { return m_Children.size(); }

private:
    /**
     * @brief Marks the local and world matrices as needing recalculation.
     */
    void MarkDirty()
    {
        m_LocalDirty = true;
        m_WorldDirty = true;

        // Propagate dirty flag to children
        for (Transform* child : m_Children)
        {
            child->m_WorldDirty = true;
            child->PropagateWorldDirty();
        }
    }

    /**
     * @brief Propagates world dirty flag to all descendants.
     */
    void PropagateWorldDirty()
    {
        for (Transform* child : m_Children)
        {
            child->m_WorldDirty = true;
            child->PropagateWorldDirty();
        }
    }

    /**
     * @brief Recalculates the local transformation matrix.
     */
    void UpdateLocalMatrix() const
    {
        glm::mat4 T = glm::translate(glm::mat4(1.0f), m_Position);
        glm::mat4 R = glm::toMat4(m_Rotation);
        glm::mat4 S = glm::scale(glm::mat4(1.0f), m_Scale);
        m_LocalMatrix = T * R * S;
        m_LocalDirty = false;
    }

    /**
     * @brief Recalculates the world transformation matrix.
     */
    void UpdateWorldMatrix() const
    {
        if (m_LocalDirty)
        {
            UpdateLocalMatrix();
        }

        if (m_Parent != nullptr)
        {
            m_WorldMatrix = m_Parent->GetWorldMatrix() * m_LocalMatrix;
        }
        else
        {
            m_WorldMatrix = m_LocalMatrix;
        }
        m_WorldDirty = false;
    }

private:
    // Local transform components
    glm::vec3 m_Position = glm::vec3(0.0f);
    glm::quat m_Rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 m_Scale = glm::vec3(1.0f);

    // Cached matrices
    mutable glm::mat4 m_LocalMatrix = glm::mat4(1.0f);
    mutable glm::mat4 m_WorldMatrix = glm::mat4(1.0f);
    mutable bool m_LocalDirty = true;
    mutable bool m_WorldDirty = true;

    // Hierarchy
    Transform* m_Parent = nullptr;
    std::vector<Transform*> m_Children;
};

} // namespace Scene
