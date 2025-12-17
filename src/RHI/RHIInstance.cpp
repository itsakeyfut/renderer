#include "RHI/RHIInstance.h"
#include "Core/Log.h"
#include "Core/Assert.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <cstring>

namespace RHI
{
    // Validation layer names
    static const std::vector<const char*> s_ValidationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };

    // Extension function pointers (loaded dynamically)
    static PFN_vkCreateDebugUtilsMessengerEXT s_CreateDebugUtilsMessengerEXT = nullptr;
    static PFN_vkDestroyDebugUtilsMessengerEXT s_DestroyDebugUtilsMessengerEXT = nullptr;

    Core::Ref<RHIInstance> RHIInstance::Create(const RHIInstanceConfig& config)
    {
        auto instance = Core::Ref<RHIInstance>(new RHIInstance());

        if (!instance->Initialize(config))
        {
            LOG_ERROR("Failed to initialize RHI Instance");
            return nullptr;
        }

        return instance;
    }

    RHIInstance::~RHIInstance()
    {
        if (m_DebugMessenger != VK_NULL_HANDLE && s_DestroyDebugUtilsMessengerEXT)
        {
            s_DestroyDebugUtilsMessengerEXT(m_Instance, m_DebugMessenger, nullptr);
            m_DebugMessenger = VK_NULL_HANDLE;
            LOG_DEBUG("Debug messenger destroyed");
        }

        if (m_Instance != VK_NULL_HANDLE)
        {
            vkDestroyInstance(m_Instance, nullptr);
            m_Instance = VK_NULL_HANDLE;
            LOG_DEBUG("Vulkan instance destroyed");
        }
    }

    bool RHIInstance::Initialize(const RHIInstanceConfig& config)
    {
        // Determine if validation should be enabled
        // Only enable in debug builds if requested
#if RENDERER_DEBUG
        m_ValidationEnabled = config.EnableValidation;
#else
        m_ValidationEnabled = false;
        if (config.EnableValidation)
        {
            LOG_WARN("Validation layers requested but disabled in release build");
        }
#endif

        // Check validation layer support if enabled
        if (m_ValidationEnabled && !CheckValidationLayerSupport())
        {
            LOG_WARN("Validation layers requested but not available, disabling validation");
            m_ValidationEnabled = false;
        }

        // Setup application info
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = config.ApplicationName.c_str();
        appInfo.applicationVersion = config.ApplicationVersion;
        appInfo.pEngineName = config.EngineName.c_str();
        appInfo.engineVersion = config.EngineVersion;
        appInfo.apiVersion = config.ApiVersion;

        // Get required extensions
        m_EnabledExtensions = GetRequiredExtensions();

        // Log enabled extensions
#if RENDERER_DEBUG
        LOG_DEBUG("Enabling {} Vulkan instance extensions:", m_EnabledExtensions.size());
        for (const auto* extension : m_EnabledExtensions)
        {
            LOG_DEBUG("  - {}", extension);
        }
#endif

        // Setup instance create info
        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledExtensionCount = static_cast<uint32_t>(m_EnabledExtensions.size());
        createInfo.ppEnabledExtensionNames = m_EnabledExtensions.data();

        // Setup debug messenger create info for instance creation/destruction debugging
        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};

        if (m_ValidationEnabled)
        {
            createInfo.enabledLayerCount = static_cast<uint32_t>(s_ValidationLayers.size());
            createInfo.ppEnabledLayerNames = s_ValidationLayers.data();

            // Enable debug messenger for vkCreateInstance and vkDestroyInstance
            PopulateDebugMessengerCreateInfo(debugCreateInfo);
            createInfo.pNext = &debugCreateInfo;

#if RENDERER_DEBUG
            LOG_DEBUG("Enabling {} validation layers:", s_ValidationLayers.size());
            for (const auto* layer : s_ValidationLayers)
            {
                LOG_DEBUG("  - {}", layer);
            }
#endif
        }
        else
        {
            createInfo.enabledLayerCount = 0;
            createInfo.pNext = nullptr;
        }

        // Create the Vulkan instance
        VkResult result = vkCreateInstance(&createInfo, nullptr, &m_Instance);
        if (result != VK_SUCCESS)
        {
            LOG_ERROR("Failed to create Vulkan instance, VkResult: {}", static_cast<int>(result));
            return false;
        }

        LOG_INFO("Vulkan instance created successfully");
        LOG_INFO("  Application: {} (v{}.{}.{})",
            config.ApplicationName,
            VK_VERSION_MAJOR(config.ApplicationVersion),
            VK_VERSION_MINOR(config.ApplicationVersion),
            VK_VERSION_PATCH(config.ApplicationVersion));
        LOG_INFO("  Engine: {} (v{}.{}.{})",
            config.EngineName,
            VK_VERSION_MAJOR(config.EngineVersion),
            VK_VERSION_MINOR(config.EngineVersion),
            VK_VERSION_PATCH(config.EngineVersion));
        LOG_INFO("  Vulkan API: {}.{}.{}",
            VK_VERSION_MAJOR(config.ApiVersion),
            VK_VERSION_MINOR(config.ApiVersion),
            VK_VERSION_PATCH(config.ApiVersion));

        // Setup debug messenger if validation is enabled
        if (m_ValidationEnabled)
        {
            SetupDebugMessenger();
        }

        return true;
    }

    bool RHIInstance::CheckValidationLayerSupport()
    {
        uint32_t layerCount = 0;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        // Check if all required validation layers are available
        for (const char* layerName : s_ValidationLayers)
        {
            bool layerFound = false;

            for (const auto& layerProperties : availableLayers)
            {
                if (std::strcmp(layerName, layerProperties.layerName) == 0)
                {
                    layerFound = true;
                    break;
                }
            }

            if (!layerFound)
            {
                LOG_WARN("Validation layer '{}' not found", layerName);
                return false;
            }
        }

        LOG_DEBUG("All required validation layers are available");
        return true;
    }

    std::vector<const char*> RHIInstance::GetRequiredExtensions()
    {
        // Get GLFW required extensions for Vulkan surface creation
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        // Add debug utils extension for validation layer messaging
        if (m_ValidationEnabled)
        {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        return extensions;
    }

    void RHIInstance::SetupDebugMessenger()
    {
        ASSERT(m_ValidationEnabled);
        ASSERT(m_Instance != VK_NULL_HANDLE);

        // Load extension function pointers
        s_CreateDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(m_Instance, "vkCreateDebugUtilsMessengerEXT"));
        s_DestroyDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(m_Instance, "vkDestroyDebugUtilsMessengerEXT"));

        if (!s_CreateDebugUtilsMessengerEXT)
        {
            LOG_ERROR("Failed to load vkCreateDebugUtilsMessengerEXT function");
            return;
        }

        VkDebugUtilsMessengerCreateInfoEXT createInfo{};
        PopulateDebugMessengerCreateInfo(createInfo);

        VkResult result = s_CreateDebugUtilsMessengerEXT(m_Instance, &createInfo, nullptr, &m_DebugMessenger);
        if (result != VK_SUCCESS)
        {
            LOG_ERROR("Failed to create debug messenger, VkResult: {}", static_cast<int>(result));
            return;
        }

        LOG_INFO("Vulkan debug messenger created");
    }

    void RHIInstance::PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
    {
        createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;

        // Message severity levels to capture
        createInfo.messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

        // Message types to capture
        createInfo.messageType =
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

        createInfo.pfnUserCallback = DebugCallback;
        createInfo.pUserData = nullptr;
    }

    VKAPI_ATTR VkBool32 VKAPI_CALL RHIInstance::DebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        [[maybe_unused]] VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        [[maybe_unused]] void* pUserData)
    {
        // Route Vulkan validation messages to our logging system
        switch (messageSeverity)
        {
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
                LOG_TRACE("[Vulkan] {}", pCallbackData->pMessage);
                break;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
                LOG_DEBUG("[Vulkan] {}", pCallbackData->pMessage);
                break;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
                LOG_WARN("[Vulkan] {}", pCallbackData->pMessage);
                break;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
                LOG_ERROR("[Vulkan] {}", pCallbackData->pMessage);
                break;
            default:
                LOG_DEBUG("[Vulkan] {}", pCallbackData->pMessage);
                break;
        }

        // Return VK_FALSE to indicate the call should not be aborted
        return VK_FALSE;
    }

} // namespace RHI
