// G-Buffer Pixel (Fragment) Shader
// Outputs geometry information to G-Buffer for deferred lighting
//
// This shader writes material and geometry data to multiple render targets:
//   - RT0 (Albedo):   Base color from albedo texture * factor
//   - RT1 (Normal):   World-space normal encoded with octahedral encoding
//   - RT2 (Material): Metallic, roughness, and AO packed
//   - RT3 (Emissive): Emissive color * factor
//
// Normal mapping is applied before encoding the normal.
// No lighting calculation is performed - deferred to the lighting pass.
//
// Descriptor Set Layout:
//   Set 0: Scene data
//     - binding 0: CameraData (VS+PS)
//     - binding 1: ObjectData (VS)
//   Set 1: Material data
//     - binding 0: MaterialData UBO
//     - binding 1: albedoMap
//     - binding 2: normalMap
//     - binding 3: metallicRoughnessMap
//     - binding 4: occlusionMap
//     - binding 5: emissiveMap
//
// Reference: "A Survey of Efficient Representations for Independent Unit Vectors"
//            (Cigolle et al., JCGT 2014) for octahedral encoding

#include "../gbuffer.hlsli"

// Camera uniform buffer (Set 0, Binding 0)
[[vk::binding(0, 0)]]
cbuffer CameraData : register(b0)
{
    float4x4 view;
    float4x4 projection;
    float4x4 viewProjection;
    float3 cameraPosition;
    float cameraPadding;
};

// Material uniform buffer (Set 1, Binding 0)
[[vk::binding(0, 1)]]
cbuffer MaterialData : register(b1)
{
    float4 baseColorFactor;
    float metallicFactor;
    float roughnessFactor;
    float ambientOcclusionFactor;
    float normalScale;
    float3 emissiveFactor;
    float alphaCutoff;

    // Texture presence flags
    int hasBaseColorTexture;
    int hasNormalTexture;
    int hasMetallicRoughnessTexture;
    int hasOcclusionTexture;
    int hasEmissiveTexture;
    int3 materialPadding;
};

// Combined image samplers (Set 1, Bindings 1-5)
[[vk::binding(1, 1)]] [[vk::combinedImageSampler]]
Texture2D albedoMap : register(t0);
[[vk::binding(1, 1)]] [[vk::combinedImageSampler]]
SamplerState albedoSampler : register(s0);

[[vk::binding(2, 1)]] [[vk::combinedImageSampler]]
Texture2D normalMap : register(t1);
[[vk::binding(2, 1)]] [[vk::combinedImageSampler]]
SamplerState normalSampler : register(s1);

[[vk::binding(3, 1)]] [[vk::combinedImageSampler]]
Texture2D metallicRoughnessMap : register(t2);
[[vk::binding(3, 1)]] [[vk::combinedImageSampler]]
SamplerState metallicRoughnessSampler : register(s2);

[[vk::binding(4, 1)]] [[vk::combinedImageSampler]]
Texture2D occlusionMap : register(t3);
[[vk::binding(4, 1)]] [[vk::combinedImageSampler]]
SamplerState occlusionSampler : register(s3);

[[vk::binding(5, 1)]] [[vk::combinedImageSampler]]
Texture2D emissiveMap : register(t4);
[[vk::binding(5, 1)]] [[vk::combinedImageSampler]]
SamplerState emissiveSampler : register(s4);

struct PSInput
{
    float4 Position    : SV_POSITION;  // Clip-space position
    float3 WorldPos    : TEXCOORD0;    // World-space position
    float3 Normal      : TEXCOORD1;    // World-space normal
    float2 TexCoord    : TEXCOORD2;    // Texture coordinates
    float3 Tangent     : TEXCOORD3;    // World-space tangent
    float3 Bitangent   : TEXCOORD4;    // World-space bitangent
};

/**
 * @brief Sample and transform normal from normal map to world space
 *
 * If a normal map is bound, samples the tangent-space normal and
 * transforms it to world space using the TBN matrix.
 *
 * @param input Interpolated vertex data
 * @return Normalized world-space normal
 */
float3 GetWorldNormal(PSInput input)
{
    float3 N = normalize(input.Normal);

    // If no normal texture is bound, use the vertex normal
    if (hasNormalTexture == 0)
    {
        return N;
    }

    // Sample tangent-space normal from normal map
    float3 normalSample = normalMap.Sample(normalSampler, input.TexCoord).rgb;

    // Check if it's a valid normal map (not default white texture)
    // Default white textures sample as (1, 1, 1) which is invalid for normals
    if (length(normalSample - float3(1.0, 1.0, 1.0)) < 0.01)
    {
        return N;
    }

    // Convert from [0,1] to [-1,1] range
    normalSample = normalSample * 2.0 - 1.0;

    // Apply normal scale factor (controls normal map intensity)
    normalSample.xy *= normalScale;
    normalSample = normalize(normalSample);

    // Build TBN matrix (Tangent, Bitangent, Normal)
    float3 T = normalize(input.Tangent);
    float3 B = normalize(input.Bitangent);
    float3x3 TBN = float3x3(T, B, N);

    // Transform from tangent space to world space
    return normalize(mul(normalSample, TBN));
}

GBufferOutput main(PSInput input)
{
    GBufferOutput output;

    // =========================================================================
    // Albedo (RT0)
    // =========================================================================
    // Sample base color texture and multiply by factor
    float4 baseColor;
    if (hasBaseColorTexture != 0)
    {
        baseColor = albedoMap.Sample(albedoSampler, input.TexCoord) * baseColorFactor;
    }
    else
    {
        baseColor = baseColorFactor;
    }

    // Alpha cutoff test for alpha-tested materials
    if (baseColor.a < alphaCutoff)
    {
        discard;
    }

    output.Albedo = baseColor;

    // =========================================================================
    // Normal (RT1)
    // =========================================================================
    // Get world-space normal (with normal mapping applied)
    float3 worldNormal = GetWorldNormal(input);

    // Encode normal using octahedral encoding for efficient storage
    // This compresses the 3D normal to 2 components with minimal precision loss
    output.Normal = EncodeNormal(worldNormal);

    // =========================================================================
    // Material (RT2)
    // =========================================================================
    // Sample metallic-roughness texture (glTF format: roughness in G, metallic in B)
    float metallic = metallicFactor;
    float roughness = roughnessFactor;
    if (hasMetallicRoughnessTexture != 0)
    {
        float4 mrSample = metallicRoughnessMap.Sample(metallicRoughnessSampler, input.TexCoord);
        roughness *= mrSample.g;  // Green channel = roughness
        metallic *= mrSample.b;   // Blue channel = metallic
    }

    // Sample ambient occlusion texture
    float ao = ambientOcclusionFactor;
    if (hasOcclusionTexture != 0)
    {
        ao *= occlusionMap.Sample(occlusionSampler, input.TexCoord).r;
    }

    // Pack material properties: R = Metallic, G = Roughness, B = AO, A = unused
    output.Material = float4(metallic, roughness, ao, 1.0);

    // =========================================================================
    // Emissive (RT3)
    // =========================================================================
    // Sample emissive texture and multiply by factor
    float3 emissive = emissiveFactor;
    if (hasEmissiveTexture != 0)
    {
        emissive *= emissiveMap.Sample(emissiveSampler, input.TexCoord).rgb;
    }

    output.Emissive = float4(emissive, 1.0);

    return output;
}
