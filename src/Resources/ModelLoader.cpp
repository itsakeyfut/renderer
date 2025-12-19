/**
 * @file ModelLoader.cpp
 * @brief Implementation of glTF 2.0 model loader.
 */

// Disable warnings for third-party headers
#ifdef _MSC_VER
#pragma warning(push, 0)
#elif defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wtype-limits"
#pragma GCC diagnostic ignored "-Wsign-compare"
#endif

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_NOEXCEPTION

#include <tiny_gltf.h>

#ifdef _MSC_VER
#pragma warning(pop)
#elif defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

#include "Resources/ModelLoader.h"
#include "Core/Assert.h"
#include "Core/Log.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <unordered_map>

namespace Resources {

namespace {

/**
 * @brief Helper to read accessor data from glTF buffer.
 *
 * @tparam T Target data type.
 * @param model The tinygltf model.
 * @param accessorIndex Accessor index.
 * @param outData Output vector for the data.
 * @return true on success.
 */
template<typename T>
bool ReadAccessorData(
    const tinygltf::Model& model,
    int accessorIndex,
    std::vector<T>& outData)
{
    if (accessorIndex < 0 ||
        accessorIndex >= static_cast<int>(model.accessors.size())) {
        return false;
    }

    const auto& accessor = model.accessors[accessorIndex];
    const auto& bufferView = model.bufferViews[accessor.bufferView];
    const auto& buffer = model.buffers[bufferView.buffer];

    outData.resize(accessor.count);

    const uint8_t* dataPtr =
        buffer.data.data() + bufferView.byteOffset + accessor.byteOffset;

    size_t stride = bufferView.byteStride;
    if (stride == 0) {
        stride = sizeof(T);
    }

    for (size_t i = 0; i < accessor.count; ++i) {
        memcpy(&outData[i], dataPtr + i * stride, sizeof(T));
    }

    return true;
}

/**
 * @brief Read scalar accessor data.
 *
 * @param model The tinygltf model.
 * @param accessorIndex Accessor index.
 * @param outData Output vector.
 * @return true on success.
 */
bool ReadScalarAccessor(
    const tinygltf::Model& model,
    int accessorIndex,
    std::vector<uint32_t>& outData)
{
    if (accessorIndex < 0 ||
        accessorIndex >= static_cast<int>(model.accessors.size())) {
        return false;
    }

    const auto& accessor = model.accessors[accessorIndex];
    const auto& bufferView = model.bufferViews[accessor.bufferView];
    const auto& buffer = model.buffers[bufferView.buffer];

    outData.resize(accessor.count);

    const uint8_t* dataPtr =
        buffer.data.data() + bufferView.byteOffset + accessor.byteOffset;

    size_t stride = bufferView.byteStride;

    switch (accessor.componentType) {
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
            stride = stride == 0 ? sizeof(uint8_t) : stride;
            for (size_t i = 0; i < accessor.count; ++i) {
                outData[i] = *reinterpret_cast<const uint8_t*>(dataPtr + i * stride);
            }
            break;

        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
            stride = stride == 0 ? sizeof(uint16_t) : stride;
            for (size_t i = 0; i < accessor.count; ++i) {
                outData[i] = *reinterpret_cast<const uint16_t*>(dataPtr + i * stride);
            }
            break;

        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
            stride = stride == 0 ? sizeof(uint32_t) : stride;
            for (size_t i = 0; i < accessor.count; ++i) {
                outData[i] = *reinterpret_cast<const uint32_t*>(dataPtr + i * stride);
            }
            break;

        default:
            LOG_ERROR("Unsupported index component type: {}", accessor.componentType);
            return false;
    }

    return true;
}

/**
 * @brief Calculate flat normals for a mesh.
 *
 * @param vertices The vertices to calculate normals for.
 * @param indices The index buffer.
 */
void CalculateFlatNormals(
    std::vector<Vertex>& vertices,
    const std::vector<uint32_t>& indices)
{
    // Reset all normals
    for (auto& vertex : vertices) {
        vertex.Normal = glm::vec3(0.0f);
    }

    // Calculate face normals and accumulate
    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        uint32_t i0 = indices[i];
        uint32_t i1 = indices[i + 1];
        uint32_t i2 = indices[i + 2];

        const glm::vec3& p0 = vertices[i0].Position;
        const glm::vec3& p1 = vertices[i1].Position;
        const glm::vec3& p2 = vertices[i2].Position;

        glm::vec3 edge1 = p1 - p0;
        glm::vec3 edge2 = p2 - p0;
        glm::vec3 normal = glm::cross(edge1, edge2);

        vertices[i0].Normal += normal;
        vertices[i1].Normal += normal;
        vertices[i2].Normal += normal;
    }

    // Normalize
    for (auto& vertex : vertices) {
        float length = glm::length(vertex.Normal);
        if (length > 0.0001f) {
            vertex.Normal /= length;
        }
        else {
            vertex.Normal = glm::vec3(0.0f, 1.0f, 0.0f);
        }
    }
}

/**
 * @brief Calculate tangent vectors using MikkTSpace-like algorithm.
 *
 * Computes tangent and bitangent sign for normal mapping.
 *
 * @param vertices The vertices to calculate tangents for.
 * @param indices The index buffer.
 */
void CalculateTangents(
    std::vector<Vertex>& vertices,
    const std::vector<uint32_t>& indices)
{
    std::vector<glm::vec3> tangents(vertices.size(), glm::vec3(0.0f));
    std::vector<glm::vec3> bitangents(vertices.size(), glm::vec3(0.0f));

    // Calculate tangent and bitangent for each triangle
    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        uint32_t i0 = indices[i];
        uint32_t i1 = indices[i + 1];
        uint32_t i2 = indices[i + 2];

        const glm::vec3& p0 = vertices[i0].Position;
        const glm::vec3& p1 = vertices[i1].Position;
        const glm::vec3& p2 = vertices[i2].Position;

        const glm::vec2& uv0 = vertices[i0].TexCoord;
        const glm::vec2& uv1 = vertices[i1].TexCoord;
        const glm::vec2& uv2 = vertices[i2].TexCoord;

        glm::vec3 edge1 = p1 - p0;
        glm::vec3 edge2 = p2 - p0;

        glm::vec2 deltaUV1 = uv1 - uv0;
        glm::vec2 deltaUV2 = uv2 - uv0;

        float det = deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y;

        glm::vec3 tangent;
        glm::vec3 bitangent;

        if (std::abs(det) > 0.000001f) {
            float invDet = 1.0f / det;

            tangent = glm::vec3(
                invDet * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x),
                invDet * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y),
                invDet * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z));

            bitangent = glm::vec3(
                invDet * (-deltaUV2.x * edge1.x + deltaUV1.x * edge2.x),
                invDet * (-deltaUV2.x * edge1.y + deltaUV1.x * edge2.y),
                invDet * (-deltaUV2.x * edge1.z + deltaUV1.x * edge2.z));
        }
        else {
            // Degenerate UV, use default tangent
            tangent = glm::vec3(1.0f, 0.0f, 0.0f);
            bitangent = glm::vec3(0.0f, 1.0f, 0.0f);
        }

        tangents[i0] += tangent;
        tangents[i1] += tangent;
        tangents[i2] += tangent;

        bitangents[i0] += bitangent;
        bitangents[i1] += bitangent;
        bitangents[i2] += bitangent;
    }

    // Orthonormalize and store tangent with handedness
    for (size_t i = 0; i < vertices.size(); ++i) {
        const glm::vec3& n = vertices[i].Normal;
        glm::vec3& t = tangents[i];
        const glm::vec3& b = bitangents[i];

        // Gram-Schmidt orthonormalization
        t = t - n * glm::dot(n, t);
        float tLength = glm::length(t);

        if (tLength > 0.000001f) {
            t /= tLength;
        }
        else {
            // Generate fallback tangent
            if (std::abs(n.x) < 0.9f) {
                t = glm::normalize(glm::cross(n, glm::vec3(1.0f, 0.0f, 0.0f)));
            }
            else {
                t = glm::normalize(glm::cross(n, glm::vec3(0.0f, 1.0f, 0.0f)));
            }
        }

        // Calculate handedness
        float handedness = glm::dot(glm::cross(n, t), b) < 0.0f ? -1.0f : 1.0f;

        vertices[i].Tangent = glm::vec4(t, handedness);
    }
}

/**
 * @brief Process a single glTF primitive into a mesh.
 *
 * @param model The tinygltf model.
 * @param primitive The primitive to process.
 * @param options Loading options.
 * @param outMesh Output mesh.
 * @return true on success.
 */
bool ProcessPrimitive(
    const tinygltf::Model& model,
    const tinygltf::Primitive& primitive,
    const ModelLoadOptions& options,
    Mesh& outMesh)
{
    // Only support triangles
    if (primitive.mode != TINYGLTF_MODE_TRIANGLES &&
        primitive.mode != -1) {  // -1 means default (triangles)
        LOG_WARN("Skipping non-triangle primitive (mode={})", primitive.mode);
        return false;
    }

    // Read positions (required)
    auto posIt = primitive.attributes.find("POSITION");
    if (posIt == primitive.attributes.end()) {
        LOG_ERROR("Primitive missing POSITION attribute");
        return false;
    }

    std::vector<glm::vec3> positions;
    if (!ReadAccessorData(model, posIt->second, positions)) {
        LOG_ERROR("Failed to read POSITION data");
        return false;
    }

    // Read normals (optional)
    std::vector<glm::vec3> normals;
    auto normIt = primitive.attributes.find("NORMAL");
    bool hasNormals = false;
    if (normIt != primitive.attributes.end()) {
        hasNormals = ReadAccessorData(model, normIt->second, normals);
    }

    // Read texture coordinates (optional)
    std::vector<glm::vec2> texCoords;
    auto texIt = primitive.attributes.find("TEXCOORD_0");
    bool hasTexCoords = false;
    if (texIt != primitive.attributes.end()) {
        hasTexCoords = ReadAccessorData(model, texIt->second, texCoords);
    }

    // Read tangents (optional)
    std::vector<glm::vec4> tangents;
    auto tanIt = primitive.attributes.find("TANGENT");
    bool hasTangents = false;
    if (tanIt != primitive.attributes.end()) {
        hasTangents = ReadAccessorData(model, tanIt->second, tangents);
    }

    // Build vertices
    outMesh.Vertices.resize(positions.size());
    for (size_t i = 0; i < positions.size(); ++i) {
        Vertex& vertex = outMesh.Vertices[i];

        vertex.Position = positions[i];

        if (hasNormals && i < normals.size()) {
            vertex.Normal = normals[i];
        }
        else {
            vertex.Normal = glm::vec3(0.0f, 1.0f, 0.0f);
        }

        if (hasTexCoords && i < texCoords.size()) {
            vertex.TexCoord = texCoords[i];
            if (options.FlipTexCoordV) {
                vertex.TexCoord.y = 1.0f - vertex.TexCoord.y;
            }
        }
        else {
            vertex.TexCoord = glm::vec2(0.0f);
        }

        if (hasTangents && i < tangents.size()) {
            vertex.Tangent = tangents[i];
        }
        else {
            vertex.Tangent = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
        }
    }

    // Read indices
    if (primitive.indices >= 0) {
        if (!ReadScalarAccessor(model, primitive.indices, outMesh.Indices)) {
            LOG_ERROR("Failed to read indices");
            return false;
        }
    }
    else {
        // Generate indices for non-indexed geometry
        outMesh.Indices.resize(positions.size());
        for (size_t i = 0; i < positions.size(); ++i) {
            outMesh.Indices[i] = static_cast<uint32_t>(i);
        }
    }

    // Calculate normals if needed
    if (!hasNormals && options.CalculateNormals) {
        CalculateFlatNormals(outMesh.Vertices, outMesh.Indices);
    }

    // Calculate tangents if needed
    if (!hasTangents && options.CalculateTangents) {
        CalculateTangents(outMesh.Vertices, outMesh.Indices);
    }

    // Store material index
    outMesh.MaterialIndex = primitive.material;

    return true;
}

/**
 * @brief Process a glTF node recursively.
 *
 * @param model The tinygltf model.
 * @param node The node to process.
 * @param options Loading options.
 * @param outModel Output model.
 */
void ProcessNode(
    const tinygltf::Model& model,
    const tinygltf::Node& node,
    const ModelLoadOptions& options,
    Model& outModel)
{
    // Process mesh if present
    if (node.mesh >= 0 && node.mesh < static_cast<int>(model.meshes.size())) {
        const auto& gltfMesh = model.meshes[node.mesh];

        for (const auto& primitive : gltfMesh.primitives) {
            Mesh mesh;
            mesh.Name = gltfMesh.name;

            if (ProcessPrimitive(model, primitive, options, mesh)) {
                outModel.AddMesh(std::move(mesh));
            }
        }
    }

    // Process children
    for (int childIndex : node.children) {
        if (childIndex >= 0 &&
            childIndex < static_cast<int>(model.nodes.size())) {
            ProcessNode(model, model.nodes[childIndex], options, outModel);
        }
    }
}

/**
 * @brief Get the texture path from a glTF texture.
 *
 * @param model The tinygltf model.
 * @param textureIndex Texture index.
 * @param basePath Base path for relative texture paths.
 * @return Texture file path, or empty string if not found.
 */
std::string GetTexturePath(
    const tinygltf::Model& model,
    int textureIndex,
    const std::filesystem::path& basePath)
{
    if (textureIndex < 0 ||
        textureIndex >= static_cast<int>(model.textures.size())) {
        return "";
    }

    const auto& texture = model.textures[textureIndex];
    if (texture.source < 0 ||
        texture.source >= static_cast<int>(model.images.size())) {
        return "";
    }

    const auto& image = model.images[texture.source];

    // If image has URI, construct full path
    if (!image.uri.empty()) {
        // Check if it's a data URI
        if (image.uri.rfind("data:", 0) == 0) {
            LOG_DEBUG("Embedded texture (data URI) - not supported for external loading");
            return "";
        }

        // Construct path relative to model file
        std::filesystem::path texturePath = basePath / image.uri;
        return texturePath.string();
    }

    // Image might be embedded in buffer (not a file)
    return "";
}

/**
 * @brief Process glTF materials.
 *
 * @param gltfModel The tinygltf model.
 * @param basePath Base path for texture files.
 * @param outModel Output model to add materials to.
 */
void ProcessMaterials(
    const tinygltf::Model& gltfModel,
    const std::filesystem::path& basePath,
    Model& outModel)
{
    for (const auto& gltfMat : gltfModel.materials) {
        MaterialData matData;
        matData.Name = gltfMat.name;

        // PBR Metallic-Roughness workflow
        const auto& pbr = gltfMat.pbrMetallicRoughness;

        // Base color texture
        if (pbr.baseColorTexture.index >= 0) {
            matData.BaseColorTexturePath = GetTexturePath(
                gltfModel, pbr.baseColorTexture.index, basePath);
        }

        // Base color factor
        if (pbr.baseColorFactor.size() >= 4) {
            matData.BaseColorFactor[0] = static_cast<float>(pbr.baseColorFactor[0]);
            matData.BaseColorFactor[1] = static_cast<float>(pbr.baseColorFactor[1]);
            matData.BaseColorFactor[2] = static_cast<float>(pbr.baseColorFactor[2]);
            matData.BaseColorFactor[3] = static_cast<float>(pbr.baseColorFactor[3]);
        }

        // Metallic-roughness texture
        if (pbr.metallicRoughnessTexture.index >= 0) {
            matData.MetallicRoughnessTexturePath = GetTexturePath(
                gltfModel, pbr.metallicRoughnessTexture.index, basePath);
        }

        // Metallic and roughness factors
        matData.MetallicFactor = static_cast<float>(pbr.metallicFactor);
        matData.RoughnessFactor = static_cast<float>(pbr.roughnessFactor);

        // Normal texture
        if (gltfMat.normalTexture.index >= 0) {
            matData.NormalTexturePath = GetTexturePath(
                gltfModel, gltfMat.normalTexture.index, basePath);
        }

        // Occlusion texture
        if (gltfMat.occlusionTexture.index >= 0) {
            matData.OcclusionTexturePath = GetTexturePath(
                gltfModel, gltfMat.occlusionTexture.index, basePath);
        }

        // Emissive texture
        if (gltfMat.emissiveTexture.index >= 0) {
            matData.EmissiveTexturePath = GetTexturePath(
                gltfModel, gltfMat.emissiveTexture.index, basePath);
        }

        // Emissive factor
        if (gltfMat.emissiveFactor.size() >= 3) {
            matData.EmissiveFactor[0] = static_cast<float>(gltfMat.emissiveFactor[0]);
            matData.EmissiveFactor[1] = static_cast<float>(gltfMat.emissiveFactor[1]);
            matData.EmissiveFactor[2] = static_cast<float>(gltfMat.emissiveFactor[2]);
        }

        // Alpha mode
        if (gltfMat.alphaMode == "MASK") {
            matData.Alpha = MaterialData::AlphaMode::Mask;
            matData.AlphaCutoff = static_cast<float>(gltfMat.alphaCutoff);
        } else if (gltfMat.alphaMode == "BLEND") {
            matData.Alpha = MaterialData::AlphaMode::Blend;
        } else {
            matData.Alpha = MaterialData::AlphaMode::Opaque;
        }

        // Double-sided
        matData.DoubleSided = gltfMat.doubleSided;

        LOG_DEBUG("Loaded material: {}", matData.Name);

        outModel.AddMaterialData(std::move(matData));
    }
}

}  // anonymous namespace

Core::Ref<Model> ModelLoader::LoadGLTF(
    const std::string& filepath,
    const ModelLoadOptions& options)
{
    LOG_INFO("Loading glTF model: {}", filepath);

    // Check file exists
    if (!std::filesystem::exists(filepath)) {
        LOG_ERROR("Model file not found: {}", filepath);
        return nullptr;
    }

    // Determine file type
    std::string extension = std::filesystem::path(filepath).extension().string();
    std::transform(
        extension.begin(),
        extension.end(),
        extension.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    // Load with tinygltf
    tinygltf::Model gltfModel;
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;

    bool success = false;
    if (extension == ".glb") {
        success = loader.LoadBinaryFromFile(&gltfModel, &err, &warn, filepath);
    }
    else if (extension == ".gltf") {
        success = loader.LoadASCIIFromFile(&gltfModel, &err, &warn, filepath);
    }
    else {
        LOG_ERROR("Unsupported file format: {}", extension);
        return nullptr;
    }

    if (!warn.empty()) {
        LOG_WARN("glTF warning: {}", warn);
    }

    if (!err.empty()) {
        LOG_ERROR("glTF error: {}", err);
    }

    if (!success) {
        LOG_ERROR("Failed to load glTF model: {}", filepath);
        return nullptr;
    }

    // Create model
    auto model = Core::CreateRef<Model>(filepath);

    // Extract model name from filename
    std::string filename = std::filesystem::path(filepath).stem().string();
    model->SetName(filename);

    // Get base path for texture loading
    std::filesystem::path basePath = std::filesystem::path(filepath).parent_path();

    // Process materials first (before meshes reference them)
    ProcessMaterials(gltfModel, basePath, *model);

    // Process all scenes (or default scene)
    if (gltfModel.defaultScene >= 0 &&
        gltfModel.defaultScene < static_cast<int>(gltfModel.scenes.size())) {
        const auto& scene = gltfModel.scenes[gltfModel.defaultScene];
        for (int nodeIndex : scene.nodes) {
            if (nodeIndex >= 0 &&
                nodeIndex < static_cast<int>(gltfModel.nodes.size())) {
                ProcessNode(gltfModel, gltfModel.nodes[nodeIndex], options, *model);
            }
        }
    }
    else if (!gltfModel.scenes.empty()) {
        // Use first scene if no default
        const auto& scene = gltfModel.scenes[0];
        for (int nodeIndex : scene.nodes) {
            if (nodeIndex >= 0 &&
                nodeIndex < static_cast<int>(gltfModel.nodes.size())) {
                ProcessNode(gltfModel, gltfModel.nodes[nodeIndex], options, *model);
            }
        }
    }
    else {
        // No scenes, process all root nodes
        for (size_t i = 0; i < gltfModel.nodes.size(); ++i) {
            ProcessNode(gltfModel, gltfModel.nodes[i], options, *model);
        }
    }

    // Calculate bounding box
    model->CalculateBounds();

    LOG_INFO(
        "Loaded model '{}': {} meshes, {} materials, {} vertices, {} triangles",
        model->GetName(),
        model->GetMeshCount(),
        model->GetMaterialDataCount(),
        model->GetTotalVertexCount(),
        model->GetTotalIndexCount() / 3);

    return model;
}

bool ModelLoader::IsFormatSupported(const std::string& extension)
{
    std::string ext = extension;
    std::transform(
        ext.begin(),
        ext.end(),
        ext.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    return ext == ".gltf" || ext == ".glb";
}

std::vector<std::string> ModelLoader::GetSupportedExtensions()
{
    return {".gltf", ".glb"};
}

} // namespace Resources
