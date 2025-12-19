/**
 * @file RHIDescriptorSet.h
 * @brief Vulkan Descriptor Set wrapper for the RHI layer.
 *
 * Manages VkDescriptorSet updates and provides a high-level interface
 * for binding resources to shaders. Descriptor sets are allocated from
 * pools and updated to reference actual buffer/image resources.
 */

#pragma once

#include "Core/Types.h"
#include "RHI/RHIDevice.h"
#include "RHI/RHIDescriptorPool.h"
#include "RHI/RHIDescriptorSetLayout.h"

#include <vulkan/vulkan.h>
#include <vector>
#include <variant>

namespace RHI
{
    // Forward declarations
    class RHIBuffer;

    /**
     * @brief Descriptor write information for a buffer binding.
     */
    struct DescriptorBufferWrite
    {
        /**
         * @brief Binding number in the descriptor set.
         */
        uint32_t Binding = 0;

        /**
         * @brief Type of descriptor (uniform buffer, storage buffer, etc.).
         */
        VkDescriptorType Type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

        /**
         * @brief Buffer handle.
         */
        VkBuffer Buffer = VK_NULL_HANDLE;

        /**
         * @brief Offset into the buffer.
         */
        VkDeviceSize Offset = 0;

        /**
         * @brief Size of the buffer range (VK_WHOLE_SIZE for entire buffer).
         */
        VkDeviceSize Range = VK_WHOLE_SIZE;

        /**
         * @brief Array element (for descriptor arrays).
         */
        uint32_t ArrayElement = 0;
    };

    /**
     * @brief Descriptor write information for an image binding.
     */
    struct DescriptorImageWrite
    {
        /**
         * @brief Binding number in the descriptor set.
         */
        uint32_t Binding = 0;

        /**
         * @brief Type of descriptor (sampler, sampled image, combined image sampler, etc.).
         */
        VkDescriptorType Type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

        /**
         * @brief Image view handle.
         */
        VkImageView ImageView = VK_NULL_HANDLE;

        /**
         * @brief Sampler handle (for samplers and combined image samplers).
         */
        VkSampler Sampler = VK_NULL_HANDLE;

        /**
         * @brief Image layout.
         */
        VkImageLayout ImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        /**
         * @brief Array element (for descriptor arrays).
         */
        uint32_t ArrayElement = 0;
    };

    /**
     * @brief Vulkan Descriptor Set wrapper class.
     *
     * Provides a high-level interface for managing descriptor sets.
     * Handles allocation, updates, and binding of resources to shaders.
     *
     * Usage:
     * @code
     * // Create descriptor set
     * auto descriptorSet = RHIDescriptorSet::Create(device, pool, layout);
     *
     * // Update with uniform buffer
     * descriptorSet->UpdateBuffer(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
     *                             uniformBuffer->GetHandle(), 0, sizeof(CameraUBO));
     *
     * // Bind in command buffer
     * commandBuffer->BindDescriptorSets(
     *     VK_PIPELINE_BIND_POINT_GRAPHICS,
     *     pipeline->GetLayout(),
     *     0,
     *     {descriptorSet->GetHandle()});
     * @endcode
     */
    class RHIDescriptorSet
    {
    public:
        /**
         * @brief Factory method to create a descriptor set.
         *
         * Allocates a descriptor set from the pool with the specified layout.
         *
         * @param device The logical device.
         * @param pool The descriptor pool to allocate from.
         * @param layout The descriptor set layout.
         * @return Shared pointer to the created descriptor set, or nullptr on failure.
         */
        static Core::Ref<RHIDescriptorSet> Create(
            const Core::Ref<RHIDevice>& device,
            const Core::Ref<RHIDescriptorPool>& pool,
            const Core::Ref<RHIDescriptorSetLayout>& layout);

        /**
         * @brief Factory method to create a descriptor set from raw handle.
         *
         * Wraps an existing VkDescriptorSet handle.
         *
         * @param device The logical device.
         * @param descriptorSet The existing descriptor set handle.
         * @return Shared pointer to the wrapper, or nullptr on failure.
         */
        static Core::Ref<RHIDescriptorSet> CreateFromHandle(
            const Core::Ref<RHIDevice>& device,
            VkDescriptorSet descriptorSet);

        /**
         * @brief Destructor.
         *
         * Note: Descriptor sets are freed when the pool is destroyed or reset.
         */
        ~RHIDescriptorSet() = default;

        // Non-copyable
        RHIDescriptorSet(const RHIDescriptorSet&) = delete;
        RHIDescriptorSet& operator=(const RHIDescriptorSet&) = delete;

        // Non-movable
        RHIDescriptorSet(RHIDescriptorSet&&) = delete;
        RHIDescriptorSet& operator=(RHIDescriptorSet&&) = delete;

        /**
         * @brief Get the native VkDescriptorSet handle.
         * @return VkDescriptorSet handle.
         */
        VkDescriptorSet GetHandle() const { return m_DescriptorSet; }

        /**
         * @brief Check if the descriptor set is valid.
         * @return true if the handle is valid.
         */
        bool IsValid() const { return m_DescriptorSet != VK_NULL_HANDLE; }

        /**
         * @brief Update a single buffer descriptor.
         *
         * @param binding Binding number.
         * @param type Descriptor type.
         * @param buffer Buffer handle.
         * @param offset Offset into buffer.
         * @param range Size of buffer range.
         */
        void UpdateBuffer(
            uint32_t binding,
            VkDescriptorType type,
            VkBuffer buffer,
            VkDeviceSize offset = 0,
            VkDeviceSize range = VK_WHOLE_SIZE);

        /**
         * @brief Update a single buffer descriptor using RHIBuffer.
         *
         * @param binding Binding number.
         * @param type Descriptor type.
         * @param buffer RHIBuffer wrapper.
         */
        void UpdateBuffer(
            uint32_t binding,
            VkDescriptorType type,
            const Core::Ref<RHIBuffer>& buffer);

        /**
         * @brief Update a single image descriptor.
         *
         * @param binding Binding number.
         * @param type Descriptor type.
         * @param imageView Image view handle.
         * @param sampler Sampler handle.
         * @param imageLayout Image layout.
         */
        void UpdateImage(
            uint32_t binding,
            VkDescriptorType type,
            VkImageView imageView,
            VkSampler sampler,
            VkImageLayout imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        /**
         * @brief Update multiple buffer descriptors at once.
         *
         * @param writes Vector of buffer write descriptors.
         */
        void UpdateBuffers(const std::vector<DescriptorBufferWrite>& writes);

        /**
         * @brief Update multiple image descriptors at once.
         *
         * @param writes Vector of image write descriptors.
         */
        void UpdateImages(const std::vector<DescriptorImageWrite>& writes);

        /**
         * @brief Update the descriptor set using raw Vulkan write descriptors.
         *
         * @param writes Vector of VkWriteDescriptorSet structures.
         */
        void Update(const std::vector<VkWriteDescriptorSet>& writes);

    private:
        /**
         * @brief Private constructor - use Create() factory methods.
         */
        RHIDescriptorSet() = default;

        /**
         * @brief Initialize the descriptor set.
         *
         * @param device The logical device.
         * @param pool The descriptor pool.
         * @param layout The descriptor set layout.
         * @return true on success, false on failure.
         */
        bool Initialize(
            const Core::Ref<RHIDevice>& device,
            const Core::Ref<RHIDescriptorPool>& pool,
            const Core::Ref<RHIDescriptorSetLayout>& layout);

        /**
         * @brief Initialize from existing handle.
         *
         * @param device The logical device.
         * @param descriptorSet The existing descriptor set.
         * @return true on success, false on failure.
         */
        bool InitializeFromHandle(
            const Core::Ref<RHIDevice>& device,
            VkDescriptorSet descriptorSet);

        VkDevice m_Device = VK_NULL_HANDLE;
        VkDescriptorSet m_DescriptorSet = VK_NULL_HANDLE;
    };

} // namespace RHI
