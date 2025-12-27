// G-Buffer utilities for deferred rendering
// Contains octahedral normal encoding/decoding functions
//
// Reference: "A Survey of Efficient Representations for Independent Unit Vectors"
//            (Cigolle et al., JCGT 2014)
//
// Octahedral encoding compresses 3D unit vectors to 2 components with
// better precision than spheremap encoding, especially near the poles.

#ifndef GBUFFER_HLSLI
#define GBUFFER_HLSLI

// ============================================================================
// Octahedral Normal Encoding
// ============================================================================
// Maps a unit normal to [-1,1]^2 octahedron, then to [0,1]^2 for storage
// Provides better precision than spheremap encoding across all directions

/**
 * @brief Wrap function for octahedral encoding
 *
 * Used to reflect vectors in the lower hemisphere to the proper
 * octant when projecting onto the octahedron's base.
 *
 * @param v 2D vector to wrap
 * @return Wrapped 2D vector
 */
float2 OctWrap(float2 v)
{
    // Sign function that returns 1.0 for positive values, -1.0 for negative
    // Using component-wise ternary for portability across compilers
    float2 s;
    s.x = (v.x >= 0.0) ? 1.0 : -1.0;
    s.y = (v.y >= 0.0) ? 1.0 : -1.0;
    return (1.0 - abs(v.yx)) * s;
}

/**
 * @brief Encode a world-space normal to octahedral representation
 *
 * Projects the 3D unit vector onto an octahedron and unfolds it
 * to a 2D representation. The lower hemisphere is reflected to
 * the corners of the unit square.
 *
 * @param n Normalized world-space normal (unit length)
 * @return 2D encoded normal in [0,1] range for texture storage
 */
float2 EncodeNormal(float3 n)
{
    // Project to octahedron by dividing by L1 norm
    n /= (abs(n.x) + abs(n.y) + abs(n.z));

    // Unfold: lower hemisphere wraps to corners
    n.xy = (n.z >= 0.0) ? n.xy : OctWrap(n.xy);

    // Map from [-1,1] to [0,1] for texture storage
    n.xy = n.xy * 0.5 + 0.5;

    return n.xy;
}

/**
 * @brief Decode an octahedral normal back to world-space
 *
 * Reverses the encoding process to reconstruct the original
 * 3D unit normal from the 2D representation.
 *
 * @param encoded 2D encoded normal in [0,1] range from texture
 * @return Normalized world-space normal
 */
float3 DecodeNormal(float2 encoded)
{
    // Map from [0,1] to [-1,1]
    float2 f = encoded * 2.0 - 1.0;

    // Reconstruct z from L1 norm = 1 constraint
    float3 n = float3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));

    // Unfold corners for lower hemisphere
    // Using component-wise ternary for portability across compilers
    float t = saturate(-n.z);
    n.x += (n.x >= 0.0) ? -t : t;
    n.y += (n.y >= 0.0) ? -t : t;

    return normalize(n);
}

// ============================================================================
// G-Buffer Structure
// ============================================================================
// Defines the G-Buffer render target layout:
//   RT0 (Albedo):   R8G8B8A8_SRGB    - RGB = Base color, A = unused
//   RT1 (Normal):   R16G16_SFLOAT   - RG = Octahedral encoded normal
//   RT2 (Material): R8G8B8A8_UNORM  - R = Metallic, G = Roughness, B = AO, A = unused
//   RT3 (Emissive): R16G16B16A16_SFLOAT - RGB = Emissive, A = unused

/**
 * @brief Pixel shader output for G-Buffer pass
 *
 * Multiple Render Target (MRT) output structure that writes
 * geometry information to all G-Buffer textures in a single pass.
 */
struct GBufferOutput
{
    float4 Albedo   : SV_TARGET0;  // RGB = Base color, A = alpha (for alpha testing)
    float2 Normal   : SV_TARGET1;  // RG = Octahedral encoded world normal
    float4 Material : SV_TARGET2;  // R = Metallic, G = Roughness, B = AO, A = unused
    float4 Emissive : SV_TARGET3;  // RGB = Emissive color * factor, A = unused
};

#endif // GBUFFER_HLSLI
