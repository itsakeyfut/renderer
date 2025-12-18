/**
 * @file RHIShader.h
 * @brief Vulkan Shader Module wrapper for the RHI layer.
 *
 * Manages VkShaderModule creation from SPIR-V bytecode or HLSL source files.
 * Supports runtime HLSL compilation using DXC (DirectX Shader Compiler).
 */

#pragma once

#include "Core/Types.h"
#include "RHI/RHIDevice.h"

#include <vulkan/vulkan.h>

#include <string>
#include <vector>
#include <filesystem>

namespace RHI
{
    /**
     * @brief Shader stage type enumeration
     */
    enum class ShaderStage
    {
        Vertex,         ///< Vertex shader stage
        Fragment,       ///< Fragment (pixel) shader stage
        Compute,        ///< Compute shader stage
        Geometry,       ///< Geometry shader stage
        TessControl,    ///< Tessellation control shader stage
        TessEvaluation  ///< Tessellation evaluation shader stage
    };

    /**
     * @brief Configuration for HLSL shader compilation
     */
    struct ShaderCompileConfig
    {
        /**
         * @brief Entry point function name in the shader
         */
        std::string EntryPoint = "main";

        /**
         * @brief Additional include directories for shader compilation
         */
        std::vector<std::string> IncludeDirs;

        /**
         * @brief Preprocessor macro definitions (name=value format)
         */
        std::vector<std::string> Defines;

        /**
         * @brief Enable debug information in compiled shader
         */
        bool EnableDebugInfo = false;

        /**
         * @brief Optimization level (0-3, where 0 is no optimization)
         */
        int OptimizationLevel = 3;
    };

    /**
     * @brief Vulkan Shader Module wrapper class
     *
     * Manages the shader module lifecycle including:
     * - HLSL to SPIR-V compilation using DXC
     * - SPIR-V bytecode loading
     * - VkShaderModule creation and destruction
     *
     * This class follows RAII principles - resources are cleaned up
     * when the RHIShader object is destroyed.
     *
     * Usage:
     * @code
     * // Create from HLSL file
     * auto vertexShader = RHIShader::CreateFromHLSL(
     *     device, "shaders/hlsl/vertex/triangle.hlsl", ShaderStage::Vertex);
     *
     * // Create from pre-compiled SPIR-V
     * auto fragmentShader = RHIShader::CreateFromSPIRV(
     *     device, spirvCode, ShaderStage::Fragment);
     *
     * // Get stage info for pipeline creation
     * VkPipelineShaderStageCreateInfo stageInfo = shader->GetStageInfo();
     * @endcode
     */
    class RHIShader
    {
    public:
        /**
         * @brief Factory method to create a shader from an HLSL file
         *
         * Compiles the HLSL source to SPIR-V using DXC and creates a VkShaderModule.
         *
         * @param device The RHI device to create the shader module on
         * @param filepath Path to the HLSL source file
         * @param stage The shader stage type
         * @param config Compilation configuration (optional)
         * @return Shared pointer to the created shader, or nullptr on failure
         */
        static Core::Ref<RHIShader> CreateFromHLSL(
            const Core::Ref<RHIDevice>& device,
            const std::filesystem::path& filepath,
            ShaderStage stage,
            const ShaderCompileConfig& config = {});

        /**
         * @brief Factory method to create a shader from SPIR-V bytecode
         *
         * Creates a VkShaderModule directly from pre-compiled SPIR-V code.
         *
         * @param device The RHI device to create the shader module on
         * @param spirvCode Vector of SPIR-V bytecode (uint32_t words)
         * @param stage The shader stage type
         * @param entryPoint The entry point function name (default: "main")
         * @return Shared pointer to the created shader, or nullptr on failure
         */
        static Core::Ref<RHIShader> CreateFromSPIRV(
            const Core::Ref<RHIDevice>& device,
            const std::vector<uint32_t>& spirvCode,
            ShaderStage stage,
            const std::string& entryPoint = "main");

        /**
         * @brief Factory method to create a shader from a SPIR-V file
         *
         * Loads and creates a VkShaderModule from a pre-compiled SPIR-V file.
         *
         * @param device The RHI device to create the shader module on
         * @param filepath Path to the SPIR-V binary file
         * @param stage The shader stage type
         * @param entryPoint The entry point function name (default: "main")
         * @return Shared pointer to the created shader, or nullptr on failure
         */
        static Core::Ref<RHIShader> CreateFromSPIRVFile(
            const Core::Ref<RHIDevice>& device,
            const std::filesystem::path& filepath,
            ShaderStage stage,
            const std::string& entryPoint = "main");

        /**
         * @brief Destructor - destroys VkShaderModule
         */
        ~RHIShader();

        // Non-copyable
        RHIShader(const RHIShader&) = delete;
        RHIShader& operator=(const RHIShader&) = delete;

        // Non-movable (Vulkan handles cannot be safely moved)
        RHIShader(RHIShader&&) = delete;
        RHIShader& operator=(RHIShader&&) = delete;

        /**
         * @brief Get the native VkShaderModule handle
         * @return VkShaderModule handle
         */
        VkShaderModule GetHandle() const { return m_ShaderModule; }

        /**
         * @brief Get the shader stage
         * @return Shader stage type
         */
        ShaderStage GetStage() const { return m_Stage; }

        /**
         * @brief Get the entry point name
         * @return Entry point function name
         */
        const std::string& GetEntryPoint() const { return m_EntryPoint; }

        /**
         * @brief Get the pipeline shader stage create info
         *
         * Returns a fully populated VkPipelineShaderStageCreateInfo structure
         * ready to be used in pipeline creation.
         *
         * @return VkPipelineShaderStageCreateInfo for this shader
         */
        VkPipelineShaderStageCreateInfo GetStageInfo() const;

        /**
         * @brief Check if DXC compiler is available for HLSL compilation
         * @return true if DXC is available, false otherwise
         */
        static bool IsDXCAvailable();

        /**
         * @brief Get the path to the DXC compiler executable
         * @return Path to dxc.exe, or empty string if not found
         */
        static std::string GetDXCPath();

    private:
        /**
         * @brief Private constructor - use factory methods
         */
        RHIShader() = default;

        /**
         * @brief Initialize the shader from SPIR-V bytecode
         * @param device The RHI device
         * @param spirvCode SPIR-V bytecode
         * @param stage Shader stage
         * @param entryPoint Entry point name
         * @return true on success, false on failure
         */
        bool Initialize(
            const Core::Ref<RHIDevice>& device,
            const std::vector<uint32_t>& spirvCode,
            ShaderStage stage,
            const std::string& entryPoint);

        /**
         * @brief Compile HLSL source to SPIR-V using DXC
         * @param filepath Path to HLSL file
         * @param stage Shader stage
         * @param config Compilation configuration
         * @param outSpirvCode Output vector for SPIR-V bytecode
         * @return true on success, false on failure
         */
        static bool CompileHLSLToSPIRV(
            const std::filesystem::path& filepath,
            ShaderStage stage,
            const ShaderCompileConfig& config,
            std::vector<uint32_t>& outSpirvCode);

        /**
         * @brief Read SPIR-V bytecode from a file
         * @param filepath Path to SPIR-V file
         * @param outSpirvCode Output vector for SPIR-V bytecode
         * @return true on success, false on failure
         */
        static bool ReadSPIRVFile(
            const std::filesystem::path& filepath,
            std::vector<uint32_t>& outSpirvCode);

        /**
         * @brief Convert ShaderStage to Vulkan VkShaderStageFlagBits
         * @param stage Shader stage
         * @return Corresponding VkShaderStageFlagBits
         */
        static VkShaderStageFlagBits ToVkShaderStage(ShaderStage stage);

        /**
         * @brief Get the DXC target profile string for a shader stage
         * @param stage Shader stage
         * @return DXC target profile string (e.g., "vs_6_0", "ps_6_0")
         */
        static std::string GetDXCTargetProfile(ShaderStage stage);

        VkDevice m_Device = VK_NULL_HANDLE;
        VkShaderModule m_ShaderModule = VK_NULL_HANDLE;
        ShaderStage m_Stage = ShaderStage::Vertex;
        std::string m_EntryPoint = "main";
    };

} // namespace RHI
