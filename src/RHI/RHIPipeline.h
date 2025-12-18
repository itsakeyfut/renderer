/**
 * @file RHIPipeline.h
 * @brief Vulkan Pipeline wrapper for the RHI layer.
 *
 * Manages VkPipeline and VkPipelineLayout creation for graphics and compute pipelines.
 * Supports configurable pipeline states and dynamic state management.
 */

#pragma once

#include "Core/Types.h"
#include "RHI/RHIDevice.h"
#include "RHI/RHIShader.h"

#include <vulkan/vulkan.h>

#include <vector>

namespace RHI
{
    /**
     * @brief Color blend attachment configuration
     *
     * Defines blending behavior for a single color attachment in the pipeline.
     */
    struct ColorBlendAttachment
    {
        bool BlendEnable = false;
        VkBlendFactor SrcColorFactor = VK_BLEND_FACTOR_ONE;
        VkBlendFactor DstColorFactor = VK_BLEND_FACTOR_ZERO;
        VkBlendOp ColorBlendOp = VK_BLEND_OP_ADD;
        VkBlendFactor SrcAlphaFactor = VK_BLEND_FACTOR_ONE;
        VkBlendFactor DstAlphaFactor = VK_BLEND_FACTOR_ZERO;
        VkBlendOp AlphaBlendOp = VK_BLEND_OP_ADD;
        VkColorComponentFlags ColorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    };

    /**
     * @brief Graphics pipeline description
     *
     * Contains all configuration needed to create a graphics pipeline.
     * Uses sensible defaults for common use cases.
     */
    struct GraphicsPipelineDesc
    {
        // ========== Shader Stages ==========
        /**
         * @brief Vertex shader (required)
         */
        Core::Ref<RHIShader> VertexShader;

        /**
         * @brief Fragment shader (optional, but typically required for rendering)
         */
        Core::Ref<RHIShader> FragmentShader;

        /**
         * @brief Geometry shader (optional)
         */
        Core::Ref<RHIShader> GeometryShader;

        /**
         * @brief Tessellation control shader (optional)
         */
        Core::Ref<RHIShader> TessControlShader;

        /**
         * @brief Tessellation evaluation shader (optional)
         */
        Core::Ref<RHIShader> TessEvalShader;

        // ========== Vertex Input State ==========
        /**
         * @brief Vertex binding descriptions
         */
        std::vector<VkVertexInputBindingDescription> VertexBindings;

        /**
         * @brief Vertex attribute descriptions
         */
        std::vector<VkVertexInputAttributeDescription> VertexAttributes;

        // ========== Input Assembly State ==========
        /**
         * @brief Primitive topology for input assembly
         */
        VkPrimitiveTopology Topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        /**
         * @brief Enable primitive restart for strip topologies
         */
        bool PrimitiveRestartEnable = false;

        // ========== Rasterization State ==========
        /**
         * @brief Polygon rendering mode (fill, line, point)
         */
        VkPolygonMode PolygonMode = VK_POLYGON_MODE_FILL;

        /**
         * @brief Face culling mode
         */
        VkCullModeFlags CullMode = VK_CULL_MODE_BACK_BIT;

        /**
         * @brief Front face winding order
         */
        VkFrontFace FrontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

        /**
         * @brief Line width for line primitives
         */
        float LineWidth = 1.0f;

        /**
         * @brief Enable depth clamping (requires feature)
         */
        bool DepthClampEnable = false;

        /**
         * @brief Discard all primitives before rasterization
         */
        bool RasterizerDiscardEnable = false;

        /**
         * @brief Enable depth bias
         */
        bool DepthBiasEnable = false;

        /**
         * @brief Depth bias constant factor
         */
        float DepthBiasConstantFactor = 0.0f;

        /**
         * @brief Depth bias clamp value
         */
        float DepthBiasClamp = 0.0f;

        /**
         * @brief Depth bias slope factor
         */
        float DepthBiasSlopeFactor = 0.0f;

        // ========== Depth/Stencil State ==========
        /**
         * @brief Enable depth testing
         */
        bool DepthTestEnable = true;

        /**
         * @brief Enable depth writing
         */
        bool DepthWriteEnable = true;

        /**
         * @brief Depth comparison operation
         */
        VkCompareOp DepthCompareOp = VK_COMPARE_OP_LESS;

        /**
         * @brief Enable depth bounds testing
         */
        bool DepthBoundsTestEnable = false;

        /**
         * @brief Minimum depth bounds
         */
        float MinDepthBounds = 0.0f;

        /**
         * @brief Maximum depth bounds
         */
        float MaxDepthBounds = 1.0f;

        /**
         * @brief Enable stencil testing
         */
        bool StencilTestEnable = false;

        /**
         * @brief Front face stencil operation state
         */
        VkStencilOpState StencilFront = {};

        /**
         * @brief Back face stencil operation state
         */
        VkStencilOpState StencilBack = {};

        // ========== Color Blend State ==========
        /**
         * @brief Per-attachment color blend configurations
         */
        std::vector<ColorBlendAttachment> ColorBlendAttachments;

        /**
         * @brief Enable logic operations for color blending
         */
        bool LogicOpEnable = false;

        /**
         * @brief Logic operation to apply
         */
        VkLogicOp LogicOp = VK_LOGIC_OP_COPY;

        /**
         * @brief Blend constants for blend factor operations
         */
        float BlendConstants[4] = {0.0f, 0.0f, 0.0f, 0.0f};

        // ========== Multisampling State ==========
        /**
         * @brief Number of samples per pixel
         */
        VkSampleCountFlagBits SampleCount = VK_SAMPLE_COUNT_1_BIT;

        /**
         * @brief Enable sample shading
         */
        bool SampleShadingEnable = false;

        /**
         * @brief Minimum fraction of samples to shade
         */
        float MinSampleShading = 1.0f;

        /**
         * @brief Sample mask (nullptr uses all samples)
         */
        const VkSampleMask* SampleMask = nullptr;

        /**
         * @brief Enable alpha-to-coverage
         */
        bool AlphaToCoverageEnable = false;

        /**
         * @brief Enable alpha-to-one
         */
        bool AlphaToOneEnable = false;

        // ========== Dynamic State ==========
        /**
         * @brief Dynamic states that can be changed at command buffer time
         *
         * By default, viewport and scissor are dynamic (most common case).
         */
        std::vector<VkDynamicState> DynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };

        // ========== Render Target Formats (Dynamic Rendering) ==========
        /**
         * @brief Color attachment formats for dynamic rendering
         */
        std::vector<VkFormat> ColorAttachmentFormats;

        /**
         * @brief Depth attachment format (VK_FORMAT_UNDEFINED if not used)
         */
        VkFormat DepthAttachmentFormat = VK_FORMAT_UNDEFINED;

        /**
         * @brief Stencil attachment format (VK_FORMAT_UNDEFINED if not used)
         */
        VkFormat StencilAttachmentFormat = VK_FORMAT_UNDEFINED;

        // ========== Pipeline Layout ==========
        /**
         * @brief Descriptor set layouts for the pipeline
         */
        std::vector<VkDescriptorSetLayout> DescriptorSetLayouts;

        /**
         * @brief Push constant ranges for the pipeline
         */
        std::vector<VkPushConstantRange> PushConstantRanges;
    };

    /**
     * @brief Vulkan Pipeline wrapper class
     *
     * Manages VkPipeline and VkPipelineLayout lifecycle.
     * Supports both graphics and compute pipelines.
     *
     * This class follows RAII principles - resources are cleaned up
     * when the RHIPipeline object is destroyed.
     *
     * Usage:
     * @code
     * GraphicsPipelineDesc desc;
     * desc.VertexShader = vertexShader;
     * desc.FragmentShader = fragmentShader;
     * desc.ColorAttachmentFormats = { VK_FORMAT_B8G8R8A8_SRGB };
     *
     * auto pipeline = RHIPipeline::CreateGraphics(device, desc);
     * if (pipeline) {
     *     commandBuffer->BindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetHandle());
     * }
     * @endcode
     */
    class RHIPipeline
    {
    public:
        /**
         * @brief Factory method to create a graphics pipeline
         *
         * Creates a VkPipeline with the specified configuration.
         * Uses Vulkan 1.3 dynamic rendering (no render pass required).
         *
         * @param device The RHI device to create the pipeline on
         * @param desc Graphics pipeline configuration
         * @return Shared pointer to the created pipeline, or nullptr on failure
         */
        static Core::Ref<RHIPipeline> CreateGraphics(
            const Core::Ref<RHIDevice>& device,
            const GraphicsPipelineDesc& desc);

        /**
         * @brief Factory method to create a compute pipeline
         *
         * Creates a compute pipeline with the specified shader and layout.
         *
         * @param device The RHI device to create the pipeline on
         * @param computeShader The compute shader module
         * @param descriptorSetLayouts Descriptor set layouts for the pipeline
         * @param pushConstantRanges Push constant ranges for the pipeline
         * @return Shared pointer to the created pipeline, or nullptr on failure
         */
        static Core::Ref<RHIPipeline> CreateCompute(
            const Core::Ref<RHIDevice>& device,
            const Core::Ref<RHIShader>& computeShader,
            const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts = {},
            const std::vector<VkPushConstantRange>& pushConstantRanges = {});

        /**
         * @brief Destructor - destroys VkPipeline and VkPipelineLayout
         */
        ~RHIPipeline();

        // Non-copyable
        RHIPipeline(const RHIPipeline&) = delete;
        RHIPipeline& operator=(const RHIPipeline&) = delete;

        // Non-movable (Vulkan handles cannot be safely moved)
        RHIPipeline(RHIPipeline&&) = delete;
        RHIPipeline& operator=(RHIPipeline&&) = delete;

        /**
         * @brief Get the native VkPipeline handle
         * @return VkPipeline handle
         */
        VkPipeline GetHandle() const { return m_Pipeline; }

        /**
         * @brief Get the VkPipelineLayout handle
         * @return VkPipelineLayout handle
         */
        VkPipelineLayout GetLayout() const { return m_PipelineLayout; }

        /**
         * @brief Get the pipeline bind point
         * @return VkPipelineBindPoint (graphics or compute)
         */
        VkPipelineBindPoint GetBindPoint() const { return m_BindPoint; }

    private:
        /**
         * @brief Private constructor - use factory methods
         */
        RHIPipeline() = default;

        /**
         * @brief Initialize a graphics pipeline
         * @param device The RHI device
         * @param desc Graphics pipeline description
         * @return true on success, false on failure
         */
        bool InitializeGraphics(
            const Core::Ref<RHIDevice>& device,
            const GraphicsPipelineDesc& desc);

        /**
         * @brief Initialize a compute pipeline
         * @param device The RHI device
         * @param computeShader The compute shader
         * @param descriptorSetLayouts Descriptor set layouts
         * @param pushConstantRanges Push constant ranges
         * @return true on success, false on failure
         */
        bool InitializeCompute(
            const Core::Ref<RHIDevice>& device,
            const Core::Ref<RHIShader>& computeShader,
            const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts,
            const std::vector<VkPushConstantRange>& pushConstantRanges);

        /**
         * @brief Create a pipeline layout
         * @param device The Vulkan device
         * @param descriptorSetLayouts Descriptor set layouts
         * @param pushConstantRanges Push constant ranges
         * @return true on success, false on failure
         */
        bool CreatePipelineLayout(
            VkDevice device,
            const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts,
            const std::vector<VkPushConstantRange>& pushConstantRanges);

        VkDevice m_Device = VK_NULL_HANDLE;
        VkPipeline m_Pipeline = VK_NULL_HANDLE;
        VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
        VkPipelineBindPoint m_BindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    };

} // namespace RHI
