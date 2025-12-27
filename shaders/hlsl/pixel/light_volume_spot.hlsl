// Light Volume Spot Light Pixel Shader
// Performs deferred lighting calculations for a single spot light using
// the light volume technique. Only pixels that pass the stencil test
// (inside the light's bounding cone but behind geometry) are shaded.
//
// G-Buffer Sampling:
//   The pixel's screen position is used to calculate UV coordinates for
//   sampling the G-Buffer textures (albedo, normal, material, depth).
//
// Lighting Calculation:
//   Uses Cook-Torrance BRDF with GGX distribution for PBR lighting.
//   Applies spot light cone attenuation based on inner/outer angles.
//   The light index from push constants identifies which spot light to use.

#include "../common.hlsli"
#include "../gbuffer.hlsli"
#include "../lights.hlsli"
#include "../pbr.hlsli"

// ============================================================================
// Push Constants
// ============================================================================

struct PushConstants
{
    float4x4 ModelViewProjection;
    float4x4 Model;
    uint     LightIndex;
    uint3    Padding;
};

[[vk::push_constant]]
ConstantBuffer<PushConstants> pushConstants;

// ============================================================================
// Camera Data (Set 0, Binding 0)
// ============================================================================

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

[[vk::binding(1, 0)]] [[vk::combinedImageSampler]]
Texture2D<float4> gAlbedo : register(t0);
[[vk::binding(1, 0)]] [[vk::combinedImageSampler]]
SamplerState albedoSampler : register(s0);

[[vk::binding(2, 0)]] [[vk::combinedImageSampler]]
Texture2D<float2> gNormal : register(t1);
[[vk::binding(2, 0)]] [[vk::combinedImageSampler]]
SamplerState normalSampler : register(s1);

[[vk::binding(3, 0)]] [[vk::combinedImageSampler]]
Texture2D<float4> gMaterial : register(t2);
[[vk::binding(3, 0)]] [[vk::combinedImageSampler]]
SamplerState materialSampler : register(s2);

[[vk::binding(5, 0)]] [[vk::combinedImageSampler]]
Texture2D<float> gDepth : register(t4);
[[vk::binding(5, 0)]] [[vk::combinedImageSampler]]
SamplerState depthSampler : register(s4);

// ============================================================================
// Light Data (Set 1)
// ============================================================================

[[vk::binding(2, 1)]]
StructuredBuffer<SpotLight> spotLights : register(t6);

// ============================================================================
// Input Structure
// ============================================================================

struct PSInput
{
    float4 Position     : SV_POSITION;
    float3 WorldPos     : WORLD_POS;
    float4 ScreenPos    : SCREEN_POS;
};

// ============================================================================
// World Position Reconstruction
// ============================================================================

float3 ReconstructWorldPos(float2 uv, float depth)
{
    float4 clipPos;
    clipPos.x = uv.x * 2.0 - 1.0;
    clipPos.y = -(uv.y * 2.0 - 1.0);
    clipPos.z = depth;
    clipPos.w = 1.0;

    float4 worldPos = mul(inverseViewProjection, clipPos);
    return worldPos.xyz / worldPos.w;
}

// ============================================================================
// Main Entry Point
// ============================================================================

float4 main(PSInput input) : SV_TARGET
{
    // Calculate UV from screen position (perspective divide + NDC to UV)
    float2 ndc = input.ScreenPos.xy / input.ScreenPos.w;
    float2 uv = float2((ndc.x + 1.0) * 0.5, (-ndc.y + 1.0) * 0.5);

    // Sample G-Buffer
    float depth = gDepth.Sample(depthSampler, uv);

    // Early out for sky pixels
    if (depth >= 1.0)
    {
        discard;
    }

    float4 albedoSample = gAlbedo.Sample(albedoSampler, uv);
    float3 albedo = albedoSample.rgb;

    float2 encodedNormal = gNormal.Sample(normalSampler, uv);
    float3 N = DecodeNormal(encodedNormal);

    float4 materialSample = gMaterial.Sample(materialSampler, uv);
    float metallic = materialSample.r;
    float roughness = ClampRoughness(materialSample.g);
    float ao = materialSample.b;

    // Reconstruct world position
    float3 worldPos = ReconstructWorldPos(uv, depth);

    // View direction
    float3 V = normalize(cameraPosition - worldPos);

    // Get spot light data
    SpotLight light = spotLights[pushConstants.LightIndex];

    // Light vector and distance
    float3 lightVec = light.Position - worldPos;
    float distance = length(lightVec);
    float3 L = lightVec / distance;

    // Distance attenuation (using default radius for spot lights)
    float radius = 50.0;
    float distanceAttenuation = CalculateAttenuation(distance, radius);

    // Spot cone attenuation
    float spotAttenuation = CalculateSpotAttenuation(
        L,
        normalize(light.Direction),
        light.InnerConeAngle,
        light.OuterConeAngle
    );

    float3 radiance = light.Color * light.Intensity * distanceAttenuation * spotAttenuation;

    // Create PBR material
    PBRMaterial material;
    material.albedo = albedo;
    material.metallic = metallic;
    material.roughness = roughness;
    material.ao = ao;
    material.emissive = float3(0.0, 0.0, 0.0);

    // Calculate PBR lighting
    float3 Lo = CalculatePBRDirect(N, V, L, radiance, material);

    return float4(Lo, 1.0);
}
