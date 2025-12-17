#include "Core/Log.h"
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

    // Main loop
    while (!window.ShouldClose())
    {
        window.PollEvents();

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

        // Future: Render frame here
    }

    LOG_INFO("Vulkan Renderer shutting down...");

    // Shutdown logging
    Core::Log::Shutdown();

    return EXIT_SUCCESS;
}
