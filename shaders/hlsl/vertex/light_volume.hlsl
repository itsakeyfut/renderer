// Light Volume Vertex Shader
// Transforms light volume mesh vertices (sphere for point lights, cone for spot lights)
// to clip space for deferred light volume rendering.
//
// The volume mesh is a unit geometry (sphere or cone) that gets scaled and
// positioned using push constants to match each light's position and range.
//
// Push Constant Layout:
//   - ModelViewProjection (mat4): Combined MVP matrix for vertex transformation
//   - Model (mat4): Model matrix for world position reconstruction in pixel shader
//   - LightIndex (uint): Index into the light storage buffer

// Push constants for per-light transforms
struct PushConstants
{
    float4x4 ModelViewProjection;   // MVP matrix for vertex transformation
    float4x4 Model;                 // Model matrix for world position
    uint     LightIndex;            // Index into light buffer
    uint3    Padding;               // Alignment padding
};

[[vk::push_constant]]
ConstantBuffer<PushConstants> pushConstants;

// Input vertex structure (position only for light volumes)
struct VSInput
{
    [[vk::location(0)]] float3 Position : POSITION;
};

// Output to pixel shader
struct VSOutput
{
    float4 Position     : SV_POSITION;  // Clip-space position
    float3 WorldPos     : WORLD_POS;    // World-space position for lighting
    float4 ScreenPos    : SCREEN_POS;   // Screen-space position for G-Buffer sampling
};

VSOutput main(VSInput input)
{
    VSOutput output;

    // Transform vertex to clip space
    output.Position = mul(pushConstants.ModelViewProjection, float4(input.Position, 1.0));

    // Calculate world position for lighting calculations
    float4 worldPos4 = mul(pushConstants.Model, float4(input.Position, 1.0));
    output.WorldPos = worldPos4.xyz / worldPos4.w;

    // Pass screen position for G-Buffer UV calculation in pixel shader
    output.ScreenPos = output.Position;

    return output;
}
