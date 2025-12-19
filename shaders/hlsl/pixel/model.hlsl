// Model Pixel (Fragment) Shader
// Simple Blinn-Phong shading without textures

// Camera uniform buffer (for view direction calculation)
cbuffer CameraData : register(b0)
{
    float4x4 view;
    float4x4 projection;
    float4x4 viewProjection;
    float3 cameraPosition;
    float padding;
};

struct PSInput
{
    float4 Position    : SV_POSITION;
    float3 WorldPos    : TEXCOORD0;
    float3 Normal      : TEXCOORD1;
    float2 TexCoord    : TEXCOORD2;
    float3 Tangent     : TEXCOORD3;
    float3 Bitangent   : TEXCOORD4;
};

float4 main(PSInput input) : SV_TARGET
{
    // Simple directional light
    float3 lightDir = normalize(float3(1.0, 1.0, 1.0));
    float3 lightColor = float3(1.0, 1.0, 1.0);
    float3 ambient = float3(0.1, 0.1, 0.1);

    // Base color (gray)
    float3 baseColor = float3(0.7, 0.7, 0.7);

    // Normalize interpolated normal
    float3 N = normalize(input.Normal);

    // View direction
    float3 V = normalize(cameraPosition - input.WorldPos);

    // Half vector for Blinn-Phong
    float3 H = normalize(lightDir + V);

    // Diffuse
    float NdotL = max(dot(N, lightDir), 0.0);
    float3 diffuse = baseColor * lightColor * NdotL;

    // Specular (Blinn-Phong)
    float NdotH = max(dot(N, H), 0.0);
    float shininess = 32.0;
    float3 specular = lightColor * pow(NdotH, shininess) * 0.5;

    // Final color
    float3 color = ambient * baseColor + diffuse + specular;

    return float4(color, 1.0);
}
