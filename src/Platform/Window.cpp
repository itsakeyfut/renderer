#include "Platform/Window.h"
#include "Core/Event.h"
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
        glfwWindowHint(GLFW_VISIBLE, config.Visible ? GLFW_TRUE : GLFW_FALSE);

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

        // Set window callbacks for event dispatching
        glfwSetFramebufferSizeCallback(m_Window, FramebufferResizeCallback);
        glfwSetWindowCloseCallback(m_Window, WindowCloseCallback);
        glfwSetWindowFocusCallback(m_Window, WindowFocusCallback);
        glfwSetWindowIconifyCallback(m_Window, WindowIconifyCallback);
        glfwSetKeyCallback(m_Window, KeyCallback);
        glfwSetMouseButtonCallback(m_Window, MouseButtonCallback);
        glfwSetCursorPosCallback(m_Window, CursorPosCallback);
        glfwSetScrollCallback(m_Window, ScrollCallback);

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
        , m_EventDispatcher(std::move(other.m_EventDispatcher))
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
            m_EventDispatcher = std::move(other.m_EventDispatcher);

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

    void Window::WaitEvents() const
    {
        glfwWaitEvents();
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

    VkSurfaceKHR Window::CreateSurface(VkInstance instance) const
    {
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        VkResult result = glfwCreateWindowSurface(instance, m_Window, nullptr, &surface);

        if (result != VK_SUCCESS)
        {
            LOG_ERROR("Failed to create Vulkan window surface, VkResult: {}", static_cast<int>(result));
            return VK_NULL_HANDLE;
        }

        LOG_INFO("Vulkan window surface created successfully");
        return surface;
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

            // Legacy callback support
            if (windowInstance->m_ResizeCallback)
            {
                windowInstance->m_ResizeCallback(
                    static_cast<uint32_t>(width),
                    static_cast<uint32_t>(height)
                );
            }

            // Dispatch event to subscribers
            Core::WindowResizeEvent event(
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height)
            );
            windowInstance->m_EventDispatcher.Dispatch(event);

            LOG_DEBUG("Window resized: {0}x{1}", width, height);
        }
    }

    void Window::WindowCloseCallback(GLFWwindow* window)
    {
        auto* windowInstance = static_cast<Window*>(glfwGetWindowUserPointer(window));
        if (windowInstance)
        {
            Core::WindowCloseEvent event;
            windowInstance->m_EventDispatcher.Dispatch(event);

            LOG_DEBUG("Window close requested");
        }
    }

    void Window::WindowFocusCallback(GLFWwindow* window, int focused)
    {
        auto* windowInstance = static_cast<Window*>(glfwGetWindowUserPointer(window));
        if (windowInstance)
        {
            if (focused)
            {
                Core::WindowFocusEvent event;
                windowInstance->m_EventDispatcher.Dispatch(event);
                LOG_DEBUG("Window gained focus");
            }
            else
            {
                Core::WindowLostFocusEvent event;
                windowInstance->m_EventDispatcher.Dispatch(event);
                LOG_DEBUG("Window lost focus");
            }
        }
    }

    void Window::WindowIconifyCallback(GLFWwindow* window, int iconified)
    {
        auto* windowInstance = static_cast<Window*>(glfwGetWindowUserPointer(window));
        if (windowInstance)
        {
            if (iconified)
            {
                Core::WindowMinimizeEvent event;
                windowInstance->m_EventDispatcher.Dispatch(event);
                LOG_DEBUG("Window minimized");
            }
            else
            {
                Core::WindowRestoreEvent event;
                windowInstance->m_EventDispatcher.Dispatch(event);
                LOG_DEBUG("Window restored");
            }
        }
    }

    void Window::KeyCallback(GLFWwindow* window, int key, int /*scancode*/, int action, int mods)
    {
        auto* windowInstance = static_cast<Window*>(glfwGetWindowUserPointer(window));
        if (windowInstance)
        {
            switch (action)
            {
            case GLFW_PRESS:
            {
                Core::KeyPressEvent event(key, mods);
                windowInstance->m_EventDispatcher.Dispatch(event);
                break;
            }
            case GLFW_RELEASE:
            {
                Core::KeyReleaseEvent event(key, mods);
                windowInstance->m_EventDispatcher.Dispatch(event);
                break;
            }
            case GLFW_REPEAT:
            {
                Core::KeyRepeatEvent event(key, mods);
                windowInstance->m_EventDispatcher.Dispatch(event);
                break;
            }
            }
        }
    }

    void Window::MouseButtonCallback(GLFWwindow* window, int button, int action, int /*mods*/)
    {
        auto* windowInstance = static_cast<Window*>(glfwGetWindowUserPointer(window));
        if (windowInstance)
        {
            if (action == GLFW_PRESS)
            {
                Core::MouseButtonPressEvent event(button);
                windowInstance->m_EventDispatcher.Dispatch(event);
            }
            else if (action == GLFW_RELEASE)
            {
                Core::MouseButtonReleaseEvent event(button);
                windowInstance->m_EventDispatcher.Dispatch(event);
            }
        }
    }

    void Window::CursorPosCallback(GLFWwindow* window, double xpos, double ypos)
    {
        auto* windowInstance = static_cast<Window*>(glfwGetWindowUserPointer(window));
        if (windowInstance)
        {
            Core::MouseMoveEvent event(xpos, ypos);
            windowInstance->m_EventDispatcher.Dispatch(event);
        }
    }

    void Window::ScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
    {
        auto* windowInstance = static_cast<Window*>(glfwGetWindowUserPointer(window));
        if (windowInstance)
        {
            Core::MouseScrollEvent event(xoffset, yoffset);
            windowInstance->m_EventDispatcher.Dispatch(event);
        }
    }
}
