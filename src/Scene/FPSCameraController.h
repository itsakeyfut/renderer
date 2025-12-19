/**
 * @file FPSCameraController.h
 * @brief First-person shooter style camera controller.
 *
 * Provides WASD movement and mouse look controls for a Camera.
 */

#pragma once

#include "Scene/Camera.h"
#include "Platform/Input.h"

#include <algorithm>

namespace Scene {

/**
 * @brief FPS-style camera controller.
 *
 * Controls a Camera with:
 * - WASD keys for movement (forward/back/left/right)
 * - Q/E keys for vertical movement (down/up)
 * - Mouse movement for looking around
 * - Shift for faster movement
 */
class FPSCameraController {
public:
    /**
     * @brief Default constructor.
     */
    FPSCameraController() = default;

    /**
     * @brief Constructs a controller for a specific camera.
     * @param camera Pointer to the camera to control.
     */
    explicit FPSCameraController(Camera* camera)
        : m_Camera(camera)
    {
    }

    // =========================================================================
    // Camera Assignment
    // =========================================================================

    /**
     * @brief Sets the camera to control.
     * @param camera Pointer to the camera.
     */
    void SetCamera(Camera* camera) { m_Camera = camera; }

    /**
     * @brief Gets the controlled camera.
     * @return Pointer to the camera.
     */
    Camera* GetCamera() const { return m_Camera; }

    // =========================================================================
    // Settings
    // =========================================================================

    /**
     * @brief Sets the movement speed.
     * @param speed Movement speed in units per second.
     */
    void SetMoveSpeed(float speed) { m_MoveSpeed = speed; }

    /**
     * @brief Gets the movement speed.
     * @return Movement speed in units per second.
     */
    float GetMoveSpeed() const { return m_MoveSpeed; }

    /**
     * @brief Sets the mouse sensitivity.
     * @param sensitivity Mouse sensitivity multiplier.
     */
    void SetMouseSensitivity(float sensitivity) { m_MouseSensitivity = sensitivity; }

    /**
     * @brief Gets the mouse sensitivity.
     * @return Mouse sensitivity multiplier.
     */
    float GetMouseSensitivity() const { return m_MouseSensitivity; }

    /**
     * @brief Sets the sprint speed multiplier.
     * @param multiplier Sprint speed multiplier.
     */
    void SetSprintMultiplier(float multiplier) { m_SprintMultiplier = multiplier; }

    /**
     * @brief Gets the sprint speed multiplier.
     * @return Sprint speed multiplier.
     */
    float GetSprintMultiplier() const { return m_SprintMultiplier; }

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

    /**
     * @brief Sets the pitch angle constraints.
     * @param minPitch Minimum pitch angle in degrees (negative = look down).
     * @param maxPitch Maximum pitch angle in degrees (positive = look up).
     */
    void SetPitchConstraints(float minPitch, float maxPitch)
    {
        m_MinPitch = minPitch;
        m_MaxPitch = maxPitch;
    }

    // =========================================================================
    // Update
    // =========================================================================

    /**
     * @brief Updates the camera based on input.
     * @param deltaTime Time elapsed since last update in seconds.
     *
     * Call this once per frame after Input::Update().
     */
    void Update(float deltaTime)
    {
        if (!m_Enabled || m_Camera == nullptr)
        {
            return;
        }

        ProcessMouseMovement();
        ProcessKeyboardMovement(deltaTime);
    }

private:
    /**
     * @brief Processes mouse input for camera rotation.
     */
    void ProcessMouseMovement()
    {
        float deltaX = static_cast<float>(Platform::Input::GetMouseDeltaX());
        float deltaY = static_cast<float>(Platform::Input::GetMouseDeltaY());

        float yaw = m_Camera->GetYaw();
        float pitch = m_Camera->GetPitch();

        yaw += deltaX * m_MouseSensitivity;
        pitch -= deltaY * m_MouseSensitivity;  // Invert Y for natural feel

        // Clamp pitch to prevent camera flip
        pitch = std::clamp(pitch, m_MinPitch, m_MaxPitch);

        m_Camera->SetRotation(pitch, yaw);
    }

    /**
     * @brief Processes keyboard input for camera movement.
     * @param deltaTime Time elapsed since last frame.
     */
    void ProcessKeyboardMovement(float deltaTime)
    {
        glm::vec3 movement(0.0f);

        glm::vec3 forward = m_Camera->GetForward();
        glm::vec3 right = m_Camera->GetRight();
        glm::vec3 up(0.0f, 1.0f, 0.0f);  // World up for WASD movement

        // WASD movement
        if (Platform::Input::IsKeyDown(Platform::KeyCode::W))
        {
            movement += forward;
        }
        if (Platform::Input::IsKeyDown(Platform::KeyCode::S))
        {
            movement -= forward;
        }
        if (Platform::Input::IsKeyDown(Platform::KeyCode::A))
        {
            movement -= right;
        }
        if (Platform::Input::IsKeyDown(Platform::KeyCode::D))
        {
            movement += right;
        }

        // Vertical movement (Q = down, E = up)
        if (Platform::Input::IsKeyDown(Platform::KeyCode::E))
        {
            movement += up;
        }
        if (Platform::Input::IsKeyDown(Platform::KeyCode::Q))
        {
            movement -= up;
        }

        // Apply movement if any input was given
        if (glm::length(movement) > 0.0f)
        {
            movement = glm::normalize(movement);

            float speed = m_MoveSpeed;

            // Sprint with Shift
            if (Platform::Input::IsKeyDown(Platform::KeyCode::LeftShift) ||
                Platform::Input::IsKeyDown(Platform::KeyCode::RightShift))
            {
                speed *= m_SprintMultiplier;
            }

            glm::vec3 newPosition = m_Camera->GetPosition() + movement * speed * deltaTime;
            m_Camera->SetPosition(newPosition);
        }
    }

private:
    Camera* m_Camera = nullptr;

    // Control settings
    float m_MoveSpeed = 5.0f;
    float m_MouseSensitivity = 0.1f;
    float m_SprintMultiplier = 2.0f;

    // Pitch constraints
    float m_MinPitch = -89.0f;
    float m_MaxPitch = 89.0f;

    // State
    bool m_Enabled = true;
};

} // namespace Scene
