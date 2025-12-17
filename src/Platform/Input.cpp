#include "Platform/Input.h"
#include "Platform/Window.h"
#include "Core/Log.h"
#include "Core/Assert.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace Platform
{
    // Static member initialization
    bool Input::s_Initialized = false;
    GLFWwindow* Input::s_Window = nullptr;

    std::array<bool, Input::MAX_KEYS> Input::s_Keys = {};
    std::array<bool, Input::MAX_KEYS> Input::s_KeysPrevious = {};

    std::array<bool, Input::MAX_MOUSE_BUTTONS> Input::s_MouseButtons = {};
    std::array<bool, Input::MAX_MOUSE_BUTTONS> Input::s_MouseButtonsPrevious = {};

    double Input::s_MouseX = 0.0;
    double Input::s_MouseY = 0.0;
    double Input::s_MousePrevX = 0.0;
    double Input::s_MousePrevY = 0.0;
    double Input::s_ScrollDelta = 0.0;

    CursorMode Input::s_CursorMode = CursorMode::Normal;

    void Input::Init(Window& window)
    {
        ASSERT_MSG(!s_Initialized, "Input system already initialized");

        s_Window = window.GetHandle();
        ASSERT_MSG(s_Window != nullptr, "Window handle is null");

        // Reset all state
        s_Keys.fill(false);
        s_KeysPrevious.fill(false);
        s_MouseButtons.fill(false);
        s_MouseButtonsPrevious.fill(false);

        // Get initial mouse position
        glfwGetCursorPos(s_Window, &s_MouseX, &s_MouseY);
        s_MousePrevX = s_MouseX;
        s_MousePrevY = s_MouseY;
        s_ScrollDelta = 0.0;

        // Register GLFW callbacks
        glfwSetKeyCallback(s_Window, KeyCallback);
        glfwSetMouseButtonCallback(s_Window, MouseButtonCallback);
        glfwSetCursorPosCallback(s_Window, CursorPosCallback);
        glfwSetScrollCallback(s_Window, ScrollCallback);

        s_Initialized = true;
        LOG_INFO("Input system initialized");
    }

    void Input::Shutdown()
    {
        if (!s_Initialized)
        {
            return;
        }

        // Remove callbacks
        if (s_Window)
        {
            glfwSetKeyCallback(s_Window, nullptr);
            glfwSetMouseButtonCallback(s_Window, nullptr);
            glfwSetCursorPosCallback(s_Window, nullptr);
            glfwSetScrollCallback(s_Window, nullptr);
        }

        // Reset state
        s_Keys.fill(false);
        s_KeysPrevious.fill(false);
        s_MouseButtons.fill(false);
        s_MouseButtonsPrevious.fill(false);
        s_MouseX = 0.0;
        s_MouseY = 0.0;
        s_MousePrevX = 0.0;
        s_MousePrevY = 0.0;
        s_ScrollDelta = 0.0;
        s_Window = nullptr;

        s_Initialized = false;
        LOG_DEBUG("Input system shutdown");
    }

    void Input::Update()
    {
        if (!s_Initialized)
        {
            return;
        }

        // Copy current state to previous state for press/release detection
        s_KeysPrevious = s_Keys;
        s_MouseButtonsPrevious = s_MouseButtons;

        // Update mouse delta
        s_MousePrevX = s_MouseX;
        s_MousePrevY = s_MouseY;

        // Get current mouse position
        glfwGetCursorPos(s_Window, &s_MouseX, &s_MouseY);

        // Reset scroll delta (accumulates between frames via callback)
        s_ScrollDelta = 0.0;
    }

    // =========================================================================
    // Keyboard Input
    // =========================================================================

    bool Input::IsKeyDown(KeyCode key)
    {
        int keyIndex = static_cast<int>(key);
        if (keyIndex < 0 || keyIndex >= MAX_KEYS)
        {
            return false;
        }
        return s_Keys[keyIndex];
    }

    bool Input::IsKeyPressed(KeyCode key)
    {
        int keyIndex = static_cast<int>(key);
        if (keyIndex < 0 || keyIndex >= MAX_KEYS)
        {
            return false;
        }
        return s_Keys[keyIndex] && !s_KeysPrevious[keyIndex];
    }

    bool Input::IsKeyReleased(KeyCode key)
    {
        int keyIndex = static_cast<int>(key);
        if (keyIndex < 0 || keyIndex >= MAX_KEYS)
        {
            return false;
        }
        return !s_Keys[keyIndex] && s_KeysPrevious[keyIndex];
    }

    // =========================================================================
    // Mouse Button Input
    // =========================================================================

    bool Input::IsMouseButtonDown(MouseButton button)
    {
        int buttonIndex = static_cast<int>(button);
        if (buttonIndex < 0 || buttonIndex >= MAX_MOUSE_BUTTONS)
        {
            return false;
        }
        return s_MouseButtons[buttonIndex];
    }

    bool Input::IsMouseButtonPressed(MouseButton button)
    {
        int buttonIndex = static_cast<int>(button);
        if (buttonIndex < 0 || buttonIndex >= MAX_MOUSE_BUTTONS)
        {
            return false;
        }
        return s_MouseButtons[buttonIndex] && !s_MouseButtonsPrevious[buttonIndex];
    }

    bool Input::IsMouseButtonReleased(MouseButton button)
    {
        int buttonIndex = static_cast<int>(button);
        if (buttonIndex < 0 || buttonIndex >= MAX_MOUSE_BUTTONS)
        {
            return false;
        }
        return !s_MouseButtons[buttonIndex] && s_MouseButtonsPrevious[buttonIndex];
    }

    // =========================================================================
    // Mouse Position
    // =========================================================================

    double Input::GetMouseX()
    {
        return s_MouseX;
    }

    double Input::GetMouseY()
    {
        return s_MouseY;
    }

    double Input::GetMouseDeltaX()
    {
        return s_MouseX - s_MousePrevX;
    }

    double Input::GetMouseDeltaY()
    {
        return s_MouseY - s_MousePrevY;
    }

    double Input::GetScrollDelta()
    {
        return s_ScrollDelta;
    }

    // =========================================================================
    // Cursor Control
    // =========================================================================

    void Input::SetCursorMode(CursorMode mode)
    {
        if (!s_Window)
        {
            return;
        }

        int glfwMode = GLFW_CURSOR_NORMAL;
        switch (mode)
        {
            case CursorMode::Normal:
                glfwMode = GLFW_CURSOR_NORMAL;
                break;
            case CursorMode::Hidden:
                glfwMode = GLFW_CURSOR_HIDDEN;
                break;
            case CursorMode::Disabled:
                glfwMode = GLFW_CURSOR_DISABLED;
                break;
        }

        glfwSetInputMode(s_Window, GLFW_CURSOR, glfwMode);
        s_CursorMode = mode;

        LOG_DEBUG("Cursor mode changed to: {}", static_cast<int>(mode));
    }

    CursorMode Input::GetCursorMode()
    {
        return s_CursorMode;
    }

    // =========================================================================
    // GLFW Callbacks
    // =========================================================================

    void Input::KeyCallback(GLFWwindow* /*window*/, int key, int /*scancode*/, int action, int /*mods*/)
    {
        if (key < 0 || key >= MAX_KEYS)
        {
            return;
        }

        if (action == GLFW_PRESS)
        {
            s_Keys[key] = true;
        }
        else if (action == GLFW_RELEASE)
        {
            s_Keys[key] = false;
        }
        // GLFW_REPEAT is ignored - key remains pressed
    }

    void Input::MouseButtonCallback(GLFWwindow* /*window*/, int button, int action, int /*mods*/)
    {
        if (button < 0 || button >= MAX_MOUSE_BUTTONS)
        {
            return;
        }

        if (action == GLFW_PRESS)
        {
            s_MouseButtons[button] = true;
        }
        else if (action == GLFW_RELEASE)
        {
            s_MouseButtons[button] = false;
        }
    }

    void Input::CursorPosCallback(GLFWwindow* /*window*/, double xpos, double ypos)
    {
        s_MouseX = xpos;
        s_MouseY = ypos;
    }

    void Input::ScrollCallback(GLFWwindow* /*window*/, double /*xoffset*/, double yoffset)
    {
        // Accumulate scroll delta (in case multiple scroll events happen per frame)
        s_ScrollDelta += yoffset;
    }
}
