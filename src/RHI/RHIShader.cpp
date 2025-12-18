/**
 * @file RHIShader.cpp
 * @brief Implementation of the RHIShader class.
 */

#include "RHI/RHIShader.h"
#include "Core/Assert.h"
#include "Core/Log.h"

#include <fstream>
#include <sstream>
#include <array>
#include <cstdlib>

#if defined(_WIN32)
    #include <windows.h>
#endif

namespace RHI
{
    // =========================================================================
    // Static Helper Functions
    // =========================================================================

    VkShaderStageFlagBits RHIShader::ToVkShaderStage(ShaderStage stage)
    {
        switch (stage)
        {
            case ShaderStage::Vertex:         return VK_SHADER_STAGE_VERTEX_BIT;
            case ShaderStage::Fragment:       return VK_SHADER_STAGE_FRAGMENT_BIT;
            case ShaderStage::Compute:        return VK_SHADER_STAGE_COMPUTE_BIT;
            case ShaderStage::Geometry:       return VK_SHADER_STAGE_GEOMETRY_BIT;
            case ShaderStage::TessControl:    return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
            case ShaderStage::TessEvaluation: return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
            default:
                LOG_ERROR("Unknown shader stage: {}", static_cast<int>(stage));
                return VK_SHADER_STAGE_VERTEX_BIT;
        }
    }

    std::string RHIShader::GetDXCTargetProfile(ShaderStage stage)
    {
        switch (stage)
        {
            case ShaderStage::Vertex:         return "vs_6_0";
            case ShaderStage::Fragment:       return "ps_6_0";
            case ShaderStage::Compute:        return "cs_6_0";
            case ShaderStage::Geometry:       return "gs_6_0";
            case ShaderStage::TessControl:    return "hs_6_0";
            case ShaderStage::TessEvaluation: return "ds_6_0";
            default:
                LOG_ERROR("Unknown shader stage for DXC profile: {}", static_cast<int>(stage));
                return "vs_6_0";
        }
    }

    // =========================================================================
    // DXC Path Discovery
    // =========================================================================

    std::string RHIShader::GetDXCPath()
    {
        // Cache the result to avoid repeated filesystem searches
        static std::string s_DXCPath;
        static bool s_Searched = false;

        if (s_Searched)
        {
            return s_DXCPath;
        }

        s_Searched = true;

#if defined(_WIN32)
        // Check environment variable first
        const char* vulkanSDK = std::getenv("VULKAN_SDK");
        if (vulkanSDK != nullptr)
        {
            std::filesystem::path dxcPath = std::filesystem::path(vulkanSDK) / "Bin" / "dxc.exe";
            if (std::filesystem::exists(dxcPath))
            {
                s_DXCPath = dxcPath.string();
                LOG_INFO("DXC found at: {}", s_DXCPath);
                return s_DXCPath;
            }
        }

        // Search common Vulkan SDK installation paths on Windows
        std::vector<std::filesystem::path> searchPaths = {
            "C:/VulkanSDK"
        };

        for (const auto& basePath : searchPaths)
        {
            if (!std::filesystem::exists(basePath))
            {
                continue;
            }

            // Find the newest SDK version
            std::vector<std::filesystem::path> sdkVersions;
            for (const auto& entry : std::filesystem::directory_iterator(basePath))
            {
                if (entry.is_directory())
                {
                    sdkVersions.push_back(entry.path());
                }
            }

            // Sort descending to get newest version first
            std::sort(sdkVersions.begin(), sdkVersions.end(), std::greater<>());

            for (const auto& sdkPath : sdkVersions)
            {
                std::filesystem::path dxcPath = sdkPath / "Bin" / "dxc.exe";
                if (std::filesystem::exists(dxcPath))
                {
                    s_DXCPath = dxcPath.string();
                    LOG_INFO("DXC found at: {}", s_DXCPath);
                    return s_DXCPath;
                }
            }
        }

        // Try PATH environment variable
        std::filesystem::path dxcInPath = "dxc.exe";
        char pathBuffer[MAX_PATH];
        DWORD result = SearchPathA(nullptr, "dxc.exe", nullptr, MAX_PATH, pathBuffer, nullptr);
        if (result > 0)
        {
            s_DXCPath = pathBuffer;
            LOG_INFO("DXC found in PATH: {}", s_DXCPath);
            return s_DXCPath;
        }
#else
        // Linux/macOS: Check common paths
        std::vector<std::filesystem::path> searchPaths = {
            "/usr/bin/dxc",
            "/usr/local/bin/dxc",
            std::filesystem::path(std::getenv("HOME") ? std::getenv("HOME") : "") / ".local/bin/dxc"
        };

        const char* vulkanSDK = std::getenv("VULKAN_SDK");
        if (vulkanSDK != nullptr)
        {
            searchPaths.insert(searchPaths.begin(), std::filesystem::path(vulkanSDK) / "bin" / "dxc");
        }

        for (const auto& path : searchPaths)
        {
            if (std::filesystem::exists(path))
            {
                s_DXCPath = path.string();
                LOG_INFO("DXC found at: {}", s_DXCPath);
                return s_DXCPath;
            }
        }
#endif

        LOG_WARN("DXC compiler not found. HLSL compilation will not be available.");
        return s_DXCPath;
    }

    bool RHIShader::IsDXCAvailable()
    {
        return !GetDXCPath().empty();
    }

    // =========================================================================
    // HLSL Compilation
    // =========================================================================

    bool RHIShader::CompileHLSLToSPIRV(
        const std::filesystem::path& filepath,
        ShaderStage stage,
        const ShaderCompileConfig& config,
        std::vector<uint32_t>& outSpirvCode)
    {
        std::string dxcPath = GetDXCPath();
        if (dxcPath.empty())
        {
            LOG_ERROR("DXC compiler not found. Cannot compile HLSL shader: {}", filepath.string());
            return false;
        }

        if (!std::filesystem::exists(filepath))
        {
            LOG_ERROR("HLSL shader file not found: {}", filepath.string());
            return false;
        }

        // Create a temporary output file for SPIR-V
        std::filesystem::path tempDir = std::filesystem::temp_directory_path();
        std::filesystem::path outputPath = tempDir / (filepath.stem().string() + ".spv");

        // Build DXC command line
        std::ostringstream cmdStream;
        cmdStream << "\"" << dxcPath << "\"";
        cmdStream << " -T " << GetDXCTargetProfile(stage);
        cmdStream << " -E " << config.EntryPoint;
        cmdStream << " -spirv";  // Output SPIR-V
        cmdStream << " -fspv-target-env=vulkan1.3";  // Target Vulkan 1.3

        // Optimization level
        if (config.OptimizationLevel == 0)
        {
            cmdStream << " -Od";  // Disable optimizations
        }
        else
        {
            cmdStream << " -O" << config.OptimizationLevel;
        }

        // Debug info
        if (config.EnableDebugInfo)
        {
            cmdStream << " -Zi";  // Enable debug info
            cmdStream << " -fspv-debug=vulkan-with-source";
        }

        // Include directories
        for (const auto& includeDir : config.IncludeDirs)
        {
            cmdStream << " -I \"" << includeDir << "\"";
        }

        // Add the shader's directory as an include path
        cmdStream << " -I \"" << filepath.parent_path().string() << "\"";

        // Macro definitions
        for (const auto& define : config.Defines)
        {
            cmdStream << " -D " << define;
        }

        // Input and output files
        cmdStream << " \"" << filepath.string() << "\"";
        cmdStream << " -Fo \"" << outputPath.string() << "\"";

        std::string command = cmdStream.str();
        LOG_DEBUG("Compiling shader with command: {}", command);

        // Execute DXC
#if defined(_WIN32)
        // Use CreateProcess for better error handling on Windows
        STARTUPINFOA si = {};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;

        PROCESS_INFORMATION pi = {};

        // Create a mutable copy of the command string
        std::vector<char> cmdBuffer(command.begin(), command.end());
        cmdBuffer.push_back('\0');

        BOOL success = CreateProcessA(
            nullptr,
            cmdBuffer.data(),
            nullptr,
            nullptr,
            FALSE,
            CREATE_NO_WINDOW,
            nullptr,
            nullptr,
            &si,
            &pi);

        if (!success)
        {
            DWORD error = GetLastError();
            LOG_ERROR("Failed to execute DXC compiler (error code: {})", error);
            return false;
        }

        // Wait for the process to complete
        WaitForSingleObject(pi.hProcess, INFINITE);

        DWORD exitCode = 0;
        GetExitCodeProcess(pi.hProcess, &exitCode);

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        if (exitCode != 0)
        {
            LOG_ERROR("DXC compilation failed with exit code: {}", exitCode);
            LOG_ERROR("Shader: {}", filepath.string());
            return false;
        }
#else
        int result = std::system(command.c_str());
        if (result != 0)
        {
            LOG_ERROR("DXC compilation failed with exit code: {}", result);
            LOG_ERROR("Shader: {}", filepath.string());
            return false;
        }
#endif

        // Read the compiled SPIR-V
        if (!ReadSPIRVFile(outputPath, outSpirvCode))
        {
            LOG_ERROR("Failed to read compiled SPIR-V from: {}", outputPath.string());
            return false;
        }

        // Clean up temporary file
        std::error_code ec;
        std::filesystem::remove(outputPath, ec);
        if (ec)
        {
            LOG_WARN("Failed to remove temporary SPIR-V file: {}", outputPath.string());
        }

        LOG_INFO("Successfully compiled shader: {} ({} bytes SPIR-V)",
            filepath.filename().string(),
            outSpirvCode.size() * sizeof(uint32_t));

        return true;
    }

    // =========================================================================
    // SPIR-V File Reading
    // =========================================================================

    bool RHIShader::ReadSPIRVFile(
        const std::filesystem::path& filepath,
        std::vector<uint32_t>& outSpirvCode)
    {
        std::ifstream file(filepath, std::ios::binary | std::ios::ate);
        if (!file.is_open())
        {
            LOG_ERROR("Failed to open SPIR-V file: {}", filepath.string());
            return false;
        }

        std::streamsize fileSize = file.tellg();
        if (fileSize <= 0)
        {
            LOG_ERROR("SPIR-V file is empty: {}", filepath.string());
            return false;
        }

        // SPIR-V files must be aligned to 4 bytes
        if (fileSize % sizeof(uint32_t) != 0)
        {
            LOG_ERROR("SPIR-V file size is not aligned to 4 bytes: {}", filepath.string());
            return false;
        }

        file.seekg(0, std::ios::beg);

        outSpirvCode.resize(static_cast<size_t>(fileSize) / sizeof(uint32_t));
        file.read(reinterpret_cast<char*>(outSpirvCode.data()), fileSize);

        if (!file)
        {
            LOG_ERROR("Failed to read SPIR-V file contents: {}", filepath.string());
            return false;
        }

        // Verify SPIR-V magic number
        constexpr uint32_t SPIRV_MAGIC = 0x07230203;
        if (outSpirvCode.empty() || outSpirvCode[0] != SPIRV_MAGIC)
        {
            LOG_ERROR("Invalid SPIR-V file (bad magic number): {}", filepath.string());
            return false;
        }

        return true;
    }

    // =========================================================================
    // Factory Methods
    // =========================================================================

    Core::Ref<RHIShader> RHIShader::CreateFromHLSL(
        const Core::Ref<RHIDevice>& device,
        const std::filesystem::path& filepath,
        ShaderStage stage,
        const ShaderCompileConfig& config)
    {
        if (!device)
        {
            LOG_ERROR("Cannot create shader: device is null");
            return nullptr;
        }

        std::vector<uint32_t> spirvCode;
        if (!CompileHLSLToSPIRV(filepath, stage, config, spirvCode))
        {
            return nullptr;
        }

        auto shader = Core::Ref<RHIShader>(new RHIShader());
        if (!shader->Initialize(device, spirvCode, stage, config.EntryPoint))
        {
            LOG_ERROR("Failed to create shader module from compiled HLSL: {}", filepath.string());
            return nullptr;
        }

        return shader;
    }

    Core::Ref<RHIShader> RHIShader::CreateFromSPIRV(
        const Core::Ref<RHIDevice>& device,
        const std::vector<uint32_t>& spirvCode,
        ShaderStage stage,
        const std::string& entryPoint)
    {
        if (!device)
        {
            LOG_ERROR("Cannot create shader: device is null");
            return nullptr;
        }

        if (spirvCode.empty())
        {
            LOG_ERROR("Cannot create shader: SPIR-V code is empty");
            return nullptr;
        }

        auto shader = Core::Ref<RHIShader>(new RHIShader());
        if (!shader->Initialize(device, spirvCode, stage, entryPoint))
        {
            LOG_ERROR("Failed to create shader module from SPIR-V");
            return nullptr;
        }

        return shader;
    }

    Core::Ref<RHIShader> RHIShader::CreateFromSPIRVFile(
        const Core::Ref<RHIDevice>& device,
        const std::filesystem::path& filepath,
        ShaderStage stage,
        const std::string& entryPoint)
    {
        if (!device)
        {
            LOG_ERROR("Cannot create shader: device is null");
            return nullptr;
        }

        std::vector<uint32_t> spirvCode;
        if (!ReadSPIRVFile(filepath, spirvCode))
        {
            return nullptr;
        }

        auto shader = Core::Ref<RHIShader>(new RHIShader());
        if (!shader->Initialize(device, spirvCode, stage, entryPoint))
        {
            LOG_ERROR("Failed to create shader module from SPIR-V file: {}", filepath.string());
            return nullptr;
        }

        LOG_INFO("Successfully loaded shader: {} ({} bytes SPIR-V)",
            filepath.filename().string(),
            spirvCode.size() * sizeof(uint32_t));

        return shader;
    }

    // =========================================================================
    // Initialization
    // =========================================================================

    bool RHIShader::Initialize(
        const Core::Ref<RHIDevice>& device,
        const std::vector<uint32_t>& spirvCode,
        ShaderStage stage,
        const std::string& entryPoint)
    {
        ASSERT(device != nullptr);
        ASSERT(!spirvCode.empty());

        m_Device = device->GetHandle();
        m_Stage = stage;
        m_EntryPoint = entryPoint;

        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = spirvCode.size() * sizeof(uint32_t);
        createInfo.pCode = spirvCode.data();

        VkResult result = vkCreateShaderModule(m_Device, &createInfo, nullptr, &m_ShaderModule);
        if (result != VK_SUCCESS)
        {
            LOG_ERROR("Failed to create VkShaderModule, VkResult: {}", static_cast<int>(result));
            return false;
        }

        LOG_DEBUG("Created shader module: stage={}, entry={}",
            static_cast<int>(stage), entryPoint);

        return true;
    }

    // =========================================================================
    // Destructor
    // =========================================================================

    RHIShader::~RHIShader()
    {
        if (m_ShaderModule != VK_NULL_HANDLE && m_Device != VK_NULL_HANDLE)
        {
            vkDestroyShaderModule(m_Device, m_ShaderModule, nullptr);
            m_ShaderModule = VK_NULL_HANDLE;
            LOG_DEBUG("Destroyed shader module");
        }
    }

    // =========================================================================
    // Public Methods
    // =========================================================================

    VkPipelineShaderStageCreateInfo RHIShader::GetStageInfo() const
    {
        VkPipelineShaderStageCreateInfo stageInfo{};
        stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stageInfo.stage = ToVkShaderStage(m_Stage);
        stageInfo.module = m_ShaderModule;
        stageInfo.pName = m_EntryPoint.c_str();
        stageInfo.pSpecializationInfo = nullptr;

        return stageInfo;
    }

} // namespace RHI
