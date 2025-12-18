/**
 * @file TestRHIShader.cpp
 * @brief Test file for RHI/RHIShader.h using GoogleTest.
 *
 * This file tests that the RHIShader class correctly compiles HLSL shaders
 * to SPIR-V using DXC and creates VkShaderModule handles.
 *
 * Note: These tests require a Vulkan-capable system and DXC compiler to pass.
 */

#include <gtest/gtest.h>
#include "RHI/RHIShader.h"
#include "RHI/RHIDevice.h"
#include "RHI/RHIPhysicalDevice.h"
#include "RHI/RHIInstance.h"
#include "Platform/Window.h"
#include "Core/Log.h"

#include <filesystem>
#include <fstream>

// =============================================================================
// Test Fixture for RHIShader Tests
// =============================================================================

class RHIShaderTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!Core::Log::IsInitialized()) {
            Core::Log::Init();
        }

        // Create a window to initialize GLFW and get a surface
        Platform::WindowConfig windowConfig;
        windowConfig.Width = 100;
        windowConfig.Height = 100;
        windowConfig.Title = "RHI Shader Test Window";
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

        // Find the shader directory (relative to where tests run from)
        FindShaderDirectory();
    }

    void TearDown() override {
        // Shaders must be destroyed before device
        m_VertexShader.reset();
        m_FragmentShader.reset();

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
        // Try various paths to find the shader directory
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

    // Helper to create a temporary SPIR-V file for testing
    bool CreateTestSPIRVFile(const std::filesystem::path& path) {
        // Minimal valid SPIR-V bytecode (just a magic number for testing)
        // This is a minimal valid SPIR-V module
        std::vector<uint32_t> minimalSpirv = {
            0x07230203,  // Magic number
            0x00010000,  // Version 1.0
            0x00080001,  // Generator magic
            0x00000001,  // Bound
            0x00000000,  // Schema
        };

        std::ofstream file(path, std::ios::binary);
        if (!file) return false;

        file.write(reinterpret_cast<const char*>(minimalSpirv.data()),
                   minimalSpirv.size() * sizeof(uint32_t));
        return file.good();
    }

    std::unique_ptr<Platform::Window> m_Window;
    Core::Ref<RHI::RHIInstance> m_Instance;
    Core::Ref<RHI::RHIPhysicalDevice> m_PhysicalDevice;
    Core::Ref<RHI::RHIDevice> m_Device;
    VkSurfaceKHR m_Surface = VK_NULL_HANDLE;

    Core::Ref<RHI::RHIShader> m_VertexShader;
    Core::Ref<RHI::RHIShader> m_FragmentShader;

    std::filesystem::path m_ShaderDir;
};

// =============================================================================
// DXC Availability Tests
// =============================================================================

TEST_F(RHIShaderTest, CheckDXCAvailability) {
    // This test just logs whether DXC is available
    bool dxcAvailable = RHI::RHIShader::IsDXCAvailable();
    if (dxcAvailable) {
        std::string dxcPath = RHI::RHIShader::GetDXCPath();
        EXPECT_FALSE(dxcPath.empty());
        LOG_INFO("DXC is available at: {}", dxcPath);
    } else {
        LOG_WARN("DXC is not available - HLSL compilation tests will be skipped");
    }
}

// =============================================================================
// HLSL Compilation Tests (require DXC)
// =============================================================================

TEST_F(RHIShaderTest, CompileVertexShaderFromHLSL) {
    if (!RHI::RHIShader::IsDXCAvailable()) {
        GTEST_SKIP() << "DXC compiler not available";
    }

    ASSERT_NE(m_Device, nullptr);
    ASSERT_FALSE(m_ShaderDir.empty()) << "Shader directory not found";

    std::filesystem::path shaderPath = m_ShaderDir / "vertex" / "triangle.hlsl";
    ASSERT_TRUE(std::filesystem::exists(shaderPath))
        << "Vertex shader not found: " << shaderPath;

    m_VertexShader = RHI::RHIShader::CreateFromHLSL(
        m_Device, shaderPath, RHI::ShaderStage::Vertex);

    ASSERT_NE(m_VertexShader, nullptr);
    EXPECT_NE(m_VertexShader->GetHandle(), VK_NULL_HANDLE);
    EXPECT_EQ(m_VertexShader->GetStage(), RHI::ShaderStage::Vertex);
    EXPECT_EQ(m_VertexShader->GetEntryPoint(), "main");
}

TEST_F(RHIShaderTest, CompileFragmentShaderFromHLSL) {
    if (!RHI::RHIShader::IsDXCAvailable()) {
        GTEST_SKIP() << "DXC compiler not available";
    }

    ASSERT_NE(m_Device, nullptr);
    ASSERT_FALSE(m_ShaderDir.empty()) << "Shader directory not found";

    std::filesystem::path shaderPath = m_ShaderDir / "pixel" / "triangle.hlsl";
    ASSERT_TRUE(std::filesystem::exists(shaderPath))
        << "Fragment shader not found: " << shaderPath;

    m_FragmentShader = RHI::RHIShader::CreateFromHLSL(
        m_Device, shaderPath, RHI::ShaderStage::Fragment);

    ASSERT_NE(m_FragmentShader, nullptr);
    EXPECT_NE(m_FragmentShader->GetHandle(), VK_NULL_HANDLE);
    EXPECT_EQ(m_FragmentShader->GetStage(), RHI::ShaderStage::Fragment);
    EXPECT_EQ(m_FragmentShader->GetEntryPoint(), "main");
}

TEST_F(RHIShaderTest, CompileWithCustomEntryPoint) {
    if (!RHI::RHIShader::IsDXCAvailable()) {
        GTEST_SKIP() << "DXC compiler not available";
    }

    ASSERT_NE(m_Device, nullptr);
    ASSERT_FALSE(m_ShaderDir.empty()) << "Shader directory not found";

    std::filesystem::path shaderPath = m_ShaderDir / "vertex" / "triangle.hlsl";
    ASSERT_TRUE(std::filesystem::exists(shaderPath));

    RHI::ShaderCompileConfig config;
    config.EntryPoint = "main";  // Our shader uses "main"

    m_VertexShader = RHI::RHIShader::CreateFromHLSL(
        m_Device, shaderPath, RHI::ShaderStage::Vertex, config);

    ASSERT_NE(m_VertexShader, nullptr);
    EXPECT_EQ(m_VertexShader->GetEntryPoint(), "main");
}

TEST_F(RHIShaderTest, CompileWithDebugInfo) {
    if (!RHI::RHIShader::IsDXCAvailable()) {
        GTEST_SKIP() << "DXC compiler not available";
    }

    ASSERT_NE(m_Device, nullptr);
    ASSERT_FALSE(m_ShaderDir.empty()) << "Shader directory not found";

    std::filesystem::path shaderPath = m_ShaderDir / "vertex" / "triangle.hlsl";
    ASSERT_TRUE(std::filesystem::exists(shaderPath));

    RHI::ShaderCompileConfig config;
    config.EnableDebugInfo = true;
    config.OptimizationLevel = 0;

    m_VertexShader = RHI::RHIShader::CreateFromHLSL(
        m_Device, shaderPath, RHI::ShaderStage::Vertex, config);

    ASSERT_NE(m_VertexShader, nullptr);
    EXPECT_NE(m_VertexShader->GetHandle(), VK_NULL_HANDLE);
}

TEST_F(RHIShaderTest, CompileNonExistentFileFails) {
    if (!RHI::RHIShader::IsDXCAvailable()) {
        GTEST_SKIP() << "DXC compiler not available";
    }

    ASSERT_NE(m_Device, nullptr);

    auto shader = RHI::RHIShader::CreateFromHLSL(
        m_Device, "nonexistent/shader.hlsl", RHI::ShaderStage::Vertex);

    EXPECT_EQ(shader, nullptr);
}

// =============================================================================
// SPIR-V Loading Tests
// =============================================================================

TEST_F(RHIShaderTest, CreateFromSPIRVBytecode) {
    ASSERT_NE(m_Device, nullptr);

    // Create a simple valid SPIR-V module (minimal vertex shader)
    // This is a minimal OpCapability Shader + OpMemoryModel + OpEntryPoint structure
    std::vector<uint32_t> spirvCode = {
        0x07230203,  // Magic
        0x00010500,  // Version 1.5
        0x00080001,  // Generator
        0x00000006,  // Bound
        0x00000000,  // Schema
        // OpCapability Shader
        0x00020011, 0x00000001,
        // OpMemoryModel Logical GLSL450
        0x0003000E, 0x00000000, 0x00000001,
        // OpEntryPoint Vertex %main "main"
        0x0005000F, 0x00000000, 0x00000001, 0x6E69616D, 0x00000000,
        // %void = OpTypeVoid
        0x00020013, 0x00000002,
        // %func = OpTypeFunction %void
        0x00030021, 0x00000003, 0x00000002,
        // %main = OpFunction %void None %func
        0x00050020, 0x00000002, 0x00000001, 0x00000000, 0x00000003,
        // OpLabel
        0x000200F8, 0x00000004,
        // OpReturn
        0x000100FD,
        // OpFunctionEnd
        0x00010038
    };

    auto shader = RHI::RHIShader::CreateFromSPIRV(
        m_Device, spirvCode, RHI::ShaderStage::Vertex);

    ASSERT_NE(shader, nullptr);
    EXPECT_NE(shader->GetHandle(), VK_NULL_HANDLE);
    EXPECT_EQ(shader->GetStage(), RHI::ShaderStage::Vertex);
}

TEST_F(RHIShaderTest, CreateFromEmptySPIRVFails) {
    ASSERT_NE(m_Device, nullptr);

    std::vector<uint32_t> emptySpirvCode;

    auto shader = RHI::RHIShader::CreateFromSPIRV(
        m_Device, emptySpirvCode, RHI::ShaderStage::Vertex);

    EXPECT_EQ(shader, nullptr);
}

TEST_F(RHIShaderTest, CreateWithNullDeviceFails) {
    std::vector<uint32_t> spirvCode = { 0x07230203, 0x00010000, 0x00080001, 0x00000001, 0x00000000 };

    auto shader = RHI::RHIShader::CreateFromSPIRV(
        nullptr, spirvCode, RHI::ShaderStage::Vertex);

    EXPECT_EQ(shader, nullptr);
}

// =============================================================================
// GetStageInfo Tests
// =============================================================================

TEST_F(RHIShaderTest, GetStageInfoReturnsCorrectVertexInfo) {
    if (!RHI::RHIShader::IsDXCAvailable()) {
        GTEST_SKIP() << "DXC compiler not available";
    }

    ASSERT_NE(m_Device, nullptr);
    ASSERT_FALSE(m_ShaderDir.empty()) << "Shader directory not found";

    std::filesystem::path shaderPath = m_ShaderDir / "vertex" / "triangle.hlsl";
    ASSERT_TRUE(std::filesystem::exists(shaderPath));

    m_VertexShader = RHI::RHIShader::CreateFromHLSL(
        m_Device, shaderPath, RHI::ShaderStage::Vertex);

    ASSERT_NE(m_VertexShader, nullptr);

    VkPipelineShaderStageCreateInfo stageInfo = m_VertexShader->GetStageInfo();

    EXPECT_EQ(stageInfo.sType, VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO);
    EXPECT_EQ(stageInfo.stage, VK_SHADER_STAGE_VERTEX_BIT);
    EXPECT_EQ(stageInfo.module, m_VertexShader->GetHandle());
    EXPECT_STREQ(stageInfo.pName, "main");
    EXPECT_EQ(stageInfo.pSpecializationInfo, nullptr);
}

TEST_F(RHIShaderTest, GetStageInfoReturnsCorrectFragmentInfo) {
    if (!RHI::RHIShader::IsDXCAvailable()) {
        GTEST_SKIP() << "DXC compiler not available";
    }

    ASSERT_NE(m_Device, nullptr);
    ASSERT_FALSE(m_ShaderDir.empty()) << "Shader directory not found";

    std::filesystem::path shaderPath = m_ShaderDir / "pixel" / "triangle.hlsl";
    ASSERT_TRUE(std::filesystem::exists(shaderPath));

    m_FragmentShader = RHI::RHIShader::CreateFromHLSL(
        m_Device, shaderPath, RHI::ShaderStage::Fragment);

    ASSERT_NE(m_FragmentShader, nullptr);

    VkPipelineShaderStageCreateInfo stageInfo = m_FragmentShader->GetStageInfo();

    EXPECT_EQ(stageInfo.sType, VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO);
    EXPECT_EQ(stageInfo.stage, VK_SHADER_STAGE_FRAGMENT_BIT);
    EXPECT_EQ(stageInfo.module, m_FragmentShader->GetHandle());
    EXPECT_STREQ(stageInfo.pName, "main");
}

// =============================================================================
// Shader Stage Conversion Tests
// =============================================================================

TEST_F(RHIShaderTest, AllShaderStagesHaveCorrectVkFlags) {
    // Test that stage info contains correct Vulkan flags for each stage type
    // We need DXC for HLSL compilation, but we can test stage conversion using SPIR-V
    ASSERT_NE(m_Device, nullptr);

    // Minimal SPIR-V for each stage type
    struct TestCase {
        RHI::ShaderStage stage;
        VkShaderStageFlagBits expectedFlag;
    };

    std::vector<TestCase> testCases = {
        { RHI::ShaderStage::Vertex, VK_SHADER_STAGE_VERTEX_BIT },
        { RHI::ShaderStage::Fragment, VK_SHADER_STAGE_FRAGMENT_BIT },
        { RHI::ShaderStage::Compute, VK_SHADER_STAGE_COMPUTE_BIT },
        { RHI::ShaderStage::Geometry, VK_SHADER_STAGE_GEOMETRY_BIT },
        { RHI::ShaderStage::TessControl, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT },
        { RHI::ShaderStage::TessEvaluation, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT }
    };

    // Simple SPIR-V bytecode for testing (vertex shader capability)
    std::vector<uint32_t> spirvCode = {
        0x07230203, 0x00010500, 0x00080001, 0x00000006, 0x00000000,
        0x00020011, 0x00000001,
        0x0003000E, 0x00000000, 0x00000001,
        0x0005000F, 0x00000000, 0x00000001, 0x6E69616D, 0x00000000,
        0x00020013, 0x00000002,
        0x00030021, 0x00000003, 0x00000002,
        0x00050020, 0x00000002, 0x00000001, 0x00000000, 0x00000003,
        0x000200F8, 0x00000004,
        0x000100FD,
        0x00010038
    };

    for (const auto& tc : testCases) {
        auto shader = RHI::RHIShader::CreateFromSPIRV(m_Device, spirvCode, tc.stage);
        ASSERT_NE(shader, nullptr) << "Failed to create shader for stage " << static_cast<int>(tc.stage);

        VkPipelineShaderStageCreateInfo info = shader->GetStageInfo();
        EXPECT_EQ(info.stage, tc.expectedFlag)
            << "Stage mismatch for " << static_cast<int>(tc.stage);
    }
}

// =============================================================================
// Destruction Tests
// =============================================================================

TEST_F(RHIShaderTest, ShaderCanBeDestroyedSafely) {
    if (!RHI::RHIShader::IsDXCAvailable()) {
        GTEST_SKIP() << "DXC compiler not available";
    }

    ASSERT_NE(m_Device, nullptr);
    ASSERT_FALSE(m_ShaderDir.empty()) << "Shader directory not found";

    std::filesystem::path shaderPath = m_ShaderDir / "vertex" / "triangle.hlsl";
    ASSERT_TRUE(std::filesystem::exists(shaderPath));

    {
        auto shader = RHI::RHIShader::CreateFromHLSL(
            m_Device, shaderPath, RHI::ShaderStage::Vertex);
        ASSERT_NE(shader, nullptr);
        // Shader destroyed when leaving scope
    }

    // Device should still be valid
    EXPECT_NE(m_Device->GetHandle(), VK_NULL_HANDLE);
}

TEST_F(RHIShaderTest, MultipleShadersCanBeCreatedAndDestroyed) {
    if (!RHI::RHIShader::IsDXCAvailable()) {
        GTEST_SKIP() << "DXC compiler not available";
    }

    ASSERT_NE(m_Device, nullptr);
    ASSERT_FALSE(m_ShaderDir.empty()) << "Shader directory not found";

    std::filesystem::path vertexPath = m_ShaderDir / "vertex" / "triangle.hlsl";
    std::filesystem::path fragmentPath = m_ShaderDir / "pixel" / "triangle.hlsl";

    ASSERT_TRUE(std::filesystem::exists(vertexPath));
    ASSERT_TRUE(std::filesystem::exists(fragmentPath));

    // Create multiple shaders
    m_VertexShader = RHI::RHIShader::CreateFromHLSL(
        m_Device, vertexPath, RHI::ShaderStage::Vertex);
    m_FragmentShader = RHI::RHIShader::CreateFromHLSL(
        m_Device, fragmentPath, RHI::ShaderStage::Fragment);

    EXPECT_NE(m_VertexShader, nullptr);
    EXPECT_NE(m_FragmentShader, nullptr);

    // They should have different handles
    EXPECT_NE(m_VertexShader->GetHandle(), m_FragmentShader->GetHandle());

    // Both should be valid
    EXPECT_NE(m_VertexShader->GetHandle(), VK_NULL_HANDLE);
    EXPECT_NE(m_FragmentShader->GetHandle(), VK_NULL_HANDLE);
}
