/**
 * @file TestCamera.cpp
 * @brief Unit tests for the Camera system (Camera, FPSCameraController, OrbitCameraController).
 */

#include <gtest/gtest.h>
#include <glm/glm.hpp>
#include <glm/gtc/epsilon.hpp>
#include <glm/gtc/constants.hpp>

#include "Scene/Camera.h"
#include "Scene/FPSCameraController.h"
#include "Scene/OrbitCameraController.h"
#include "Resources/UniformBufferObjects.h"

// =============================================================================
// Camera Tests
// =============================================================================

class CameraTest : public ::testing::Test {
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

TEST_F(CameraTest, DefaultConstructor)
{
    Scene::Camera camera;

    // Default position is (0, 0, 5)
    EXPECT_TRUE(VecEqual(camera.GetPosition(), glm::vec3(0.0f, 0.0f, 5.0f)));

    // Default angles: pitch = 0, yaw = -90 (facing -Z)
    EXPECT_FLOAT_EQ(camera.GetPitch(), 0.0f);
    EXPECT_FLOAT_EQ(camera.GetYaw(), -90.0f);

    // Default projection is perspective
    EXPECT_EQ(camera.GetProjectionType(), Scene::Camera::ProjectionType::Perspective);

    // Default perspective parameters
    EXPECT_FLOAT_EQ(camera.GetFovY(), 45.0f);
    EXPECT_FLOAT_EQ(camera.GetNearPlane(), 0.1f);
    EXPECT_FLOAT_EQ(camera.GetFarPlane(), 1000.0f);
}

TEST_F(CameraTest, SetPosition)
{
    Scene::Camera camera;
    glm::vec3 newPos(10.0f, 20.0f, 30.0f);

    camera.SetPosition(newPos);

    EXPECT_TRUE(VecEqual(camera.GetPosition(), newPos));
}

TEST_F(CameraTest, SetRotation)
{
    Scene::Camera camera;

    camera.SetRotation(45.0f, 180.0f);

    EXPECT_FLOAT_EQ(camera.GetPitch(), 45.0f);
    EXPECT_FLOAT_EQ(camera.GetYaw(), 180.0f);
}

TEST_F(CameraTest, SetPerspective)
{
    Scene::Camera camera;

    camera.SetPerspective(60.0f, 16.0f / 9.0f, 0.5f, 500.0f);

    EXPECT_EQ(camera.GetProjectionType(), Scene::Camera::ProjectionType::Perspective);
    EXPECT_FLOAT_EQ(camera.GetFovY(), 60.0f);
    EXPECT_FLOAT_EQ(camera.GetAspectRatio(), 16.0f / 9.0f);
    EXPECT_FLOAT_EQ(camera.GetNearPlane(), 0.5f);
    EXPECT_FLOAT_EQ(camera.GetFarPlane(), 500.0f);
}

TEST_F(CameraTest, SetOrthographic)
{
    Scene::Camera camera;

    camera.SetOrthographic(-10.0f, 10.0f, -5.0f, 5.0f, 0.1f, 100.0f);

    EXPECT_EQ(camera.GetProjectionType(), Scene::Camera::ProjectionType::Orthographic);
    EXPECT_FLOAT_EQ(camera.GetNearPlane(), 0.1f);
    EXPECT_FLOAT_EQ(camera.GetFarPlane(), 100.0f);
}

TEST_F(CameraTest, GetForward)
{
    Scene::Camera camera;

    // Default: pitch = 0, yaw = -90 should give forward = (0, 0, -1)
    glm::vec3 forward = camera.GetForward();

    // With yaw = -90 and pitch = 0:
    // forward.x = cos(0) * sin(-90) = 1 * (-1) = -1... wait
    // Let me check the formula:
    // forward.x = cos(pitch) * sin(yaw)
    // forward.y = sin(pitch)
    // forward.z = -cos(pitch) * cos(yaw)
    // With pitch = 0, yaw = -90:
    // forward.x = cos(0) * sin(-90) = 1 * (-1) = -1
    // forward.y = sin(0) = 0
    // forward.z = -cos(0) * cos(-90) = -1 * 0 = 0
    // So forward = (-1, 0, 0)... but we want (0, 0, -1)

    // Actually, looking at the implementation:
    // yaw = -90 degrees means we're looking in the -Z direction
    // Let me verify the expected forward vector from the implementation
    EXPECT_NEAR(forward.z, -1.0f, 0.01f);
    EXPECT_NEAR(forward.y, 0.0f, 0.01f);
    EXPECT_NEAR(std::abs(forward.x), 0.0f, 0.01f);
}

TEST_F(CameraTest, GetForwardAfterRotation)
{
    Scene::Camera camera;

    // Looking in +X direction (yaw = 0)
    camera.SetRotation(0.0f, 0.0f);
    glm::vec3 forward = camera.GetForward();

    // With yaw = 0, pitch = 0, forward should point in +X direction
    // forward.x = cos(0) * cos(0) = 1
    // forward.z = cos(0) * sin(0) = 0
    EXPECT_NEAR(forward.x, 1.0f, 0.01f);
    EXPECT_NEAR(forward.z, 0.0f, 0.01f);
}

TEST_F(CameraTest, GetRight)
{
    Scene::Camera camera;
    camera.SetRotation(0.0f, -90.0f);  // Default orientation

    glm::vec3 right = camera.GetRight();

    // Right should be perpendicular to forward and up
    glm::vec3 forward = camera.GetForward();
    float dot = glm::dot(forward, right);
    EXPECT_NEAR(dot, 0.0f, 0.01f);

    // Right vector should be roughly (1, 0, 0) at default orientation
    EXPECT_NEAR(right.x, 1.0f, 0.01f);
    EXPECT_NEAR(right.y, 0.0f, 0.01f);
}

TEST_F(CameraTest, GetUp)
{
    Scene::Camera camera;
    camera.SetRotation(0.0f, -90.0f);

    glm::vec3 up = camera.GetUp();

    // Up should be perpendicular to forward
    glm::vec3 forward = camera.GetForward();
    float dot = glm::dot(forward, up);
    EXPECT_NEAR(dot, 0.0f, 0.01f);

    // At default orientation with pitch = 0, up should be (0, 1, 0)
    EXPECT_NEAR(up.y, 1.0f, 0.01f);
}

TEST_F(CameraTest, ViewMatrix)
{
    Scene::Camera camera;
    camera.SetPosition(glm::vec3(0.0f, 0.0f, 5.0f));
    camera.SetRotation(0.0f, -90.0f);

    glm::mat4 view = camera.GetViewMatrix();

    // View matrix should be valid (not identity)
    EXPECT_FALSE(MatEqual(view, glm::mat4(1.0f)));

    // A point at origin should be at (0, 0, -5) in view space
    glm::vec4 originInView = view * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    EXPECT_NEAR(originInView.z, -5.0f, 0.01f);
}

TEST_F(CameraTest, ProjectionMatrix)
{
    Scene::Camera camera;
    camera.SetPerspective(45.0f, 16.0f / 9.0f, 0.1f, 100.0f);

    glm::mat4 projection = camera.GetProjectionMatrix();

    // Projection matrix should not be identity
    EXPECT_FALSE(MatEqual(projection, glm::mat4(1.0f)));
}

TEST_F(CameraTest, ViewProjectionMatrix)
{
    Scene::Camera camera;
    camera.SetPosition(glm::vec3(0.0f, 0.0f, 10.0f));
    camera.SetPerspective(45.0f, 16.0f / 9.0f, 0.1f, 100.0f);

    glm::mat4 viewProj = camera.GetViewProjectionMatrix();
    glm::mat4 view = camera.GetViewMatrix();
    glm::mat4 proj = camera.GetProjectionMatrix();

    // ViewProjection should equal Projection * View
    glm::mat4 expected = proj * view;
    EXPECT_TRUE(MatEqual(viewProj, expected));
}

TEST_F(CameraTest, LookAt)
{
    Scene::Camera camera;
    camera.SetPosition(glm::vec3(5.0f, 0.0f, 0.0f));

    camera.LookAt(glm::vec3(0.0f, 0.0f, 0.0f));

    // After looking at origin from (5, 0, 0), forward should be (-1, 0, 0)
    // direction = (0,0,0) - (5,0,0) = (-1, 0, 0) normalized
    // yaw = atan2(0, -1) = 180 degrees
    // GetForward: x = cos(0)*cos(180) = -1, z = cos(0)*sin(180) = 0
    glm::vec3 forward = camera.GetForward();
    EXPECT_NEAR(forward.x, -1.0f, 0.01f);
    EXPECT_NEAR(forward.y, 0.0f, 0.01f);
    EXPECT_NEAR(forward.z, 0.0f, 0.01f);
}

TEST_F(CameraTest, LookAtSamePosition)
{
    Scene::Camera camera;
    camera.SetPosition(glm::vec3(5.0f, 0.0f, 0.0f));
    camera.SetRotation(30.0f, 45.0f);

    float originalPitch = camera.GetPitch();
    float originalYaw = camera.GetYaw();

    // LookAt with target at same position as camera should be a no-op
    camera.LookAt(glm::vec3(5.0f, 0.0f, 0.0f));

    // Rotation should remain unchanged
    EXPECT_FLOAT_EQ(camera.GetPitch(), originalPitch);
    EXPECT_FLOAT_EQ(camera.GetYaw(), originalYaw);
}

TEST_F(CameraTest, VulkanClipCorrection)
{
    glm::mat4 correction = Scene::Camera::GetVulkanClipCorrection();

    // The Y-flip matrix should negate Y
    glm::vec4 point(1.0f, 1.0f, 1.0f, 1.0f);
    glm::vec4 result = correction * point;

    EXPECT_FLOAT_EQ(result.x, 1.0f);
    EXPECT_FLOAT_EQ(result.y, -1.0f);  // Y should be flipped
    EXPECT_FLOAT_EQ(result.z, 1.0f);
    EXPECT_FLOAT_EQ(result.w, 1.0f);
}

TEST_F(CameraTest, AspectRatioUpdate)
{
    Scene::Camera camera;
    camera.SetPerspective(45.0f, 4.0f / 3.0f, 0.1f, 100.0f);

    EXPECT_FLOAT_EQ(camera.GetAspectRatio(), 4.0f / 3.0f);

    camera.SetAspectRatio(16.0f / 9.0f);

    EXPECT_FLOAT_EQ(camera.GetAspectRatio(), 16.0f / 9.0f);
}

TEST_F(CameraTest, BuildCameraUBO)
{
    Scene::Camera camera;
    camera.SetPosition(glm::vec3(1.0f, 2.0f, 3.0f));
    camera.SetPerspective(45.0f, 16.0f / 9.0f, 0.1f, 100.0f);

    Resources::CameraUBO ubo = camera.BuildCameraUBO();

    // Verify position is copied correctly
    EXPECT_TRUE(VecEqual(ubo.CameraPosition, glm::vec3(1.0f, 2.0f, 3.0f)));

    // Verify matrices are populated (not identity)
    EXPECT_FALSE(MatEqual(ubo.View, glm::mat4(1.0f)));
    EXPECT_FALSE(MatEqual(ubo.Projection, glm::mat4(1.0f)));
    EXPECT_FALSE(MatEqual(ubo.ViewProjection, glm::mat4(1.0f)));

    // Verify ViewProjection = Projection * View
    glm::mat4 expectedViewProj = ubo.Projection * ubo.View;
    EXPECT_TRUE(MatEqual(ubo.ViewProjection, expectedViewProj));
}

// =============================================================================
// FPSCameraController Tests
// =============================================================================

class FPSCameraControllerTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(FPSCameraControllerTest, DefaultConstructor)
{
    Scene::FPSCameraController controller;

    EXPECT_EQ(controller.GetCamera(), nullptr);
    EXPECT_FLOAT_EQ(controller.GetMoveSpeed(), 5.0f);
    EXPECT_FLOAT_EQ(controller.GetMouseSensitivity(), 0.1f);
    EXPECT_TRUE(controller.IsEnabled());
}

TEST_F(FPSCameraControllerTest, ConstructWithCamera)
{
    Scene::Camera camera;
    Scene::FPSCameraController controller(&camera);

    EXPECT_EQ(controller.GetCamera(), &camera);
}

TEST_F(FPSCameraControllerTest, SetCamera)
{
    Scene::Camera camera;
    Scene::FPSCameraController controller;

    controller.SetCamera(&camera);

    EXPECT_EQ(controller.GetCamera(), &camera);
}

TEST_F(FPSCameraControllerTest, SetMoveSpeed)
{
    Scene::FPSCameraController controller;

    controller.SetMoveSpeed(10.0f);

    EXPECT_FLOAT_EQ(controller.GetMoveSpeed(), 10.0f);
}

TEST_F(FPSCameraControllerTest, SetMouseSensitivity)
{
    Scene::FPSCameraController controller;

    controller.SetMouseSensitivity(0.5f);

    EXPECT_FLOAT_EQ(controller.GetMouseSensitivity(), 0.5f);
}

TEST_F(FPSCameraControllerTest, SetSprintMultiplier)
{
    Scene::FPSCameraController controller;

    controller.SetSprintMultiplier(3.0f);

    EXPECT_FLOAT_EQ(controller.GetSprintMultiplier(), 3.0f);
}

TEST_F(FPSCameraControllerTest, SetEnabled)
{
    Scene::FPSCameraController controller;

    controller.SetEnabled(false);
    EXPECT_FALSE(controller.IsEnabled());

    controller.SetEnabled(true);
    EXPECT_TRUE(controller.IsEnabled());
}

TEST_F(FPSCameraControllerTest, SetPitchConstraints)
{
    Scene::FPSCameraController controller;

    // Just verify no crash - constraints are internal
    controller.SetPitchConstraints(-45.0f, 45.0f);
}

// =============================================================================
// OrbitCameraController Tests
// =============================================================================

class OrbitCameraControllerTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}

    static bool VecEqual(const glm::vec3& a, const glm::vec3& b, float epsilon = 0.0001f)
    {
        return glm::all(glm::epsilonEqual(a, b, epsilon));
    }
};

TEST_F(OrbitCameraControllerTest, DefaultConstructor)
{
    Scene::OrbitCameraController controller;

    EXPECT_EQ(controller.GetCamera(), nullptr);
    EXPECT_TRUE(VecEqual(controller.GetTarget(), glm::vec3(0.0f)));
    EXPECT_FLOAT_EQ(controller.GetDistance(), 10.0f);
    EXPECT_TRUE(controller.IsEnabled());
}

TEST_F(OrbitCameraControllerTest, ConstructWithCamera)
{
    Scene::Camera camera;
    camera.SetPosition(glm::vec3(0.0f, 5.0f, 10.0f));

    Scene::OrbitCameraController controller(&camera);

    EXPECT_EQ(controller.GetCamera(), &camera);
    // Distance should be calculated from camera position to target
    EXPECT_GT(controller.GetDistance(), 0.0f);
}

TEST_F(OrbitCameraControllerTest, SetTarget)
{
    Scene::OrbitCameraController controller;
    glm::vec3 target(5.0f, 10.0f, 15.0f);

    controller.SetTarget(target);

    EXPECT_TRUE(VecEqual(controller.GetTarget(), target));
}

TEST_F(OrbitCameraControllerTest, SetDistance)
{
    Scene::Camera camera;
    Scene::OrbitCameraController controller(&camera);

    controller.SetDistance(20.0f);

    EXPECT_FLOAT_EQ(controller.GetDistance(), 20.0f);
}

TEST_F(OrbitCameraControllerTest, SetAzimuth)
{
    Scene::Camera camera;
    Scene::OrbitCameraController controller(&camera);

    controller.SetAzimuth(45.0f);

    EXPECT_FLOAT_EQ(controller.GetAzimuth(), 45.0f);
}

TEST_F(OrbitCameraControllerTest, SetElevation)
{
    Scene::Camera camera;
    Scene::OrbitCameraController controller(&camera);

    controller.SetElevation(60.0f);

    EXPECT_FLOAT_EQ(controller.GetElevation(), 60.0f);
}

TEST_F(OrbitCameraControllerTest, ElevationClamping)
{
    Scene::Camera camera;
    Scene::OrbitCameraController controller(&camera);

    // Default constraints are -89 to 89
    controller.SetElevation(100.0f);
    EXPECT_LE(controller.GetElevation(), 89.0f);

    controller.SetElevation(-100.0f);
    EXPECT_GE(controller.GetElevation(), -89.0f);
}

TEST_F(OrbitCameraControllerTest, SetDistanceConstraints)
{
    Scene::Camera camera;
    Scene::OrbitCameraController controller(&camera);

    controller.SetDistanceConstraints(5.0f, 50.0f);
    controller.SetDistance(100.0f);

    // Distance should be clamped
    EXPECT_LE(controller.GetDistance(), 50.0f);

    controller.SetDistance(1.0f);
    EXPECT_GE(controller.GetDistance(), 5.0f);
}

TEST_F(OrbitCameraControllerTest, SetSensitivity)
{
    Scene::OrbitCameraController controller;

    controller.SetRotationSensitivity(1.0f);
    EXPECT_FLOAT_EQ(controller.GetRotationSensitivity(), 1.0f);

    controller.SetPanSensitivity(2.0f);
    EXPECT_FLOAT_EQ(controller.GetPanSensitivity(), 2.0f);

    controller.SetZoomSensitivity(3.0f);
    EXPECT_FLOAT_EQ(controller.GetZoomSensitivity(), 3.0f);
}

TEST_F(OrbitCameraControllerTest, SetEnabled)
{
    Scene::OrbitCameraController controller;

    controller.SetEnabled(false);
    EXPECT_FALSE(controller.IsEnabled());

    controller.SetEnabled(true);
    EXPECT_TRUE(controller.IsEnabled());
}

TEST_F(OrbitCameraControllerTest, CameraPositionUpdatesWithOrbit)
{
    Scene::Camera camera;
    Scene::OrbitCameraController controller(&camera);

    glm::vec3 target(0.0f, 0.0f, 0.0f);
    controller.SetTarget(target);
    controller.SetDistance(10.0f);
    controller.SetAzimuth(0.0f);
    controller.SetElevation(0.0f);

    // Camera should be positioned at distance from target along Z axis
    glm::vec3 camPos = camera.GetPosition();
    float distanceToTarget = glm::length(camPos - target);
    EXPECT_NEAR(distanceToTarget, 10.0f, 0.1f);
}
