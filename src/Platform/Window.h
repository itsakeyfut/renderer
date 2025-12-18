#pragma once

#include "Core/Types.h"

#include <string>
#include <functional>

// Forward declarations to avoid including GLFW/Vulkan headers in header
struct GLFWwindow;
struct VkInstance_T;
struct VkSurfaceKHR_T;
using VkInstance = VkInstance_T*;
using VkSurfaceKHR = VkSurfaceKHR_T*;

namespace Platform
{
    /**
     * @brief Window configuration parameters
     */
    struct WindowConfig
    {
        uint32_t Width = 1280;
        uint32_t Height = 720;
        std::string Title = "Vulkan Renderer";
        bool Resizable = true;
        bool Fullscreen = false;

        /**
         * @brief Whether the window should be visible.
         * Set to false for headless testing to avoid window flashing.
         */
        bool Visible = true;
    };

    /**
     * @brief GLFW Window wrapper class
     *
     * Manages window creation, input handling, and Vulkan surface creation.
     * This class follows RAII principles - the window is destroyed when
     * the Window object goes out of scope.
     */
    class Window
    {
    public:
        using ResizeCallback = std::function<void(uint32_t, uint32_t)>;

        /**
         * @brief Create a new window
         * @param config Window configuration parameters
         */
        explicit Window(const WindowConfig& config);

        /**
         * @brief Destructor - destroys the GLFW window
         */
        ~Window();

        // Non-copyable
        Window(const Window&) = delete;
        Window& operator=(const Window&) = delete;

        // Movable
        Window(Window&& other) noexcept;
        Window& operator=(Window&& other) noexcept;

        /**
         * @brief Check if the window should close
         * @return true if close was requested
         */
        bool ShouldClose() const;

        /**
         * @brief Process pending window events
         */
        void PollEvents() const;

        /**
         * @brief Wait for window events (blocks until an event occurs)
         */
        void WaitEvents() const;

        /**
         * @brief Get the native GLFW window handle
         * @return Pointer to GLFWwindow
         */
        GLFWwindow* GetHandle() const { return m_Window; }

        /**
         * @brief Get current window width
         * @return Width in pixels
         */
        uint32_t GetWidth() const { return m_Width; }

        /**
         * @brief Get current window height
         * @return Height in pixels
         */
        uint32_t GetHeight() const { return m_Height; }

        /**
         * @brief Get framebuffer size (may differ from window size on HiDPI displays)
         * @param outWidth Output framebuffer width
         * @param outHeight Output framebuffer height
         */
        void GetFramebufferSize(uint32_t& outWidth, uint32_t& outHeight) const;

        /**
         * @brief Set callback for window resize events
         * @param callback Function to call when window is resized
         */
        void SetResizeCallback(ResizeCallback callback);

        /**
         * @brief Get required Vulkan instance extensions for surface creation
         * @param count Output number of extensions
         * @return Array of extension names
         */
        static const char** GetRequiredVulkanExtensions(uint32_t& count);

        /**
         * @brief Create a Vulkan surface for this window
         * @param instance The Vulkan instance to create the surface with
         * @return The created VkSurfaceKHR handle, or VK_NULL_HANDLE on failure
         *
         * The caller is responsible for destroying the returned surface using
         * vkDestroySurfaceKHR when it is no longer needed.
         */
        VkSurfaceKHR CreateSurface(VkInstance instance) const;

        /**
         * @brief Set window title
         * @param title New window title
         */
        void SetTitle(const std::string& title);

        /**
         * @brief Check if window was resized since last frame
         * @return true if resized
         */
        bool WasResized() const { return m_Resized; }

        /**
         * @brief Reset the resized flag
         */
        void ResetResizedFlag() { m_Resized = false; }

    private:
        GLFWwindow* m_Window = nullptr;
        uint32_t m_Width = 0;
        uint32_t m_Height = 0;
        bool m_Resized = false;
        ResizeCallback m_ResizeCallback;

        static void FramebufferResizeCallback(GLFWwindow* window, int width, int height);
    };
}
