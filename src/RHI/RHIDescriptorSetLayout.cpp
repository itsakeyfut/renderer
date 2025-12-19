/**
 * @file RHIDescriptorSetLayout.cpp
 * @brief Implementation of Vulkan Descriptor Set Layout wrapper.
 */

#include "RHI/RHIDescriptorSetLayout.h"
#include "Core/Log.h"

namespace RHI
{
    Core::Ref<RHIDescriptorSetLayout> RHIDescriptorSetLayout::Create(
        const Core::Ref<RHIDevice>& device,
        const DescriptorSetLayoutDesc& desc)
    {
        auto layout = Core::Ref<RHIDescriptorSetLayout>(new RHIDescriptorSetLayout());
        if (!layout->Initialize(device, desc))
        {
            return nullptr;
        }
        return layout;
    }

    Core::Ref<RHIDescriptorSetLayout> RHIDescriptorSetLayout::Create(
        const Core::Ref<RHIDevice>& device,
        const std::vector<DescriptorBinding>& bindings)
    {
        DescriptorSetLayoutDesc desc;
        desc.Bindings = bindings;
        return Create(device, desc);
    }

    RHIDescriptorSetLayout::~RHIDescriptorSetLayout()
    {
        if (m_Layout != VK_NULL_HANDLE && m_Device != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorSetLayout(m_Device, m_Layout, nullptr);
            LOG_DEBUG("Destroyed descriptor set layout");
        }
    }

    bool RHIDescriptorSetLayout::Initialize(
        const Core::Ref<RHIDevice>& device,
        const DescriptorSetLayoutDesc& desc)
    {
        if (!device)
        {
            LOG_ERROR("Cannot create descriptor set layout: device is null");
            return false;
        }

        m_Device = device->GetHandle();
        m_Bindings = desc.Bindings;

        // Convert bindings to Vulkan format
        std::vector<VkDescriptorSetLayoutBinding> vkBindings;
        vkBindings.reserve(desc.Bindings.size());

        for (const auto& binding : desc.Bindings)
        {
            VkDescriptorSetLayoutBinding vkBinding{};
            vkBinding.binding = binding.Binding;
            vkBinding.descriptorType = binding.Type;
            vkBinding.descriptorCount = binding.Count;
            vkBinding.stageFlags = binding.Stages;
            vkBinding.pImmutableSamplers = binding.ImmutableSamplers;
            vkBindings.push_back(vkBinding);
        }

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(vkBindings.size());
        layoutInfo.pBindings = vkBindings.data();
        layoutInfo.flags = desc.Flags;

        VkResult result = vkCreateDescriptorSetLayout(m_Device, &layoutInfo, nullptr, &m_Layout);
        if (result != VK_SUCCESS)
        {
            LOG_ERROR("Failed to create descriptor set layout. VkResult: {}", static_cast<int>(result));
            return false;
        }

        LOG_DEBUG("Created descriptor set layout with {} bindings", desc.Bindings.size());
        return true;
    }

} // namespace RHI
