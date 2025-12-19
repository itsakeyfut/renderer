/**
 * @file Event.h
 * @brief Base event class and common event types for the renderer.
 *
 * This file provides the foundation for a loosely-coupled event system
 * that allows modules to communicate without direct dependencies.
 */

#pragma once

#include "Core/Types.h"

#include <cstdint>
#include <string>

namespace Core {

// =============================================================================
// Event Category Flags
// =============================================================================

/**
 * @brief Bit flags for event categorization.
 *
 * Events can belong to multiple categories, allowing listeners
 * to filter events by type.
 */
enum class EventCategory : uint32_t
{
    None        = 0,
    Application = 1 << 0,
    Window      = 1 << 1,
    Input       = 1 << 2,
    Keyboard    = 1 << 3,
    Mouse       = 1 << 4,
    MouseButton = 1 << 5
};

/**
 * @brief Bitwise OR operator for EventCategory flags.
 */
inline EventCategory operator|(EventCategory lhs, EventCategory rhs)
{
    return static_cast<EventCategory>(
        static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs)
    );
}

/**
 * @brief Bitwise AND operator for EventCategory flags.
 */
inline EventCategory operator&(EventCategory lhs, EventCategory rhs)
{
    return static_cast<EventCategory>(
        static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs)
    );
}

// =============================================================================
// Event Type Enum
// =============================================================================

/**
 * @brief Enumeration of all event types.
 */
enum class EventType
{
    None = 0,

    // Window events
    WindowClose,
    WindowResize,
    WindowFocus,
    WindowLostFocus,
    WindowMinimize,
    WindowRestore,

    // Keyboard events
    KeyPress,
    KeyRelease,
    KeyRepeat,

    // Mouse events
    MouseMove,
    MouseScroll,
    MouseButtonPress,
    MouseButtonRelease
};

// =============================================================================
// Event Base Class
// =============================================================================

/**
 * @brief Base class for all events in the system.
 *
 * Events carry information about something that happened (e.g., window resize,
 * key press) and can be dispatched to registered listeners. The `Handled` flag
 * allows listeners to stop event propagation.
 *
 * Example usage:
 * @code
 * void OnEvent(Event& event) {
 *     if (event.GetType() == EventType::WindowResize) {
 *         auto& resize = static_cast<WindowResizeEvent&>(event);
 *         RecreateSwapchain(resize.GetWidth(), resize.GetHeight());
 *         event.Handled = true;
 *     }
 * }
 * @endcode
 */
class Event
{
public:
    virtual ~Event() = default;

    /**
     * @brief Get the event type identifier.
     * @return The EventType of this event.
     */
    virtual EventType GetType() const = 0;

    /**
     * @brief Get the human-readable name of the event.
     * @return String name of the event type.
     */
    virtual const char* GetName() const = 0;

    /**
     * @brief Get the category flags for this event.
     * @return Combined EventCategory flags.
     */
    virtual EventCategory GetCategoryFlags() const = 0;

    /**
     * @brief Check if this event belongs to a specific category.
     * @param category The category to check against.
     * @return true if the event is in the specified category.
     */
    bool IsInCategory(EventCategory category) const
    {
        return (GetCategoryFlags() & category) != EventCategory::None;
    }

    /**
     * @brief Get a string representation of the event for debugging.
     * @return String describing the event.
     */
    virtual std::string ToString() const { return GetName(); }

    /**
     * @brief Flag indicating whether the event has been handled.
     *
     * When set to true, the event will not be propagated to further listeners.
     */
    bool Handled = false;
};

// =============================================================================
// Window Events
// =============================================================================

/**
 * @brief Event fired when the window is closed.
 */
class WindowCloseEvent : public Event
{
public:
    WindowCloseEvent() = default;

    EventType GetType() const override { return EventType::WindowClose; }
    const char* GetName() const override { return "WindowClose"; }
    EventCategory GetCategoryFlags() const override
    {
        return EventCategory::Application | EventCategory::Window;
    }
};

/**
 * @brief Event fired when the window is resized.
 */
class WindowResizeEvent : public Event
{
public:
    /**
     * @brief Construct a window resize event.
     * @param width New window width in pixels.
     * @param height New window height in pixels.
     */
    WindowResizeEvent(uint32_t width, uint32_t height)
        : m_Width(width), m_Height(height) {}

    EventType GetType() const override { return EventType::WindowResize; }
    const char* GetName() const override { return "WindowResize"; }
    EventCategory GetCategoryFlags() const override
    {
        return EventCategory::Application | EventCategory::Window;
    }

    /**
     * @brief Get the new window width.
     * @return Width in pixels.
     */
    uint32_t GetWidth() const { return m_Width; }

    /**
     * @brief Get the new window height.
     * @return Height in pixels.
     */
    uint32_t GetHeight() const { return m_Height; }

    std::string ToString() const override
    {
        return std::string("WindowResize: ") + std::to_string(m_Width) +
               " x " + std::to_string(m_Height);
    }

private:
    uint32_t m_Width;
    uint32_t m_Height;
};

/**
 * @brief Event fired when the window gains focus.
 */
class WindowFocusEvent : public Event
{
public:
    WindowFocusEvent() = default;

    EventType GetType() const override { return EventType::WindowFocus; }
    const char* GetName() const override { return "WindowFocus"; }
    EventCategory GetCategoryFlags() const override
    {
        return EventCategory::Application | EventCategory::Window;
    }
};

/**
 * @brief Event fired when the window loses focus.
 */
class WindowLostFocusEvent : public Event
{
public:
    WindowLostFocusEvent() = default;

    EventType GetType() const override { return EventType::WindowLostFocus; }
    const char* GetName() const override { return "WindowLostFocus"; }
    EventCategory GetCategoryFlags() const override
    {
        return EventCategory::Application | EventCategory::Window;
    }
};

/**
 * @brief Event fired when the window is minimized.
 */
class WindowMinimizeEvent : public Event
{
public:
    WindowMinimizeEvent() = default;

    EventType GetType() const override { return EventType::WindowMinimize; }
    const char* GetName() const override { return "WindowMinimize"; }
    EventCategory GetCategoryFlags() const override
    {
        return EventCategory::Application | EventCategory::Window;
    }
};

/**
 * @brief Event fired when the window is restored from minimized state.
 */
class WindowRestoreEvent : public Event
{
public:
    WindowRestoreEvent() = default;

    EventType GetType() const override { return EventType::WindowRestore; }
    const char* GetName() const override { return "WindowRestore"; }
    EventCategory GetCategoryFlags() const override
    {
        return EventCategory::Application | EventCategory::Window;
    }
};

// =============================================================================
// Keyboard Events
// =============================================================================

/**
 * @brief Base class for keyboard events.
 */
class KeyEvent : public Event
{
public:
    /**
     * @brief Get the key code for this event.
     * @return The key code (maps to GLFW key codes).
     */
    int GetKeyCode() const { return m_KeyCode; }

    /**
     * @brief Get the modifier keys held during this event.
     * @return Bitmask of modifier keys (shift, ctrl, alt, etc.).
     */
    int GetMods() const { return m_Mods; }

    EventCategory GetCategoryFlags() const override
    {
        return EventCategory::Input | EventCategory::Keyboard;
    }

protected:
    KeyEvent(int keyCode, int mods)
        : m_KeyCode(keyCode), m_Mods(mods) {}

    int m_KeyCode;
    int m_Mods;
};

/**
 * @brief Event fired when a key is pressed.
 */
class KeyPressEvent : public KeyEvent
{
public:
    /**
     * @brief Construct a key press event.
     * @param keyCode The key that was pressed.
     * @param mods Modifier keys held during the press.
     */
    KeyPressEvent(int keyCode, int mods)
        : KeyEvent(keyCode, mods) {}

    EventType GetType() const override { return EventType::KeyPress; }
    const char* GetName() const override { return "KeyPress"; }

    std::string ToString() const override
    {
        return std::string("KeyPress: ") + std::to_string(m_KeyCode);
    }
};

/**
 * @brief Event fired when a key is released.
 */
class KeyReleaseEvent : public KeyEvent
{
public:
    /**
     * @brief Construct a key release event.
     * @param keyCode The key that was released.
     * @param mods Modifier keys held during the release.
     */
    KeyReleaseEvent(int keyCode, int mods)
        : KeyEvent(keyCode, mods) {}

    EventType GetType() const override { return EventType::KeyRelease; }
    const char* GetName() const override { return "KeyRelease"; }

    std::string ToString() const override
    {
        return std::string("KeyRelease: ") + std::to_string(m_KeyCode);
    }
};

/**
 * @brief Event fired when a key is held and repeating.
 */
class KeyRepeatEvent : public KeyEvent
{
public:
    /**
     * @brief Construct a key repeat event.
     * @param keyCode The key that is repeating.
     * @param mods Modifier keys held during the repeat.
     */
    KeyRepeatEvent(int keyCode, int mods)
        : KeyEvent(keyCode, mods) {}

    EventType GetType() const override { return EventType::KeyRepeat; }
    const char* GetName() const override { return "KeyRepeat"; }

    std::string ToString() const override
    {
        return std::string("KeyRepeat: ") + std::to_string(m_KeyCode);
    }
};

// =============================================================================
// Mouse Events
// =============================================================================

/**
 * @brief Event fired when the mouse moves.
 */
class MouseMoveEvent : public Event
{
public:
    /**
     * @brief Construct a mouse move event.
     * @param x The new X coordinate in window space.
     * @param y The new Y coordinate in window space.
     */
    MouseMoveEvent(double x, double y)
        : m_X(x), m_Y(y) {}

    EventType GetType() const override { return EventType::MouseMove; }
    const char* GetName() const override { return "MouseMove"; }
    EventCategory GetCategoryFlags() const override
    {
        return EventCategory::Input | EventCategory::Mouse;
    }

    /**
     * @brief Get the mouse X position.
     * @return X coordinate in window space.
     */
    double GetX() const { return m_X; }

    /**
     * @brief Get the mouse Y position.
     * @return Y coordinate in window space.
     */
    double GetY() const { return m_Y; }

    std::string ToString() const override
    {
        return std::string("MouseMove: ") + std::to_string(m_X) +
               ", " + std::to_string(m_Y);
    }

private:
    double m_X;
    double m_Y;
};

/**
 * @brief Event fired when the mouse wheel is scrolled.
 */
class MouseScrollEvent : public Event
{
public:
    /**
     * @brief Construct a mouse scroll event.
     * @param xOffset Horizontal scroll offset.
     * @param yOffset Vertical scroll offset (positive = up).
     */
    MouseScrollEvent(double xOffset, double yOffset)
        : m_XOffset(xOffset), m_YOffset(yOffset) {}

    EventType GetType() const override { return EventType::MouseScroll; }
    const char* GetName() const override { return "MouseScroll"; }
    EventCategory GetCategoryFlags() const override
    {
        return EventCategory::Input | EventCategory::Mouse;
    }

    /**
     * @brief Get the horizontal scroll offset.
     * @return X scroll delta.
     */
    double GetXOffset() const { return m_XOffset; }

    /**
     * @brief Get the vertical scroll offset.
     * @return Y scroll delta (positive = scroll up).
     */
    double GetYOffset() const { return m_YOffset; }

    std::string ToString() const override
    {
        return std::string("MouseScroll: ") + std::to_string(m_XOffset) +
               ", " + std::to_string(m_YOffset);
    }

private:
    double m_XOffset;
    double m_YOffset;
};

/**
 * @brief Base class for mouse button events.
 */
class MouseButtonEvent : public Event
{
public:
    /**
     * @brief Get the mouse button for this event.
     * @return The button code (maps to GLFW mouse button codes).
     */
    int GetButton() const { return m_Button; }

    EventCategory GetCategoryFlags() const override
    {
        return EventCategory::Input | EventCategory::Mouse | EventCategory::MouseButton;
    }

protected:
    explicit MouseButtonEvent(int button)
        : m_Button(button) {}

    int m_Button;
};

/**
 * @brief Event fired when a mouse button is pressed.
 */
class MouseButtonPressEvent : public MouseButtonEvent
{
public:
    /**
     * @brief Construct a mouse button press event.
     * @param button The button that was pressed.
     */
    explicit MouseButtonPressEvent(int button)
        : MouseButtonEvent(button) {}

    EventType GetType() const override { return EventType::MouseButtonPress; }
    const char* GetName() const override { return "MouseButtonPress"; }

    std::string ToString() const override
    {
        return std::string("MouseButtonPress: ") + std::to_string(m_Button);
    }
};

/**
 * @brief Event fired when a mouse button is released.
 */
class MouseButtonReleaseEvent : public MouseButtonEvent
{
public:
    /**
     * @brief Construct a mouse button release event.
     * @param button The button that was released.
     */
    explicit MouseButtonReleaseEvent(int button)
        : MouseButtonEvent(button) {}

    EventType GetType() const override { return EventType::MouseButtonRelease; }
    const char* GetName() const override { return "MouseButtonRelease"; }

    std::string ToString() const override
    {
        return std::string("MouseButtonRelease: ") + std::to_string(m_Button);
    }
};

} // namespace Core
