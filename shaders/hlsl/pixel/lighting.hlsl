// Lighting Pass Pixel (Fragment) Shader - Deferred Rendering
// Samples G-Buffer textures and performs full PBR lighting calculation
//
// This shader performs the lighting pass for deferred rendering:
//   1. Sample all G-Buffer textures (Albedo, Normal, Material, Emissive, Depth)
//   2. Reconstruct world position from depth using inverse view-projection
//   3. Calculate PBR lighting (Cook-Torrance BRDF + IBL)
//   4. Apply Cascaded Shadow Maps (CSM) for directional light
//
// G-Buffer Layout (from gbuffer.hlsli):
//   RT0 (Albedo):   R8G8B8A8_SRGB    - RGB = Base color, A = alpha
//   RT1 (Normal):   R16G16_SFLOAT   - RG = Octahedral encoded world normal
//   RT2 (Material): R8G8B8A8_UNORM  - R = Metallic, G = Roughness, B = AO
//   RT3 (Emissive): R16G16B16A16_SFLOAT - RGB = Emissive color
//   Depth:          D32_SFLOAT      - Hardware depth buffer
//
// Descriptor Set Layout:
//   Set 0: G-Buffer textures
//     - binding 0: CameraData UBO (includes inverse matrices)
//     - binding 1: gAlbedo
//     - binding 2: gNormal
//     - binding 3: gMaterial
//     - binding 4: gEmissive
//     - binding 5: gDepth
//   Set 1: Light data
//     - binding 0: LightUBO (directional light + counts)
//     - binding 1: PointLight storage buffer
//     - binding 2: SpotLight storage buffer
//   Set 2: Shadow data
//     - binding 0: CSM shadow map array + comparison sampler
//     - binding 1: CSMParams UBO
//   Set 3: IBL data
//     - binding 0: irradianceMap (TextureCube)
//     - binding 1: prefilteredMap (TextureCube)
//     - binding 2: brdfLUT (Texture2D)
//
// Reference: "Deferred Shading" (NVIDIA GPU Gems 2, Chapter 9)
//            UE: FDeferredLightPixelShader for lighting passes

#include "../common.hlsli"
#include "../gbuffer.hlsli"
#include "../lights.hlsli"
#include "../pbr.hlsli"
#include "../shadow_csm.hlsli"

// ============================================================================
// Camera Data with Inverse Matrices (Set 0, Binding 0)
// ============================================================================
// Extended camera buffer for deferred rendering
// Includes inverse matrices for world position reconstruction
[[vk::binding(0, 0)]]
cbuffer CameraData : register(b0)
{
    float4x4 view;
    float4x4 projection;
    float4x4 viewProjection;
    float4x4 inverseView;
    float4x4 inverseProjection;
    float4x4 inverseViewProjection;
    float3   cameraPosition;
    float    cameraPadding;
    float2   screenSize;
    float2   invScreenSize;
};

// ============================================================================
// G-Buffer Textures (Set 0, Bindings 1-5)
// ============================================================================

// Albedo (Base color + alpha)
[[vk::binding(1, 0)]] [[vk::combinedImageSampler]]
Texture2D<float4> gAlbedo : register(t0);
[[vk::binding(1, 0)]] [[vk::combinedImageSampler]]
SamplerState albedoSampler : register(s0);

// Normal (Octahedral encoded world-space normal)
[[vk::binding(2, 0)]] [[vk::combinedImageSampler]]
Texture2D<float2> gNormal : register(t1);
[[vk::binding(2, 0)]] [[vk::combinedImageSampler]]
SamplerState normalSampler : register(s1);

// Material (Metallic, Roughness, AO)
[[vk::binding(3, 0)]] [[vk::combinedImageSampler]]
Texture2D<float4> gMaterial : register(t2);
[[vk::binding(3, 0)]] [[vk::combinedImageSampler]]
SamplerState materialSampler : register(s2);

// Emissive
[[vk::binding(4, 0)]] [[vk::combinedImageSampler]]
Texture2D<float4> gEmissive : register(t3);
[[vk::binding(4, 0)]] [[vk::combinedImageSampler]]
SamplerState emissiveSampler : register(s3);

// Depth (Hardware depth buffer)
[[vk::binding(5, 0)]] [[vk::combinedImageSampler]]
Texture2D<float> gDepth : register(t4);
[[vk::binding(5, 0)]] [[vk::combinedImageSampler]]
SamplerState depthSampler : register(s4);

// ============================================================================
// Light Data (Set 1)
// ============================================================================

// Light uniform buffer (Set 1, Binding 0)
[[vk::binding(0, 1)]]
cbuffer LightData : register(b1)
{
    DirectionalLight directionalLight;
    uint numPointLights;
    uint numSpotLights;
    float2 lightPadding;
};

// Point light storage buffer (Set 1, Binding 1)
[[vk::binding(1, 1)]]
StructuredBuffer<PointLight> pointLights : register(t5);

// Spot light storage buffer (Set 1, Binding 2)
[[vk::binding(2, 1)]]
StructuredBuffer<SpotLight> spotLights : register(t6);

// ============================================================================
// Shadow Data (Set 2)
// ============================================================================

// CSM shadow map array with comparison sampler (Set 2, Binding 0)
[[vk::binding(0, 2)]] [[vk::combinedImageSampler]]
Texture2DArray<float> shadowMap : register(t10);
[[vk::binding(0, 2)]] [[vk::combinedImageSampler]]
SamplerComparisonState shadowSampler : register(s8);

// CSM parameters uniform buffer (Set 2, Binding 1)
[[vk::binding(1, 2)]]
cbuffer ShadowData : register(b2)
{
    CSMParams csmParams;
};

// ============================================================================
// IBL Data (Set 3)
// ============================================================================

// Irradiance map for diffuse IBL (Set 3, Binding 0)
[[vk::binding(0, 3)]] [[vk::combinedImageSampler]]
TextureCube<float4> irradianceMap : register(t7);
[[vk::binding(0, 3)]] [[vk::combinedImageSampler]]
SamplerState irradianceSampler : register(s5);

// Prefiltered environment map for specular IBL (Set 3, Binding 1)
[[vk::binding(1, 3)]] [[vk::combinedImageSampler]]
TextureCube<float4> prefilteredMap : register(t8);
[[vk::binding(1, 3)]] [[vk::combinedImageSampler]]
SamplerState prefilteredSampler : register(s6);

// BRDF LUT for split-sum approximation (Set 3, Binding 2)
[[vk::binding(2, 3)]] [[vk::combinedImageSampler]]
Texture2D<float4> brdfLUT : register(t9);
[[vk::binding(2, 3)]] [[vk::combinedImageSampler]]
SamplerState brdfSampler : register(s7);

// ============================================================================
// Input Structure
// ============================================================================

struct PSInput
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

// ============================================================================
// World Position Reconstruction
// ============================================================================

/**
 * @brief Reconstruct world position from depth and UV coordinates
 *
 * Uses the inverse view-projection matrix to transform from
 * normalized device coordinates (NDC) back to world space.
 *
 * @param uv Texture coordinates [0,1]
 * @param depth Hardware depth value [0,1] (Vulkan convention: 0=near, 1=far)
 * @return World-space position
 */
float3 ReconstructWorldPos(float2 uv, float depth)
{
    // Convert UV to NDC (Normalized Device Coordinates)
    // UV (0,0) = top-left  -> NDC (-1, 1)
    // UV (1,1) = bottom-right -> NDC (1, -1)
    float4 clipPos;
    clipPos.x = uv.x * 2.0 - 1.0;
    clipPos.y = -(uv.y * 2.0 - 1.0);  // Flip Y for Vulkan (top-down texture coords)
    clipPos.z = depth;                 // Vulkan depth: 0=near, 1=far
    clipPos.w = 1.0;

    // Transform from clip space to world space
    float4 worldPos = mul(inverseViewProjection, clipPos);

    // Perspective divide to get actual world position
    return worldPos.xyz / worldPos.w;
}

// ============================================================================
// Main Entry Point
// ============================================================================

float4 main(PSInput input) : SV_TARGET
{
    // =========================================================================
    // Sample G-Buffer Textures
    // =========================================================================

    // Point sampler for G-Buffer (no interpolation between geometry samples)
    float2 uv = input.TexCoord;

    // Albedo (base color)
    // Stored in SRGB format, hardware automatically converts to linear on read
    float4 albedoSample = gAlbedo.Sample(albedoSampler, uv);
    float3 albedo = albedoSample.rgb;

    // Depth (hardware depth buffer)
    float depth = gDepth.Sample(depthSampler, uv);

    // Early out for sky pixels (depth = 1.0 means no geometry)
    // Return black for sky - compositing pass will add skybox
    if (depth >= 1.0)
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }

    // Normal (octahedral encoded, decode to world space)
    float2 encodedNormal = gNormal.Sample(normalSampler, uv);
    float3 N = DecodeNormal(encodedNormal);

    // Material properties
    // Layout: R = Metallic, G = Roughness, B = AO, A = unused
    float4 materialSample = gMaterial.Sample(materialSampler, uv);
    float metallic = materialSample.r;
    float roughness = materialSample.g;
    float ao = materialSample.b;

    // Clamp roughness to avoid numerical issues (division by zero in GGX)
    roughness = ClampRoughness(roughness);

    // Emissive
    float3 emissive = gEmissive.Sample(emissiveSampler, uv).rgb;

    // =========================================================================
    // Reconstruct World Position
    // =========================================================================

    float3 worldPos = ReconstructWorldPos(uv, depth);

    // View direction (from surface to camera)
    float3 V = normalize(cameraPosition - worldPos);

    // Reflection direction for specular IBL
    float3 R = reflect(-V, N);

    // =========================================================================
    // Create PBR Material
    // =========================================================================

    PBRMaterial material;
    material.albedo = albedo;
    material.metallic = metallic;
    material.roughness = roughness;
    material.ao = ao;
    material.emissive = emissive;

    // =========================================================================
    // Direct Lighting (Cook-Torrance BRDF with GGX)
    // =========================================================================

    float3 Lo = float3(0.0, 0.0, 0.0);

    // -------------------------------------------------------------------------
    // Directional Light with CSM Shadows
    // -------------------------------------------------------------------------
    {
        float3 L = normalize(-directionalLight.Direction);
        float3 radiance = directionalLight.Color * directionalLight.Intensity;

        // Calculate shadow factor using Cascaded Shadow Maps
        // Use reconstructed world position and clip-space depth for cascade selection
        // Convert world position to clip space to get depth for cascade selection
        float4 clipSpacePos = mul(viewProjection, float4(worldPos, 1.0));
        float clipSpaceDepth = clipSpacePos.z / clipSpacePos.w;

        float shadow = CalculateShadowCSM(
            shadowMap,
            shadowSampler,
            csmParams,
            worldPos,
            N,
            L,
            clipSpaceDepth
        );

        // Calculate PBR direct lighting with shadow attenuation
        Lo += CalculatePBRDirect(N, V, L, radiance, material) * shadow;
    }

    // -------------------------------------------------------------------------
    // Point Lights
    // -------------------------------------------------------------------------
    for (uint i = 0; i < numPointLights; ++i)
    {
        PointLight light = pointLights[i];

        // Light vector and distance
        float3 lightVec = light.Position - worldPos;
        float distance = length(lightVec);
        float3 L = lightVec / distance;

        // Distance attenuation (inverse square law with smooth falloff)
        float attenuation = CalculateAttenuation(distance, light.Radius);
        float3 radiance = light.Color * light.Intensity * attenuation;

        // Accumulate PBR lighting contribution
        Lo += CalculatePBRDirect(N, V, L, radiance, material);
    }

    // -------------------------------------------------------------------------
    // Spot Lights
    // -------------------------------------------------------------------------
    for (uint j = 0; j < numSpotLights; ++j)
    {
        SpotLight light = spotLights[j];

        // Light vector and distance
        float3 lightVec = light.Position - worldPos;
        float distance = length(lightVec);
        float3 L = lightVec / distance;

        // Distance attenuation
        float radius = 50.0;  // Default radius for spot lights
        float distanceAttenuation = CalculateAttenuation(distance, radius);

        // Spot cone attenuation
        float spotAttenuation = CalculateSpotAttenuation(
            L,
            normalize(light.Direction),
            light.InnerConeAngle,
            light.OuterConeAngle
        );

        float3 radiance = light.Color * light.Intensity * distanceAttenuation * spotAttenuation;

        // Accumulate PBR lighting contribution
        Lo += CalculatePBRDirect(N, V, L, radiance, material);
    }

    // =========================================================================
    // Image-Based Lighting (IBL)
    // =========================================================================
    // Split-sum approximation for real-time IBL
    // Reference: "Real Shading in Unreal Engine 4" (Brian Karis, SIGGRAPH 2013)

    // Calculate F0 (base reflectivity at normal incidence)
    // Dielectrics: 0.04 (4% reflectance)
    // Metals: use albedo color (tinted reflections)
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), material.albedo, material.metallic);

    // NdotV for Fresnel and BRDF LUT lookup
    float NdotV = max(dot(N, V), 0.0);

    // Fresnel with roughness consideration for IBL
    float3 F = FresnelSchlickRoughness(NdotV, F0, material.roughness);

    // Energy conservation for diffuse/specular split
    float3 kS = F;
    float3 kD = (1.0 - kS) * (1.0 - material.metallic);

    // Diffuse IBL - sample irradiance map using normal direction
    float3 irradiance = irradianceMap.Sample(irradianceSampler, N).rgb;
    float3 diffuseIBL = irradiance * material.albedo;

    // Specular IBL - sample prefiltered environment map using reflection
    // Higher roughness samples from higher mip levels (more blurred)
    float3 prefilteredColor = prefilteredMap.SampleLevel(
        prefilteredSampler,
        R,
        material.roughness * MAX_REFLECTION_LOD
    ).rgb;

    // BRDF LUT lookup: U = NdotV, V = roughness
    // R = F0 scale, G = F0 bias
    float2 brdf = brdfLUT.Sample(brdfSampler, float2(NdotV, material.roughness)).rg;
    float3 specularIBL = prefilteredColor * (F0 * brdf.x + brdf.y);

    // Combine diffuse and specular IBL with ambient occlusion
    float3 ambient = (kD * diffuseIBL + specularIBL) * material.ao;

    // =========================================================================
    // Final Color Composition
    // =========================================================================
    // Combine all lighting contributions:
    //   - ambient: IBL contribution (diffuse + specular, with AO)
    //   - Lo: Direct lighting (directional + point + spot, with shadows)
    //   - emissive: Self-emission (unaffected by lighting)

    float3 color = ambient + Lo + material.emissive;

    // Output final color (alpha = 1.0 for opaque lighting pass)
    return float4(color, 1.0);
}
