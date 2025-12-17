/**
 * @file RHIDevice.h
 * @brief Vulkan Logical Device wrapper for the RHI layer.
 *
 * Manages VkDevice creation, queue retrieval, and VMA allocator initialization.
 * This class serves as the primary interface to the GPU for command submission.
 */

#pragma once

#include "Core/Types.h"
#include "RHI/RHIPhysicalDevice.h"

#include <vulkan/vulkan.h>

// Disable warnings for VMA library (third-party code)
#if defined(__GNUC__) || defined(__clang__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wunused-parameter"
    #pragma GCC diagnostic ignored "-Wunused-variable"
    #pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#elif defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable: 4100) // unreferenced formal parameter
    #pragma warning(disable: 4189) // local variable initialized but not referenced
    #pragma warning(disable: 4127) // conditional expression is constant
#endif

#include <vk_mem_alloc.h>

#if defined(__GNUC__) || defined(__clang__)
    #pragma GCC diagnostic pop
#elif defined(_MSC_VER)
    #pragma warning(pop)
#endif

#include <vector>

namespace RHI
{
    // Forward declarations
    class RHIInstance;

    /**
     * @brief Configuration for logical device creation
     */
    struct RHIDeviceConfig
    {
        /**
         * @brief Queue priority for all queues (0.0 to 1.0)
         * Higher values indicate higher priority.
         */
        float QueuePriority = 1.0f;

        /**
         * @brief Enable features required for the renderer.
         * If false, only minimal features are enabled.
         */
        bool EnableRequiredFeatures = true;
    };

    /**
     * @brief Vulkan Logical Device wrapper class
     *
     * Manages the logical device lifecycle including:
     * - VkDevice creation with required extensions and features
     * - Queue retrieval for Graphics, Present, Compute, and Transfer
     * - VMA allocator initialization for memory management
     *
     * This class follows RAII principles - resources are cleaned up
     * when the RHIDevice object is destroyed.
     *
     * Usage:
     * @code
     * auto instance = RHIInstance::Create();
     * auto physicalDevice = RHIPhysicalDevice::Select(instance, surface);
     * auto device = RHIDevice::Create(instance, physicalDevice, surface);
     * if (device) {
     *     VkQueue graphicsQueue = device->GetGraphicsQueue();
     *     VmaAllocator allocator = device->GetAllocator();
     * }
     * @endcode
     */
    class RHIDevice
    {
    public:
        /**
         * @brief Factory method to create an RHI Device
         * @param instance The Vulkan instance
         * @param physicalDevice The selected physical device
         * @param surface The window surface (used for swapchain extension check)
         * @param config Device configuration parameters
         * @return Shared pointer to the created device, or nullptr on failure
         */
        static Core::Ref<RHIDevice> Create(
            const Core::Ref<RHIInstance>& instance,
            const Core::Ref<RHIPhysicalDevice>& physicalDevice,
            VkSurfaceKHR surface,
            const RHIDeviceConfig& config = {});

        /**
         * @brief Destructor - destroys VkDevice and VMA allocator
         */
        ~RHIDevice();

        // Non-copyable
        RHIDevice(const RHIDevice&) = delete;
        RHIDevice& operator=(const RHIDevice&) = delete;

        // Non-movable (VkDevice handles cannot be safely moved)
        RHIDevice(RHIDevice&&) = delete;
        RHIDevice& operator=(RHIDevice&&) = delete;

        /**
         * @brief Get the native VkDevice handle
         * @return VkDevice handle
         */
        VkDevice GetHandle() const { return m_Device; }

        /**
         * @brief Get the associated physical device handle
         * @return VkPhysicalDevice handle
         */
        VkPhysicalDevice GetPhysicalDevice() const { return m_PhysicalDevice; }

        /**
         * @brief Get the graphics queue
         * @return VkQueue for graphics operations
         */
        VkQueue GetGraphicsQueue() const { return m_GraphicsQueue; }

        /**
         * @brief Get the present queue
         * @return VkQueue for presentation operations
         */
        VkQueue GetPresentQueue() const { return m_PresentQueue; }

        /**
         * @brief Get the compute queue
         * @return VkQueue for compute operations, or VK_NULL_HANDLE if not available
         */
        VkQueue GetComputeQueue() const { return m_ComputeQueue; }

        /**
         * @brief Get the transfer queue
         * @return VkQueue for transfer operations, or VK_NULL_HANDLE if not available
         */
        VkQueue GetTransferQueue() const { return m_TransferQueue; }

        /**
         * @brief Get the queue family indices
         * @return Reference to QueueFamilyIndices structure
         */
        const QueueFamilyIndices& GetQueueFamilies() const { return m_QueueFamilies; }

        /**
         * @brief Get the VMA allocator
         * @return VmaAllocator handle for memory allocation
         */
        VmaAllocator GetAllocator() const { return m_Allocator; }

        /**
         * @brief Get the list of enabled device extensions
         * @return Vector of enabled extension names
         */
        const std::vector<const char*>& GetEnabledExtensions() const { return m_EnabledExtensions; }

        /**
         * @brief Wait for all queues to complete execution
         *
         * Blocks until all submitted commands have completed.
         * Useful before resource destruction.
         */
        void WaitIdle() const;

    private:
        /**
         * @brief Private constructor - use Create() factory method
         */
        RHIDevice() = default;

        /**
         * @brief Initialize the logical device
         * @param instance The Vulkan instance
         * @param physicalDevice The selected physical device
         * @param surface The window surface
         * @param config Device configuration
         * @return true on success, false on failure
         */
        bool Initialize(
            const Core::Ref<RHIInstance>& instance,
            const Core::Ref<RHIPhysicalDevice>& physicalDevice,
            VkSurfaceKHR surface,
            const RHIDeviceConfig& config);

        /**
         * @brief Get required device extensions
         * @return Vector of required extension names
         */
        std::vector<const char*> GetRequiredExtensions();

        /**
         * @brief Check if all required extensions are supported
         * @param physicalDevice The physical device to check
         * @return true if all required extensions are supported
         */
        bool CheckExtensionSupport(const Core::Ref<RHIPhysicalDevice>& physicalDevice);

        /**
         * @brief Create queue create infos for unique queue families
         * @param config Device configuration
         * @return Vector of VkDeviceQueueCreateInfo structures
         */
        std::vector<VkDeviceQueueCreateInfo> CreateQueueCreateInfos(const RHIDeviceConfig& config);

        /**
         * @brief Initialize VMA allocator
         * @param instance The Vulkan instance
         * @return true on success, false on failure
         */
        bool InitializeVMA(const Core::Ref<RHIInstance>& instance);

        VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
        VkDevice m_Device = VK_NULL_HANDLE;
        VmaAllocator m_Allocator = VK_NULL_HANDLE;

        VkQueue m_GraphicsQueue = VK_NULL_HANDLE;
        VkQueue m_PresentQueue = VK_NULL_HANDLE;
        VkQueue m_ComputeQueue = VK_NULL_HANDLE;
        VkQueue m_TransferQueue = VK_NULL_HANDLE;

        QueueFamilyIndices m_QueueFamilies;
        std::vector<const char*> m_EnabledExtensions;
    };

} // namespace RHI
