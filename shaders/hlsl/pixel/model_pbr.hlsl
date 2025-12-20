// Model Pixel (Fragment) Shader - PBR Textured Version
// Blinn-Phong shading with texture support and dynamic lighting
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
//     - sampler is combined with each texture
//   Set 2: Light data
//     - binding 0: LightUBO (directional light + counts)
//     - binding 1: PointLight storage buffer
//     - binding 2: SpotLight storage buffer

#include "../lights.hlsli"

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
    // Lighting calculation
    // =========================================================================

    // Get world space normal (with normal mapping if available)
    float3 N = GetWorldNormal(input);

    // View direction (from surface to camera)
    float3 V = normalize(cameraPosition - input.WorldPos);

    // Calculate shininess from roughness
    float shininess = RoughnessToShininess(roughness);

    // Metallic workflow: metals have no diffuse, dielectrics have full diffuse
    // Specular color: metals use albedo, dielectrics use 0.04 (4% reflectance)
    float3 diffuseColor = albedo * (1.0 - metallic);
    float3 specularColor = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);

    // Accumulated lighting from all sources
    float3 lighting = float3(0.0, 0.0, 0.0);

    // -------------------------------------------------------------------------
    // Directional light (from LightUBO)
    // -------------------------------------------------------------------------
    {
        float3 lightDir = normalize(-directionalLight.Direction);
        float3 lightColor = directionalLight.Color * directionalLight.Intensity;

        // Diffuse (Lambertian)
        float NdotL = max(dot(N, lightDir), 0.0);
        float3 diffuse = NdotL * lightColor * diffuseColor;

        // Specular (Blinn-Phong)
        float3 specular = float3(0.0, 0.0, 0.0);
        if (NdotL > 0.0)
        {
            float3 halfDir = normalize(lightDir + V);
            float NdotH = max(dot(N, halfDir), 0.0);
            specular = pow(NdotH, shininess) * lightColor * specularColor;
        }

        lighting += diffuse + specular;
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
        float3 lightDir = lightVec / distance;

        // Attenuation
        float attenuation = CalculateAttenuation(distance, light.Radius);
        float3 lightColor = light.Color * light.Intensity * attenuation;

        // Diffuse (Lambertian)
        float NdotL = max(dot(N, lightDir), 0.0);
        float3 diffuse = NdotL * lightColor * diffuseColor;

        // Specular (Blinn-Phong)
        float3 specular = float3(0.0, 0.0, 0.0);
        if (NdotL > 0.0)
        {
            float3 halfDir = normalize(lightDir + V);
            float NdotH = max(dot(N, halfDir), 0.0);
            specular = pow(NdotH, shininess) * lightColor * specularColor;
        }

        lighting += diffuse + specular;
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
        float3 lightDir = lightVec / distance;

        // Distance attenuation
        float radius = 50.0; // Default radius for spot lights
        float distanceAttenuation = CalculateAttenuation(distance, radius);

        // Spot cone attenuation
        float spotAttenuation = CalculateSpotAttenuation(
            lightDir,
            normalize(light.Direction),
            light.InnerConeAngle,
            light.OuterConeAngle);

        float3 lightColor = light.Color * light.Intensity * distanceAttenuation * spotAttenuation;

        // Diffuse (Lambertian)
        float NdotL = max(dot(N, lightDir), 0.0);
        float3 diffuse = NdotL * lightColor * diffuseColor;

        // Specular (Blinn-Phong)
        float3 specular = float3(0.0, 0.0, 0.0);
        if (NdotL > 0.0)
        {
            float3 halfDir = normalize(lightDir + V);
            float NdotH = max(dot(N, halfDir), 0.0);
            specular = pow(NdotH, shininess) * lightColor * specularColor;
        }

        lighting += diffuse + specular;
    }

    // -------------------------------------------------------------------------
    // Ambient / Environment approximation
    // -------------------------------------------------------------------------
    // Simple hemisphere ambient: blend between ground color and sky color
    float3 skyColor = float3(0.15, 0.18, 0.25);   // Cool sky ambient
    float3 groundColor = float3(0.08, 0.06, 0.04); // Warm ground bounce
    float upFactor = N.y * 0.5 + 0.5; // Map -1..1 to 0..1
    float3 ambientColor = lerp(groundColor, skyColor, upFactor);
    float3 ambient = ambientColor * diffuseColor * ao;

    // Apply AO to all non-emissive lighting (not just ambient)
    lighting *= lerp(1.0, ao, 0.5); // Partial AO influence on direct lighting

    // =========================================================================
    // Final color composition
    // =========================================================================

    float3 color = ambient + lighting + emissive;

    return float4(color, baseColor.a);
}
