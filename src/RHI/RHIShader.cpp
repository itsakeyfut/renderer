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
#include <cctype>
#include <cstdlib>
#include <cstring>

#if defined(_WIN32)
    #include <windows.h>
#else
    #include <spawn.h>
    #include <sys/wait.h>
    extern char** environ;
#endif

namespace RHI
{
    // =========================================================================
    // Input Validation Helpers
    // =========================================================================
    // NOTE: These validation functions help mitigate command injection risks.
    // The shader compilation inputs (EntryPoint, IncludeDirs, Defines, filepath)
    // are expected to come from trusted sources (developer-controlled config files
    // or build systems). However, we still validate them as defense-in-depth.

    /**
     * @brief Check if a string is a valid HLSL/C identifier
     * @param name The identifier to validate
     * @return true if valid (alphanumeric + underscore, not starting with digit)
     */
    static bool IsValidIdentifier(const std::string& name)
    {
        if (name.empty())
        {
            return false;
        }

        // First character must be letter or underscore
        if (!std::isalpha(static_cast<unsigned char>(name[0])) && name[0] != '_')
        {
            return false;
        }

        // Remaining characters must be alphanumeric or underscore
        for (size_t i = 1; i < name.size(); ++i)
        {
            char c = name[i];
            if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_')
            {
                return false;
            }
        }

        return true;
    }

    /**
     * @brief Check if a string contains shell metacharacters that could enable injection
     * @param str The string to check
     * @return true if the string contains dangerous characters
     */
    static bool ContainsShellMetacharacters(const std::string& str)
    {
        // Characters that could enable command injection in shell contexts
        static const char* dangerousChars = ";|&`$<>(){}[]!#~\n\r";

        for (char c : str)
        {
            if (std::strchr(dangerousChars, c) != nullptr)
            {
                return true;
            }
        }

        return false;
    }

    /**
     * @brief Validate a macro definition string (NAME or NAME=VALUE format)
     * @param define The define string to validate
     * @return true if valid
     */
    static bool IsValidDefine(const std::string& define)
    {
        if (define.empty())
        {
            return false;
        }

        // Check for shell metacharacters
        if (ContainsShellMetacharacters(define))
        {
            return false;
        }

        // Find the macro name (before '=' if present)
        size_t eqPos = define.find('=');
        std::string macroName = (eqPos != std::string::npos) ? define.substr(0, eqPos) : define;

        // Macro name must be a valid identifier
        return IsValidIdentifier(macroName);
    }

    /**
     * @brief Validate a file path for use in command line
     * @param path The path to validate
     * @return true if the path is safe to use
     */
    static bool IsValidPath(const std::string& path)
    {
        if (path.empty())
        {
            return false;
        }

        return !ContainsShellMetacharacters(path);
    }

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

        // Validate inputs to prevent command injection
        if (!IsValidIdentifier(config.EntryPoint))
        {
            LOG_ERROR("Invalid entry point name: '{}'. Must be a valid identifier.", config.EntryPoint);
            return false;
        }

        if (!IsValidPath(filepath.string()))
        {
            LOG_ERROR("Invalid characters in shader path: {}", filepath.string());
            return false;
        }

        for (const auto& includeDir : config.IncludeDirs)
        {
            if (!IsValidPath(includeDir))
            {
                LOG_ERROR("Invalid characters in include directory: {}", includeDir);
                return false;
            }
        }

        for (const auto& define : config.Defines)
        {
            if (!IsValidDefine(define))
            {
                LOG_ERROR("Invalid macro definition: '{}'. Must be NAME or NAME=VALUE format.", define);
                return false;
            }
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
        // Use posix_spawn to execute DXC without shell interpretation
        // Build argument vector
        std::vector<std::string> argStrings;
        argStrings.push_back(dxcPath);
        argStrings.push_back("-T");
        argStrings.push_back(GetDXCTargetProfile(stage));
        argStrings.push_back("-E");
        argStrings.push_back(config.EntryPoint);
        argStrings.push_back("-spirv");
        argStrings.push_back("-fspv-target-env=vulkan1.3");

        if (config.OptimizationLevel == 0)
        {
            argStrings.push_back("-Od");
        }
        else
        {
            argStrings.push_back("-O" + std::to_string(config.OptimizationLevel));
        }

        if (config.EnableDebugInfo)
        {
            argStrings.push_back("-Zi");
            argStrings.push_back("-fspv-debug=vulkan-with-source");
        }

        for (const auto& includeDir : config.IncludeDirs)
        {
            argStrings.push_back("-I");
            argStrings.push_back(includeDir);
        }

        argStrings.push_back("-I");
        argStrings.push_back(filepath.parent_path().string());

        for (const auto& define : config.Defines)
        {
            argStrings.push_back("-D");
            argStrings.push_back(define);
        }

        argStrings.push_back(filepath.string());
        argStrings.push_back("-Fo");
        argStrings.push_back(outputPath.string());

        // Convert to char* array for posix_spawn
        std::vector<char*> argv;
        for (auto& arg : argStrings)
        {
            argv.push_back(arg.data());
        }
        argv.push_back(nullptr);

        pid_t pid;
        int spawnResult = posix_spawn(&pid, dxcPath.c_str(), nullptr, nullptr, argv.data(), environ);
        if (spawnResult != 0)
        {
            LOG_ERROR("Failed to spawn DXC compiler (error code: {})", spawnResult);
            return false;
        }

        int status;
        if (waitpid(pid, &status, 0) == -1)
        {
            LOG_ERROR("Failed to wait for DXC compiler process");
            return false;
        }

        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
        {
            int exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
            LOG_ERROR("DXC compilation failed with exit code: {}", exitCode);
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
