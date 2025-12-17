/**
 * @file RHIPhysicalDevice.h
 * @brief Vulkan Physical Device wrapper for the RHI layer.
 *
 * Manages physical device (GPU) selection, enumeration, and queue family
 * discovery. Prefers discrete GPUs and validates required features.
 */

#pragma once

#include "Core/Types.h"

#include <vulkan/vulkan.h>

#include <optional>
#include <string>
#include <vector>

namespace RHI
{
    // Forward declarations
    class RHIInstance;

    /**
     * @brief Indices for different queue families on a physical device.
     *
     * Queue families represent different types of command processing:
     * - Graphics: Rendering commands (draw calls, etc.)
     * - Present: Swapchain presentation
     * - Compute: Compute shader dispatch
     * - Transfer: Memory copy operations
     *
     * Note: Graphics and Present often share the same queue family.
     * A dedicated Transfer queue enables async data uploads.
     */
    struct QueueFamilyIndices
    {
        std::optional<uint32_t> GraphicsFamily;
        std::optional<uint32_t> PresentFamily;
        std::optional<uint32_t> ComputeFamily;
        std::optional<uint32_t> TransferFamily;

        /**
         * @brief Check if minimum required queue families are available.
         * @return true if both Graphics and Present families are found.
         */
        bool IsComplete() const
        {
            return GraphicsFamily.has_value() && PresentFamily.has_value();
        }

        /**
         * @brief Check if all queue families are available.
         * @return true if all four queue families are found.
         */
        bool HasAllFamilies() const
        {
            return GraphicsFamily.has_value() &&
                   PresentFamily.has_value() &&
                   ComputeFamily.has_value() &&
                   TransferFamily.has_value();
        }

        /**
         * @brief Check if a dedicated transfer queue is available.
         *
         * A dedicated transfer queue (different from graphics) enables
         * asynchronous memory transfers without blocking rendering.
         *
         * @return true if TransferFamily differs from GraphicsFamily.
         */
        bool HasDedicatedTransferQueue() const
        {
            return TransferFamily.has_value() &&
                   GraphicsFamily.has_value() &&
                   TransferFamily.value() != GraphicsFamily.value();
        }
    };

    /**
     * @brief Comprehensive information about a physical device (GPU).
     *
     * Contains all relevant properties needed for device selection
     * and capability querying.
     */
    struct PhysicalDeviceInfo
    {
        VkPhysicalDevice Device = VK_NULL_HANDLE;
        VkPhysicalDeviceProperties Properties{};
        VkPhysicalDeviceFeatures Features{};
        VkPhysicalDeviceMemoryProperties MemoryProperties{};
        QueueFamilyIndices QueueFamilies;

        /**
         * @brief Get the device name as a string.
         * @return Device name from properties.
         */
        std::string GetDeviceName() const
        {
            return std::string(Properties.deviceName);
        }

        /**
         * @brief Check if this is a discrete GPU.
         * @return true if device type is discrete GPU.
         */
        bool IsDiscreteGPU() const
        {
            return Properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
        }

        /**
         * @brief Check if this is an integrated GPU.
         * @return true if device type is integrated GPU.
         */
        bool IsIntegratedGPU() const
        {
            return Properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;
        }

        /**
         * @brief Get total device local memory in bytes.
         * @return Total VRAM size for discrete GPUs.
         */
        VkDeviceSize GetDeviceLocalMemorySize() const;

        /**
         * @brief Convert device type to human-readable string.
         * @return String representation of device type.
         */
        std::string GetDeviceTypeString() const;
    };

    /**
     * @brief Vulkan Physical Device wrapper class.
     *
     * Handles GPU enumeration, selection, and queue family discovery.
     * The selection algorithm prefers:
     * 1. Discrete GPUs over integrated
     * 2. GPUs with more VRAM
     * 3. GPUs with dedicated transfer queues
     *
     * Usage:
     * @code
     * auto instance = RHIInstance::Create();
     * VkSurfaceKHR surface = window->CreateSurface(instance->GetHandle());
     * auto physicalDevice = RHIPhysicalDevice::Select(instance, surface);
     * if (physicalDevice) {
     *     const auto& info = physicalDevice->GetInfo();
     *     LOG_INFO("Selected GPU: {}", info.GetDeviceName());
     * }
     * @endcode
     */
    class RHIPhysicalDevice
    {
    public:
        /**
         * @brief Select the best available physical device.
         * @param instance The Vulkan instance.
         * @param surface The window surface for present capability check.
         * @return Shared pointer to the selected device, or nullptr if none suitable.
         */
        static Core::Ref<RHIPhysicalDevice> Select(
            const Core::Ref<RHIInstance>& instance,
            VkSurfaceKHR surface);

        /**
         * @brief Get all available physical devices.
         *
         * Returns information about all Vulkan-capable GPUs in the system.
         * Useful for UI device selection or debugging.
         *
         * @param instance The Vulkan instance.
         * @param surface The window surface for present capability check.
         * @return Vector of device info for all available GPUs.
         */
        static std::vector<PhysicalDeviceInfo> EnumerateDevices(
            const Core::Ref<RHIInstance>& instance,
            VkSurfaceKHR surface);

        /**
         * @brief Destructor.
         */
        ~RHIPhysicalDevice() = default;

        // Non-copyable
        RHIPhysicalDevice(const RHIPhysicalDevice&) = delete;
        RHIPhysicalDevice& operator=(const RHIPhysicalDevice&) = delete;

        // Movable
        RHIPhysicalDevice(RHIPhysicalDevice&&) = default;
        RHIPhysicalDevice& operator=(RHIPhysicalDevice&&) = default;

        /**
         * @brief Get the native VkPhysicalDevice handle.
         * @return VkPhysicalDevice handle.
         */
        VkPhysicalDevice GetHandle() const { return m_DeviceInfo.Device; }

        /**
         * @brief Get complete device information.
         * @return Reference to PhysicalDeviceInfo structure.
         */
        const PhysicalDeviceInfo& GetInfo() const { return m_DeviceInfo; }

        /**
         * @brief Get queue family indices.
         * @return Reference to QueueFamilyIndices structure.
         */
        const QueueFamilyIndices& GetQueueFamilies() const { return m_DeviceInfo.QueueFamilies; }

        /**
         * @brief Get device properties.
         * @return Reference to VkPhysicalDeviceProperties.
         */
        const VkPhysicalDeviceProperties& GetProperties() const { return m_DeviceInfo.Properties; }

        /**
         * @brief Get device features.
         * @return Reference to VkPhysicalDeviceFeatures.
         */
        const VkPhysicalDeviceFeatures& GetFeatures() const { return m_DeviceInfo.Features; }

        /**
         * @brief Get memory properties.
         * @return Reference to VkPhysicalDeviceMemoryProperties.
         */
        const VkPhysicalDeviceMemoryProperties& GetMemoryProperties() const
        {
            return m_DeviceInfo.MemoryProperties;
        }

        /**
         * @brief Check if a device extension is supported.
         * @param extensionName Name of the extension to check.
         * @return true if the extension is supported.
         */
        bool IsExtensionSupported(const char* extensionName) const;

        /**
         * @brief Get list of supported device extensions.
         * @return Vector of supported extension names.
         */
        const std::vector<VkExtensionProperties>& GetSupportedExtensions() const
        {
            return m_SupportedExtensions;
        }

    private:
        /**
         * @brief Private constructor - use Select() factory method.
         */
        RHIPhysicalDevice() = default;

        /**
         * @brief Initialize with a specific physical device.
         * @param device VkPhysicalDevice handle.
         * @param surface VkSurfaceKHR for queue family discovery.
         * @return true on success, false on failure.
         */
        bool Initialize(VkPhysicalDevice device, VkSurfaceKHR surface);

        /**
         * @brief Find queue family indices for a physical device.
         * @param device VkPhysicalDevice to query.
         * @param surface VkSurfaceKHR for present support check.
         * @return QueueFamilyIndices with discovered families.
         */
        static QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);

        /**
         * @brief Check if a device is suitable for rendering.
         * @param device VkPhysicalDevice to check.
         * @param surface VkSurfaceKHR for present support.
         * @return true if device meets minimum requirements.
         */
        static bool IsDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface);

        /**
         * @brief Calculate a score for device selection.
         *
         * Higher scores are better. Considers:
         * - Device type (discrete > integrated > other)
         * - VRAM size
         * - Feature availability
         *
         * @param info Device information to score.
         * @return Integer score (higher is better).
         */
        static int32_t RateDevice(const PhysicalDeviceInfo& info);

        /**
         * @brief Query supported extensions for the device.
         */
        void QuerySupportedExtensions();

        PhysicalDeviceInfo m_DeviceInfo;
        std::vector<VkExtensionProperties> m_SupportedExtensions;
    };

} // namespace RHI
