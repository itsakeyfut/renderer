/**
 * @file RHIPipeline.cpp
 * @brief Implementation of the Vulkan Pipeline wrapper.
 */

#include "RHI/RHIPipeline.h"
#include "Core/Log.h"

namespace RHI
{
    Core::Ref<RHIPipeline> RHIPipeline::CreateGraphics(
        const Core::Ref<RHIDevice>& device,
        const GraphicsPipelineDesc& desc)
    {
        if (!device)
        {
            LOG_ERROR("RHIPipeline::CreateGraphics: device is null");
            return nullptr;
        }

        if (!desc.VertexShader)
        {
            LOG_ERROR("RHIPipeline::CreateGraphics: VertexShader is required");
            return nullptr;
        }

        auto pipeline = Core::Ref<RHIPipeline>(new RHIPipeline());
        if (!pipeline->InitializeGraphics(device, desc))
        {
            return nullptr;
        }

        LOG_INFO("RHIPipeline: Graphics pipeline created successfully");
        return pipeline;
    }

    Core::Ref<RHIPipeline> RHIPipeline::CreateCompute(
        const Core::Ref<RHIDevice>& device,
        const Core::Ref<RHIShader>& computeShader,
        const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts,
        const std::vector<VkPushConstantRange>& pushConstantRanges)
    {
        if (!device)
        {
            LOG_ERROR("RHIPipeline::CreateCompute: device is null");
            return nullptr;
        }

        if (!computeShader)
        {
            LOG_ERROR("RHIPipeline::CreateCompute: computeShader is required");
            return nullptr;
        }

        if (computeShader->GetStage() != ShaderStage::Compute)
        {
            LOG_ERROR("RHIPipeline::CreateCompute: shader must be a compute shader");
            return nullptr;
        }

        auto pipeline = Core::Ref<RHIPipeline>(new RHIPipeline());
        if (!pipeline->InitializeCompute(device, computeShader, descriptorSetLayouts, pushConstantRanges))
        {
            return nullptr;
        }

        LOG_INFO("RHIPipeline: Compute pipeline created successfully");
        return pipeline;
    }

    RHIPipeline::~RHIPipeline()
    {
        if (m_Pipeline != VK_NULL_HANDLE)
        {
            vkDestroyPipeline(m_Device, m_Pipeline, nullptr);
            m_Pipeline = VK_NULL_HANDLE;
            LOG_DEBUG("RHIPipeline: Pipeline destroyed");
        }

        if (m_PipelineLayout != VK_NULL_HANDLE)
        {
            vkDestroyPipelineLayout(m_Device, m_PipelineLayout, nullptr);
            m_PipelineLayout = VK_NULL_HANDLE;
            LOG_DEBUG("RHIPipeline: Pipeline layout destroyed");
        }
    }

    bool RHIPipeline::InitializeGraphics(
        const Core::Ref<RHIDevice>& device,
        const GraphicsPipelineDesc& desc)
    {
        m_Device = device->GetHandle();
        m_BindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

        // Create pipeline layout first
        if (!CreatePipelineLayout(m_Device, desc.DescriptorSetLayouts, desc.PushConstantRanges))
        {
            return false;
        }

        // Collect shader stages
        std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

        if (desc.VertexShader)
        {
            shaderStages.push_back(desc.VertexShader->GetStageInfo());
        }

        if (desc.FragmentShader)
        {
            shaderStages.push_back(desc.FragmentShader->GetStageInfo());
        }

        if (desc.GeometryShader)
        {
            shaderStages.push_back(desc.GeometryShader->GetStageInfo());
        }

        if (desc.TessControlShader)
        {
            shaderStages.push_back(desc.TessControlShader->GetStageInfo());
        }

        if (desc.TessEvalShader)
        {
            shaderStages.push_back(desc.TessEvalShader->GetStageInfo());
        }

        // Vertex Input State
        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(desc.VertexBindings.size());
        vertexInputInfo.pVertexBindingDescriptions = desc.VertexBindings.empty() ? nullptr : desc.VertexBindings.data();
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(desc.VertexAttributes.size());
        vertexInputInfo.pVertexAttributeDescriptions = desc.VertexAttributes.empty() ? nullptr : desc.VertexAttributes.data();

        // Input Assembly State
        VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo{};
        inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssemblyInfo.topology = desc.Topology;
        inputAssemblyInfo.primitiveRestartEnable = desc.PrimitiveRestartEnable ? VK_TRUE : VK_FALSE;

        // Check if viewport/scissor are dynamic
        bool viewportDynamic = false;
        bool scissorDynamic = false;
        for (const auto& state : desc.DynamicStates)
        {
            if (state == VK_DYNAMIC_STATE_VIEWPORT) viewportDynamic = true;
            if (state == VK_DYNAMIC_STATE_SCISSOR) scissorDynamic = true;
        }

        // Default viewport and scissor (used when not dynamic)
        VkViewport defaultViewport{};
        defaultViewport.x = 0.0f;
        defaultViewport.y = 0.0f;
        defaultViewport.width = 1.0f;  // Will be set dynamically typically
        defaultViewport.height = 1.0f;
        defaultViewport.minDepth = 0.0f;
        defaultViewport.maxDepth = 1.0f;

        VkRect2D defaultScissor{};
        defaultScissor.offset = { 0, 0 };
        defaultScissor.extent = { 1, 1 };  // Will be set dynamically typically

        // Viewport State
        VkPipelineViewportStateCreateInfo viewportStateInfo{};
        viewportStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportStateInfo.viewportCount = 1;
        viewportStateInfo.pViewports = viewportDynamic ? nullptr : &defaultViewport;
        viewportStateInfo.scissorCount = 1;
        viewportStateInfo.pScissors = scissorDynamic ? nullptr : &defaultScissor;

        // Rasterization State
        VkPipelineRasterizationStateCreateInfo rasterizationInfo{};
        rasterizationInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizationInfo.depthClampEnable = desc.DepthClampEnable ? VK_TRUE : VK_FALSE;
        rasterizationInfo.rasterizerDiscardEnable = desc.RasterizerDiscardEnable ? VK_TRUE : VK_FALSE;
        rasterizationInfo.polygonMode = desc.PolygonMode;
        rasterizationInfo.cullMode = desc.CullMode;
        rasterizationInfo.frontFace = desc.FrontFace;
        rasterizationInfo.depthBiasEnable = desc.DepthBiasEnable ? VK_TRUE : VK_FALSE;
        rasterizationInfo.depthBiasConstantFactor = desc.DepthBiasConstantFactor;
        rasterizationInfo.depthBiasClamp = desc.DepthBiasClamp;
        rasterizationInfo.depthBiasSlopeFactor = desc.DepthBiasSlopeFactor;
        rasterizationInfo.lineWidth = desc.LineWidth;

        // Multisampling State
        VkPipelineMultisampleStateCreateInfo multisampleInfo{};
        multisampleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampleInfo.rasterizationSamples = desc.SampleCount;
        multisampleInfo.sampleShadingEnable = desc.SampleShadingEnable ? VK_TRUE : VK_FALSE;
        multisampleInfo.minSampleShading = desc.MinSampleShading;
        multisampleInfo.pSampleMask = desc.SampleMask;
        multisampleInfo.alphaToCoverageEnable = desc.AlphaToCoverageEnable ? VK_TRUE : VK_FALSE;
        multisampleInfo.alphaToOneEnable = desc.AlphaToOneEnable ? VK_TRUE : VK_FALSE;

        // Depth/Stencil State
        VkPipelineDepthStencilStateCreateInfo depthStencilInfo{};
        depthStencilInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencilInfo.depthTestEnable = desc.DepthTestEnable ? VK_TRUE : VK_FALSE;
        depthStencilInfo.depthWriteEnable = desc.DepthWriteEnable ? VK_TRUE : VK_FALSE;
        depthStencilInfo.depthCompareOp = desc.DepthCompareOp;
        depthStencilInfo.depthBoundsTestEnable = desc.DepthBoundsTestEnable ? VK_TRUE : VK_FALSE;
        depthStencilInfo.stencilTestEnable = desc.StencilTestEnable ? VK_TRUE : VK_FALSE;
        depthStencilInfo.front = desc.StencilFront;
        depthStencilInfo.back = desc.StencilBack;
        depthStencilInfo.minDepthBounds = desc.MinDepthBounds;
        depthStencilInfo.maxDepthBounds = desc.MaxDepthBounds;

        // Color Blend Attachments
        std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments;
        if (desc.ColorBlendAttachments.empty() && !desc.ColorAttachmentFormats.empty())
        {
            // Default: one attachment per color format with no blending
            for (size_t i = 0; i < desc.ColorAttachmentFormats.size(); ++i)
            {
                VkPipelineColorBlendAttachmentState attachment{};
                attachment.blendEnable = VK_FALSE;
                attachment.colorWriteMask =
                    VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                    VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
                colorBlendAttachments.push_back(attachment);
            }
        }
        else
        {
            for (const auto& blend : desc.ColorBlendAttachments)
            {
                VkPipelineColorBlendAttachmentState attachment{};
                attachment.blendEnable = blend.BlendEnable ? VK_TRUE : VK_FALSE;
                attachment.srcColorBlendFactor = blend.SrcColorFactor;
                attachment.dstColorBlendFactor = blend.DstColorFactor;
                attachment.colorBlendOp = blend.ColorBlendOp;
                attachment.srcAlphaBlendFactor = blend.SrcAlphaFactor;
                attachment.dstAlphaBlendFactor = blend.DstAlphaFactor;
                attachment.alphaBlendOp = blend.AlphaBlendOp;
                attachment.colorWriteMask = blend.ColorWriteMask;
                colorBlendAttachments.push_back(attachment);
            }
        }

        // Color Blend State
        VkPipelineColorBlendStateCreateInfo colorBlendInfo{};
        colorBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlendInfo.logicOpEnable = desc.LogicOpEnable ? VK_TRUE : VK_FALSE;
        colorBlendInfo.logicOp = desc.LogicOp;
        colorBlendInfo.attachmentCount = static_cast<uint32_t>(colorBlendAttachments.size());
        colorBlendInfo.pAttachments = colorBlendAttachments.empty() ? nullptr : colorBlendAttachments.data();
        colorBlendInfo.blendConstants[0] = desc.BlendConstants[0];
        colorBlendInfo.blendConstants[1] = desc.BlendConstants[1];
        colorBlendInfo.blendConstants[2] = desc.BlendConstants[2];
        colorBlendInfo.blendConstants[3] = desc.BlendConstants[3];

        // Dynamic State
        VkPipelineDynamicStateCreateInfo dynamicStateInfo{};
        dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(desc.DynamicStates.size());
        dynamicStateInfo.pDynamicStates = desc.DynamicStates.empty() ? nullptr : desc.DynamicStates.data();

        // Dynamic Rendering (Vulkan 1.3)
        VkPipelineRenderingCreateInfo renderingInfo{};
        renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        renderingInfo.colorAttachmentCount = static_cast<uint32_t>(desc.ColorAttachmentFormats.size());
        renderingInfo.pColorAttachmentFormats = desc.ColorAttachmentFormats.empty() ? nullptr : desc.ColorAttachmentFormats.data();
        renderingInfo.depthAttachmentFormat = desc.DepthAttachmentFormat;
        renderingInfo.stencilAttachmentFormat = desc.StencilAttachmentFormat;

        // Create Graphics Pipeline
        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.pNext = &renderingInfo;
        pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
        pipelineInfo.pStages = shaderStages.data();
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssemblyInfo;
        pipelineInfo.pTessellationState = nullptr; // TODO: Add tessellation support if needed
        pipelineInfo.pViewportState = &viewportStateInfo;
        pipelineInfo.pRasterizationState = &rasterizationInfo;
        pipelineInfo.pMultisampleState = &multisampleInfo;
        pipelineInfo.pDepthStencilState = &depthStencilInfo;
        pipelineInfo.pColorBlendState = &colorBlendInfo;
        pipelineInfo.pDynamicState = desc.DynamicStates.empty() ? nullptr : &dynamicStateInfo;
        pipelineInfo.layout = m_PipelineLayout;
        pipelineInfo.renderPass = VK_NULL_HANDLE; // Using dynamic rendering
        pipelineInfo.subpass = 0;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
        pipelineInfo.basePipelineIndex = -1;

        VkResult result = vkCreateGraphicsPipelines(
            m_Device,
            VK_NULL_HANDLE, // Pipeline cache (could be added later)
            1,
            &pipelineInfo,
            nullptr,
            &m_Pipeline);

        if (result != VK_SUCCESS)
        {
            LOG_ERROR("RHIPipeline::InitializeGraphics: Failed to create graphics pipeline (VkResult: {})", static_cast<int>(result));
            return false;
        }

        LOG_DEBUG("RHIPipeline: Graphics pipeline initialized with {} shader stages", shaderStages.size());
        return true;
    }

    bool RHIPipeline::InitializeCompute(
        const Core::Ref<RHIDevice>& device,
        const Core::Ref<RHIShader>& computeShader,
        const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts,
        const std::vector<VkPushConstantRange>& pushConstantRanges)
    {
        m_Device = device->GetHandle();
        m_BindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;

        // Create pipeline layout first
        if (!CreatePipelineLayout(m_Device, descriptorSetLayouts, pushConstantRanges))
        {
            return false;
        }

        // Create Compute Pipeline
        VkComputePipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineInfo.stage = computeShader->GetStageInfo();
        pipelineInfo.layout = m_PipelineLayout;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
        pipelineInfo.basePipelineIndex = -1;

        VkResult result = vkCreateComputePipelines(
            m_Device,
            VK_NULL_HANDLE, // Pipeline cache
            1,
            &pipelineInfo,
            nullptr,
            &m_Pipeline);

        if (result != VK_SUCCESS)
        {
            LOG_ERROR("RHIPipeline::InitializeCompute: Failed to create compute pipeline (VkResult: {})", static_cast<int>(result));
            return false;
        }

        LOG_DEBUG("RHIPipeline: Compute pipeline initialized");
        return true;
    }

    bool RHIPipeline::CreatePipelineLayout(
        VkDevice device,
        const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts,
        const std::vector<VkPushConstantRange>& pushConstantRanges)
    {
        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
        layoutInfo.pSetLayouts = descriptorSetLayouts.empty() ? nullptr : descriptorSetLayouts.data();
        layoutInfo.pushConstantRangeCount = static_cast<uint32_t>(pushConstantRanges.size());
        layoutInfo.pPushConstantRanges = pushConstantRanges.empty() ? nullptr : pushConstantRanges.data();

        VkResult result = vkCreatePipelineLayout(device, &layoutInfo, nullptr, &m_PipelineLayout);
        if (result != VK_SUCCESS)
        {
            LOG_ERROR("RHIPipeline::CreatePipelineLayout: Failed to create pipeline layout (VkResult: {})", static_cast<int>(result));
            return false;
        }

        LOG_DEBUG("RHIPipeline: Pipeline layout created with {} descriptor sets and {} push constant ranges",
            descriptorSetLayouts.size(), pushConstantRanges.size());
        return true;
    }

} // namespace RHI
