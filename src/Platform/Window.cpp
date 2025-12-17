#include "Platform/Window.h"
#include "Core/Log.h"

#include <cstdlib>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace Platform
{
    static bool s_GLFWInitialized = false;
    static uint32_t s_WindowCount = 0;

    static void GLFWErrorCallback(int error, const char* description)
    {
        LOG_ERROR("GLFW Error ({0}): {1}", error, description);
    }

    Window::Window(const WindowConfig& config)
        : m_Width(config.Width)
        , m_Height(config.Height)
    {
        // Initialize GLFW if not already done
        if (!s_GLFWInitialized)
        {
            if (!glfwInit())
            {
                LOG_FATAL("Failed to initialize GLFW");
                std::abort();
            }

            glfwSetErrorCallback(GLFWErrorCallback);

            // Check for Vulkan support
            if (!glfwVulkanSupported())
            {
                LOG_FATAL("Vulkan is not supported on this system");
                std::abort();
            }

            s_GLFWInitialized = true;
            LOG_INFO("GLFW initialized successfully");
        }

        // Don't create OpenGL context - we're using Vulkan
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, config.Resizable ? GLFW_TRUE : GLFW_FALSE);

        // Create the window
        GLFWmonitor* monitor = config.Fullscreen ? glfwGetPrimaryMonitor() : nullptr;
        m_Window = glfwCreateWindow(
            static_cast<int>(m_Width),
            static_cast<int>(m_Height),
            config.Title.c_str(),
            monitor,
            nullptr
        );

        if (!m_Window)
        {
            LOG_FATAL("Failed to create GLFW window");
            std::abort();
        }

        // Store this pointer for callbacks
        glfwSetWindowUserPointer(m_Window, this);

        // Set framebuffer resize callback
        glfwSetFramebufferSizeCallback(m_Window, FramebufferResizeCallback);

        ++s_WindowCount;

        LOG_INFO("Window created: {0}x{1} - \"{2}\"", m_Width, m_Height, config.Title);
    }

    Window::~Window()
    {
        if (m_Window)
        {
            glfwDestroyWindow(m_Window);
            m_Window = nullptr;
            --s_WindowCount;

            LOG_DEBUG("Window destroyed");

            // Terminate GLFW if no windows remain
            if (s_WindowCount == 0 && s_GLFWInitialized)
            {
                glfwTerminate();
                s_GLFWInitialized = false;
                LOG_DEBUG("GLFW terminated");
            }
        }
    }

    Window::Window(Window&& other) noexcept
        : m_Window(other.m_Window)
        , m_Width(other.m_Width)
        , m_Height(other.m_Height)
        , m_Resized(other.m_Resized)
        , m_ResizeCallback(std::move(other.m_ResizeCallback))
    {
        other.m_Window = nullptr;
        other.m_Width = 0;
        other.m_Height = 0;

        // Update user pointer to point to this object
        if (m_Window)
        {
            glfwSetWindowUserPointer(m_Window, this);
        }
    }

    Window& Window::operator=(Window&& other) noexcept
    {
        if (this != &other)
        {
            // Clean up existing window
            if (m_Window)
            {
                glfwDestroyWindow(m_Window);
                --s_WindowCount;
            }

            m_Window = other.m_Window;
            m_Width = other.m_Width;
            m_Height = other.m_Height;
            m_Resized = other.m_Resized;
            m_ResizeCallback = std::move(other.m_ResizeCallback);

            other.m_Window = nullptr;
            other.m_Width = 0;
            other.m_Height = 0;

            // Update user pointer
            if (m_Window)
            {
                glfwSetWindowUserPointer(m_Window, this);
            }
        }
        return *this;
    }

    bool Window::ShouldClose() const
    {
        return glfwWindowShouldClose(m_Window);
    }

    void Window::PollEvents() const
    {
        glfwPollEvents();
    }

    void Window::GetFramebufferSize(uint32_t& outWidth, uint32_t& outHeight) const
    {
        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(m_Window, &width, &height);
        outWidth = static_cast<uint32_t>(width);
        outHeight = static_cast<uint32_t>(height);
    }

    void Window::SetResizeCallback(ResizeCallback callback)
    {
        m_ResizeCallback = std::move(callback);
    }

    const char** Window::GetRequiredVulkanExtensions(uint32_t& count)
    {
        return glfwGetRequiredInstanceExtensions(&count);
    }

    void Window::SetTitle(const std::string& title)
    {
        glfwSetWindowTitle(m_Window, title.c_str());
    }

    void Window::FramebufferResizeCallback(GLFWwindow* window, int width, int height)
    {
        auto* windowInstance = static_cast<Window*>(glfwGetWindowUserPointer(window));
        if (windowInstance)
        {
            windowInstance->m_Width = static_cast<uint32_t>(width);
            windowInstance->m_Height = static_cast<uint32_t>(height);
            windowInstance->m_Resized = true;

            if (windowInstance->m_ResizeCallback)
            {
                windowInstance->m_ResizeCallback(
                    static_cast<uint32_t>(width),
                    static_cast<uint32_t>(height)
                );
            }

            LOG_DEBUG("Window resized: {0}x{1}", width, height);
        }
    }
}
