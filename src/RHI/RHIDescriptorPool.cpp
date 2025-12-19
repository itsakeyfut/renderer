/**
 * @file RHIDescriptorPool.cpp
 * @brief Implementation of Vulkan Descriptor Pool wrapper.
 */

#include "RHI/RHIDescriptorPool.h"
#include "RHI/RHIDescriptorSetLayout.h"
#include "Core/Log.h"

namespace RHI
{
    Core::Ref<RHIDescriptorPool> RHIDescriptorPool::Create(
        const Core::Ref<RHIDevice>& device,
        const DescriptorPoolDesc& desc)
    {
        auto pool = Core::Ref<RHIDescriptorPool>(new RHIDescriptorPool());
        if (!pool->Initialize(device, desc))
        {
            return nullptr;
        }
        return pool;
    }

    Core::Ref<RHIDescriptorPool> RHIDescriptorPool::Create(
        const Core::Ref<RHIDevice>& device,
        uint32_t maxSets,
        const std::vector<DescriptorPoolSize>& poolSizes,
        VkDescriptorPoolCreateFlags flags)
    {
        DescriptorPoolDesc desc;
        desc.MaxSets = maxSets;
        desc.PoolSizes = poolSizes;
        desc.Flags = flags;
        return Create(device, desc);
    }

    RHIDescriptorPool::~RHIDescriptorPool()
    {
        if (m_Pool != VK_NULL_HANDLE && m_Device != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorPool(m_Device, m_Pool, nullptr);
            LOG_DEBUG("Destroyed descriptor pool");
        }
    }

    bool RHIDescriptorPool::Initialize(
        const Core::Ref<RHIDevice>& device,
        const DescriptorPoolDesc& desc)
    {
        if (!device)
        {
            LOG_ERROR("Cannot create descriptor pool: device is null");
            return false;
        }

        if (desc.MaxSets == 0)
        {
            LOG_ERROR("Cannot create descriptor pool: maxSets is 0");
            return false;
        }

        if (desc.PoolSizes.empty())
        {
            LOG_ERROR("Cannot create descriptor pool: no pool sizes specified");
            return false;
        }

        m_Device = device->GetHandle();
        m_MaxSets = desc.MaxSets;
        m_Flags = desc.Flags;

        // Convert pool sizes to Vulkan format
        std::vector<VkDescriptorPoolSize> vkPoolSizes;
        vkPoolSizes.reserve(desc.PoolSizes.size());

        for (const auto& size : desc.PoolSizes)
        {
            VkDescriptorPoolSize vkSize{};
            vkSize.type = size.Type;
            vkSize.descriptorCount = size.Count;
            vkPoolSizes.push_back(vkSize);
        }

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(vkPoolSizes.size());
        poolInfo.pPoolSizes = vkPoolSizes.data();
        poolInfo.maxSets = desc.MaxSets;
        poolInfo.flags = desc.Flags;

        VkResult result = vkCreateDescriptorPool(m_Device, &poolInfo, nullptr, &m_Pool);
        if (result != VK_SUCCESS)
        {
            LOG_ERROR("Failed to create descriptor pool. VkResult: {}", static_cast<int>(result));
            return false;
        }

        LOG_DEBUG("Created descriptor pool with {} max sets and {} pool sizes",
                  desc.MaxSets, desc.PoolSizes.size());
        return true;
    }

    VkDescriptorSet RHIDescriptorPool::Allocate(VkDescriptorSetLayout layout)
    {
        if (layout == VK_NULL_HANDLE)
        {
            LOG_ERROR("Cannot allocate descriptor set: layout is null");
            return VK_NULL_HANDLE;
        }

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_Pool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &layout;

        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
        VkResult result = vkAllocateDescriptorSets(m_Device, &allocInfo, &descriptorSet);
        if (result != VK_SUCCESS)
        {
            LOG_ERROR("Failed to allocate descriptor set. VkResult: {}", static_cast<int>(result));
            return VK_NULL_HANDLE;
        }

        return descriptorSet;
    }

    VkDescriptorSet RHIDescriptorPool::Allocate(const Core::Ref<RHIDescriptorSetLayout>& layout)
    {
        if (!layout)
        {
            LOG_ERROR("Cannot allocate descriptor set: layout is null");
            return VK_NULL_HANDLE;
        }
        return Allocate(layout->GetHandle());
    }

    std::vector<VkDescriptorSet> RHIDescriptorPool::AllocateMultiple(VkDescriptorSetLayout layout, uint32_t count)
    {
        if (layout == VK_NULL_HANDLE)
        {
            LOG_ERROR("Cannot allocate descriptor sets: layout is null");
            return {};
        }

        if (count == 0)
        {
            return {};
        }

        std::vector<VkDescriptorSetLayout> layouts(count, layout);
        return AllocateMultiple(layouts);
    }

    std::vector<VkDescriptorSet> RHIDescriptorPool::AllocateMultiple(const std::vector<VkDescriptorSetLayout>& layouts)
    {
        if (layouts.empty())
        {
            return {};
        }

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_Pool;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
        allocInfo.pSetLayouts = layouts.data();

        std::vector<VkDescriptorSet> descriptorSets(layouts.size(), VK_NULL_HANDLE);
        VkResult result = vkAllocateDescriptorSets(m_Device, &allocInfo, descriptorSets.data());
        if (result != VK_SUCCESS)
        {
            LOG_ERROR("Failed to allocate {} descriptor sets. VkResult: {}",
                      layouts.size(), static_cast<int>(result));
            return {};
        }

        LOG_DEBUG("Allocated {} descriptor sets", layouts.size());
        return descriptorSets;
    }

    bool RHIDescriptorPool::Free(const std::vector<VkDescriptorSet>& sets)
    {
        if (sets.empty())
        {
            return true;
        }

        // Check if pool was created with FREE_DESCRIPTOR_SET_BIT
        if ((m_Flags & VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT) == 0)
        {
            LOG_ERROR("Cannot free descriptor sets: pool was not created with FREE_DESCRIPTOR_SET_BIT");
            return false;
        }

        VkResult result = vkFreeDescriptorSets(
            m_Device,
            m_Pool,
            static_cast<uint32_t>(sets.size()),
            sets.data());

        if (result != VK_SUCCESS)
        {
            LOG_ERROR("Failed to free descriptor sets. VkResult: {}", static_cast<int>(result));
            return false;
        }

        LOG_DEBUG("Freed {} descriptor sets", sets.size());
        return true;
    }

    bool RHIDescriptorPool::Reset()
    {
        VkResult result = vkResetDescriptorPool(m_Device, m_Pool, 0);
        if (result != VK_SUCCESS)
        {
            LOG_ERROR("Failed to reset descriptor pool. VkResult: {}", static_cast<int>(result));
            return false;
        }

        LOG_DEBUG("Reset descriptor pool");
        return true;
    }

} // namespace RHI
