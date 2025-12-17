/**
 * @file RHISwapchain.cpp
 * @brief Implementation of Vulkan Swapchain wrapper.
 */

#include "RHI/RHISwapchain.h"
#include "RHI/RHIDevice.h"
#include "Core/Assert.h"
#include "Core/Log.h"

#include <algorithm>
#include <limits>

namespace RHI
{
    SwapchainSupportDetails RHISwapchain::QuerySwapchainSupport(
        VkPhysicalDevice physicalDevice,
        VkSurfaceKHR surface)
    {
        SwapchainSupportDetails details;

        // Query surface capabilities
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &details.Capabilities);

        // Query surface formats
        uint32_t formatCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);

        if (formatCount != 0)
        {
            details.Formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, details.Formats.data());
        }

        // Query present modes
        uint32_t presentModeCount = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);

        if (presentModeCount != 0)
        {
            details.PresentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(
                physicalDevice, surface, &presentModeCount, details.PresentModes.data());
        }

        return details;
    }

    Core::Ref<RHISwapchain> RHISwapchain::Create(
        const Core::Ref<RHIDevice>& device,
        VkSurfaceKHR surface,
        uint32_t width,
        uint32_t height,
        const SwapchainConfig& config)
    {
        auto swapchain = Core::Ref<RHISwapchain>(new RHISwapchain());

        if (!swapchain->Initialize(device, surface, width, height, config))
        {
            LOG_ERROR("Failed to initialize RHI Swapchain");
            return nullptr;
        }

        return swapchain;
    }

    RHISwapchain::~RHISwapchain()
    {
        // Destroy image views
        DestroyImageViews();

        // Destroy swapchain
        if (m_Swapchain != VK_NULL_HANDLE)
        {
            vkDestroySwapchainKHR(m_Device, m_Swapchain, nullptr);
            m_Swapchain = VK_NULL_HANDLE;
            LOG_DEBUG("Vulkan swapchain destroyed");
        }
    }

    bool RHISwapchain::Initialize(
        const Core::Ref<RHIDevice>& device,
        VkSurfaceKHR surface,
        uint32_t width,
        uint32_t height,
        const SwapchainConfig& config)
    {
        ASSERT(device != nullptr);
        ASSERT(surface != VK_NULL_HANDLE);

        m_Device = device->GetHandle();
        m_PhysicalDevice = device->GetPhysicalDevice();
        m_Surface = surface;
        m_Config = config;
        m_DesiredWidth = width;
        m_DesiredHeight = height;

        // Store queue family indices for sharing mode determination
        const auto& queueFamilies = device->GetQueueFamilies();
        m_GraphicsFamily = queueFamilies.GraphicsFamily.value();
        m_PresentFamily = queueFamilies.PresentFamily.value();

        return CreateSwapchain();
    }

    bool RHISwapchain::Recreate(uint32_t width, uint32_t height)
    {
        ASSERT(m_Device != VK_NULL_HANDLE);

        // Wait for device to be idle before recreation
        vkDeviceWaitIdle(m_Device);

        m_DesiredWidth = width;
        m_DesiredHeight = height;
        m_NeedsRecreation = false;

        // Store old swapchain for smooth transition
        VkSwapchainKHR oldSwapchain = m_Swapchain;
        m_Swapchain = VK_NULL_HANDLE;

        // Destroy old image views
        DestroyImageViews();
        m_Images.clear();

        // Create new swapchain (passing old for potential reuse)
        bool success = CreateSwapchain(oldSwapchain);

        // Destroy old swapchain after new one is created
        if (oldSwapchain != VK_NULL_HANDLE)
        {
            vkDestroySwapchainKHR(m_Device, oldSwapchain, nullptr);
        }

        if (success)
        {
            LOG_INFO("Swapchain recreated: {}x{}", m_Extent.width, m_Extent.height);
        }

        return success;
    }

    bool RHISwapchain::CreateSwapchain(VkSwapchainKHR oldSwapchain)
    {
        // Query swapchain support
        SwapchainSupportDetails supportDetails = QuerySwapchainSupport(m_PhysicalDevice, m_Surface);

        if (!supportDetails.IsAdequate())
        {
            LOG_ERROR("Swapchain support is inadequate");
            return false;
        }

        // Choose best settings
        VkSurfaceFormatKHR surfaceFormat = ChooseSurfaceFormat(supportDetails.Formats);
        VkPresentModeKHR presentMode = ChoosePresentMode(supportDetails.PresentModes);
        VkExtent2D extent = ChooseExtent(supportDetails.Capabilities, m_DesiredWidth, m_DesiredHeight);

        // Choose image count (prefer triple buffering)
        uint32_t imageCount = supportDetails.Capabilities.minImageCount + 1;
        if (supportDetails.Capabilities.maxImageCount > 0 &&
            imageCount > supportDetails.Capabilities.maxImageCount)
        {
            imageCount = supportDetails.Capabilities.maxImageCount;
        }

        // Create swapchain
        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = m_Surface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        // Handle queue family sharing mode
        uint32_t queueFamilyIndices[] = {m_GraphicsFamily, m_PresentFamily};

        if (m_GraphicsFamily != m_PresentFamily)
        {
            // Different queue families: use concurrent sharing mode
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
            LOG_DEBUG("Using concurrent sharing mode for swapchain (graphics: {}, present: {})",
                m_GraphicsFamily, m_PresentFamily);
        }
        else
        {
            // Same queue family: use exclusive mode for better performance
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.queueFamilyIndexCount = 0;
            createInfo.pQueueFamilyIndices = nullptr;
        }

        createInfo.preTransform = supportDetails.Capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = oldSwapchain;

        VkResult result = vkCreateSwapchainKHR(m_Device, &createInfo, nullptr, &m_Swapchain);
        if (result != VK_SUCCESS)
        {
            LOG_ERROR("Failed to create swapchain, VkResult: {}", static_cast<int>(result));
            return false;
        }

        // Store properties
        m_ImageFormat = surfaceFormat.format;
        m_Extent = extent;

        // Retrieve swapchain images
        uint32_t actualImageCount = 0;
        vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &actualImageCount, nullptr);
        m_Images.resize(actualImageCount);
        vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &actualImageCount, m_Images.data());

        LOG_INFO("Swapchain created:");
        LOG_INFO("  Extent: {}x{}", m_Extent.width, m_Extent.height);
        LOG_INFO("  Format: {}", static_cast<int>(m_ImageFormat));
        LOG_INFO("  Present Mode: {}", static_cast<int>(presentMode));
        LOG_INFO("  Image Count: {}", m_Images.size());

        // Create image views
        if (!CreateImageViews())
        {
            LOG_ERROR("Failed to create swapchain image views");
            return false;
        }

        return true;
    }

    bool RHISwapchain::CreateImageViews()
    {
        m_ImageViews.resize(m_Images.size());

        for (size_t i = 0; i < m_Images.size(); ++i)
        {
            VkImageViewCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image = m_Images[i];
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            createInfo.format = m_ImageFormat;

            // Default component mapping
            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

            // Color aspect, single mip level, single layer
            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount = 1;

            VkResult result = vkCreateImageView(m_Device, &createInfo, nullptr, &m_ImageViews[i]);
            if (result != VK_SUCCESS)
            {
                LOG_ERROR("Failed to create image view {}, VkResult: {}", i, static_cast<int>(result));
                // Clean up previously created image views
                for (size_t j = 0; j < i; ++j)
                {
                    vkDestroyImageView(m_Device, m_ImageViews[j], nullptr);
                }
                m_ImageViews.clear();
                return false;
            }
        }

        LOG_DEBUG("Created {} swapchain image views", m_ImageViews.size());
        return true;
    }

    void RHISwapchain::DestroyImageViews()
    {
        for (VkImageView imageView : m_ImageViews)
        {
            if (imageView != VK_NULL_HANDLE)
            {
                vkDestroyImageView(m_Device, imageView, nullptr);
            }
        }
        m_ImageViews.clear();
    }

    VkSurfaceFormatKHR RHISwapchain::ChooseSurfaceFormat(
        const std::vector<VkSurfaceFormatKHR>& availableFormats)
    {
        // Look for preferred format (B8G8R8A8_SRGB with SRGB_NONLINEAR color space)
        for (const auto& format : availableFormats)
        {
            if (format.format == m_Config.PreferredFormat &&
                format.colorSpace == m_Config.PreferredColorSpace)
            {
                LOG_DEBUG("Using preferred surface format: {} with color space: {}",
                    static_cast<int>(format.format), static_cast<int>(format.colorSpace));
                return format;
            }
        }

        // Try to find any SRGB format
        for (const auto& format : availableFormats)
        {
            if (format.format == VK_FORMAT_B8G8R8A8_SRGB ||
                format.format == VK_FORMAT_R8G8B8A8_SRGB)
            {
                LOG_DEBUG("Using fallback SRGB format: {} with color space: {}",
                    static_cast<int>(format.format), static_cast<int>(format.colorSpace));
                return format;
            }
        }

        // Fall back to first available format
        LOG_WARN("Preferred surface format not available, using first available: {} with color space: {}",
            static_cast<int>(availableFormats[0].format), static_cast<int>(availableFormats[0].colorSpace));
        return availableFormats[0];
    }

    VkPresentModeKHR RHISwapchain::ChoosePresentMode(
        const std::vector<VkPresentModeKHR>& availablePresentModes)
    {
        // If VSync is forced, use FIFO (guaranteed available)
        if (m_Config.VSync)
        {
            LOG_DEBUG("VSync enabled, using FIFO present mode");
            return VK_PRESENT_MODE_FIFO_KHR;
        }

        // Look for preferred present mode
        for (const auto& mode : availablePresentModes)
        {
            if (mode == m_Config.PreferredPresentMode)
            {
                LOG_DEBUG("Using preferred present mode: {}", static_cast<int>(mode));
                return mode;
            }
        }

        // Try Mailbox (triple buffering) if available
        for (const auto& mode : availablePresentModes)
        {
            if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
            {
                LOG_DEBUG("Using Mailbox present mode (triple buffering)");
                return VK_PRESENT_MODE_MAILBOX_KHR;
            }
        }

        // FIFO is guaranteed to be available
        LOG_DEBUG("Using FIFO present mode (VSync)");
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D RHISwapchain::ChooseExtent(
        const VkSurfaceCapabilitiesKHR& capabilities,
        uint32_t desiredWidth,
        uint32_t desiredHeight)
    {
        // If currentExtent is not UINT32_MAX, the window size is fixed
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
        {
            return capabilities.currentExtent;
        }

        // Otherwise, choose an extent within the allowed range
        VkExtent2D actualExtent = {desiredWidth, desiredHeight};

        actualExtent.width = std::clamp(
            actualExtent.width,
            capabilities.minImageExtent.width,
            capabilities.maxImageExtent.width);

        actualExtent.height = std::clamp(
            actualExtent.height,
            capabilities.minImageExtent.height,
            capabilities.maxImageExtent.height);

        return actualExtent;
    }

    VkImage RHISwapchain::GetImage(uint32_t index) const
    {
        ASSERT(index < m_Images.size());
        return m_Images[index];
    }

    VkImageView RHISwapchain::GetImageView(uint32_t index) const
    {
        ASSERT(index < m_ImageViews.size());
        return m_ImageViews[index];
    }

    uint32_t RHISwapchain::AcquireNextImage(
        VkSemaphore signalSemaphore,
        VkFence fence,
        uint64_t timeout)
    {
        uint32_t imageIndex = 0;

        VkResult result = vkAcquireNextImageKHR(
            m_Device,
            m_Swapchain,
            timeout,
            signalSemaphore,
            fence,
            &imageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            // Swapchain is out of date (e.g., window resized)
            m_NeedsRecreation = true;
            LOG_DEBUG("Swapchain out of date during image acquisition");
            return std::numeric_limits<uint32_t>::max();
        }
        else if (result == VK_SUBOPTIMAL_KHR)
        {
            // Swapchain is suboptimal but still usable
            m_NeedsRecreation = true;
            LOG_DEBUG("Swapchain suboptimal during image acquisition");
            // Continue with current image
        }
        else if (result != VK_SUCCESS)
        {
            LOG_ERROR("Failed to acquire swapchain image, VkResult: {}", static_cast<int>(result));
            return std::numeric_limits<uint32_t>::max();
        }

        return imageIndex;
    }

    bool RHISwapchain::Present(
        VkQueue queue,
        uint32_t imageIndex,
        VkSemaphore waitSemaphore)
    {
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

        if (waitSemaphore != VK_NULL_HANDLE)
        {
            presentInfo.waitSemaphoreCount = 1;
            presentInfo.pWaitSemaphores = &waitSemaphore;
        }

        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &m_Swapchain;
        presentInfo.pImageIndices = &imageIndex;
        presentInfo.pResults = nullptr;

        VkResult result = vkQueuePresentKHR(queue, &presentInfo);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
        {
            m_NeedsRecreation = true;
            LOG_DEBUG("Swapchain needs recreation after present");
            return result == VK_SUBOPTIMAL_KHR; // Suboptimal is still a successful present
        }
        else if (result != VK_SUCCESS)
        {
            LOG_ERROR("Failed to present swapchain image, VkResult: {}", static_cast<int>(result));
            return false;
        }

        return true;
    }

} // namespace RHI
