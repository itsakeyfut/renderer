/**
 * @file ModelLoader.h
 * @brief glTF 2.0 model loader interface.
 *
 * Provides functionality to load 3D models from glTF 2.0 format files,
 * extracting mesh data, materials, and computing tangent space data.
 */

#pragma once

#include "Core/Types.h"
#include "Resources/Model.h"

#include <string>

namespace Resources {

/**
 * @brief Options for model loading.
 */
struct ModelLoadOptions {
    /**
     * @brief Calculate normals if not present in the model.
     */
    bool CalculateNormals = false;

    /**
     * @brief Calculate tangent vectors if not present in the model.
     *
     * Tangent vectors are required for normal mapping. Uses the
     * MikkTSpace algorithm for consistent results.
     */
    bool CalculateTangents = true;

    /**
     * @brief Flip V texture coordinate (1.0 - v).
     *
     * Some engines expect V=0 at the bottom, while glTF uses V=0 at top.
     * Enable this to flip for compatibility with such systems.
     */
    bool FlipTexCoordV = false;
};

/**
 * @brief Static class for loading 3D models from various formats.
 *
 * Currently supports glTF 2.0 format (.gltf, .glb).
 *
 * Usage:
 * @code
 * ModelLoadOptions options;
 * options.CalculateTangents = true;
 *
 * auto model = ModelLoader::LoadGLTF("assets/models/cube.gltf", options);
 * if (model) {
 *     for (size_t i = 0; i < model->GetMeshCount(); ++i) {
 *         const auto& mesh = model->GetMesh(i);
 *         // Process mesh data...
 *     }
 * }
 * @endcode
 */
class ModelLoader {
public:
    ModelLoader() = delete;
    ~ModelLoader() = delete;

    /**
     * @brief Load a glTF 2.0 model from file.
     *
     * Supports both .gltf (JSON + separate binary) and .glb (binary) formats.
     * Extracts all meshes, computes bounding box, and optionally generates
     * tangent vectors for normal mapping.
     *
     * @param filepath Path to the glTF file.
     * @param options Loading options.
     * @return Shared pointer to the loaded model, or nullptr on failure.
     */
    static Core::Ref<Model> LoadGLTF(
        const std::string& filepath,
        const ModelLoadOptions& options = {});

    /**
     * @brief Check if a file extension is supported.
     *
     * @param extension File extension (e.g., ".gltf", ".glb").
     * @return true if the format is supported.
     */
    static bool IsFormatSupported(const std::string& extension);

    /**
     * @brief Get a list of supported file extensions.
     *
     * @return Vector of supported extensions (e.g., {".gltf", ".glb"}).
     */
    static std::vector<std::string> GetSupportedExtensions();
};

} // namespace Resources
