/**
 * @file EventDispatcher.h
 * @brief Event dispatcher for loosely-coupled module communication.
 *
 * This file provides the EventDispatcher class that manages event subscriptions
 * and dispatches events to registered listeners based on event type.
 */

#pragma once

#include "Core/Event.h"
#include "Core/Types.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <vector>

namespace Core {

// =============================================================================
// Subscription Handle
// =============================================================================

/**
 * @brief Handle returned when subscribing to events.
 *
 * Used to unsubscribe from events later. The handle is unique per subscription.
 */
using SubscriptionHandle = uint64_t;

/**
 * @brief Invalid subscription handle value.
 */
constexpr SubscriptionHandle INVALID_SUBSCRIPTION = 0;

// =============================================================================
// EventDispatcher Class
// =============================================================================

/**
 * @brief Central hub for event-based communication between modules.
 *
 * The EventDispatcher allows modules to subscribe to specific event types
 * and receive callbacks when those events are dispatched. This enables
 * loosely-coupled communication without direct dependencies between modules.
 *
 * Features:
 * - Type-safe event subscription using templates
 * - Multiple listeners per event type
 * - Subscription handles for easy unsubscription
 * - Event handling flag to stop propagation
 *
 * Example usage:
 * @code
 * EventDispatcher dispatcher;
 *
 * // Subscribe to window resize events
 * auto handle = dispatcher.Subscribe<WindowResizeEvent>(
 *     [](WindowResizeEvent& e) {
 *         LOG_INFO("Window resized to {}x{}", e.GetWidth(), e.GetHeight());
 *         // Recreate swapchain, update camera aspect ratio, etc.
 *     }
 * );
 *
 * // Later, when window is resized
 * WindowResizeEvent event(1920, 1080);
 * dispatcher.Dispatch(event);
 *
 * // Unsubscribe when no longer needed
 * dispatcher.Unsubscribe(handle);
 * @endcode
 */
class EventDispatcher
{
public:
    /**
     * @brief Callback function type for event handlers.
     */
    using EventCallback = std::function<void(Event&)>;

    /**
     * @brief Construct an empty event dispatcher.
     */
    EventDispatcher() = default;

    /**
     * @brief Destructor.
     */
    ~EventDispatcher() = default;

    // Non-copyable but movable
    EventDispatcher(const EventDispatcher&) = delete;
    EventDispatcher& operator=(const EventDispatcher&) = delete;
    EventDispatcher(EventDispatcher&&) = default;
    EventDispatcher& operator=(EventDispatcher&&) = default;

    /**
     * @brief Subscribe to a specific event type.
     *
     * @tparam T The event type to subscribe to (must derive from Event).
     * @param callback The function to call when the event is dispatched.
     * @return A subscription handle for later unsubscription.
     *
     * Example:
     * @code
     * auto handle = dispatcher.Subscribe<WindowResizeEvent>(
     *     [](WindowResizeEvent& e) {
     *         RecreateSwapchain(e.GetWidth(), e.GetHeight());
     *     }
     * );
     * @endcode
     */
    template<typename T>
    SubscriptionHandle Subscribe(std::function<void(T&)> callback)
    {
        static_assert(std::is_base_of_v<Event, T>,
                      "T must derive from Event");

        std::type_index typeIndex = std::type_index(typeid(T));
        SubscriptionHandle handle = GenerateHandle();

        // Wrap the typed callback in a generic callback
        EventCallback wrapper = [callback](Event& e) {
            callback(static_cast<T&>(e));
        };

        m_Listeners[typeIndex].push_back({handle, std::move(wrapper)});
        return handle;
    }

    /**
     * @brief Unsubscribe from events using a subscription handle.
     *
     * @param handle The handle returned from Subscribe().
     * @return true if the subscription was found and removed.
     *
     * Example:
     * @code
     * dispatcher.Unsubscribe(handle);
     * @endcode
     */
    bool Unsubscribe(SubscriptionHandle handle)
    {
        for (auto& [typeIndex, listeners] : m_Listeners)
        {
            auto it = std::find_if(listeners.begin(), listeners.end(),
                [handle](const Listener& l) { return l.Handle == handle; });

            if (it != listeners.end())
            {
                listeners.erase(it);
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Unsubscribe all listeners for a specific event type.
     *
     * @tparam T The event type to unsubscribe from.
     */
    template<typename T>
    void UnsubscribeAll()
    {
        static_assert(std::is_base_of_v<Event, T>,
                      "T must derive from Event");

        std::type_index typeIndex = std::type_index(typeid(T));
        m_Listeners.erase(typeIndex);
    }

    /**
     * @brief Remove all subscriptions for all event types.
     */
    void Clear()
    {
        m_Listeners.clear();
    }

    /**
     * @brief Dispatch an event to all registered listeners.
     *
     * @param event The event to dispatch.
     *
     * Listeners are called in the order they were registered.
     * If a listener sets event.Handled = true, further listeners
     * will not be called.
     *
     * Example:
     * @code
     * WindowResizeEvent event(1920, 1080);
     * dispatcher.Dispatch(event);
     * @endcode
     */
    template<typename T>
    void Dispatch(T& event)
    {
        static_assert(std::is_base_of_v<Event, T>,
                      "T must derive from Event");

        std::type_index typeIndex = std::type_index(typeid(T));

        auto it = m_Listeners.find(typeIndex);
        if (it == m_Listeners.end())
        {
            return;
        }

        for (const auto& listener : it->second)
        {
            if (event.Handled)
            {
                break;
            }
            listener.Callback(event);
        }
    }

    /**
     * @brief Get the number of listeners for a specific event type.
     *
     * @tparam T The event type to query.
     * @return Number of registered listeners.
     */
    template<typename T>
    size_t GetListenerCount() const
    {
        static_assert(std::is_base_of_v<Event, T>,
                      "T must derive from Event");

        std::type_index typeIndex = std::type_index(typeid(T));

        auto it = m_Listeners.find(typeIndex);
        if (it == m_Listeners.end())
        {
            return 0;
        }
        return it->second.size();
    }

    /**
     * @brief Get the total number of listeners across all event types.
     *
     * @return Total number of registered listeners.
     */
    size_t GetTotalListenerCount() const
    {
        size_t count = 0;
        for (const auto& [typeIndex, listeners] : m_Listeners)
        {
            count += listeners.size();
        }
        return count;
    }

private:
    /**
     * @brief Internal listener record.
     */
    struct Listener
    {
        SubscriptionHandle Handle;
        EventCallback Callback;
    };

    /**
     * @brief Generate a unique subscription handle.
     */
    SubscriptionHandle GenerateHandle()
    {
        return ++m_NextHandle;
    }

    /**
     * @brief Map from event type to list of listeners.
     */
    std::unordered_map<std::type_index, std::vector<Listener>> m_Listeners;

    /**
     * @brief Counter for generating unique subscription handles.
     */
    SubscriptionHandle m_NextHandle = 0;
};

// =============================================================================
// EventHandler Helper
// =============================================================================

/**
 * @brief Helper class for handling events with type dispatch.
 *
 * This class provides a convenient way to dispatch events to type-specific
 * handlers in a single function. Useful for routing events in OnEvent callbacks.
 *
 * Example usage:
 * @code
 * void OnEvent(Event& event) {
 *     EventHandler handler(event);
 *     handler.Handle<WindowResizeEvent>([this](WindowResizeEvent& e) {
 *         OnWindowResize(e);
 *         return true; // Event was handled
 *     });
 *     handler.Handle<KeyPressEvent>([this](KeyPressEvent& e) {
 *         OnKeyPress(e);
 *         return true;
 *     });
 * }
 * @endcode
 */
class EventHandler
{
public:
    /**
     * @brief Construct an event handler for the given event.
     * @param event The event to handle.
     */
    explicit EventHandler(Event& event)
        : m_Event(event) {}

    /**
     * @brief Try to handle the event as a specific type.
     *
     * @tparam T The event type to check for.
     * @tparam F The handler function type.
     * @param func The handler function, should return true if handled.
     * @return true if the event matched the type and was handled.
     *
     * The handler function receives a reference to the typed event
     * and should return true if the event was handled.
     */
    template<typename T, typename F>
    bool Handle(F&& func)
    {
        static_assert(std::is_base_of_v<Event, T>,
                      "T must derive from Event");

        if (m_Event.Handled)
        {
            return false;
        }

        // Use typeid to compare types without requiring default constructor
        if (typeid(m_Event) == typeid(T))
        {
            m_Event.Handled = func(static_cast<T&>(m_Event));
            return m_Event.Handled;
        }

        return false;
    }

private:
    Event& m_Event;
};

} // namespace Core
