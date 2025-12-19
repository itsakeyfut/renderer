/**
 * @file OrbitCameraController.h
 * @brief Orbit (arcball) style camera controller.
 *
 * Provides mouse-based orbital controls around a target point.
 */

#pragma once

#include "Scene/Camera.h"
#include "Platform/Input.h"

#include <algorithm>
#include <cmath>

namespace Scene {

/**
 * @brief Orbit-style camera controller.
 *
 * Controls a Camera with:
 * - Left mouse drag for orbital rotation around target
 * - Middle mouse drag for panning
 * - Scroll wheel for zoom (distance from target)
 * - Right mouse drag for alternative rotation
 */
class OrbitCameraController {
public:
    /**
     * @brief Default constructor.
     */
    OrbitCameraController() = default;

    /**
     * @brief Constructs a controller for a specific camera.
     * @param camera Pointer to the camera to control.
     */
    explicit OrbitCameraController(Camera* camera)
        : m_Camera(camera)
    {
        if (m_Camera != nullptr)
        {
            // Initialize from camera's current position
            UpdateFromCamera();
        }
    }

    // =========================================================================
    // Camera Assignment
    // =========================================================================

    /**
     * @brief Sets the camera to control.
     * @param camera Pointer to the camera.
     */
    void SetCamera(Camera* camera)
    {
        m_Camera = camera;
        if (m_Camera != nullptr)
        {
            UpdateFromCamera();
        }
    }

    /**
     * @brief Gets the controlled camera.
     * @return Pointer to the camera.
     */
    Camera* GetCamera() const { return m_Camera; }

    // =========================================================================
    // Target Settings
    // =========================================================================

    /**
     * @brief Sets the orbit target position.
     * @param target World position to orbit around.
     */
    void SetTarget(const glm::vec3& target)
    {
        m_Target = target;
        UpdateCameraPosition();
    }

    /**
     * @brief Gets the orbit target position.
     * @return Target position in world space.
     */
    const glm::vec3& GetTarget() const { return m_Target; }

    /**
     * @brief Sets the distance from the target.
     * @param distance Distance in world units.
     */
    void SetDistance(float distance)
    {
        m_Distance = std::clamp(distance, m_MinDistance, m_MaxDistance);
        UpdateCameraPosition();
    }

    /**
     * @brief Gets the distance from the target.
     * @return Distance in world units.
     */
    float GetDistance() const { return m_Distance; }

    // =========================================================================
    // Orbit Settings
    // =========================================================================

    /**
     * @brief Sets the horizontal orbit angle.
     * @param azimuth Azimuth angle in degrees.
     */
    void SetAzimuth(float azimuth)
    {
        m_Azimuth = azimuth;
        UpdateCameraPosition();
    }

    /**
     * @brief Gets the horizontal orbit angle.
     * @return Azimuth angle in degrees.
     */
    float GetAzimuth() const { return m_Azimuth; }

    /**
     * @brief Sets the vertical orbit angle.
     * @param elevation Elevation angle in degrees.
     */
    void SetElevation(float elevation)
    {
        m_Elevation = std::clamp(elevation, m_MinElevation, m_MaxElevation);
        UpdateCameraPosition();
    }

    /**
     * @brief Gets the vertical orbit angle.
     * @return Elevation angle in degrees.
     */
    float GetElevation() const { return m_Elevation; }

    // =========================================================================
    // Control Settings
    // =========================================================================

    /**
     * @brief Sets the rotation sensitivity.
     * @param sensitivity Rotation sensitivity multiplier.
     */
    void SetRotationSensitivity(float sensitivity) { m_RotationSensitivity = sensitivity; }

    /**
     * @brief Gets the rotation sensitivity.
     * @return Rotation sensitivity multiplier.
     */
    float GetRotationSensitivity() const { return m_RotationSensitivity; }

    /**
     * @brief Sets the pan sensitivity.
     * @param sensitivity Pan sensitivity multiplier.
     */
    void SetPanSensitivity(float sensitivity) { m_PanSensitivity = sensitivity; }

    /**
     * @brief Gets the pan sensitivity.
     * @return Pan sensitivity multiplier.
     */
    float GetPanSensitivity() const { return m_PanSensitivity; }

    /**
     * @brief Sets the zoom sensitivity.
     * @param sensitivity Zoom sensitivity multiplier.
     */
    void SetZoomSensitivity(float sensitivity) { m_ZoomSensitivity = sensitivity; }

    /**
     * @brief Gets the zoom sensitivity.
     * @return Zoom sensitivity multiplier.
     */
    float GetZoomSensitivity() const { return m_ZoomSensitivity; }

    /**
     * @brief Sets the distance constraints.
     * @param minDistance Minimum allowed distance.
     * @param maxDistance Maximum allowed distance.
     */
    void SetDistanceConstraints(float minDistance, float maxDistance)
    {
        m_MinDistance = minDistance;
        m_MaxDistance = maxDistance;
        m_Distance = std::clamp(m_Distance, m_MinDistance, m_MaxDistance);
    }

    /**
     * @brief Sets the elevation constraints.
     * @param minElevation Minimum elevation angle in degrees.
     * @param maxElevation Maximum elevation angle in degrees.
     */
    void SetElevationConstraints(float minElevation, float maxElevation)
    {
        m_MinElevation = minElevation;
        m_MaxElevation = maxElevation;
        m_Elevation = std::clamp(m_Elevation, m_MinElevation, m_MaxElevation);
    }

    /**
     * @brief Enables or disables the controller.
     * @param enabled Whether the controller should process input.
     */
    void SetEnabled(bool enabled) { m_Enabled = enabled; }

    /**
     * @brief Checks if the controller is enabled.
     * @return true if the controller processes input.
     */
    bool IsEnabled() const { return m_Enabled; }

    // =========================================================================
    // Update
    // =========================================================================

    /**
     * @brief Updates the camera based on input.
     * @param deltaTime Time elapsed since last update in seconds.
     *
     * Call this once per frame after Input::Update().
     */
    void Update([[maybe_unused]] float deltaTime)
    {
        if (!m_Enabled || m_Camera == nullptr)
        {
            return;
        }

        ProcessRotation();
        ProcessPan();
        ProcessZoom();
    }

private:
    /**
     * @brief Updates orbit parameters from the camera's current position.
     */
    void UpdateFromCamera()
    {
        if (m_Camera == nullptr)
        {
            return;
        }

        glm::vec3 offset = m_Camera->GetPosition() - m_Target;
        m_Distance = glm::length(offset);

        if (m_Distance > 0.001f)
        {
            offset = glm::normalize(offset);
            m_Elevation = glm::degrees(asin(offset.y));
            m_Azimuth = glm::degrees(atan2(offset.x, offset.z));
        }
    }

    /**
     * @brief Updates camera position from orbit parameters.
     */
    void UpdateCameraPosition()
    {
        if (m_Camera == nullptr)
        {
            return;
        }

        float elevationRad = glm::radians(m_Elevation);
        float azimuthRad = glm::radians(m_Azimuth);

        // Spherical to Cartesian
        float cosElevation = cos(elevationRad);
        glm::vec3 offset(
            m_Distance * cosElevation * sin(azimuthRad),
            m_Distance * sin(elevationRad),
            m_Distance * cosElevation * cos(azimuthRad)
        );

        m_Camera->SetPosition(m_Target + offset);
        m_Camera->LookAt(m_Target);
    }

    /**
     * @brief Processes mouse input for orbital rotation.
     */
    void ProcessRotation()
    {
        // Rotate when left mouse button is held
        if (Platform::Input::IsMouseButtonDown(Platform::MouseButton::Left))
        {
            float deltaX = static_cast<float>(Platform::Input::GetMouseDeltaX());
            float deltaY = static_cast<float>(Platform::Input::GetMouseDeltaY());

            m_Azimuth -= deltaX * m_RotationSensitivity;
            m_Elevation += deltaY * m_RotationSensitivity;

            // Clamp elevation
            m_Elevation = std::clamp(m_Elevation, m_MinElevation, m_MaxElevation);

            UpdateCameraPosition();
        }
    }

    /**
     * @brief Processes mouse input for panning.
     */
    void ProcessPan()
    {
        // Pan when middle mouse button is held
        if (Platform::Input::IsMouseButtonDown(Platform::MouseButton::Middle))
        {
            float deltaX = static_cast<float>(Platform::Input::GetMouseDeltaX());
            float deltaY = static_cast<float>(Platform::Input::GetMouseDeltaY());

            // Get camera's right and up vectors for screen-space panning
            glm::vec3 right = m_Camera->GetRight();
            glm::vec3 up = m_Camera->GetUp();

            // Scale pan speed by distance (farther = faster pan)
            float panScale = m_Distance * m_PanSensitivity * 0.01f;

            m_Target -= right * deltaX * panScale;
            m_Target += up * deltaY * panScale;

            UpdateCameraPosition();
        }
    }

    /**
     * @brief Processes scroll wheel input for zooming.
     */
    void ProcessZoom()
    {
        float scroll = static_cast<float>(Platform::Input::GetScrollDelta());

        if (std::abs(scroll) > 0.001f)
        {
            // Zoom with scroll wheel
            m_Distance -= scroll * m_ZoomSensitivity * m_Distance * 0.1f;
            m_Distance = std::clamp(m_Distance, m_MinDistance, m_MaxDistance);

            UpdateCameraPosition();
        }
    }

private:
    Camera* m_Camera = nullptr;

    // Orbit parameters
    glm::vec3 m_Target = glm::vec3(0.0f);
    float m_Distance = 10.0f;
    float m_Azimuth = 0.0f;      // degrees (horizontal angle)
    float m_Elevation = 30.0f;   // degrees (vertical angle)

    // Sensitivity settings
    float m_RotationSensitivity = 0.5f;
    float m_PanSensitivity = 0.5f;
    float m_ZoomSensitivity = 1.0f;

    // Constraints
    float m_MinDistance = 0.5f;
    float m_MaxDistance = 1000.0f;
    float m_MinElevation = -89.0f;
    float m_MaxElevation = 89.0f;

    // State
    bool m_Enabled = true;
};

} // namespace Scene
