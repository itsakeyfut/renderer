#pragma once

#include "Core/Types.h"

#include <array>

// Forward declaration
struct GLFWwindow;

namespace Platform
{
    /**
     * @brief Keyboard key codes (maps to GLFW key codes)
     */
    enum class KeyCode : int
    {
        // Printable keys
        Space = 32,
        Apostrophe = 39,
        Comma = 44,
        Minus = 45,
        Period = 46,
        Slash = 47,

        // Numbers
        Num0 = 48, Num1 = 49, Num2 = 50, Num3 = 51, Num4 = 52,
        Num5 = 53, Num6 = 54, Num7 = 55, Num8 = 56, Num9 = 57,

        Semicolon = 59,
        Equal = 61,

        // Letters
        A = 65, B = 66, C = 67, D = 68, E = 69, F = 70, G = 71,
        H = 72, I = 73, J = 74, K = 75, L = 76, M = 77, N = 78,
        O = 79, P = 80, Q = 81, R = 82, S = 83, T = 84, U = 85,
        V = 86, W = 87, X = 88, Y = 89, Z = 90,

        LeftBracket = 91,
        Backslash = 92,
        RightBracket = 93,
        GraveAccent = 96,

        // Function keys
        Escape = 256,
        Enter = 257,
        Tab = 258,
        Backspace = 259,
        Insert = 260,
        Delete = 261,
        Right = 262,
        Left = 263,
        Down = 264,
        Up = 265,
        PageUp = 266,
        PageDown = 267,
        Home = 268,
        End = 269,
        CapsLock = 280,
        ScrollLock = 281,
        NumLock = 282,
        PrintScreen = 283,
        Pause = 284,

        F1 = 290, F2 = 291, F3 = 292, F4 = 293, F5 = 294, F6 = 295,
        F7 = 296, F8 = 297, F9 = 298, F10 = 299, F11 = 300, F12 = 301,

        // Keypad
        KP0 = 320, KP1 = 321, KP2 = 322, KP3 = 323, KP4 = 324,
        KP5 = 325, KP6 = 326, KP7 = 327, KP8 = 328, KP9 = 329,
        KPDecimal = 330,
        KPDivide = 331,
        KPMultiply = 332,
        KPSubtract = 333,
        KPAdd = 334,
        KPEnter = 335,
        KPEqual = 336,

        // Modifier keys
        LeftShift = 340,
        LeftControl = 341,
        LeftAlt = 342,
        LeftSuper = 343,
        RightShift = 344,
        RightControl = 345,
        RightAlt = 346,
        RightSuper = 347,
        Menu = 348,

        Last = 348
    };

    /**
     * @brief Mouse button codes
     */
    enum class MouseButton : int
    {
        Left = 0,
        Right = 1,
        Middle = 2,
        Button4 = 3,
        Button5 = 4,
        Button6 = 5,
        Button7 = 6,
        Button8 = 7,

        Last = 7
    };

    /**
     * @brief Cursor visibility modes
     */
    enum class CursorMode : int
    {
        Normal = 0,   // Cursor is visible and behaves normally
        Hidden = 1,   // Cursor is invisible but not locked
        Disabled = 2  // Cursor is hidden and locked to window (for FPS controls)
    };

    class Window;  // Forward declaration

    /**
     * @brief Input handling system for keyboard and mouse
     *
     * Provides static methods to query input state. Must be initialized
     * with a Window before use. The Input::Update() method should be
     * called once per frame after Window::PollEvents() to properly
     * track pressed/released state transitions.
     *
     * Usage example:
     * @code
     * // Initialization
     * Platform::Input::Init(window);
     *
     * // In game loop
     * window.PollEvents();
     * Platform::Input::Update();
     *
     * if (Platform::Input::IsKeyDown(Platform::KeyCode::W)) {
     *     // Move forward
     * }
     * if (Platform::Input::IsKeyPressed(Platform::KeyCode::Space)) {
     *     // Jump (only triggers once per press)
     * }
     * @endcode
     */
    class Input
    {
    public:
        /**
         * @brief Initialize the input system with a window
         * @param window The window to capture input from
         *
         * This must be called before any other Input methods.
         * Registers GLFW callbacks for input events.
         */
        static void Init(Window& window);

        /**
         * @brief Shutdown the input system
         *
         * Removes GLFW callbacks and resets internal state.
         */
        static void Shutdown();

        /**
         * @brief Update input state
         *
         * Must be called once per frame after Window::PollEvents()
         * to properly track pressed/released transitions.
         */
        static void Update();

        // =====================================================================
        // Keyboard Input
        // =====================================================================

        /**
         * @brief Check if a key is currently held down
         * @param key The key to check
         * @return true if the key is pressed
         */
        static bool IsKeyDown(KeyCode key);

        /**
         * @brief Check if a key was pressed this frame
         * @param key The key to check
         * @return true if the key was just pressed this frame
         */
        static bool IsKeyPressed(KeyCode key);

        /**
         * @brief Check if a key was released this frame
         * @param key The key to check
         * @return true if the key was just released this frame
         */
        static bool IsKeyReleased(KeyCode key);

        // =====================================================================
        // Mouse Button Input
        // =====================================================================

        /**
         * @brief Check if a mouse button is currently held down
         * @param button The mouse button to check
         * @return true if the button is pressed
         */
        static bool IsMouseButtonDown(MouseButton button);

        /**
         * @brief Check if a mouse button was pressed this frame
         * @param button The mouse button to check
         * @return true if the button was just pressed this frame
         */
        static bool IsMouseButtonPressed(MouseButton button);

        /**
         * @brief Check if a mouse button was released this frame
         * @param button The mouse button to check
         * @return true if the button was just released this frame
         */
        static bool IsMouseButtonReleased(MouseButton button);

        // =====================================================================
        // Mouse Position
        // =====================================================================

        /**
         * @brief Get current mouse X position
         * @return Mouse X coordinate in window space
         */
        static double GetMouseX();

        /**
         * @brief Get current mouse Y position
         * @return Mouse Y coordinate in window space
         */
        static double GetMouseY();

        /**
         * @brief Get mouse movement since last frame
         * @return X movement delta
         */
        static double GetMouseDeltaX();

        /**
         * @brief Get mouse movement since last frame
         * @return Y movement delta
         */
        static double GetMouseDeltaY();

        /**
         * @brief Get mouse scroll wheel delta
         * @return Scroll delta (positive = up, negative = down)
         */
        static double GetScrollDelta();

        // =====================================================================
        // Cursor Control
        // =====================================================================

        /**
         * @brief Set cursor visibility mode
         * @param mode The cursor mode to set
         */
        static void SetCursorMode(CursorMode mode);

        /**
         * @brief Get current cursor mode
         * @return Current cursor mode
         */
        static CursorMode GetCursorMode();

    private:
        // Internal state
        static constexpr int MAX_KEYS = 349;
        static constexpr int MAX_MOUSE_BUTTONS = 8;

        static bool s_Initialized;
        static GLFWwindow* s_Window;

        // Key state arrays
        static std::array<bool, MAX_KEYS> s_Keys;
        static std::array<bool, MAX_KEYS> s_KeysPrevious;

        // Mouse button state arrays
        static std::array<bool, MAX_MOUSE_BUTTONS> s_MouseButtons;
        static std::array<bool, MAX_MOUSE_BUTTONS> s_MouseButtonsPrevious;

        // Mouse position
        static double s_MouseX;
        static double s_MouseY;
        static double s_MousePrevX;
        static double s_MousePrevY;
        static double s_ScrollDelta;

        static CursorMode s_CursorMode;

        // GLFW callbacks
        static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
        static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
        static void CursorPosCallback(GLFWwindow* window, double xpos, double ypos);
        static void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
    };
}
