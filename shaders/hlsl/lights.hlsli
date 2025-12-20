// Light structure definitions for HLSL shaders
// These structures must match the C++ definitions in Scene/Light.h
//
// Usage:
//   #include "lights.hlsli"
//
// Binding:
//   cbuffer LightData : register(b2) - Contains LightUBO
//   StructuredBuffer<PointLight> pointLights : register(t0, space1)
//   StructuredBuffer<SpotLight> spotLights : register(t1, space1)

#ifndef LIGHTS_HLSLI
#define LIGHTS_HLSLI

// Directional light - infinitely distant light source (e.g., sun)
// Size: 32 bytes
struct DirectionalLight
{
    float3 Direction;   // Normalized direction from light to surface
    float  Intensity;   // Light intensity multiplier
    float3 Color;       // RGB color
    float  Padding;     // 16-byte alignment padding
};

// Point light - omnidirectional light from a position
// Size: 32 bytes
struct PointLight
{
    float3 Position;    // World position
    float  Radius;      // Maximum influence radius
    float3 Color;       // RGB color
    float  Intensity;   // Light intensity multiplier
};

// Spot light - cone-shaped light from a position
// Size: 48 bytes
struct SpotLight
{
    float3 Position;        // World position
    float  InnerConeAngle;  // cos(inner angle) - full intensity
    float3 Direction;       // Normalized direction
    float  OuterConeAngle;  // cos(outer angle) - falloff to zero
    float3 Color;           // RGB color
    float  Intensity;       // Light intensity multiplier
};

// Light uniform buffer object
// Bound to register(b2)
struct LightUBO
{
    DirectionalLight DirectionalLightData;
    uint NumPointLights;
    uint NumSpotLights;
    float2 Padding;
};

// ============================================================================
// Light Attenuation Functions
// ============================================================================

// Calculate smooth attenuation for point/spot lights
// Uses inverse square law with smooth falloff at radius boundary
float CalculateAttenuation(float distance, float radius)
{
    // Inverse square law with smooth cutoff
    float attenuation = 1.0 / (distance * distance + 1.0);

    // Smooth falloff at radius boundary
    float falloff = saturate(1.0 - distance / radius);
    falloff = falloff * falloff;

    return attenuation * falloff;
}

// Calculate spot light cone attenuation
// Uses smooth interpolation between inner and outer cone angles
float CalculateSpotAttenuation(float3 lightDir, float3 spotDir, float innerCos, float outerCos)
{
    float cosAngle = dot(-lightDir, spotDir);
    return saturate((cosAngle - outerCos) / (innerCos - outerCos));
}

// ============================================================================
// Light Calculation Helpers
// ============================================================================

// Calculate directional light contribution
float3 CalculateDirectionalLight(
    DirectionalLight light,
    float3 normal,
    float3 viewDir,
    float3 albedo,
    float roughness,
    float metallic)
{
    float3 lightDir = normalize(-light.Direction);
    float NdotL = max(dot(normal, lightDir), 0.0);

    // Simple Lambertian diffuse
    float3 diffuse = albedo * NdotL;

    return diffuse * light.Color * light.Intensity;
}

// Calculate point light contribution
float3 CalculatePointLight(
    PointLight light,
    float3 worldPos,
    float3 normal,
    float3 viewDir,
    float3 albedo,
    float roughness,
    float metallic)
{
    float3 lightVec = light.Position - worldPos;
    float distance = length(lightVec);
    float3 lightDir = lightVec / distance;

    float attenuation = CalculateAttenuation(distance, light.Radius);
    float NdotL = max(dot(normal, lightDir), 0.0);

    // Simple Lambertian diffuse
    float3 diffuse = albedo * NdotL;

    return diffuse * light.Color * light.Intensity * attenuation;
}

// Calculate spot light contribution
float3 CalculateSpotLight(
    SpotLight light,
    float3 worldPos,
    float3 normal,
    float3 viewDir,
    float3 albedo,
    float roughness,
    float metallic)
{
    float3 lightVec = light.Position - worldPos;
    float distance = length(lightVec);
    float3 lightDir = lightVec / distance;

    // Distance attenuation (using radius from intensity for simplicity)
    float radius = 50.0; // Default radius for spot lights
    float distanceAttenuation = CalculateAttenuation(distance, radius);

    // Spot cone attenuation
    float spotAttenuation = CalculateSpotAttenuation(
        lightDir,
        normalize(light.Direction),
        light.InnerConeAngle,
        light.OuterConeAngle);

    float NdotL = max(dot(normal, lightDir), 0.0);

    // Simple Lambertian diffuse
    float3 diffuse = albedo * NdotL;

    return diffuse * light.Color * light.Intensity * distanceAttenuation * spotAttenuation;
}

#endif // LIGHTS_HLSLI
