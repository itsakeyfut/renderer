/**
 * @file TestRHIPipeline.cpp
 * @brief Test file for RHI/RHIPipeline.h using GoogleTest.
 *
 * This file tests that the RHIPipeline class correctly creates graphics and
 * compute pipelines with various configurations using Vulkan 1.3 dynamic rendering.
 *
 * Note: These tests require a Vulkan-capable system and DXC compiler to pass.
 */

#include <gtest/gtest.h>
#include "RHI/RHIPipeline.h"
#include "RHI/RHIShader.h"
#include "RHI/RHIDevice.h"
#include "RHI/RHIPhysicalDevice.h"
#include "RHI/RHIInstance.h"
#include "Platform/Window.h"
#include "Core/Log.h"

#include <filesystem>

// =============================================================================
// Test Fixture for RHIPipeline Tests
// =============================================================================

class RHIPipelineTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!Core::Log::IsInitialized()) {
            Core::Log::Init();
        }

        // Create a window to initialize GLFW and get a surface
        Platform::WindowConfig windowConfig;
        windowConfig.Width = 100;
        windowConfig.Height = 100;
        windowConfig.Title = "RHI Pipeline Test Window";
        windowConfig.Visible = false;  // Hide window during tests
        m_Window = std::make_unique<Platform::Window>(windowConfig);

        // Create Vulkan instance
        RHI::RHIInstanceConfig instanceConfig;
        instanceConfig.EnableValidation = false;
        m_Instance = RHI::RHIInstance::Create(instanceConfig);

        // Create surface
        if (m_Instance) {
            m_Surface = m_Window->CreateSurface(m_Instance->GetHandle());
        }

        // Select physical device
        if (m_Instance && m_Surface != VK_NULL_HANDLE) {
            m_PhysicalDevice = RHI::RHIPhysicalDevice::Select(m_Instance, m_Surface);
        }

        // Create logical device
        if (m_PhysicalDevice) {
            m_Device = RHI::RHIDevice::Create(m_Instance, m_PhysicalDevice, m_Surface);
        }

        // Find the shader directory
        FindShaderDirectory();

        // Create shaders if DXC is available
        if (RHI::RHIShader::IsDXCAvailable() && m_Device && !m_ShaderDir.empty()) {
            CreateTestShaders();
        }
    }

    void TearDown() override {
        // Pipelines must be destroyed before shaders
        m_Pipeline.reset();

        // Shaders must be destroyed before device
        m_VertexShader.reset();
        m_FragmentShader.reset();
        m_ComputeShader.reset();

        // Device must be destroyed before surface and instance
        m_Device.reset();
        m_PhysicalDevice.reset();

        if (m_Surface != VK_NULL_HANDLE && m_Instance) {
            vkDestroySurfaceKHR(m_Instance->GetHandle(), m_Surface, nullptr);
            m_Surface = VK_NULL_HANDLE;
        }

        m_Instance.reset();
        m_Window.reset();
    }

    void FindShaderDirectory() {
        std::vector<std::filesystem::path> searchPaths = {
            "shaders/hlsl",
            "../shaders/hlsl",
            "../../shaders/hlsl",
            "../../../shaders/hlsl"
        };

        for (const auto& path : searchPaths) {
            if (std::filesystem::exists(path)) {
                m_ShaderDir = std::filesystem::absolute(path);
                break;
            }
        }
    }

    void CreateTestShaders() {
        std::filesystem::path vertexPath = m_ShaderDir / "vertex" / "triangle.hlsl";
        std::filesystem::path fragmentPath = m_ShaderDir / "pixel" / "triangle.hlsl";

        if (std::filesystem::exists(vertexPath)) {
            m_VertexShader = RHI::RHIShader::CreateFromHLSL(
                m_Device, vertexPath, RHI::ShaderStage::Vertex);
        }

        if (std::filesystem::exists(fragmentPath)) {
            m_FragmentShader = RHI::RHIShader::CreateFromHLSL(
                m_Device, fragmentPath, RHI::ShaderStage::Fragment);
        }

        // Try to find compute shader
        std::filesystem::path computePath = m_ShaderDir / "compute" / "simple.hlsl";
        if (std::filesystem::exists(computePath)) {
            m_ComputeShader = RHI::RHIShader::CreateFromHLSL(
                m_Device, computePath, RHI::ShaderStage::Compute);
        }
    }

    std::unique_ptr<Platform::Window> m_Window;
    Core::Ref<RHI::RHIInstance> m_Instance;
    Core::Ref<RHI::RHIPhysicalDevice> m_PhysicalDevice;
    Core::Ref<RHI::RHIDevice> m_Device;
    VkSurfaceKHR m_Surface = VK_NULL_HANDLE;

    Core::Ref<RHI::RHIShader> m_VertexShader;
    Core::Ref<RHI::RHIShader> m_FragmentShader;
    Core::Ref<RHI::RHIShader> m_ComputeShader;
    Core::Ref<RHI::RHIPipeline> m_Pipeline;

    std::filesystem::path m_ShaderDir;
};

// =============================================================================
// Basic Pipeline Creation Tests
// =============================================================================

TEST_F(RHIPipelineTest, CreateGraphicsPipelineWithVertexAndFragmentShaders) {
    if (!RHI::RHIShader::IsDXCAvailable()) {
        GTEST_SKIP() << "DXC compiler not available";
    }

    ASSERT_NE(m_Device, nullptr);
    ASSERT_NE(m_VertexShader, nullptr) << "Vertex shader creation failed";
    ASSERT_NE(m_FragmentShader, nullptr) << "Fragment shader creation failed";

    RHI::GraphicsPipelineDesc desc;
    desc.VertexShader = m_VertexShader;
    desc.FragmentShader = m_FragmentShader;
    desc.ColorAttachmentFormats = { VK_FORMAT_B8G8R8A8_SRGB };

    m_Pipeline = RHI::RHIPipeline::CreateGraphics(m_Device, desc);

    ASSERT_NE(m_Pipeline, nullptr);
    EXPECT_NE(m_Pipeline->GetHandle(), VK_NULL_HANDLE);
    EXPECT_NE(m_Pipeline->GetLayout(), VK_NULL_HANDLE);
    EXPECT_EQ(m_Pipeline->GetBindPoint(), VK_PIPELINE_BIND_POINT_GRAPHICS);
}

TEST_F(RHIPipelineTest, CreateGraphicsPipelineWithOnlyVertexShader) {
    if (!RHI::RHIShader::IsDXCAvailable()) {
        GTEST_SKIP() << "DXC compiler not available";
    }

    ASSERT_NE(m_Device, nullptr);
    ASSERT_NE(m_VertexShader, nullptr) << "Vertex shader creation failed";

    // Pipeline with only vertex shader (depth-only pass use case)
    RHI::GraphicsPipelineDesc desc;
    desc.VertexShader = m_VertexShader;
    desc.DepthAttachmentFormat = VK_FORMAT_D32_SFLOAT;

    m_Pipeline = RHI::RHIPipeline::CreateGraphics(m_Device, desc);

    ASSERT_NE(m_Pipeline, nullptr);
    EXPECT_NE(m_Pipeline->GetHandle(), VK_NULL_HANDLE);
}

TEST_F(RHIPipelineTest, CreateGraphicsPipelineWithNullDeviceFails) {
    RHI::GraphicsPipelineDesc desc;
    // desc.VertexShader intentionally null

    auto pipeline = RHI::RHIPipeline::CreateGraphics(nullptr, desc);

    EXPECT_EQ(pipeline, nullptr);
}

TEST_F(RHIPipelineTest, CreateGraphicsPipelineWithNullVertexShaderFails) {
    ASSERT_NE(m_Device, nullptr);

    RHI::GraphicsPipelineDesc desc;
    desc.ColorAttachmentFormats = { VK_FORMAT_B8G8R8A8_SRGB };
    // desc.VertexShader intentionally null

    auto pipeline = RHI::RHIPipeline::CreateGraphics(m_Device, desc);

    EXPECT_EQ(pipeline, nullptr);
}

// =============================================================================
// Pipeline State Configuration Tests
// =============================================================================

TEST_F(RHIPipelineTest, CreateGraphicsPipelineWithCustomRasterizationState) {
    if (!RHI::RHIShader::IsDXCAvailable()) {
        GTEST_SKIP() << "DXC compiler not available";
    }

    ASSERT_NE(m_Device, nullptr);
    ASSERT_NE(m_VertexShader, nullptr);
    ASSERT_NE(m_FragmentShader, nullptr);

    RHI::GraphicsPipelineDesc desc;
    desc.VertexShader = m_VertexShader;
    desc.FragmentShader = m_FragmentShader;
    desc.ColorAttachmentFormats = { VK_FORMAT_B8G8R8A8_SRGB };

    // Custom rasterization state
    desc.PolygonMode = VK_POLYGON_MODE_FILL;
    desc.CullMode = VK_CULL_MODE_NONE;  // No culling
    desc.FrontFace = VK_FRONT_FACE_CLOCKWISE;
    desc.LineWidth = 1.0f;

    m_Pipeline = RHI::RHIPipeline::CreateGraphics(m_Device, desc);

    ASSERT_NE(m_Pipeline, nullptr);
    EXPECT_NE(m_Pipeline->GetHandle(), VK_NULL_HANDLE);
}

TEST_F(RHIPipelineTest, CreateGraphicsPipelineWithDepthState) {
    if (!RHI::RHIShader::IsDXCAvailable()) {
        GTEST_SKIP() << "DXC compiler not available";
    }

    ASSERT_NE(m_Device, nullptr);
    ASSERT_NE(m_VertexShader, nullptr);
    ASSERT_NE(m_FragmentShader, nullptr);

    RHI::GraphicsPipelineDesc desc;
    desc.VertexShader = m_VertexShader;
    desc.FragmentShader = m_FragmentShader;
    desc.ColorAttachmentFormats = { VK_FORMAT_B8G8R8A8_SRGB };
    desc.DepthAttachmentFormat = VK_FORMAT_D32_SFLOAT;

    // Enable depth testing
    desc.DepthTestEnable = true;
    desc.DepthWriteEnable = true;
    desc.DepthCompareOp = VK_COMPARE_OP_LESS;

    m_Pipeline = RHI::RHIPipeline::CreateGraphics(m_Device, desc);

    ASSERT_NE(m_Pipeline, nullptr);
    EXPECT_NE(m_Pipeline->GetHandle(), VK_NULL_HANDLE);
}

TEST_F(RHIPipelineTest, CreateGraphicsPipelineWithColorBlending) {
    if (!RHI::RHIShader::IsDXCAvailable()) {
        GTEST_SKIP() << "DXC compiler not available";
    }

    ASSERT_NE(m_Device, nullptr);
    ASSERT_NE(m_VertexShader, nullptr);
    ASSERT_NE(m_FragmentShader, nullptr);

    RHI::GraphicsPipelineDesc desc;
    desc.VertexShader = m_VertexShader;
    desc.FragmentShader = m_FragmentShader;
    desc.ColorAttachmentFormats = { VK_FORMAT_B8G8R8A8_SRGB };

    // Alpha blending
    RHI::ColorBlendAttachment blendAttachment;
    blendAttachment.BlendEnable = true;
    blendAttachment.SrcColorFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blendAttachment.DstColorFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendAttachment.ColorBlendOp = VK_BLEND_OP_ADD;
    blendAttachment.SrcAlphaFactor = VK_BLEND_FACTOR_ONE;
    blendAttachment.DstAlphaFactor = VK_BLEND_FACTOR_ZERO;
    blendAttachment.AlphaBlendOp = VK_BLEND_OP_ADD;
    desc.ColorBlendAttachments.push_back(blendAttachment);

    m_Pipeline = RHI::RHIPipeline::CreateGraphics(m_Device, desc);

    ASSERT_NE(m_Pipeline, nullptr);
    EXPECT_NE(m_Pipeline->GetHandle(), VK_NULL_HANDLE);
}

TEST_F(RHIPipelineTest, CreateGraphicsPipelineWithDifferentTopology) {
    if (!RHI::RHIShader::IsDXCAvailable()) {
        GTEST_SKIP() << "DXC compiler not available";
    }

    ASSERT_NE(m_Device, nullptr);
    ASSERT_NE(m_VertexShader, nullptr);
    ASSERT_NE(m_FragmentShader, nullptr);

    // Test line list topology
    {
        RHI::GraphicsPipelineDesc desc;
        desc.VertexShader = m_VertexShader;
        desc.FragmentShader = m_FragmentShader;
        desc.ColorAttachmentFormats = { VK_FORMAT_B8G8R8A8_SRGB };
        desc.Topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;

        auto pipeline = RHI::RHIPipeline::CreateGraphics(m_Device, desc);
        ASSERT_NE(pipeline, nullptr);
        EXPECT_NE(pipeline->GetHandle(), VK_NULL_HANDLE);
    }

    // Test point list topology
    {
        RHI::GraphicsPipelineDesc desc;
        desc.VertexShader = m_VertexShader;
        desc.FragmentShader = m_FragmentShader;
        desc.ColorAttachmentFormats = { VK_FORMAT_B8G8R8A8_SRGB };
        desc.Topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;

        auto pipeline = RHI::RHIPipeline::CreateGraphics(m_Device, desc);
        ASSERT_NE(pipeline, nullptr);
        EXPECT_NE(pipeline->GetHandle(), VK_NULL_HANDLE);
    }
}

// =============================================================================
// Dynamic State Tests
// =============================================================================

TEST_F(RHIPipelineTest, CreateGraphicsPipelineWithDefaultDynamicState) {
    if (!RHI::RHIShader::IsDXCAvailable()) {
        GTEST_SKIP() << "DXC compiler not available";
    }

    ASSERT_NE(m_Device, nullptr);
    ASSERT_NE(m_VertexShader, nullptr);
    ASSERT_NE(m_FragmentShader, nullptr);

    RHI::GraphicsPipelineDesc desc;
    desc.VertexShader = m_VertexShader;
    desc.FragmentShader = m_FragmentShader;
    desc.ColorAttachmentFormats = { VK_FORMAT_B8G8R8A8_SRGB };
    // Default: DynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR }

    m_Pipeline = RHI::RHIPipeline::CreateGraphics(m_Device, desc);

    ASSERT_NE(m_Pipeline, nullptr);
    EXPECT_NE(m_Pipeline->GetHandle(), VK_NULL_HANDLE);
}

TEST_F(RHIPipelineTest, CreateGraphicsPipelineWithNoDynamicState) {
    if (!RHI::RHIShader::IsDXCAvailable()) {
        GTEST_SKIP() << "DXC compiler not available";
    }

    ASSERT_NE(m_Device, nullptr);
    ASSERT_NE(m_VertexShader, nullptr);
    ASSERT_NE(m_FragmentShader, nullptr);

    RHI::GraphicsPipelineDesc desc;
    desc.VertexShader = m_VertexShader;
    desc.FragmentShader = m_FragmentShader;
    desc.ColorAttachmentFormats = { VK_FORMAT_B8G8R8A8_SRGB };
    desc.DynamicStates.clear();  // No dynamic states

    m_Pipeline = RHI::RHIPipeline::CreateGraphics(m_Device, desc);

    ASSERT_NE(m_Pipeline, nullptr);
    EXPECT_NE(m_Pipeline->GetHandle(), VK_NULL_HANDLE);
}

TEST_F(RHIPipelineTest, CreateGraphicsPipelineWithExtendedDynamicState) {
    if (!RHI::RHIShader::IsDXCAvailable()) {
        GTEST_SKIP() << "DXC compiler not available";
    }

    ASSERT_NE(m_Device, nullptr);
    ASSERT_NE(m_VertexShader, nullptr);
    ASSERT_NE(m_FragmentShader, nullptr);

    RHI::GraphicsPipelineDesc desc;
    desc.VertexShader = m_VertexShader;
    desc.FragmentShader = m_FragmentShader;
    desc.ColorAttachmentFormats = { VK_FORMAT_B8G8R8A8_SRGB };

    // Extended dynamic states
    desc.DynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_LINE_WIDTH
    };

    m_Pipeline = RHI::RHIPipeline::CreateGraphics(m_Device, desc);

    ASSERT_NE(m_Pipeline, nullptr);
    EXPECT_NE(m_Pipeline->GetHandle(), VK_NULL_HANDLE);
}

// =============================================================================
// Vertex Input Tests
// =============================================================================

TEST_F(RHIPipelineTest, CreateGraphicsPipelineWithVertexInputState) {
    if (!RHI::RHIShader::IsDXCAvailable()) {
        GTEST_SKIP() << "DXC compiler not available";
    }

    ASSERT_NE(m_Device, nullptr);
    ASSERT_NE(m_VertexShader, nullptr);
    ASSERT_NE(m_FragmentShader, nullptr);

    RHI::GraphicsPipelineDesc desc;
    desc.VertexShader = m_VertexShader;
    desc.FragmentShader = m_FragmentShader;
    desc.ColorAttachmentFormats = { VK_FORMAT_B8G8R8A8_SRGB };

    // Define vertex input layout (typical vertex with position and color)
    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(float) * 6;  // 3 floats for position, 3 for color
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    desc.VertexBindings.push_back(binding);

    VkVertexInputAttributeDescription posAttrib{};
    posAttrib.binding = 0;
    posAttrib.location = 0;
    posAttrib.format = VK_FORMAT_R32G32B32_SFLOAT;
    posAttrib.offset = 0;
    desc.VertexAttributes.push_back(posAttrib);

    VkVertexInputAttributeDescription colorAttrib{};
    colorAttrib.binding = 0;
    colorAttrib.location = 1;
    colorAttrib.format = VK_FORMAT_R32G32B32_SFLOAT;
    colorAttrib.offset = sizeof(float) * 3;
    desc.VertexAttributes.push_back(colorAttrib);

    m_Pipeline = RHI::RHIPipeline::CreateGraphics(m_Device, desc);

    ASSERT_NE(m_Pipeline, nullptr);
    EXPECT_NE(m_Pipeline->GetHandle(), VK_NULL_HANDLE);
}

// =============================================================================
// Multiple Render Target Tests
// =============================================================================

TEST_F(RHIPipelineTest, CreateGraphicsPipelineWithMultipleColorAttachments) {
    if (!RHI::RHIShader::IsDXCAvailable()) {
        GTEST_SKIP() << "DXC compiler not available";
    }

    ASSERT_NE(m_Device, nullptr);
    ASSERT_NE(m_VertexShader, nullptr);
    ASSERT_NE(m_FragmentShader, nullptr);

    RHI::GraphicsPipelineDesc desc;
    desc.VertexShader = m_VertexShader;
    desc.FragmentShader = m_FragmentShader;

    // Multiple render targets (G-Buffer style)
    desc.ColorAttachmentFormats = {
        VK_FORMAT_R8G8B8A8_SRGB,      // Albedo
        VK_FORMAT_R16G16B16A16_SFLOAT, // Normal
        VK_FORMAT_R8G8B8A8_UNORM      // Material properties
    };
    desc.DepthAttachmentFormat = VK_FORMAT_D32_SFLOAT;

    m_Pipeline = RHI::RHIPipeline::CreateGraphics(m_Device, desc);

    ASSERT_NE(m_Pipeline, nullptr);
    EXPECT_NE(m_Pipeline->GetHandle(), VK_NULL_HANDLE);
}

// =============================================================================
// Pipeline Layout Tests
// =============================================================================

TEST_F(RHIPipelineTest, CreateGraphicsPipelineWithPushConstants) {
    if (!RHI::RHIShader::IsDXCAvailable()) {
        GTEST_SKIP() << "DXC compiler not available";
    }

    ASSERT_NE(m_Device, nullptr);
    ASSERT_NE(m_VertexShader, nullptr);
    ASSERT_NE(m_FragmentShader, nullptr);

    RHI::GraphicsPipelineDesc desc;
    desc.VertexShader = m_VertexShader;
    desc.FragmentShader = m_FragmentShader;
    desc.ColorAttachmentFormats = { VK_FORMAT_B8G8R8A8_SRGB };

    // Add push constant range
    VkPushConstantRange pushConstant{};
    pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstant.offset = 0;
    pushConstant.size = sizeof(float) * 16;  // 4x4 matrix
    desc.PushConstantRanges.push_back(pushConstant);

    m_Pipeline = RHI::RHIPipeline::CreateGraphics(m_Device, desc);

    ASSERT_NE(m_Pipeline, nullptr);
    EXPECT_NE(m_Pipeline->GetHandle(), VK_NULL_HANDLE);
    EXPECT_NE(m_Pipeline->GetLayout(), VK_NULL_HANDLE);
}

// =============================================================================
// Pipeline Destruction Tests
// =============================================================================

TEST_F(RHIPipelineTest, PipelineCanBeDestroyedSafely) {
    if (!RHI::RHIShader::IsDXCAvailable()) {
        GTEST_SKIP() << "DXC compiler not available";
    }

    ASSERT_NE(m_Device, nullptr);
    ASSERT_NE(m_VertexShader, nullptr);
    ASSERT_NE(m_FragmentShader, nullptr);

    {
        RHI::GraphicsPipelineDesc desc;
        desc.VertexShader = m_VertexShader;
        desc.FragmentShader = m_FragmentShader;
        desc.ColorAttachmentFormats = { VK_FORMAT_B8G8R8A8_SRGB };

        auto pipeline = RHI::RHIPipeline::CreateGraphics(m_Device, desc);
        ASSERT_NE(pipeline, nullptr);
        // Pipeline destroyed when leaving scope
    }

    // Device should still be valid
    EXPECT_NE(m_Device->GetHandle(), VK_NULL_HANDLE);
}

TEST_F(RHIPipelineTest, MultiplePipelinesCanBeCreatedAndDestroyed) {
    if (!RHI::RHIShader::IsDXCAvailable()) {
        GTEST_SKIP() << "DXC compiler not available";
    }

    ASSERT_NE(m_Device, nullptr);
    ASSERT_NE(m_VertexShader, nullptr);
    ASSERT_NE(m_FragmentShader, nullptr);

    // Create multiple pipelines with different configurations
    std::vector<Core::Ref<RHI::RHIPipeline>> pipelines;

    for (int i = 0; i < 3; ++i) {
        RHI::GraphicsPipelineDesc desc;
        desc.VertexShader = m_VertexShader;
        desc.FragmentShader = m_FragmentShader;
        desc.ColorAttachmentFormats = { VK_FORMAT_B8G8R8A8_SRGB };

        // Vary some settings
        desc.CullMode = (i == 0) ? VK_CULL_MODE_BACK_BIT :
                        (i == 1) ? VK_CULL_MODE_FRONT_BIT :
                                   VK_CULL_MODE_NONE;

        auto pipeline = RHI::RHIPipeline::CreateGraphics(m_Device, desc);
        ASSERT_NE(pipeline, nullptr);
        pipelines.push_back(pipeline);
    }

    // All pipelines should have unique handles
    EXPECT_NE(pipelines[0]->GetHandle(), pipelines[1]->GetHandle());
    EXPECT_NE(pipelines[1]->GetHandle(), pipelines[2]->GetHandle());
    EXPECT_NE(pipelines[0]->GetHandle(), pipelines[2]->GetHandle());

    // Clear all pipelines
    pipelines.clear();

    // Device should still be valid
    EXPECT_NE(m_Device->GetHandle(), VK_NULL_HANDLE);
}
