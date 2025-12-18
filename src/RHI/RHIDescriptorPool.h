/**
 * @file RHIDescriptorPool.h
 * @brief Vulkan Descriptor Pool wrapper for the RHI layer.
 *
 * Manages VkDescriptorPool creation and descriptor set allocation.
 * Descriptor pools are used to allocate descriptor sets which bind
 * resources (buffers, images, samplers) to shaders.
 */

#pragma once

#include "Core/Types.h"
#include "RHI/RHIDevice.h"

#include <vulkan/vulkan.h>
#include <vector>

namespace RHI
{
    // Forward declarations
    class RHIDescriptorSetLayout;

    /**
     * @brief Descriptor pool size entry.
     *
     * Specifies how many descriptors of a given type can be allocated from the pool.
     */
    struct DescriptorPoolSize
    {
        /**
         * @brief Type of descriptor.
         */
        VkDescriptorType Type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

        /**
         * @brief Maximum number of descriptors of this type.
         */
        uint32_t Count = 1;
    };

    /**
     * @brief Configuration for descriptor pool creation.
     */
    struct DescriptorPoolDesc
    {
        /**
         * @brief Maximum number of descriptor sets that can be allocated.
         */
        uint32_t MaxSets = 1;

        /**
         * @brief Pool sizes for each descriptor type.
         */
        std::vector<DescriptorPoolSize> PoolSizes;

        /**
         * @brief Pool creation flags (e.g., VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT).
         */
        VkDescriptorPoolCreateFlags Flags = 0;

        /**
         * @brief Debug name for the pool (optional).
         */
        const char* DebugName = nullptr;
    };

    /**
     * @brief Vulkan Descriptor Pool wrapper class.
     *
     * Manages VkDescriptorPool lifecycle and descriptor set allocation.
     * Descriptor pools are created with a fixed capacity and can allocate
     * descriptor sets up to that limit.
     *
     * Usage:
     * @code
     * DescriptorPoolDesc poolDesc;
     * poolDesc.MaxSets = 100;
     * poolDesc.PoolSizes = {
     *     {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 100},
     *     {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100}
     * };
     *
     * auto pool = RHIDescriptorPool::Create(device, poolDesc);
     *
     * // Allocate descriptor sets
     * VkDescriptorSet set = pool->Allocate(layout->GetHandle());
     * @endcode
     */
    class RHIDescriptorPool
    {
    public:
        /**
         * @brief Factory method to create a descriptor pool.
         *
         * @param device The logical device.
         * @param desc Pool description.
         * @return Shared pointer to the created pool, or nullptr on failure.
         */
        static Core::Ref<RHIDescriptorPool> Create(
            const Core::Ref<RHIDevice>& device,
            const DescriptorPoolDesc& desc);

        /**
         * @brief Convenience factory method with parameters.
         *
         * @param device The logical device.
         * @param maxSets Maximum number of descriptor sets.
         * @param poolSizes Pool sizes for each descriptor type.
         * @param flags Pool creation flags.
         * @return Shared pointer to the created pool, or nullptr on failure.
         */
        static Core::Ref<RHIDescriptorPool> Create(
            const Core::Ref<RHIDevice>& device,
            uint32_t maxSets,
            const std::vector<DescriptorPoolSize>& poolSizes,
            VkDescriptorPoolCreateFlags flags = 0);

        /**
         * @brief Destructor - destroys VkDescriptorPool.
         */
        ~RHIDescriptorPool();

        // Non-copyable
        RHIDescriptorPool(const RHIDescriptorPool&) = delete;
        RHIDescriptorPool& operator=(const RHIDescriptorPool&) = delete;

        // Non-movable
        RHIDescriptorPool(RHIDescriptorPool&&) = delete;
        RHIDescriptorPool& operator=(RHIDescriptorPool&&) = delete;

        /**
         * @brief Get the native VkDescriptorPool handle.
         * @return VkDescriptorPool handle.
         */
        VkDescriptorPool GetHandle() const { return m_Pool; }

        /**
         * @brief Allocate a single descriptor set.
         *
         * @param layout The descriptor set layout.
         * @return Allocated VkDescriptorSet, or VK_NULL_HANDLE on failure.
         */
        VkDescriptorSet Allocate(VkDescriptorSetLayout layout);

        /**
         * @brief Allocate a single descriptor set using RHIDescriptorSetLayout.
         *
         * @param layout The descriptor set layout wrapper.
         * @return Allocated VkDescriptorSet, or VK_NULL_HANDLE on failure.
         */
        VkDescriptorSet Allocate(const Core::Ref<RHIDescriptorSetLayout>& layout);

        /**
         * @brief Allocate multiple descriptor sets with the same layout.
         *
         * @param layout The descriptor set layout.
         * @param count Number of sets to allocate.
         * @return Vector of allocated descriptor sets, or empty vector on failure.
         */
        std::vector<VkDescriptorSet> AllocateMultiple(VkDescriptorSetLayout layout, uint32_t count);

        /**
         * @brief Allocate multiple descriptor sets with different layouts.
         *
         * @param layouts Vector of descriptor set layouts.
         * @return Vector of allocated descriptor sets, or empty vector on failure.
         */
        std::vector<VkDescriptorSet> AllocateMultiple(const std::vector<VkDescriptorSetLayout>& layouts);

        /**
         * @brief Free descriptor sets back to the pool.
         *
         * Only valid if the pool was created with VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT.
         *
         * @param sets Descriptor sets to free.
         * @return true on success, false on failure.
         */
        bool Free(const std::vector<VkDescriptorSet>& sets);

        /**
         * @brief Reset the pool, freeing all allocated descriptor sets.
         *
         * All descriptor sets become invalid after reset.
         *
         * @return true on success, false on failure.
         */
        bool Reset();

        /**
         * @brief Get the maximum number of sets this pool can allocate.
         * @return Maximum set count.
         */
        uint32_t GetMaxSets() const { return m_MaxSets; }

    private:
        /**
         * @brief Private constructor - use Create() factory method.
         */
        RHIDescriptorPool() = default;

        /**
         * @brief Initialize the descriptor pool.
         *
         * @param device The logical device.
         * @param desc Pool description.
         * @return true on success, false on failure.
         */
        bool Initialize(
            const Core::Ref<RHIDevice>& device,
            const DescriptorPoolDesc& desc);

        VkDevice m_Device = VK_NULL_HANDLE;
        VkDescriptorPool m_Pool = VK_NULL_HANDLE;
        uint32_t m_MaxSets = 0;
        VkDescriptorPoolCreateFlags m_Flags = 0;
    };

} // namespace RHI
