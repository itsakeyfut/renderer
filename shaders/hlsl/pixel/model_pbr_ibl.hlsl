// Model Pixel (Fragment) Shader - Full PBR with Image-Based Lighting (IBL)
// Cook-Torrance BRDF with GGX distribution, texture support, dynamic lighting, and IBL
//
// This shader integrates:
//   - Direct lighting (directional, point, spot lights) with Cook-Torrance BRDF
//   - Image-Based Lighting (IBL) using split-sum approximation
//   - Full PBR material support (albedo, normal, metallic-roughness, AO, emissive)
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
//   Set 2: Light data
//     - binding 0: LightUBO (directional light + counts)
//     - binding 1: PointLight storage buffer
//     - binding 2: SpotLight storage buffer
//   Set 3: IBL data
//     - binding 0: irradianceMap (TextureCube)
//     - binding 1: prefilteredMap (TextureCube)
//     - binding 2: brdfLUT (Texture2D)
//
// Reference: "Real Shading in Unreal Engine 4" (Brian Karis, SIGGRAPH 2013)
//            UE: Default Lit Shading Model uses the same implementation

#include "../lights.hlsli"
#include "../pbr.hlsli"

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

// Light uniform buffer (Set 2, Binding 0)
[[vk::binding(0, 2)]]
cbuffer LightData : register(b2)
{
    DirectionalLight directionalLight;
    uint numPointLights;
    uint numSpotLights;
    float2 lightPadding;
};

// Point light storage buffer (Set 2, Binding 1)
[[vk::binding(1, 2)]]
StructuredBuffer<PointLight> pointLights : register(t5);

// Spot light storage buffer (Set 2, Binding 2)
[[vk::binding(2, 2)]]
StructuredBuffer<SpotLight> spotLights : register(t6);

// ============================================================================
// IBL Textures (Set 3)
// ============================================================================

// Irradiance map for diffuse IBL (Set 3, Binding 0)
// Pre-convolved environment map containing average radiance from all directions
[[vk::binding(0, 3)]] [[vk::combinedImageSampler]]
TextureCube<float4> irradianceMap : register(t7);
[[vk::binding(0, 3)]] [[vk::combinedImageSampler]]
SamplerState irradianceSampler : register(s5);

// Prefiltered environment map for specular IBL (Set 3, Binding 1)
// Mip-mapped cubemap with roughness-based filtering
// Mip 0 = roughness 0 (mirror), higher mips = higher roughness
[[vk::binding(1, 3)]] [[vk::combinedImageSampler]]
TextureCube<float4> prefilteredMap : register(t8);
[[vk::binding(1, 3)]] [[vk::combinedImageSampler]]
SamplerState prefilteredSampler : register(s6);

// BRDF LUT for split-sum approximation (Set 3, Binding 2)
// 2D texture: U = NdotV, V = roughness
// R = F0 scale, G = F0 bias
[[vk::binding(2, 3)]] [[vk::combinedImageSampler]]
Texture2D<float2> brdfLUT : register(t9);
[[vk::binding(2, 3)]] [[vk::combinedImageSampler]]
SamplerState brdfSampler : register(s7);

struct PSInput
{
    float4 Position    : SV_POSITION;
    float3 WorldPos    : TEXCOORD0;
    float3 Normal      : TEXCOORD1;
    float2 TexCoord    : TEXCOORD2;
    float3 Tangent     : TEXCOORD3;
    float3 Bitangent   : TEXCOORD4;
};

// Sample and transform normal from normal map to world space
float3 GetWorldNormal(PSInput input)
{
    float3 N = normalize(input.Normal);

    // Check if normal texture is bound
    if (hasNormalTexture == 0)
        return N;

    // Sample normal map (tangent space normal)
    float3 normalSample = normalMap.Sample(normalSampler, input.TexCoord).rgb;

    // Check if it's a valid normal map (not default white texture)
    if (length(normalSample - float3(1.0, 1.0, 1.0)) < 0.01)
        return N;

    // Convert from [0,1] to [-1,1]
    normalSample = normalSample * 2.0 - 1.0;

    // Apply normal scale
    normalSample.xy *= normalScale;
    normalSample = normalize(normalSample);

    // Build TBN matrix (Tangent, Bitangent, Normal)
    float3 T = normalize(input.Tangent);
    float3 B = normalize(input.Bitangent);
    float3x3 TBN = float3x3(T, B, N);

    // Transform from tangent space to world space
    return normalize(mul(normalSample, TBN));
}

float4 main(PSInput input) : SV_TARGET
{
    // =========================================================================
    // Sample textures and apply factors
    // =========================================================================

    // Base color (albedo)
    float4 baseColor;
    if (hasBaseColorTexture != 0)
    {
        baseColor = albedoMap.Sample(albedoSampler, input.TexCoord) * baseColorFactor;
    }
    else
    {
        baseColor = baseColorFactor;
    }

    // Alpha cutoff test
    if (baseColor.a < alphaCutoff)
    {
        discard;
    }

    float3 albedo = baseColor.rgb;

    // Metallic-Roughness (glTF: roughness in G, metallic in B)
    float metallic = metallicFactor;
    float roughness = roughnessFactor;
    if (hasMetallicRoughnessTexture != 0)
    {
        float4 mrSample = metallicRoughnessMap.Sample(metallicRoughnessSampler, input.TexCoord);
        roughness *= mrSample.g;
        metallic *= mrSample.b;
    }

    // Ambient Occlusion
    float ao = ambientOcclusionFactor;
    if (hasOcclusionTexture != 0)
    {
        ao *= occlusionMap.Sample(occlusionSampler, input.TexCoord).r;
    }

    // Emissive
    float3 emissive = emissiveFactor;
    if (hasEmissiveTexture != 0)
    {
        emissive *= emissiveMap.Sample(emissiveSampler, input.TexCoord).rgb;
    }

    // =========================================================================
    // Lighting calculation (Cook-Torrance BRDF with GGX)
    // =========================================================================

    // Get world space normal (with normal mapping if available)
    float3 N = GetWorldNormal(input);

    // View direction (from surface to camera)
    float3 V = normalize(cameraPosition - input.WorldPos);

    // Reflection direction for specular IBL
    float3 R = reflect(-V, N);

    // Clamp roughness to avoid numerical issues
    roughness = ClampRoughness(roughness);

    // Create PBRMaterial struct for Metallic-Roughness Workflow
    PBRMaterial material;
    material.albedo = albedo;
    material.metallic = metallic;
    material.roughness = roughness;
    material.ao = ao;
    material.emissive = emissive;

    // =========================================================================
    // Direct Lighting (from all light sources)
    // =========================================================================
    float3 Lo = float3(0.0, 0.0, 0.0);

    // -------------------------------------------------------------------------
    // Directional light (from LightUBO)
    // -------------------------------------------------------------------------
    {
        float3 L = normalize(-directionalLight.Direction);
        float3 radiance = directionalLight.Color * directionalLight.Intensity;

        // Calculate PBR direct lighting using Cook-Torrance BRDF
        Lo += CalculatePBRDirect(N, V, L, radiance, material);
    }

    // -------------------------------------------------------------------------
    // Point lights (from storage buffer)
    // -------------------------------------------------------------------------
    for (uint i = 0; i < numPointLights; ++i)
    {
        PointLight light = pointLights[i];

        // Calculate light direction and distance
        float3 lightVec = light.Position - input.WorldPos;
        float distance = length(lightVec);
        float3 L = lightVec / distance;

        // Attenuation (inverse square law with smooth falloff)
        float attenuation = CalculateAttenuation(distance, light.Radius);
        float3 radiance = light.Color * light.Intensity * attenuation;

        // Calculate PBR direct lighting using Cook-Torrance BRDF
        Lo += CalculatePBRDirect(N, V, L, radiance, material);
    }

    // -------------------------------------------------------------------------
    // Spot lights (from storage buffer)
    // -------------------------------------------------------------------------
    for (uint j = 0; j < numSpotLights; ++j)
    {
        SpotLight light = spotLights[j];

        // Calculate light direction and distance
        float3 lightVec = light.Position - input.WorldPos;
        float distance = length(lightVec);
        float3 L = lightVec / distance;

        // Distance attenuation
        float radius = 50.0; // Default radius for spot lights
        float distanceAttenuation = CalculateAttenuation(distance, radius);

        // Spot cone attenuation
        float spotAttenuation = CalculateSpotAttenuation(
            L,
            normalize(light.Direction),
            light.InnerConeAngle,
            light.OuterConeAngle);

        float3 radiance = light.Color * light.Intensity * distanceAttenuation * spotAttenuation;

        // Calculate PBR direct lighting using Cook-Torrance BRDF
        Lo += CalculatePBRDirect(N, V, L, radiance, material);
    }

    // =========================================================================
    // IBL (Image-Based Lighting) - Ambient from environment
    // =========================================================================
    // Replace simple hemisphere ambient with full IBL calculation
    // Uses irradiance map for diffuse and prefiltered map + BRDF LUT for specular
    float3 ambient = CalculateIBL(
        N,
        V,
        R,
        material,
        irradianceMap,
        prefilteredMap,
        brdfLUT,
        irradianceSampler  // Use the linear sampler for all IBL sampling
    );

    // =========================================================================
    // Final color composition
    // =========================================================================
    // Combine:
    //   - ambient: IBL contribution (diffuse + specular, with AO applied)
    //   - Lo: Direct lighting contribution (directional + point + spot)
    //   - emissive: Self-emission (added last, no AO or other modifiers)
    float3 color = ambient + Lo + material.emissive;

    return float4(color, baseColor.a);
}
