/**
 * @file RHIDescriptorSetLayout.h
 * @brief Vulkan Descriptor Set Layout wrapper for the RHI layer.
 *
 * Manages VkDescriptorSetLayout creation and lifetime. Descriptor set layouts
 * define the structure of descriptor sets, specifying what types of resources
 * (buffers, images, samplers) can be bound and at which binding points.
 */

#pragma once

#include "Core/Types.h"
#include "RHI/RHIDevice.h"

#include <vulkan/vulkan.h>
#include <vector>

namespace RHI
{
    /**
     * @brief Descriptor binding configuration.
     *
     * Describes a single binding point within a descriptor set layout.
     */
    struct DescriptorBinding
    {
        /**
         * @brief Binding number in the shader (e.g., layout(binding = 0)).
         */
        uint32_t Binding = 0;

        /**
         * @brief Type of descriptor (uniform buffer, sampler, etc.).
         */
        VkDescriptorType Type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

        /**
         * @brief Number of descriptors in this binding (for arrays).
         */
        uint32_t Count = 1;

        /**
         * @brief Shader stages that access this binding.
         */
        VkShaderStageFlags Stages = VK_SHADER_STAGE_ALL;

        /**
         * @brief Immutable samplers (optional, nullptr for no immutable samplers).
         */
        const VkSampler* ImmutableSamplers = nullptr;
    };

    /**
     * @brief Configuration for descriptor set layout creation.
     */
    struct DescriptorSetLayoutDesc
    {
        /**
         * @brief Array of descriptor bindings.
         */
        std::vector<DescriptorBinding> Bindings;

        /**
         * @brief Layout creation flags (e.g., VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT).
         */
        VkDescriptorSetLayoutCreateFlags Flags = 0;

        /**
         * @brief Debug name for the layout (optional).
         */
        const char* DebugName = nullptr;
    };

    /**
     * @brief Vulkan Descriptor Set Layout wrapper class.
     *
     * Manages VkDescriptorSetLayout lifecycle. A descriptor set layout defines
     * the interface between shaders and resources, specifying what types of
     * descriptors can be bound at each binding point.
     *
     * Usage:
     * @code
     * DescriptorSetLayoutDesc desc;
     * desc.Bindings = {
     *     {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT},
     *     {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT}
     * };
     *
     * auto layout = RHIDescriptorSetLayout::Create(device, desc);
     * // Use in pipeline creation
     * pipelineDesc.DescriptorSetLayouts = { layout->GetHandle() };
     * @endcode
     */
    class RHIDescriptorSetLayout
    {
    public:
        /**
         * @brief Factory method to create a descriptor set layout.
         *
         * @param device The logical device.
         * @param desc Layout description with bindings.
         * @return Shared pointer to the created layout, or nullptr on failure.
         */
        static Core::Ref<RHIDescriptorSetLayout> Create(
            const Core::Ref<RHIDevice>& device,
            const DescriptorSetLayoutDesc& desc);

        /**
         * @brief Convenience factory method with binding vector.
         *
         * @param device The logical device.
         * @param bindings Vector of descriptor bindings.
         * @return Shared pointer to the created layout, or nullptr on failure.
         */
        static Core::Ref<RHIDescriptorSetLayout> Create(
            const Core::Ref<RHIDevice>& device,
            const std::vector<DescriptorBinding>& bindings);

        /**
         * @brief Destructor - destroys VkDescriptorSetLayout.
         */
        ~RHIDescriptorSetLayout();

        // Non-copyable
        RHIDescriptorSetLayout(const RHIDescriptorSetLayout&) = delete;
        RHIDescriptorSetLayout& operator=(const RHIDescriptorSetLayout&) = delete;

        // Non-movable
        RHIDescriptorSetLayout(RHIDescriptorSetLayout&&) = delete;
        RHIDescriptorSetLayout& operator=(RHIDescriptorSetLayout&&) = delete;

        /**
         * @brief Get the native VkDescriptorSetLayout handle.
         * @return VkDescriptorSetLayout handle.
         */
        VkDescriptorSetLayout GetHandle() const { return m_Layout; }

        /**
         * @brief Get the bindings that define this layout.
         * @return Const reference to the binding vector.
         */
        const std::vector<DescriptorBinding>& GetBindings() const { return m_Bindings; }

        /**
         * @brief Get the number of bindings in this layout.
         * @return Number of bindings.
         */
        uint32_t GetBindingCount() const { return static_cast<uint32_t>(m_Bindings.size()); }

    private:
        /**
         * @brief Private constructor - use Create() factory method.
         */
        RHIDescriptorSetLayout() = default;

        /**
         * @brief Initialize the descriptor set layout.
         *
         * @param device The logical device.
         * @param desc Layout description.
         * @return true on success, false on failure.
         */
        bool Initialize(
            const Core::Ref<RHIDevice>& device,
            const DescriptorSetLayoutDesc& desc);

        VkDevice m_Device = VK_NULL_HANDLE;
        VkDescriptorSetLayout m_Layout = VK_NULL_HANDLE;
        std::vector<DescriptorBinding> m_Bindings;
    };

} // namespace RHI
