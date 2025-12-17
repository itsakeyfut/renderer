#include "RHI/RHIPhysicalDevice.h"
#include "RHI/RHIInstance.h"
#include "Core/Assert.h"
#include "Core/Log.h"

#include <algorithm>
#include <cstring>
#include <map>

namespace RHI
{
    // =========================================================================
    // PhysicalDeviceInfo Implementation
    // =========================================================================

    VkDeviceSize PhysicalDeviceInfo::GetDeviceLocalMemorySize() const
    {
        VkDeviceSize totalSize = 0;

        for (uint32_t i = 0; i < MemoryProperties.memoryHeapCount; ++i)
        {
            const auto& heap = MemoryProperties.memoryHeaps[i];
            if (heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
            {
                totalSize += heap.size;
            }
        }

        return totalSize;
    }

    std::string PhysicalDeviceInfo::GetDeviceTypeString() const
    {
        switch (Properties.deviceType)
        {
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
                return "Discrete GPU";
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
                return "Integrated GPU";
            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
                return "Virtual GPU";
            case VK_PHYSICAL_DEVICE_TYPE_CPU:
                return "CPU";
            case VK_PHYSICAL_DEVICE_TYPE_OTHER:
            default:
                return "Other";
        }
    }

    // =========================================================================
    // RHIPhysicalDevice Implementation
    // =========================================================================

    Core::Ref<RHIPhysicalDevice> RHIPhysicalDevice::Select(
        const Core::Ref<RHIInstance>& instance,
        VkSurfaceKHR surface)
    {
        ASSERT(instance != nullptr);
        ASSERT(instance->GetHandle() != VK_NULL_HANDLE);

        // Enumerate all available devices
        auto devices = EnumerateDevices(instance, surface);

        if (devices.empty())
        {
            LOG_ERROR("No Vulkan-capable GPUs found");
            return nullptr;
        }

        // Filter to suitable devices and rate them
        std::multimap<int32_t, PhysicalDeviceInfo, std::greater<int32_t>> rankedDevices;

        for (const auto& deviceInfo : devices)
        {
            if (deviceInfo.QueueFamilies.IsComplete())
            {
                int32_t score = RateDevice(deviceInfo);
                rankedDevices.emplace(score, deviceInfo);
            }
        }

        if (rankedDevices.empty())
        {
            LOG_ERROR("No suitable GPU found (require Graphics and Present queue support)");
            return nullptr;
        }

        // Select the highest-rated device
        const auto& selectedInfo = rankedDevices.begin()->second;

        auto physicalDevice = Core::Ref<RHIPhysicalDevice>(new RHIPhysicalDevice());
        if (!physicalDevice->Initialize(selectedInfo.Device, surface))
        {
            LOG_ERROR("Failed to initialize physical device");
            return nullptr;
        }

        // Log selection result
        LOG_INFO("Selected GPU: {} ({})",
            physicalDevice->GetInfo().GetDeviceName(),
            physicalDevice->GetInfo().GetDeviceTypeString());

        LOG_INFO("  Vulkan API: {}.{}.{}",
            VK_VERSION_MAJOR(physicalDevice->GetProperties().apiVersion),
            VK_VERSION_MINOR(physicalDevice->GetProperties().apiVersion),
            VK_VERSION_PATCH(physicalDevice->GetProperties().apiVersion));

        LOG_INFO("  Driver Version: {}.{}.{}",
            VK_VERSION_MAJOR(physicalDevice->GetProperties().driverVersion),
            VK_VERSION_MINOR(physicalDevice->GetProperties().driverVersion),
            VK_VERSION_PATCH(physicalDevice->GetProperties().driverVersion));

        VkDeviceSize vramSize = physicalDevice->GetInfo().GetDeviceLocalMemorySize();
        LOG_INFO("  VRAM: {} MB", vramSize / (1024 * 1024));

        // Log queue families
        const auto& qf = physicalDevice->GetQueueFamilies();
        LOG_DEBUG("Queue Families:");
        if (qf.GraphicsFamily.has_value())
            LOG_DEBUG("  Graphics: {}", qf.GraphicsFamily.value());
        if (qf.PresentFamily.has_value())
            LOG_DEBUG("  Present: {}", qf.PresentFamily.value());
        if (qf.ComputeFamily.has_value())
            LOG_DEBUG("  Compute: {}", qf.ComputeFamily.value());
        if (qf.TransferFamily.has_value())
            LOG_DEBUG("  Transfer: {}", qf.TransferFamily.value());

        if (qf.HasDedicatedTransferQueue())
        {
            LOG_DEBUG("  (Dedicated transfer queue available for async uploads)");
        }

        return physicalDevice;
    }

    std::vector<PhysicalDeviceInfo> RHIPhysicalDevice::EnumerateDevices(
        const Core::Ref<RHIInstance>& instance,
        VkSurfaceKHR surface)
    {
        ASSERT(instance != nullptr);
        ASSERT(instance->GetHandle() != VK_NULL_HANDLE);

        VkInstance vkInstance = instance->GetHandle();

        // Get device count
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(vkInstance, &deviceCount, nullptr);

        if (deviceCount == 0)
        {
            LOG_WARN("No physical devices found with Vulkan support");
            return {};
        }

        // Get devices
        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(vkInstance, &deviceCount, devices.data());

        LOG_DEBUG("Found {} Vulkan-capable GPU(s):", deviceCount);

        std::vector<PhysicalDeviceInfo> deviceInfos;
        deviceInfos.reserve(deviceCount);

        for (VkPhysicalDevice device : devices)
        {
            PhysicalDeviceInfo info;
            info.Device = device;

            // Get device properties
            vkGetPhysicalDeviceProperties(device, &info.Properties);

            // Get device features
            vkGetPhysicalDeviceFeatures(device, &info.Features);

            // Get memory properties
            vkGetPhysicalDeviceMemoryProperties(device, &info.MemoryProperties);

            // Find queue families
            info.QueueFamilies = FindQueueFamilies(device, surface);

            LOG_DEBUG("  [{}] {} - {}",
                deviceInfos.size(),
                info.GetDeviceName(),
                info.GetDeviceTypeString());

            deviceInfos.push_back(info);
        }

        return deviceInfos;
    }

    bool RHIPhysicalDevice::Initialize(VkPhysicalDevice device, VkSurfaceKHR surface)
    {
        ASSERT(device != VK_NULL_HANDLE);

        m_DeviceInfo.Device = device;

        // Get device properties
        vkGetPhysicalDeviceProperties(device, &m_DeviceInfo.Properties);

        // Get device features
        vkGetPhysicalDeviceFeatures(device, &m_DeviceInfo.Features);

        // Get memory properties
        vkGetPhysicalDeviceMemoryProperties(device, &m_DeviceInfo.MemoryProperties);

        // Find queue families
        m_DeviceInfo.QueueFamilies = FindQueueFamilies(device, surface);

        // Query supported extensions
        QuerySupportedExtensions();

        // Verify minimum requirements
        if (!m_DeviceInfo.QueueFamilies.IsComplete())
        {
            LOG_ERROR("Device {} does not have required queue families",
                m_DeviceInfo.GetDeviceName());
            return false;
        }

        return true;
    }

    QueueFamilyIndices RHIPhysicalDevice::FindQueueFamilies(
        VkPhysicalDevice device,
        VkSurfaceKHR surface)
    {
        QueueFamilyIndices indices;

        // Get queue family count
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        // Get queue family properties
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        // Track dedicated queues for better selection
        std::optional<uint32_t> dedicatedComputeFamily;
        std::optional<uint32_t> dedicatedTransferFamily;

        for (uint32_t i = 0; i < queueFamilyCount; ++i)
        {
            const auto& queueFamily = queueFamilies[i];

            // Check for Graphics support
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                indices.GraphicsFamily = i;
            }

            // Check for Present support
            if (surface != VK_NULL_HANDLE)
            {
                VkBool32 presentSupport = VK_FALSE;
                vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
                if (presentSupport)
                {
                    indices.PresentFamily = i;
                }
            }

            // Check for Compute support
            if (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT)
            {
                // Prefer dedicated compute queue (no graphics)
                if (!(queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT))
                {
                    dedicatedComputeFamily = i;
                }
                else if (!indices.ComputeFamily.has_value())
                {
                    indices.ComputeFamily = i;
                }
            }

            // Check for Transfer support
            if (queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT)
            {
                // Prefer dedicated transfer queue (no graphics/compute)
                if (!(queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
                    !(queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT))
                {
                    dedicatedTransferFamily = i;
                }
                else if (!indices.TransferFamily.has_value())
                {
                    indices.TransferFamily = i;
                }
            }
        }

        // Use dedicated queues if available
        if (dedicatedComputeFamily.has_value())
        {
            indices.ComputeFamily = dedicatedComputeFamily;
        }

        if (dedicatedTransferFamily.has_value())
        {
            indices.TransferFamily = dedicatedTransferFamily;
        }

        return indices;
    }

    bool RHIPhysicalDevice::IsDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface)
    {
        QueueFamilyIndices indices = FindQueueFamilies(device, surface);
        return indices.IsComplete();
    }

    int32_t RHIPhysicalDevice::RateDevice(const PhysicalDeviceInfo& info)
    {
        int32_t score = 0;

        // Discrete GPUs have a significant performance advantage
        if (info.IsDiscreteGPU())
        {
            score += 10000;
        }
        else if (info.IsIntegratedGPU())
        {
            score += 1000;
        }

        // Maximum possible size of textures affects graphics quality
        score += static_cast<int32_t>(info.Properties.limits.maxImageDimension2D);

        // VRAM size (bonus per GB)
        VkDeviceSize vramMB = info.GetDeviceLocalMemorySize() / (1024 * 1024);
        score += static_cast<int32_t>(vramMB / 100);

        // Bonus for dedicated transfer queue (enables async uploads)
        if (info.QueueFamilies.HasDedicatedTransferQueue())
        {
            score += 500;
        }

        // Bonus for compute queue
        if (info.QueueFamilies.ComputeFamily.has_value())
        {
            score += 200;
        }

        // Bonus for geometry shader support
        if (info.Features.geometryShader)
        {
            score += 100;
        }

        // Bonus for tessellation shader support
        if (info.Features.tessellationShader)
        {
            score += 100;
        }

        // Bonus for wide lines support
        if (info.Features.wideLines)
        {
            score += 50;
        }

        // Bonus for anisotropic filtering
        if (info.Features.samplerAnisotropy)
        {
            score += 100;
        }

        return score;
    }

    void RHIPhysicalDevice::QuerySupportedExtensions()
    {
        uint32_t extensionCount = 0;
        vkEnumerateDeviceExtensionProperties(m_DeviceInfo.Device, nullptr, &extensionCount, nullptr);

        m_SupportedExtensions.resize(extensionCount);
        vkEnumerateDeviceExtensionProperties(
            m_DeviceInfo.Device,
            nullptr,
            &extensionCount,
            m_SupportedExtensions.data());

#if RENDERER_DEBUG
        LOG_DEBUG("Device supports {} extensions", extensionCount);
#endif
    }

    bool RHIPhysicalDevice::IsExtensionSupported(const char* extensionName) const
    {
        for (const auto& extension : m_SupportedExtensions)
        {
            if (std::strcmp(extension.extensionName, extensionName) == 0)
            {
                return true;
            }
        }
        return false;
    }

} // namespace RHI
