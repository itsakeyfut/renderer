/**
 * @file RHIDescriptorSet.cpp
 * @brief Implementation of Vulkan Descriptor Set wrapper.
 */

#include "RHI/RHIDescriptorSet.h"
#include "RHI/RHIBuffer.h"
#include "Core/Log.h"

namespace RHI
{
    Core::Ref<RHIDescriptorSet> RHIDescriptorSet::Create(
        const Core::Ref<RHIDevice>& device,
        const Core::Ref<RHIDescriptorPool>& pool,
        const Core::Ref<RHIDescriptorSetLayout>& layout)
    {
        auto descriptorSet = Core::Ref<RHIDescriptorSet>(new RHIDescriptorSet());
        if (!descriptorSet->Initialize(device, pool, layout))
        {
            return nullptr;
        }
        return descriptorSet;
    }

    Core::Ref<RHIDescriptorSet> RHIDescriptorSet::CreateFromHandle(
        const Core::Ref<RHIDevice>& device,
        VkDescriptorSet descriptorSet)
    {
        auto wrapper = Core::Ref<RHIDescriptorSet>(new RHIDescriptorSet());
        if (!wrapper->InitializeFromHandle(device, descriptorSet))
        {
            return nullptr;
        }
        return wrapper;
    }

    bool RHIDescriptorSet::Initialize(
        const Core::Ref<RHIDevice>& device,
        const Core::Ref<RHIDescriptorPool>& pool,
        const Core::Ref<RHIDescriptorSetLayout>& layout)
    {
        if (!device)
        {
            LOG_ERROR("Cannot create descriptor set: device is null");
            return false;
        }

        if (!pool)
        {
            LOG_ERROR("Cannot create descriptor set: pool is null");
            return false;
        }

        if (!layout)
        {
            LOG_ERROR("Cannot create descriptor set: layout is null");
            return false;
        }

        m_Device = device->GetHandle();
        m_DescriptorSet = pool->Allocate(layout);

        if (m_DescriptorSet == VK_NULL_HANDLE)
        {
            LOG_ERROR("Failed to allocate descriptor set from pool");
            return false;
        }

        LOG_DEBUG("Created descriptor set");
        return true;
    }

    bool RHIDescriptorSet::InitializeFromHandle(
        const Core::Ref<RHIDevice>& device,
        VkDescriptorSet descriptorSet)
    {
        if (!device)
        {
            LOG_ERROR("Cannot wrap descriptor set: device is null");
            return false;
        }

        if (descriptorSet == VK_NULL_HANDLE)
        {
            LOG_ERROR("Cannot wrap descriptor set: handle is null");
            return false;
        }

        m_Device = device->GetHandle();
        m_DescriptorSet = descriptorSet;
        return true;
    }

    void RHIDescriptorSet::UpdateBuffer(
        uint32_t binding,
        VkDescriptorType type,
        VkBuffer buffer,
        VkDeviceSize offset,
        VkDeviceSize range)
    {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = buffer;
        bufferInfo.offset = offset;
        bufferInfo.range = range;

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = m_DescriptorSet;
        descriptorWrite.dstBinding = binding;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = type;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo = &bufferInfo;

        vkUpdateDescriptorSets(m_Device, 1, &descriptorWrite, 0, nullptr);
    }

    void RHIDescriptorSet::UpdateBuffer(
        uint32_t binding,
        VkDescriptorType type,
        const Core::Ref<RHIBuffer>& buffer)
    {
        if (!buffer)
        {
            LOG_ERROR("Cannot update descriptor set: buffer is null");
            return;
        }
        UpdateBuffer(binding, type, buffer->GetHandle(), 0, buffer->GetSize());
    }

    void RHIDescriptorSet::UpdateImage(
        uint32_t binding,
        VkDescriptorType type,
        VkImageView imageView,
        VkSampler sampler,
        VkImageLayout imageLayout)
    {
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = imageLayout;
        imageInfo.imageView = imageView;
        imageInfo.sampler = sampler;

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = m_DescriptorSet;
        descriptorWrite.dstBinding = binding;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = type;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(m_Device, 1, &descriptorWrite, 0, nullptr);
    }

    void RHIDescriptorSet::UpdateBuffers(const std::vector<DescriptorBufferWrite>& writes)
    {
        if (writes.empty())
        {
            return;
        }

        std::vector<VkDescriptorBufferInfo> bufferInfos(writes.size());
        std::vector<VkWriteDescriptorSet> descriptorWrites(writes.size());

        for (size_t i = 0; i < writes.size(); ++i)
        {
            const auto& write = writes[i];

            bufferInfos[i].buffer = write.Buffer;
            bufferInfos[i].offset = write.Offset;
            bufferInfos[i].range = write.Range;

            descriptorWrites[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[i].dstSet = m_DescriptorSet;
            descriptorWrites[i].dstBinding = write.Binding;
            descriptorWrites[i].dstArrayElement = write.ArrayElement;
            descriptorWrites[i].descriptorType = write.Type;
            descriptorWrites[i].descriptorCount = 1;
            descriptorWrites[i].pBufferInfo = &bufferInfos[i];
        }

        vkUpdateDescriptorSets(
            m_Device,
            static_cast<uint32_t>(descriptorWrites.size()),
            descriptorWrites.data(),
            0,
            nullptr);
    }

    void RHIDescriptorSet::UpdateImages(const std::vector<DescriptorImageWrite>& writes)
    {
        if (writes.empty())
        {
            return;
        }

        std::vector<VkDescriptorImageInfo> imageInfos(writes.size());
        std::vector<VkWriteDescriptorSet> descriptorWrites(writes.size());

        for (size_t i = 0; i < writes.size(); ++i)
        {
            const auto& write = writes[i];

            imageInfos[i].imageLayout = write.ImageLayout;
            imageInfos[i].imageView = write.ImageView;
            imageInfos[i].sampler = write.Sampler;

            descriptorWrites[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[i].dstSet = m_DescriptorSet;
            descriptorWrites[i].dstBinding = write.Binding;
            descriptorWrites[i].dstArrayElement = write.ArrayElement;
            descriptorWrites[i].descriptorType = write.Type;
            descriptorWrites[i].descriptorCount = 1;
            descriptorWrites[i].pImageInfo = &imageInfos[i];
        }

        vkUpdateDescriptorSets(
            m_Device,
            static_cast<uint32_t>(descriptorWrites.size()),
            descriptorWrites.data(),
            0,
            nullptr);
    }

    void RHIDescriptorSet::Update(const std::vector<VkWriteDescriptorSet>& writes)
    {
        if (writes.empty())
        {
            return;
        }

        // Set dstSet for all writes
        std::vector<VkWriteDescriptorSet> modifiedWrites = writes;
        for (auto& write : modifiedWrites)
        {
            write.dstSet = m_DescriptorSet;
        }

        vkUpdateDescriptorSets(
            m_Device,
            static_cast<uint32_t>(modifiedWrites.size()),
            modifiedWrites.data(),
            0,
            nullptr);
    }

} // namespace RHI
