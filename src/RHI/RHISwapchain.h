/**
 * @file RHISwapchain.h
 * @brief Vulkan Swapchain wrapper for the RHI layer.
 *
 * Manages swapchain creation, image acquisition, and presentation.
 * Handles surface format/present mode selection and window resize.
 */

#pragma once

#include "Core/Types.h"

#include <vulkan/vulkan.h>

#include <vector>

namespace RHI
{
    // Forward declarations
    class RHIDevice;

    /**
     * @brief Swapchain support details queried from physical device.
     *
     * Contains surface capabilities, supported formats, and present modes
     * needed to create a compatible swapchain.
     */
    struct SwapchainSupportDetails
    {
        VkSurfaceCapabilitiesKHR Capabilities{};
        std::vector<VkSurfaceFormatKHR> Formats;
        std::vector<VkPresentModeKHR> PresentModes;

        /**
         * @brief Check if swapchain creation is supported.
         * @return true if at least one format and present mode are available.
         */
        bool IsAdequate() const
        {
            return !Formats.empty() && !PresentModes.empty();
        }
    };

    /**
     * @brief Configuration for swapchain creation.
     */
    struct SwapchainConfig
    {
        /**
         * @brief Preferred surface format.
         * Falls back to first available if not supported.
         */
        VkFormat PreferredFormat = VK_FORMAT_B8G8R8A8_SRGB;

        /**
         * @brief Preferred color space.
         * Falls back to first available if not supported.
         */
        VkColorSpaceKHR PreferredColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

        /**
         * @brief Preferred present mode.
         * MAILBOX = Triple buffering (low latency, no tearing)
         * FIFO = VSync (guaranteed available)
         * Falls back to FIFO if not supported.
         */
        VkPresentModeKHR PreferredPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;

        /**
         * @brief Enable VSync (force FIFO present mode).
         */
        bool VSync = false;
    };

    /**
     * @brief Vulkan Swapchain wrapper class.
     *
     * Manages the swapchain lifecycle including:
     * - Surface format and present mode selection
     * - Swapchain image and image view creation
     * - Image acquisition and presentation
     * - Window resize handling (recreation)
     *
     * Usage:
     * @code
     * auto swapchain = RHISwapchain::Create(device, surface, 1920, 1080);
     * if (swapchain) {
     *     // Render loop
     *     uint32_t imageIndex = swapchain->AcquireNextImage(imageAvailableSemaphore);
     *     // ... record commands ...
     *     swapchain->Present(presentQueue, imageIndex, renderFinishedSemaphore);
     *
     *     // On window resize
     *     swapchain->Recreate(newWidth, newHeight);
     * }
     * @endcode
     */
    class RHISwapchain
    {
    public:
        /**
         * @brief Query swapchain support details for a physical device.
         * @param physicalDevice The physical device to query.
         * @param surface The surface to check against.
         * @return SwapchainSupportDetails with capabilities, formats, and present modes.
         */
        static SwapchainSupportDetails QuerySwapchainSupport(
            VkPhysicalDevice physicalDevice,
            VkSurfaceKHR surface);

        /**
         * @brief Factory method to create an RHI Swapchain.
         * @param device The logical device.
         * @param surface The window surface.
         * @param width Desired swapchain width.
         * @param height Desired swapchain height.
         * @param config Swapchain configuration parameters.
         * @return Shared pointer to the created swapchain, or nullptr on failure.
         */
        static Core::Ref<RHISwapchain> Create(
            const Core::Ref<RHIDevice>& device,
            VkSurfaceKHR surface,
            uint32_t width,
            uint32_t height,
            const SwapchainConfig& config = {});

        /**
         * @brief Destructor - destroys swapchain and image views.
         */
        ~RHISwapchain();

        // Non-copyable
        RHISwapchain(const RHISwapchain&) = delete;
        RHISwapchain& operator=(const RHISwapchain&) = delete;

        // Non-movable
        RHISwapchain(RHISwapchain&&) = delete;
        RHISwapchain& operator=(RHISwapchain&&) = delete;

        /**
         * @brief Recreate the swapchain with new dimensions.
         *
         * Call this when the window is resized. The old swapchain is destroyed
         * and a new one is created with the updated dimensions.
         *
         * @param width New swapchain width.
         * @param height New swapchain height.
         * @return true on success, false on failure.
         */
        bool Recreate(uint32_t width, uint32_t height);

        /**
         * @brief Get the native VkSwapchainKHR handle.
         * @return VkSwapchainKHR handle.
         */
        VkSwapchainKHR GetHandle() const { return m_Swapchain; }

        /**
         * @brief Get the swapchain image format.
         * @return VkFormat of swapchain images.
         */
        VkFormat GetImageFormat() const { return m_ImageFormat; }

        /**
         * @brief Get the swapchain extent.
         * @return VkExtent2D with width and height.
         */
        VkExtent2D GetExtent() const { return m_Extent; }

        /**
         * @brief Get the number of swapchain images.
         * @return Image count (typically 2-3).
         */
        uint32_t GetImageCount() const { return static_cast<uint32_t>(m_Images.size()); }

        /**
         * @brief Get a swapchain image.
         * @param index Image index (0 to GetImageCount() - 1).
         * @return VkImage handle.
         */
        VkImage GetImage(uint32_t index) const;

        /**
         * @brief Get a swapchain image view.
         * @param index Image index (0 to GetImageCount() - 1).
         * @return VkImageView handle.
         */
        VkImageView GetImageView(uint32_t index) const;

        /**
         * @brief Get all swapchain images.
         * @return Vector of VkImage handles.
         */
        const std::vector<VkImage>& GetImages() const { return m_Images; }

        /**
         * @brief Get all swapchain image views.
         * @return Vector of VkImageView handles.
         */
        const std::vector<VkImageView>& GetImageViews() const { return m_ImageViews; }

        /**
         * @brief Acquire the next available swapchain image.
         *
         * Blocks until an image is available or timeout occurs.
         * The returned index is used for rendering and presentation.
         *
         * @param signalSemaphore Semaphore to signal when image is acquired.
         * @param fence Optional fence to signal (VK_NULL_HANDLE if not needed).
         * @param timeout Timeout in nanoseconds (UINT64_MAX for infinite).
         * @return Image index, or UINT32_MAX if swapchain needs recreation.
         */
        uint32_t AcquireNextImage(
            VkSemaphore signalSemaphore,
            VkFence fence = VK_NULL_HANDLE,
            uint64_t timeout = UINT64_MAX);

        /**
         * @brief Present a rendered image to the screen.
         *
         * Queues the specified image for presentation.
         *
         * @param queue The present queue.
         * @param imageIndex Index of the image to present.
         * @param waitSemaphore Semaphore to wait on before presenting.
         * @return true on success, false if swapchain needs recreation.
         */
        bool Present(
            VkQueue queue,
            uint32_t imageIndex,
            VkSemaphore waitSemaphore);

        /**
         * @brief Check if the swapchain needs to be recreated.
         *
         * Set to true after AcquireNextImage or Present returns suboptimal/out-of-date.
         *
         * @return true if Recreate() should be called.
         */
        bool NeedsRecreation() const { return m_NeedsRecreation; }

    private:
        /**
         * @brief Private constructor - use Create() factory method.
         */
        RHISwapchain() = default;

        /**
         * @brief Initialize the swapchain.
         * @param device The logical device.
         * @param surface The window surface.
         * @param width Desired width.
         * @param height Desired height.
         * @param config Swapchain configuration.
         * @return true on success, false on failure.
         */
        bool Initialize(
            const Core::Ref<RHIDevice>& device,
            VkSurfaceKHR surface,
            uint32_t width,
            uint32_t height,
            const SwapchainConfig& config);

        /**
         * @brief Create the swapchain.
         * @param oldSwapchain Previous swapchain for smooth transition (can be VK_NULL_HANDLE).
         * @return true on success, false on failure.
         */
        bool CreateSwapchain(VkSwapchainKHR oldSwapchain = VK_NULL_HANDLE);

        /**
         * @brief Create image views for swapchain images.
         * @return true on success, false on failure.
         */
        bool CreateImageViews();

        /**
         * @brief Destroy image views.
         */
        void DestroyImageViews();

        /**
         * @brief Choose the best surface format from available options.
         * @param availableFormats List of supported formats.
         * @return Selected surface format.
         */
        VkSurfaceFormatKHR ChooseSurfaceFormat(
            const std::vector<VkSurfaceFormatKHR>& availableFormats);

        /**
         * @brief Choose the best present mode from available options.
         * @param availablePresentModes List of supported present modes.
         * @return Selected present mode.
         */
        VkPresentModeKHR ChoosePresentMode(
            const std::vector<VkPresentModeKHR>& availablePresentModes);

        /**
         * @brief Choose the swapchain extent (resolution).
         * @param capabilities Surface capabilities.
         * @param desiredWidth Desired width.
         * @param desiredHeight Desired height.
         * @return Selected extent.
         */
        VkExtent2D ChooseExtent(
            const VkSurfaceCapabilitiesKHR& capabilities,
            uint32_t desiredWidth,
            uint32_t desiredHeight);

        // Device reference (weak to avoid circular dependency)
        VkDevice m_Device = VK_NULL_HANDLE;
        VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
        VkSurfaceKHR m_Surface = VK_NULL_HANDLE;

        // Swapchain resources
        VkSwapchainKHR m_Swapchain = VK_NULL_HANDLE;
        std::vector<VkImage> m_Images;
        std::vector<VkImageView> m_ImageViews;

        // Swapchain properties
        VkFormat m_ImageFormat = VK_FORMAT_UNDEFINED;
        VkExtent2D m_Extent{0, 0};

        // Configuration
        SwapchainConfig m_Config;
        uint32_t m_DesiredWidth = 0;
        uint32_t m_DesiredHeight = 0;

        // Queue family indices for sharing mode
        uint32_t m_GraphicsFamily = 0;
        uint32_t m_PresentFamily = 0;

        // State flags
        bool m_NeedsRecreation = false;
    };

} // namespace RHI
