// G-Buffer Vertex Shader
// Transforms model vertices for the G-Buffer geometry pass
//
// This shader outputs vertex data needed for G-Buffer pixel shader:
//   - Clip-space position for rasterization
//   - World-space position for lighting calculations
//   - World-space normal with TBN matrix for normal mapping
//   - Texture coordinates for material sampling
//
// Descriptor Set Layout:
//   Set 0: Scene data
//     - binding 0: CameraData (view, projection, viewProjection, cameraPosition)
//     - binding 1: ObjectData (model, normalMatrix)

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

// Object uniform buffer (Set 0, Binding 1)
[[vk::binding(1, 0)]]
cbuffer ObjectData : register(b1)
{
    float4x4 model;
    float4x4 normalMatrix;
};

struct VSInput
{
    float3 Position : POSITION;
    float3 Normal   : NORMAL;
    float2 TexCoord : TEXCOORD;
    float4 Tangent  : TANGENT;
};

struct VSOutput
{
    float4 Position    : SV_POSITION;  // Clip-space position
    float3 WorldPos    : TEXCOORD0;    // World-space position
    float3 Normal      : TEXCOORD1;    // World-space normal
    float2 TexCoord    : TEXCOORD2;    // Texture coordinates
    float3 Tangent     : TEXCOORD3;    // World-space tangent
    float3 Bitangent   : TEXCOORD4;    // World-space bitangent
};

VSOutput main(VSInput input)
{
    VSOutput output;

    // Transform position to world space
    float4 worldPos = mul(model, float4(input.Position, 1.0));
    output.WorldPos = worldPos.xyz;

    // Transform to clip space
    output.Position = mul(viewProjection, worldPos);

    // Transform normal to world space using normal matrix
    // Normal matrix is the inverse transpose of the model matrix
    float3 N = normalize(mul((float3x3)normalMatrix, input.Normal));

    // Transform tangent to world space using model matrix
    float3 T = normalize(mul((float3x3)model, input.Tangent.xyz));

    // Re-orthogonalize tangent with respect to normal (Gram-Schmidt process)
    // This ensures T and N remain perpendicular after interpolation
    T = normalize(T - dot(T, N) * N);

    // Calculate bitangent with handedness from tangent.w
    // tangent.w is -1 or 1 indicating the handedness of the tangent space
    float3 B = cross(N, T) * input.Tangent.w;

    output.Normal = N;
    output.Tangent = T;
    output.Bitangent = B;

    // Pass through texture coordinates unchanged
    output.TexCoord = input.TexCoord;

    return output;
}
