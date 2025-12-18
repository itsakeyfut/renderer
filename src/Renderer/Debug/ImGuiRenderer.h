/**
 * @file ImGuiRenderer.h
 * @brief ImGui integration for Vulkan debug UI rendering.
 *
 * Provides a wrapper around Dear ImGui with Vulkan and GLFW backends
 * for rendering debug UI overlays in the application.
 */

#pragma once

#include "Core/Types.h"

#include <vulkan/vulkan.h>

// Forward declarations
struct GLFWwindow;

namespace RHI
{
    class RHIDevice;
    class RHICommandBuffer;
    class RHISwapchain;
}

namespace Platform
{
    class Window;
}

namespace Renderer
{
    /**
     * @brief Statistics data for debug display
     *
     * Contains various metrics that can be displayed in the debug UI.
     */
    struct DebugStats
    {
        float FrameTime = 0.0f;         ///< Time for the last frame in milliseconds
        float FPS = 0.0f;               ///< Frames per second
        uint32_t DrawCalls = 0;         ///< Number of draw calls this frame
        uint32_t TriangleCount = 0;     ///< Total triangles rendered this frame
        uint64_t GPUMemoryUsed = 0;     ///< GPU memory usage in bytes
        uint64_t GPUMemoryTotal = 0;    ///< Total GPU memory in bytes
    };

    /**
     * @brief Configuration for ImGui renderer initialization
     */
    struct ImGuiRendererConfig
    {
        /**
         * @brief Vulkan instance handle
         */
        VkInstance Instance = VK_NULL_HANDLE;

        /**
         * @brief Queue family index for graphics operations
         */
        uint32_t QueueFamily = 0;

        /**
         * @brief Graphics queue handle
         */
        VkQueue GraphicsQueue = VK_NULL_HANDLE;

        /**
         * @brief Image format for the color attachment
         */
        VkFormat ColorFormat = VK_FORMAT_B8G8R8A8_SRGB;

        /**
         * @brief Number of swapchain images
         */
        uint32_t ImageCount = 2;
    };

    /**
     * @brief ImGui renderer for Vulkan debug UI
     *
     * Manages the lifecycle and rendering of ImGui overlays using Vulkan.
     * Integrates with GLFW for input handling and supports dynamic rendering
     * (Vulkan 1.3).
     *
     * Usage:
     * @code
     * // Initialization
     * auto imguiRenderer = ImGuiRenderer::Create(device, window, config);
     *
     * // In render loop
     * imguiRenderer->BeginFrame();
     *
     * // Draw ImGui widgets
     * ImGui::Begin("Debug");
     * ImGui::Text("FPS: %.1f", fps);
     * ImGui::End();
     *
     * imguiRenderer->EndFrame();
     * imguiRenderer->Render(commandBuffer, imageView, extent);
     * @endcode
     */
    class ImGuiRenderer
    {
    public:
        /**
         * @brief Factory method to create an ImGui renderer
         * @param device The RHI device for Vulkan operations
         * @param window The window for input handling
         * @param config Configuration parameters
         * @return Shared pointer to the created renderer, or nullptr on failure
         */
        static Core::Ref<ImGuiRenderer> Create(
            const Core::Ref<RHI::RHIDevice>& device,
            Platform::Window& window,
            const ImGuiRendererConfig& config);

        /**
         * @brief Destructor - cleans up ImGui resources
         */
        ~ImGuiRenderer();

        // Non-copyable
        ImGuiRenderer(const ImGuiRenderer&) = delete;
        ImGuiRenderer& operator=(const ImGuiRenderer&) = delete;

        // Non-movable
        ImGuiRenderer(ImGuiRenderer&&) = delete;
        ImGuiRenderer& operator=(ImGuiRenderer&&) = delete;

        /**
         * @brief Begin a new ImGui frame
         *
         * Must be called at the start of each frame before any ImGui calls.
         * Initializes the ImGui frame state and processes input.
         */
        void BeginFrame();

        /**
         * @brief End the current ImGui frame
         *
         * Finalizes the ImGui draw data. Must be called after all ImGui
         * widget calls and before Render().
         */
        void EndFrame();

        /**
         * @brief Render ImGui draw data to a command buffer
         *
         * Records ImGui rendering commands to the provided command buffer.
         * The command buffer must be in a recording state with an active
         * dynamic rendering pass.
         *
         * @param cmdBuffer Command buffer to record to
         * @param imageView Target image view for rendering
         * @param extent Render area extent
         */
        void Render(
            const Core::Ref<RHI::RHICommandBuffer>& cmdBuffer,
            VkImageView imageView,
            VkExtent2D extent);

        /**
         * @brief Show the debug statistics window
         * @param stats Statistics data to display
         * @param show Pointer to visibility flag (can be toggled by user)
         */
        void ShowStatsWindow(const DebugStats& stats, bool* show = nullptr);

        /**
         * @brief Show the ImGui demo window
         * @param show Pointer to visibility flag
         */
        void ShowDemoWindow(bool* show = nullptr);

        /**
         * @brief Check if ImGui wants to capture mouse input
         * @return true if ImGui is using the mouse
         */
        bool WantCaptureMouse() const;

        /**
         * @brief Check if ImGui wants to capture keyboard input
         * @return true if ImGui is using the keyboard
         */
        bool WantCaptureKeyboard() const;

    private:
        /**
         * @brief Private constructor - use Create() factory method
         */
        ImGuiRenderer() = default;

        /**
         * @brief Initialize ImGui with Vulkan and GLFW backends
         * @param device The RHI device
         * @param window The window
         * @param config Configuration parameters
         * @return true on success, false on failure
         */
        bool Initialize(
            const Core::Ref<RHI::RHIDevice>& device,
            Platform::Window& window,
            const ImGuiRendererConfig& config);

        /**
         * @brief Create the descriptor pool for ImGui
         * @param device Vulkan device
         * @return true on success
         */
        bool CreateDescriptorPool(VkDevice device);

        /**
         * @brief Upload ImGui font textures to GPU
         * @param device The RHI device
         * @return true on success
         */
        bool UploadFonts(const Core::Ref<RHI::RHIDevice>& device);

        VkDevice m_Device = VK_NULL_HANDLE;
        VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
        bool m_Initialized = false;
    };

} // namespace Renderer
