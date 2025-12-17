#include "Core/Log.h"
#include "Platform/Input.h"
#include "Platform/Window.h"

#include <cstdlib>

int main()
{
    // Initialize logging system
    Core::Log::Init();

    LOG_INFO("Vulkan Renderer starting...");

    // Create window configuration
    Platform::WindowConfig windowConfig;
    windowConfig.Width = 1280;
    windowConfig.Height = 720;
    windowConfig.Title = "Vulkan Renderer";
    windowConfig.Resizable = true;

    // Create the window
    Platform::Window window(windowConfig);

    // Initialize input system
    Platform::Input::Init(window);

    // Set resize callback
    window.SetResizeCallback([](uint32_t width, uint32_t height) {
        LOG_INFO("Window resized to: {0}x{1}", width, height);
    });

    // Get required Vulkan extensions for surface creation
    uint32_t extensionCount = 0;
    [[maybe_unused]] const char** extensions = Platform::Window::GetRequiredVulkanExtensions(extensionCount);
    LOG_DEBUG("Required Vulkan extensions for window surface:");
    for (uint32_t i = 0; i < extensionCount; ++i)
    {
        LOG_DEBUG("  - {0}", extensions[i]);
    }

    LOG_INFO("Entering main loop...");
    LOG_INFO("Press WASD to test input, ESC to exit, Right-click to toggle cursor lock");

    // Main loop
    while (!window.ShouldClose())
    {
        window.PollEvents();
        Platform::Input::Update();

        // Handle window minimization
        uint32_t fbWidth = 0;
        uint32_t fbHeight = 0;
        window.GetFramebufferSize(fbWidth, fbHeight);

        if (fbWidth == 0 || fbHeight == 0)
        {
            // Window is minimized, wait for events
            window.PollEvents();
            continue;
        }

        // Reset resize flag after handling
        if (window.WasResized())
        {
            window.ResetResizedFlag();
            // Future: Recreate swapchain here
        }

        // Demo: Log key presses (only when pressed this frame)
        if (Platform::Input::IsKeyPressed(Platform::KeyCode::W))
        {
            LOG_INFO("W key pressed - Move forward");
        }
        if (Platform::Input::IsKeyPressed(Platform::KeyCode::A))
        {
            LOG_INFO("A key pressed - Move left");
        }
        if (Platform::Input::IsKeyPressed(Platform::KeyCode::S))
        {
            LOG_INFO("S key pressed - Move backward");
        }
        if (Platform::Input::IsKeyPressed(Platform::KeyCode::D))
        {
            LOG_INFO("D key pressed - Move right");
        }
        if (Platform::Input::IsKeyPressed(Platform::KeyCode::Space))
        {
            LOG_INFO("Space key pressed - Jump");
        }

        // Demo: Toggle cursor mode with right mouse button
        if (Platform::Input::IsMouseButtonPressed(Platform::MouseButton::Right))
        {
            if (Platform::Input::GetCursorMode() == Platform::CursorMode::Normal)
            {
                Platform::Input::SetCursorMode(Platform::CursorMode::Disabled);
                LOG_INFO("Cursor disabled (FPS mode)");
            }
            else
            {
                Platform::Input::SetCursorMode(Platform::CursorMode::Normal);
                LOG_INFO("Cursor enabled (Normal mode)");
            }
        }

        // Demo: Log mouse movement when cursor is disabled
        if (Platform::Input::GetCursorMode() == Platform::CursorMode::Disabled)
        {
            double deltaX = Platform::Input::GetMouseDeltaX();
            double deltaY = Platform::Input::GetMouseDeltaY();
            if (deltaX != 0.0 || deltaY != 0.0)
            {
                LOG_DEBUG("Mouse delta: ({:.1f}, {:.1f})", deltaX, deltaY);
            }
        }

        // Demo: Log scroll wheel
        double scrollDelta = Platform::Input::GetScrollDelta();
        if (scrollDelta != 0.0)
        {
            LOG_INFO("Scroll delta: {:.1f}", scrollDelta);
        }

        // Future: Render frame here
    }

    LOG_INFO("Vulkan Renderer shutting down...");

    // Shutdown input system
    Platform::Input::Shutdown();

    // Shutdown logging
    Core::Log::Shutdown();

    return EXIT_SUCCESS;
}
