/**
 * @file RHIInstance.h
 * @brief Vulkan Instance wrapper for the RHI layer.
 *
 * Manages VkInstance creation with validation layers and debug messenger
 * for development builds.
 */

#pragma once

#include "Core/Types.h"

#include <vulkan/vulkan.h>

#include <string>
#include <vector>

namespace RHI
{
    /**
     * @brief Configuration for RHI Instance creation
     */
    struct RHIInstanceConfig
    {
        std::string ApplicationName = "Vulkan Application";
        uint32_t ApplicationVersion = VK_MAKE_VERSION(1, 0, 0);
        std::string EngineName = "Vulkan Renderer";
        uint32_t EngineVersion = VK_MAKE_VERSION(0, 1, 0);
        uint32_t ApiVersion = VK_API_VERSION_1_3;
        bool EnableValidation = true;
    };

    /**
     * @brief Vulkan Instance wrapper class
     *
     * Manages the VkInstance lifecycle including:
     * - Instance creation with required extensions
     * - Validation layer setup (debug builds)
     * - Debug messenger for Vulkan error reporting
     *
     * This class follows RAII principles - resources are cleaned up
     * when the RHIInstance object is destroyed.
     */
    class RHIInstance
    {
    public:
        /**
         * @brief Factory method to create an RHI Instance
         * @param config Instance configuration parameters
         * @return Shared pointer to the created instance, or nullptr on failure
         */
        static Core::Ref<RHIInstance> Create(const RHIInstanceConfig& config = {});

        /**
         * @brief Destructor - destroys VkInstance and debug messenger
         */
        ~RHIInstance();

        // Non-copyable
        RHIInstance(const RHIInstance&) = delete;
        RHIInstance& operator=(const RHIInstance&) = delete;

        // Non-movable (VkInstance handles cannot be safely moved)
        RHIInstance(RHIInstance&&) = delete;
        RHIInstance& operator=(RHIInstance&&) = delete;

        /**
         * @brief Get the native VkInstance handle
         * @return VkInstance handle
         */
        VkInstance GetHandle() const { return m_Instance; }

        /**
         * @brief Check if validation layers are enabled
         * @return true if validation is enabled
         */
        bool IsValidationEnabled() const { return m_ValidationEnabled; }

        /**
         * @brief Get the list of enabled instance extensions
         * @return Vector of enabled extension names
         */
        const std::vector<const char*>& GetEnabledExtensions() const { return m_EnabledExtensions; }

    private:
        /**
         * @brief Private constructor - use Create() factory method
         */
        RHIInstance() = default;

        /**
         * @brief Initialize the Vulkan instance
         * @param config Instance configuration
         * @return true on success, false on failure
         */
        bool Initialize(const RHIInstanceConfig& config);

        /**
         * @brief Check if required validation layers are available
         * @return true if all required validation layers are supported
         */
        bool CheckValidationLayerSupport();

        /**
         * @brief Get list of required instance extensions
         * @return Vector of required extension names
         */
        std::vector<const char*> GetRequiredExtensions();

        /**
         * @brief Setup the debug messenger for validation messages
         */
        void SetupDebugMessenger();

        /**
         * @brief Vulkan debug callback function
         */
        static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
            VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
            VkDebugUtilsMessageTypeFlagsEXT messageType,
            const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
            void* pUserData);

        /**
         * @brief Populate debug messenger create info
         * @param createInfo Structure to populate
         */
        static void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);

        VkInstance m_Instance = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT m_DebugMessenger = VK_NULL_HANDLE;
        bool m_ValidationEnabled = false;
        std::vector<const char*> m_EnabledExtensions;
    };

} // namespace RHI
