/**
 * @file RHIDevice.cpp
 * @brief Implementation of Vulkan Logical Device wrapper.
 */

// VMA_IMPLEMENTATION must be defined before including the header
// which includes vk_mem_alloc.h
#define VMA_IMPLEMENTATION

#include "RHI/RHIDevice.h"
#include "RHI/RHIInstance.h"
#include "Core/Assert.h"
#include "Core/Log.h"

#include <set>
#include <cstring>

namespace RHI
{
    // Required device extensions
    static const std::vector<const char*> s_RequiredExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    // Validation layer names (for device creation when enabled)
    static const std::vector<const char*> s_ValidationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };

    Core::Ref<RHIDevice> RHIDevice::Create(
        const Core::Ref<RHIInstance>& instance,
        const Core::Ref<RHIPhysicalDevice>& physicalDevice,
        VkSurfaceKHR surface,
        const RHIDeviceConfig& config)
    {
        auto device = Core::Ref<RHIDevice>(new RHIDevice());

        if (!device->Initialize(instance, physicalDevice, surface, config))
        {
            LOG_ERROR("Failed to initialize RHI Device");
            return nullptr;
        }

        return device;
    }

    RHIDevice::~RHIDevice()
    {
        // Destroy VMA allocator first (before device)
        if (m_Allocator != VK_NULL_HANDLE)
        {
            vmaDestroyAllocator(m_Allocator);
            m_Allocator = VK_NULL_HANDLE;
            LOG_DEBUG("VMA allocator destroyed");
        }

        // Destroy logical device
        if (m_Device != VK_NULL_HANDLE)
        {
            vkDestroyDevice(m_Device, nullptr);
            m_Device = VK_NULL_HANDLE;
            LOG_DEBUG("Vulkan logical device destroyed");
        }
    }

    bool RHIDevice::Initialize(
        const Core::Ref<RHIInstance>& instance,
        const Core::Ref<RHIPhysicalDevice>& physicalDevice,
        [[maybe_unused]] VkSurfaceKHR surface,
        const RHIDeviceConfig& config)
    {
        ASSERT(instance != nullptr);
        ASSERT(physicalDevice != nullptr);

        m_PhysicalDevice = physicalDevice->GetHandle();
        m_QueueFamilies = physicalDevice->GetQueueFamilies();

        // Verify queue families are complete
        if (!m_QueueFamilies.IsComplete())
        {
            LOG_ERROR("Physical device does not have required queue families");
            return false;
        }

        // Check extension support
        if (!CheckExtensionSupport(physicalDevice))
        {
            LOG_ERROR("Physical device does not support required extensions");
            return false;
        }

        // Get required extensions
        m_EnabledExtensions = GetRequiredExtensions();

        // Create queue create infos
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos = CreateQueueCreateInfos(config);

        // Setup device features
        VkPhysicalDeviceFeatures deviceFeatures{};
        if (config.EnableRequiredFeatures)
        {
            const auto& supportedFeatures = physicalDevice->GetFeatures();

            // Enable commonly needed features if supported
            if (supportedFeatures.samplerAnisotropy)
            {
                deviceFeatures.samplerAnisotropy = VK_TRUE;
            }

            if (supportedFeatures.fillModeNonSolid)
            {
                deviceFeatures.fillModeNonSolid = VK_TRUE;
            }

            if (supportedFeatures.wideLines)
            {
                deviceFeatures.wideLines = VK_TRUE;
            }

            if (supportedFeatures.geometryShader)
            {
                deviceFeatures.geometryShader = VK_TRUE;
            }

            if (supportedFeatures.tessellationShader)
            {
                deviceFeatures.tessellationShader = VK_TRUE;
            }

            if (supportedFeatures.multiDrawIndirect)
            {
                deviceFeatures.multiDrawIndirect = VK_TRUE;
            }

            if (supportedFeatures.drawIndirectFirstInstance)
            {
                deviceFeatures.drawIndirectFirstInstance = VK_TRUE;
            }

            if (supportedFeatures.depthClamp)
            {
                deviceFeatures.depthClamp = VK_TRUE;
            }

            if (supportedFeatures.depthBiasClamp)
            {
                deviceFeatures.depthBiasClamp = VK_TRUE;
            }
        }

        // Enable Vulkan 1.3 dynamic rendering feature
        // Required for vkCmdBeginRendering/vkCmdEndRendering
        VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeatures{};
        dynamicRenderingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
        dynamicRenderingFeatures.dynamicRendering = VK_TRUE;

        // Create device create info
        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.pNext = &dynamicRenderingFeatures;
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.pEnabledFeatures = &deviceFeatures;
        createInfo.enabledExtensionCount = static_cast<uint32_t>(m_EnabledExtensions.size());
        createInfo.ppEnabledExtensionNames = m_EnabledExtensions.data();

        // Enable validation layers on device (deprecated but still used for compatibility)
        if (instance->IsValidationEnabled())
        {
            createInfo.enabledLayerCount = static_cast<uint32_t>(s_ValidationLayers.size());
            createInfo.ppEnabledLayerNames = s_ValidationLayers.data();
        }
        else
        {
            createInfo.enabledLayerCount = 0;
        }

        // Create the logical device
        VkResult result = vkCreateDevice(m_PhysicalDevice, &createInfo, nullptr, &m_Device);
        if (result != VK_SUCCESS)
        {
            LOG_ERROR("Failed to create Vulkan logical device, VkResult: {}", static_cast<int>(result));
            return false;
        }

        LOG_INFO("Vulkan logical device created successfully");
        LOG_INFO("Dynamic rendering (Vulkan 1.3) enabled");

        // Retrieve queues
        vkGetDeviceQueue(m_Device, m_QueueFamilies.GraphicsFamily.value(), 0, &m_GraphicsQueue);
        vkGetDeviceQueue(m_Device, m_QueueFamilies.PresentFamily.value(), 0, &m_PresentQueue);

        if (m_QueueFamilies.ComputeFamily.has_value())
        {
            vkGetDeviceQueue(m_Device, m_QueueFamilies.ComputeFamily.value(), 0, &m_ComputeQueue);
        }

        if (m_QueueFamilies.TransferFamily.has_value())
        {
            vkGetDeviceQueue(m_Device, m_QueueFamilies.TransferFamily.value(), 0, &m_TransferQueue);
        }

        // Log queue info
        LOG_INFO("Device queues retrieved:");
        LOG_INFO("  Graphics Queue: Family {}", m_QueueFamilies.GraphicsFamily.value());
        LOG_INFO("  Present Queue: Family {}", m_QueueFamilies.PresentFamily.value());

        if (m_QueueFamilies.ComputeFamily.has_value())
        {
            LOG_INFO("  Compute Queue: Family {}", m_QueueFamilies.ComputeFamily.value());
        }

        if (m_QueueFamilies.TransferFamily.has_value())
        {
            LOG_INFO("  Transfer Queue: Family {}", m_QueueFamilies.TransferFamily.value());
        }

        // Log enabled extensions
#if RENDERER_DEBUG
        LOG_DEBUG("Enabled {} device extensions:", m_EnabledExtensions.size());
        for (const auto* extension : m_EnabledExtensions)
        {
            LOG_DEBUG("  - {}", extension);
        }
#endif

        // Initialize VMA
        if (!InitializeVMA(instance))
        {
            LOG_ERROR("Failed to initialize VMA allocator");
            vkDestroyDevice(m_Device, nullptr);
            m_Device = VK_NULL_HANDLE;
            return false;
        }

        return true;
    }

    std::vector<const char*> RHIDevice::GetRequiredExtensions()
    {
        return s_RequiredExtensions;
    }

    bool RHIDevice::CheckExtensionSupport(const Core::Ref<RHIPhysicalDevice>& physicalDevice)
    {
        for (const char* requiredExtension : s_RequiredExtensions)
        {
            if (!physicalDevice->IsExtensionSupported(requiredExtension))
            {
                LOG_WARN("Required device extension '{}' not supported", requiredExtension);
                return false;
            }
        }

        LOG_DEBUG("All required device extensions are supported");
        return true;
    }

    std::vector<VkDeviceQueueCreateInfo> RHIDevice::CreateQueueCreateInfos(const RHIDeviceConfig& config)
    {
        // Collect unique queue families
        std::set<uint32_t> uniqueQueueFamilies;

        uniqueQueueFamilies.insert(m_QueueFamilies.GraphicsFamily.value());
        uniqueQueueFamilies.insert(m_QueueFamilies.PresentFamily.value());

        if (m_QueueFamilies.ComputeFamily.has_value())
        {
            uniqueQueueFamilies.insert(m_QueueFamilies.ComputeFamily.value());
        }

        if (m_QueueFamilies.TransferFamily.has_value())
        {
            uniqueQueueFamilies.insert(m_QueueFamilies.TransferFamily.value());
        }

        // Store priority in member to ensure it remains valid
        static float queuePriority = 1.0f;
        queuePriority = config.QueuePriority;

        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        queueCreateInfos.reserve(uniqueQueueFamilies.size());

        for (uint32_t queueFamily : uniqueQueueFamilies)
        {
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        LOG_DEBUG("Creating {} unique queue families", queueCreateInfos.size());

        return queueCreateInfos;
    }

    bool RHIDevice::InitializeVMA(const Core::Ref<RHIInstance>& instance)
    {
        VmaAllocatorCreateInfo allocatorInfo{};
        allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;
        allocatorInfo.physicalDevice = m_PhysicalDevice;
        allocatorInfo.device = m_Device;
        allocatorInfo.instance = instance->GetHandle();

        VkResult result = vmaCreateAllocator(&allocatorInfo, &m_Allocator);
        if (result != VK_SUCCESS)
        {
            LOG_ERROR("Failed to create VMA allocator, VkResult: {}", static_cast<int>(result));
            return false;
        }

        LOG_INFO("VMA allocator initialized successfully");

        // Log VMA memory info
#if RENDERER_DEBUG
        VmaTotalStatistics stats;
        vmaCalculateStatistics(m_Allocator, &stats);
        LOG_DEBUG("VMA Statistics:");
        LOG_DEBUG("  Memory Blocks: {}", stats.total.statistics.blockCount);
        LOG_DEBUG("  Allocations: {}", stats.total.statistics.allocationCount);
#endif

        return true;
    }

    void RHIDevice::WaitIdle() const
    {
        ASSERT(m_Device != VK_NULL_HANDLE);
        vkDeviceWaitIdle(m_Device);
    }

} // namespace RHI
