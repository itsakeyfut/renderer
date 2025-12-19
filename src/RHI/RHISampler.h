/**
 * @file RHISampler.h
 * @brief Vulkan Sampler wrapper for the RHI layer.
 *
 * Manages VkSampler for texture sampling with configurable
 * filtering, addressing, and anisotropy settings.
 */

#pragma once

#include "Core/Types.h"

#include <vulkan/vulkan.h>

namespace RHI
{
    // Forward declarations
    class RHIDevice;

    /**
     * @brief Texture filtering mode
     */
    enum class FilterMode
    {
        Nearest,    ///< Nearest neighbor (no filtering)
        Linear,     ///< Bilinear filtering
        Cubic       ///< Cubic filtering (requires extension)
    };

    /**
     * @brief Texture addressing/wrap mode
     */
    enum class AddressMode
    {
        Repeat,         ///< Tile the texture
        MirroredRepeat, ///< Tile with mirroring at boundaries
        ClampToEdge,    ///< Clamp to edge pixels
        ClampToBorder,  ///< Use border color
        MirrorClampToEdge ///< Mirror once then clamp
    };

    /**
     * @brief Border color for ClampToBorder address mode
     */
    enum class BorderColor
    {
        TransparentBlack,   ///< (0, 0, 0, 0)
        OpaqueBlack,        ///< (0, 0, 0, 1)
        OpaqueWhite         ///< (1, 1, 1, 1)
    };

    /**
     * @brief Configuration for sampler creation
     */
    struct SamplerDesc
    {
        /**
         * @brief Magnification filter (when texture is enlarged)
         */
        FilterMode MagFilter = FilterMode::Linear;

        /**
         * @brief Minification filter (when texture is shrunk)
         */
        FilterMode MinFilter = FilterMode::Linear;

        /**
         * @brief Mipmap filter mode
         */
        FilterMode MipmapMode = FilterMode::Linear;

        /**
         * @brief U (horizontal) address mode
         */
        AddressMode AddressModeU = AddressMode::Repeat;

        /**
         * @brief V (vertical) address mode
         */
        AddressMode AddressModeV = AddressMode::Repeat;

        /**
         * @brief W (depth) address mode
         */
        AddressMode AddressModeW = AddressMode::Repeat;

        /**
         * @brief Enable anisotropic filtering
         */
        bool AnisotropyEnable = true;

        /**
         * @brief Maximum anisotropy level (1.0 to 16.0)
         */
        float MaxAnisotropy = 16.0f;

        /**
         * @brief Mip LOD bias
         */
        float MipLodBias = 0.0f;

        /**
         * @brief Minimum mip level
         */
        float MinLod = 0.0f;

        /**
         * @brief Maximum mip level (VK_LOD_CLAMP_NONE for no clamping)
         */
        float MaxLod = VK_LOD_CLAMP_NONE;

        /**
         * @brief Border color for ClampToBorder mode
         */
        BorderColor Border = BorderColor::OpaqueBlack;

        /**
         * @brief Enable comparison sampling (for shadow maps)
         */
        bool CompareEnable = false;

        /**
         * @brief Comparison operator for shadow mapping
         */
        VkCompareOp CompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

        /**
         * @brief Debug name for the sampler
         */
        const char* DebugName = nullptr;
    };

    /**
     * @brief Vulkan Sampler wrapper class
     *
     * Manages VkSampler lifecycle with configurable filtering
     * and addressing modes.
     *
     * Usage:
     * @code
     * SamplerDesc desc;
     * desc.MagFilter = FilterMode::Linear;
     * desc.MinFilter = FilterMode::Linear;
     * desc.AnisotropyEnable = true;
     *
     * auto sampler = RHISampler::Create(device, desc);
     *
     * // Use in descriptor set
     * VkDescriptorImageInfo imageInfo{};
     * imageInfo.sampler = sampler->GetHandle();
     * imageInfo.imageView = texture->GetImageView();
     * imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
     * @endcode
     */
    class RHISampler
    {
    public:
        /**
         * @brief Factory method to create a sampler
         * @param device The logical device
         * @param desc Sampler description
         * @return Shared pointer to the created sampler, or nullptr on failure
         */
        static Core::Ref<RHISampler> Create(
            const Core::Ref<RHIDevice>& device,
            const SamplerDesc& desc);

        /**
         * @brief Create a default linear sampler with anisotropy
         * @param device The logical device
         * @return Shared pointer to the created sampler
         */
        static Core::Ref<RHISampler> CreateLinear(const Core::Ref<RHIDevice>& device);

        /**
         * @brief Create a nearest-neighbor sampler (no filtering)
         * @param device The logical device
         * @return Shared pointer to the created sampler
         */
        static Core::Ref<RHISampler> CreateNearest(const Core::Ref<RHIDevice>& device);

        /**
         * @brief Create a shadow map sampler with depth comparison
         * @param device The logical device
         * @return Shared pointer to the created sampler
         */
        static Core::Ref<RHISampler> CreateShadow(const Core::Ref<RHIDevice>& device);

        /**
         * @brief Destructor - destroys VkSampler
         */
        ~RHISampler();

        // Non-copyable
        RHISampler(const RHISampler&) = delete;
        RHISampler& operator=(const RHISampler&) = delete;

        // Non-movable
        RHISampler(RHISampler&&) = delete;
        RHISampler& operator=(RHISampler&&) = delete;

        /**
         * @brief Get the native VkSampler handle
         * @return VkSampler handle
         */
        VkSampler GetHandle() const { return m_Sampler; }

    private:
        /**
         * @brief Private constructor - use Create() factory method
         */
        RHISampler() = default;

        /**
         * @brief Initialize the sampler
         * @param device The logical device
         * @param desc Sampler description
         * @return true on success, false on failure
         */
        bool Initialize(
            const Core::Ref<RHIDevice>& device,
            const SamplerDesc& desc);

        /**
         * @brief Convert FilterMode to VkFilter
         * @param mode FilterMode enum value
         * @return Corresponding VkFilter
         */
        static VkFilter ToVkFilter(FilterMode mode);

        /**
         * @brief Convert FilterMode to VkSamplerMipmapMode
         * @param mode FilterMode enum value
         * @return Corresponding VkSamplerMipmapMode
         */
        static VkSamplerMipmapMode ToVkMipmapMode(FilterMode mode);

        /**
         * @brief Convert AddressMode to VkSamplerAddressMode
         * @param mode AddressMode enum value
         * @return Corresponding VkSamplerAddressMode
         */
        static VkSamplerAddressMode ToVkAddressMode(AddressMode mode);

        /**
         * @brief Convert BorderColor to VkBorderColor
         * @param color BorderColor enum value
         * @return Corresponding VkBorderColor
         */
        static VkBorderColor ToVkBorderColor(BorderColor color);

        VkDevice m_Device = VK_NULL_HANDLE;
        VkSampler m_Sampler = VK_NULL_HANDLE;
    };

} // namespace RHI
