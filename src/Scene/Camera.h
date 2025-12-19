/**
 * @file Camera.h
 * @brief Camera class for 3D scene viewing with perspective and orthographic projections.
 *
 * Provides view and projection matrix calculation with Vulkan clip space correction.
 * Supports both perspective and orthographic projection modes.
 */

#pragma once

#define GLM_ENABLE_EXPERIMENTAL
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Resources/UniformBufferObjects.h"

namespace Scene {

/**
 * @brief Camera class for 3D scene rendering.
 *
 * Manages view and projection matrices with support for both perspective
 * and orthographic projections. Includes Vulkan-specific clip space correction
 * for proper Y-axis inversion and depth range [0,1].
 */
class Camera {
public:
    /**
     * @brief Projection type enumeration.
     */
    enum class ProjectionType {
        Perspective,
        Orthographic
    };

    /**
     * @brief Default constructor.
     *
     * Creates a perspective camera at position (0, 0, 5) looking down -Z axis.
     */
    Camera() = default;

    // =========================================================================
    // Projection Settings
    // =========================================================================

    /**
     * @brief Sets up perspective projection.
     * @param fovY Vertical field of view in degrees.
     * @param aspectRatio Width divided by height.
     * @param nearPlane Near clipping plane distance.
     * @param farPlane Far clipping plane distance.
     */
    void SetPerspective(float fovY, float aspectRatio, float nearPlane, float farPlane)
    {
        m_ProjectionType = ProjectionType::Perspective;
        m_FovY = fovY;
        m_AspectRatio = aspectRatio;
        m_NearPlane = nearPlane;
        m_FarPlane = farPlane;
        m_ProjectionDirty = true;
    }

    /**
     * @brief Sets up orthographic projection.
     * @param left Left clipping plane.
     * @param right Right clipping plane.
     * @param bottom Bottom clipping plane.
     * @param top Top clipping plane.
     * @param nearPlane Near clipping plane distance.
     * @param farPlane Far clipping plane distance.
     */
    void SetOrthographic(float left, float right, float bottom, float top,
                         float nearPlane, float farPlane)
    {
        m_ProjectionType = ProjectionType::Orthographic;
        m_OrthoLeft = left;
        m_OrthoRight = right;
        m_OrthoBottom = bottom;
        m_OrthoTop = top;
        m_NearPlane = nearPlane;
        m_FarPlane = farPlane;
        m_ProjectionDirty = true;
    }

    /**
     * @brief Updates the aspect ratio (for window resizing).
     * @param aspectRatio New aspect ratio (width / height).
     */
    void SetAspectRatio(float aspectRatio)
    {
        m_AspectRatio = aspectRatio;
        m_ProjectionDirty = true;
    }

    // =========================================================================
    // Transform Settings
    // =========================================================================

    /**
     * @brief Sets the camera world position.
     * @param position New camera position.
     */
    void SetPosition(const glm::vec3& position)
    {
        m_Position = position;
        m_ViewDirty = true;
    }

    /**
     * @brief Sets the camera rotation from pitch and yaw angles.
     * @param pitch Pitch angle in degrees (up/down rotation).
     * @param yaw Yaw angle in degrees (left/right rotation).
     */
    void SetRotation(float pitch, float yaw)
    {
        m_Pitch = pitch;
        m_Yaw = yaw;
        m_ViewDirty = true;
    }

    /**
     * @brief Makes the camera look at a target position.
     * @param target World position to look at.
     * @param up Up direction reference (default: world up).
     */
    void LookAt(const glm::vec3& target, const glm::vec3& up = glm::vec3(0.0f, 1.0f, 0.0f))
    {
        glm::vec3 direction = glm::normalize(target - m_Position);

        // Calculate pitch and yaw from direction vector
        // GetForward: x = cos(pitch)*cos(yaw), y = sin(pitch), z = cos(pitch)*sin(yaw)
        m_Pitch = glm::degrees(asin(direction.y));
        m_Yaw = glm::degrees(atan2(direction.z, direction.x));

        // Calculate the view matrix directly for precision
        m_ViewMatrix = glm::lookAt(m_Position, target, up);
        m_ViewDirty = false;
    }

    // =========================================================================
    // Matrix Getters
    // =========================================================================

    /**
     * @brief Gets the view matrix.
     * @return The 4x4 view transformation matrix.
     */
    const glm::mat4& GetViewMatrix() const
    {
        if (m_ViewDirty)
        {
            UpdateViewMatrix();
        }
        return m_ViewMatrix;
    }

    /**
     * @brief Gets the projection matrix (with Vulkan clip correction applied).
     * @return The 4x4 projection matrix.
     */
    const glm::mat4& GetProjectionMatrix() const
    {
        if (m_ProjectionDirty)
        {
            UpdateProjectionMatrix();
        }
        return m_ProjectionMatrix;
    }

    /**
     * @brief Gets the combined view-projection matrix.
     * @return The 4x4 view-projection matrix.
     */
    glm::mat4 GetViewProjectionMatrix() const
    {
        return GetProjectionMatrix() * GetViewMatrix();
    }

    // =========================================================================
    // Property Getters
    // =========================================================================

    /**
     * @brief Gets the camera world position.
     * @return Camera position in world space.
     */
    const glm::vec3& GetPosition() const { return m_Position; }

    /**
     * @brief Gets the pitch angle.
     * @return Pitch in degrees.
     */
    float GetPitch() const { return m_Pitch; }

    /**
     * @brief Gets the yaw angle.
     * @return Yaw in degrees.
     */
    float GetYaw() const { return m_Yaw; }

    /**
     * @brief Gets the forward direction vector.
     * @return Normalized forward direction.
     *
     * Uses standard FPS camera convention:
     * - yaw = 0 means looking at +X
     * - yaw = -90 means looking at -Z (default)
     * - yaw = +90 means looking at +Z
     */
    glm::vec3 GetForward() const
    {
        float pitchRad = glm::radians(m_Pitch);
        float yawRad = glm::radians(m_Yaw);

        return glm::normalize(glm::vec3(
            cos(pitchRad) * cos(yawRad),
            sin(pitchRad),
            cos(pitchRad) * sin(yawRad)
        ));
    }

    /**
     * @brief Gets the right direction vector.
     * @return Normalized right direction.
     */
    glm::vec3 GetRight() const
    {
        return glm::normalize(glm::cross(GetForward(), glm::vec3(0.0f, 1.0f, 0.0f)));
    }

    /**
     * @brief Gets the up direction vector.
     * @return Normalized up direction.
     */
    glm::vec3 GetUp() const
    {
        return glm::normalize(glm::cross(GetRight(), GetForward()));
    }

    /**
     * @brief Gets the current projection type.
     * @return Current projection type.
     */
    ProjectionType GetProjectionType() const { return m_ProjectionType; }

    /**
     * @brief Gets the vertical field of view.
     * @return FOV in degrees.
     */
    float GetFovY() const { return m_FovY; }

    /**
     * @brief Gets the aspect ratio.
     * @return Aspect ratio (width / height).
     */
    float GetAspectRatio() const { return m_AspectRatio; }

    /**
     * @brief Gets the near clipping plane distance.
     * @return Near plane distance.
     */
    float GetNearPlane() const { return m_NearPlane; }

    /**
     * @brief Gets the far clipping plane distance.
     * @return Far plane distance.
     */
    float GetFarPlane() const { return m_FarPlane; }

    // =========================================================================
    // Vulkan Clip Space
    // =========================================================================

    /**
     * @brief Gets the Vulkan clip space correction matrix.
     *
     * Vulkan uses a different clip space than OpenGL:
     * - Y axis is inverted (positive Y points down)
     * - Z range is [0, 1] instead of [-1, 1]
     *
     * Note: With GLM_FORCE_DEPTH_ZERO_TO_ONE defined, glm::perspective
     * already handles the Z range. This method provides the Y-flip matrix.
     *
     * @return 4x4 clip correction matrix.
     */
    static glm::mat4 GetVulkanClipCorrection()
    {
        // Y-flip matrix for Vulkan coordinate system
        // Vulkan has Y pointing down, OpenGL/GLM has Y pointing up
        return glm::mat4(
            1.0f,  0.0f, 0.0f, 0.0f,
            0.0f, -1.0f, 0.0f, 0.0f,
            0.0f,  0.0f, 1.0f, 0.0f,
            0.0f,  0.0f, 0.0f, 1.0f
        );
    }

    // =========================================================================
    // Shader Data
    // =========================================================================

    /**
     * @brief Builds a CameraUBO structure for shader uniform buffer.
     *
     * Creates a CameraUBO populated with the current camera's view matrix,
     * projection matrix, combined view-projection matrix, and position.
     * Ready to be uploaded to a GPU uniform buffer.
     *
     * @return CameraUBO structure ready for GPU upload.
     */
    Resources::CameraUBO BuildCameraUBO() const
    {
        Resources::CameraUBO ubo;
        ubo.View = GetViewMatrix();
        ubo.Projection = GetProjectionMatrix();
        ubo.ViewProjection = GetViewProjectionMatrix();
        ubo.CameraPosition = m_Position;
        ubo.Padding = 0.0f;
        return ubo;
    }

private:
    /**
     * @brief Updates the view matrix from current position and rotation.
     */
    void UpdateViewMatrix() const
    {
        glm::vec3 forward = GetForward();
        glm::vec3 target = m_Position + forward;
        glm::vec3 up(0.0f, 1.0f, 0.0f);

        m_ViewMatrix = glm::lookAt(m_Position, target, up);
        m_ViewDirty = false;
    }

    /**
     * @brief Updates the projection matrix based on current settings.
     */
    void UpdateProjectionMatrix() const
    {
        if (m_ProjectionType == ProjectionType::Perspective)
        {
            // GLM with GLM_FORCE_DEPTH_ZERO_TO_ONE handles Z range [0, 1]
            glm::mat4 projection = glm::perspective(
                glm::radians(m_FovY),
                m_AspectRatio,
                m_NearPlane,
                m_FarPlane
            );

            // Apply Vulkan Y-flip
            m_ProjectionMatrix = GetVulkanClipCorrection() * projection;
        }
        else
        {
            glm::mat4 projection = glm::ortho(
                m_OrthoLeft, m_OrthoRight,
                m_OrthoBottom, m_OrthoTop,
                m_NearPlane, m_FarPlane
            );

            // Apply Vulkan Y-flip
            m_ProjectionMatrix = GetVulkanClipCorrection() * projection;
        }

        m_ProjectionDirty = false;
    }

private:
    // Position and rotation
    glm::vec3 m_Position = glm::vec3(0.0f, 0.0f, 5.0f);
    float m_Pitch = 0.0f;    // degrees
    float m_Yaw = -90.0f;    // degrees (facing -Z initially)

    // Projection type
    ProjectionType m_ProjectionType = ProjectionType::Perspective;

    // Perspective parameters
    float m_FovY = 45.0f;
    float m_AspectRatio = 16.0f / 9.0f;
    float m_NearPlane = 0.1f;
    float m_FarPlane = 1000.0f;

    // Orthographic parameters
    float m_OrthoLeft = -10.0f;
    float m_OrthoRight = 10.0f;
    float m_OrthoBottom = -10.0f;
    float m_OrthoTop = 10.0f;

    // Cached matrices
    mutable glm::mat4 m_ViewMatrix = glm::mat4(1.0f);
    mutable glm::mat4 m_ProjectionMatrix = glm::mat4(1.0f);
    mutable bool m_ViewDirty = true;
    mutable bool m_ProjectionDirty = true;
};

} // namespace Scene
