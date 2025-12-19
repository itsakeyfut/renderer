/**
 * @file TestEvent.cpp
 * @brief Unit tests for the Event system.
 *
 * Tests the Event base class, event types, EventDispatcher,
 * and EventHandler helper class.
 */

#include <gtest/gtest.h>
#include "Core/Event.h"
#include "Core/EventDispatcher.h"

#include <string>
#include <vector>

// =============================================================================
// Event Tests
// =============================================================================

TEST(EventTest, WindowResizeEventProperties)
{
    Core::WindowResizeEvent event(1920, 1080);

    EXPECT_EQ(event.GetWidth(), 1920u);
    EXPECT_EQ(event.GetHeight(), 1080u);
    EXPECT_EQ(event.GetType(), Core::EventType::WindowResize);
    EXPECT_STREQ(event.GetName(), "WindowResize");
    EXPECT_FALSE(event.Handled);
}

TEST(EventTest, WindowResizeEventToString)
{
    Core::WindowResizeEvent event(800, 600);

    std::string str = event.ToString();
    EXPECT_TRUE(str.find("800") != std::string::npos);
    EXPECT_TRUE(str.find("600") != std::string::npos);
}

TEST(EventTest, WindowCloseEventProperties)
{
    Core::WindowCloseEvent event;

    EXPECT_EQ(event.GetType(), Core::EventType::WindowClose);
    EXPECT_STREQ(event.GetName(), "WindowClose");
}

TEST(EventTest, KeyPressEventProperties)
{
    Core::KeyPressEvent event(65, 0); // 'A' key

    EXPECT_EQ(event.GetKeyCode(), 65);
    EXPECT_EQ(event.GetMods(), 0);
    EXPECT_EQ(event.GetType(), Core::EventType::KeyPress);
    EXPECT_STREQ(event.GetName(), "KeyPress");
}

TEST(EventTest, KeyReleaseEventProperties)
{
    Core::KeyReleaseEvent event(32, 1); // Space with shift

    EXPECT_EQ(event.GetKeyCode(), 32);
    EXPECT_EQ(event.GetMods(), 1);
    EXPECT_EQ(event.GetType(), Core::EventType::KeyRelease);
}

TEST(EventTest, MouseMoveEventProperties)
{
    Core::MouseMoveEvent event(100.5, 200.5);

    EXPECT_DOUBLE_EQ(event.GetX(), 100.5);
    EXPECT_DOUBLE_EQ(event.GetY(), 200.5);
    EXPECT_EQ(event.GetType(), Core::EventType::MouseMove);
}

TEST(EventTest, MouseScrollEventProperties)
{
    Core::MouseScrollEvent event(0.0, 1.0);

    EXPECT_DOUBLE_EQ(event.GetXOffset(), 0.0);
    EXPECT_DOUBLE_EQ(event.GetYOffset(), 1.0);
    EXPECT_EQ(event.GetType(), Core::EventType::MouseScroll);
}

TEST(EventTest, MouseButtonPressEventProperties)
{
    Core::MouseButtonPressEvent event(0); // Left button

    EXPECT_EQ(event.GetButton(), 0);
    EXPECT_EQ(event.GetType(), Core::EventType::MouseButtonPress);
}

TEST(EventTest, MouseButtonReleaseEventProperties)
{
    Core::MouseButtonReleaseEvent event(1); // Right button

    EXPECT_EQ(event.GetButton(), 1);
    EXPECT_EQ(event.GetType(), Core::EventType::MouseButtonRelease);
}

TEST(EventTest, EventCategoryFlags)
{
    Core::WindowResizeEvent windowEvent(800, 600);
    Core::KeyPressEvent keyEvent(65, 0);
    Core::MouseMoveEvent mouseEvent(0, 0);

    EXPECT_TRUE(windowEvent.IsInCategory(Core::EventCategory::Window));
    EXPECT_TRUE(windowEvent.IsInCategory(Core::EventCategory::Application));
    EXPECT_FALSE(windowEvent.IsInCategory(Core::EventCategory::Input));

    EXPECT_TRUE(keyEvent.IsInCategory(Core::EventCategory::Input));
    EXPECT_TRUE(keyEvent.IsInCategory(Core::EventCategory::Keyboard));
    EXPECT_FALSE(keyEvent.IsInCategory(Core::EventCategory::Mouse));

    EXPECT_TRUE(mouseEvent.IsInCategory(Core::EventCategory::Input));
    EXPECT_TRUE(mouseEvent.IsInCategory(Core::EventCategory::Mouse));
    EXPECT_FALSE(mouseEvent.IsInCategory(Core::EventCategory::Keyboard));
}

TEST(EventTest, HandledFlag)
{
    Core::WindowResizeEvent event(800, 600);

    EXPECT_FALSE(event.Handled);
    event.Handled = true;
    EXPECT_TRUE(event.Handled);
}

// =============================================================================
// EventDispatcher Tests
// =============================================================================

class EventDispatcherTest : public ::testing::Test
{
protected:
    Core::EventDispatcher m_Dispatcher;
};

TEST_F(EventDispatcherTest, SubscribeAndDispatch)
{
    bool called = false;
    uint32_t receivedWidth = 0;
    uint32_t receivedHeight = 0;

    m_Dispatcher.Subscribe<Core::WindowResizeEvent>(
        [&](Core::WindowResizeEvent& e) {
            called = true;
            receivedWidth = e.GetWidth();
            receivedHeight = e.GetHeight();
        }
    );

    Core::WindowResizeEvent event(1920, 1080);
    m_Dispatcher.Dispatch(event);

    EXPECT_TRUE(called);
    EXPECT_EQ(receivedWidth, 1920u);
    EXPECT_EQ(receivedHeight, 1080u);
}

TEST_F(EventDispatcherTest, MultipleListeners)
{
    int callCount = 0;

    m_Dispatcher.Subscribe<Core::WindowResizeEvent>(
        [&](Core::WindowResizeEvent&) { callCount++; }
    );
    m_Dispatcher.Subscribe<Core::WindowResizeEvent>(
        [&](Core::WindowResizeEvent&) { callCount++; }
    );
    m_Dispatcher.Subscribe<Core::WindowResizeEvent>(
        [&](Core::WindowResizeEvent&) { callCount++; }
    );

    Core::WindowResizeEvent event(800, 600);
    m_Dispatcher.Dispatch(event);

    EXPECT_EQ(callCount, 3);
}

TEST_F(EventDispatcherTest, DifferentEventTypes)
{
    bool windowResized = false;
    bool keyPressed = false;

    m_Dispatcher.Subscribe<Core::WindowResizeEvent>(
        [&](Core::WindowResizeEvent&) { windowResized = true; }
    );
    m_Dispatcher.Subscribe<Core::KeyPressEvent>(
        [&](Core::KeyPressEvent&) { keyPressed = true; }
    );

    Core::WindowResizeEvent resizeEvent(800, 600);
    m_Dispatcher.Dispatch(resizeEvent);

    EXPECT_TRUE(windowResized);
    EXPECT_FALSE(keyPressed);

    Core::KeyPressEvent keyEvent(65, 0);
    m_Dispatcher.Dispatch(keyEvent);

    EXPECT_TRUE(keyPressed);
}

TEST_F(EventDispatcherTest, Unsubscribe)
{
    int callCount = 0;

    auto handle = m_Dispatcher.Subscribe<Core::WindowResizeEvent>(
        [&](Core::WindowResizeEvent&) { callCount++; }
    );

    Core::WindowResizeEvent event1(800, 600);
    m_Dispatcher.Dispatch(event1);
    EXPECT_EQ(callCount, 1);

    bool unsubscribed = m_Dispatcher.Unsubscribe(handle);
    EXPECT_TRUE(unsubscribed);

    Core::WindowResizeEvent event2(1920, 1080);
    m_Dispatcher.Dispatch(event2);
    EXPECT_EQ(callCount, 1); // Should not increase
}

TEST_F(EventDispatcherTest, UnsubscribeInvalidHandle)
{
    bool unsubscribed = m_Dispatcher.Unsubscribe(Core::INVALID_SUBSCRIPTION);
    EXPECT_FALSE(unsubscribed);

    unsubscribed = m_Dispatcher.Unsubscribe(99999);
    EXPECT_FALSE(unsubscribed);
}

TEST_F(EventDispatcherTest, UnsubscribeAll)
{
    int callCount = 0;

    m_Dispatcher.Subscribe<Core::WindowResizeEvent>(
        [&](Core::WindowResizeEvent&) { callCount++; }
    );
    m_Dispatcher.Subscribe<Core::WindowResizeEvent>(
        [&](Core::WindowResizeEvent&) { callCount++; }
    );

    EXPECT_EQ(m_Dispatcher.GetListenerCount<Core::WindowResizeEvent>(), 2u);

    m_Dispatcher.UnsubscribeAll<Core::WindowResizeEvent>();

    EXPECT_EQ(m_Dispatcher.GetListenerCount<Core::WindowResizeEvent>(), 0u);

    Core::WindowResizeEvent event(800, 600);
    m_Dispatcher.Dispatch(event);
    EXPECT_EQ(callCount, 0);
}

TEST_F(EventDispatcherTest, Clear)
{
    m_Dispatcher.Subscribe<Core::WindowResizeEvent>(
        [](Core::WindowResizeEvent&) {}
    );
    m_Dispatcher.Subscribe<Core::KeyPressEvent>(
        [](Core::KeyPressEvent&) {}
    );

    EXPECT_EQ(m_Dispatcher.GetTotalListenerCount(), 2u);

    m_Dispatcher.Clear();

    EXPECT_EQ(m_Dispatcher.GetTotalListenerCount(), 0u);
}

TEST_F(EventDispatcherTest, HandledStopsPropagation)
{
    std::vector<int> callOrder;

    m_Dispatcher.Subscribe<Core::WindowResizeEvent>(
        [&](Core::WindowResizeEvent& e) {
            callOrder.push_back(1);
            e.Handled = true; // Stop propagation
        }
    );
    m_Dispatcher.Subscribe<Core::WindowResizeEvent>(
        [&](Core::WindowResizeEvent&) {
            callOrder.push_back(2);
        }
    );

    Core::WindowResizeEvent event(800, 600);
    m_Dispatcher.Dispatch(event);

    EXPECT_EQ(callOrder.size(), 1u);
    EXPECT_EQ(callOrder[0], 1);
    EXPECT_TRUE(event.Handled);
}

TEST_F(EventDispatcherTest, ListenerCount)
{
    EXPECT_EQ(m_Dispatcher.GetListenerCount<Core::WindowResizeEvent>(), 0u);
    EXPECT_EQ(m_Dispatcher.GetTotalListenerCount(), 0u);

    m_Dispatcher.Subscribe<Core::WindowResizeEvent>([](Core::WindowResizeEvent&) {});
    EXPECT_EQ(m_Dispatcher.GetListenerCount<Core::WindowResizeEvent>(), 1u);
    EXPECT_EQ(m_Dispatcher.GetTotalListenerCount(), 1u);

    m_Dispatcher.Subscribe<Core::KeyPressEvent>([](Core::KeyPressEvent&) {});
    EXPECT_EQ(m_Dispatcher.GetListenerCount<Core::KeyPressEvent>(), 1u);
    EXPECT_EQ(m_Dispatcher.GetTotalListenerCount(), 2u);
}

TEST_F(EventDispatcherTest, DispatchToNoListeners)
{
    // Should not crash when dispatching to no listeners
    Core::WindowResizeEvent event(800, 600);
    EXPECT_NO_THROW(m_Dispatcher.Dispatch(event));
}

TEST_F(EventDispatcherTest, UniqueHandles)
{
    auto handle1 = m_Dispatcher.Subscribe<Core::WindowResizeEvent>(
        [](Core::WindowResizeEvent&) {}
    );
    auto handle2 = m_Dispatcher.Subscribe<Core::WindowResizeEvent>(
        [](Core::WindowResizeEvent&) {}
    );
    auto handle3 = m_Dispatcher.Subscribe<Core::KeyPressEvent>(
        [](Core::KeyPressEvent&) {}
    );

    EXPECT_NE(handle1, handle2);
    EXPECT_NE(handle2, handle3);
    EXPECT_NE(handle1, handle3);
    EXPECT_NE(handle1, Core::INVALID_SUBSCRIPTION);
}

TEST_F(EventDispatcherTest, UnsubscribeDuringDispatchIsSafe)
{
    // Test that unsubscribing during dispatch doesn't cause iterator invalidation
    int callCount = 0;
    Core::SubscriptionHandle handle1 = Core::INVALID_SUBSCRIPTION;

    handle1 = m_Dispatcher.Subscribe<Core::WindowResizeEvent>(
        [&](Core::WindowResizeEvent&) {
            callCount++;
            // Unsubscribe self during dispatch - should not crash
            m_Dispatcher.Unsubscribe(handle1);
        }
    );

    m_Dispatcher.Subscribe<Core::WindowResizeEvent>(
        [&](Core::WindowResizeEvent&) {
            callCount++;
        }
    );

    Core::WindowResizeEvent event(800, 600);
    EXPECT_NO_THROW(m_Dispatcher.Dispatch(event));

    // Both listeners should have been called (snapshot approach)
    EXPECT_EQ(callCount, 2);

    // After dispatch, only second listener should remain
    EXPECT_EQ(m_Dispatcher.GetListenerCount<Core::WindowResizeEvent>(), 1u);
}

TEST_F(EventDispatcherTest, SubscribeDuringDispatchIsSafe)
{
    // Test that subscribing during dispatch doesn't affect current iteration
    int callCount = 0;

    m_Dispatcher.Subscribe<Core::WindowResizeEvent>(
        [&](Core::WindowResizeEvent&) {
            callCount++;
            // Subscribe new listener during dispatch
            m_Dispatcher.Subscribe<Core::WindowResizeEvent>(
                [&](Core::WindowResizeEvent&) {
                    callCount++;
                }
            );
        }
    );

    Core::WindowResizeEvent event(800, 600);
    EXPECT_NO_THROW(m_Dispatcher.Dispatch(event));

    // Only original listener should have been called (new one added after snapshot)
    EXPECT_EQ(callCount, 1);

    // Now there should be 2 listeners
    EXPECT_EQ(m_Dispatcher.GetListenerCount<Core::WindowResizeEvent>(), 2u);
}

// =============================================================================
// EventHandler Tests
// =============================================================================

TEST(EventHandlerTest, HandleMatchingType)
{
    Core::WindowResizeEvent event(800, 600);
    Core::EventHandler handler(event);

    bool handled = handler.Handle<Core::WindowResizeEvent>(
        [](Core::WindowResizeEvent& e) {
            EXPECT_EQ(e.GetWidth(), 800u);
            return true;
        }
    );

    EXPECT_TRUE(handled);
    EXPECT_TRUE(event.Handled);
}

TEST(EventHandlerTest, HandleNonMatchingType)
{
    Core::WindowResizeEvent event(800, 600);
    Core::EventHandler handler(event);

    bool handled = handler.Handle<Core::KeyPressEvent>(
        [](Core::KeyPressEvent&) {
            return true;
        }
    );

    EXPECT_FALSE(handled);
    EXPECT_FALSE(event.Handled);
}

TEST(EventHandlerTest, HandleMultipleTypes)
{
    Core::KeyPressEvent event(65, 0);
    Core::EventHandler handler(event);

    bool resizeHandled = handler.Handle<Core::WindowResizeEvent>(
        [](Core::WindowResizeEvent&) { return true; }
    );
    bool keyHandled = handler.Handle<Core::KeyPressEvent>(
        [](Core::KeyPressEvent&) { return true; }
    );

    EXPECT_FALSE(resizeHandled);
    EXPECT_TRUE(keyHandled);
}

TEST(EventHandlerTest, SkipsIfAlreadyHandled)
{
    Core::KeyPressEvent event(65, 0);
    event.Handled = true;

    Core::EventHandler handler(event);

    bool handled = handler.Handle<Core::KeyPressEvent>(
        [](Core::KeyPressEvent&) { return true; }
    );

    EXPECT_FALSE(handled);
}
