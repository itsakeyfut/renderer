// Lighting Pass Vertex Shader - Full-screen quad generation
// Generates a full-screen triangle using vertex ID (no vertex buffer required)
//
// This shader procedurally generates a single triangle that covers the entire screen.
// Using a single oversized triangle (vs. two triangles for a quad) eliminates the
// diagonal edge and improves cache coherency.
//
// Vertex ID mapping:
//   0: (-1, -1) -> (0, 1)  // Bottom-left
//   1: ( 3, -1) -> (2, 1)  // Bottom-right (extends past viewport)
//   2: (-1,  3) -> (0,-1)  // Top-left (extends past viewport)
//
// Reference: "A Faster Vertex Shader for Fullscreen Passes" (Timothy Lottes, GDC 2014)
//            UE: FScreenPassVS for deferred lighting passes

struct VSOutput
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

VSOutput main(uint vertexID : SV_VertexID)
{
    VSOutput output;

    // Generate full-screen triangle vertices from vertex ID
    // This creates an oversized triangle that covers the entire screen:
    //   Vertex 0: (-1, -1)  Bottom-left corner
    //   Vertex 1: ( 3, -1)  Extends past right edge
    //   Vertex 2: (-1,  3)  Extends past top edge
    //
    // The triangle extends beyond the screen, but the GPU clips it automatically.
    // This approach is more efficient than a quad (2 triangles) because:
    //   1. Only 3 vertices vs 4-6 vertices
    //   2. No diagonal edge that can cause texture cache thrashing
    //   3. Better warp/wavefront occupancy
    float2 position;
    position.x = (vertexID == 1) ? 3.0 : -1.0;
    position.y = (vertexID == 2) ? 3.0 : -1.0;

    output.Position = float4(position, 0.0, 1.0);

    // Calculate UV coordinates for G-Buffer sampling
    // Map from clip space [-1,1] to UV space [0,1]
    // Note: Vulkan uses top-left origin for textures, so we flip Y
    //   Position (-1, -1) -> UV (0, 1)  Bottom-left in screen -> Bottom of texture
    //   Position ( 1,  1) -> UV (1, 0)  Top-right in screen -> Top of texture
    output.TexCoord.x = position.x * 0.5 + 0.5;
    output.TexCoord.y = 0.5 - position.y * 0.5;  // Flip Y for Vulkan

    return output;
}
