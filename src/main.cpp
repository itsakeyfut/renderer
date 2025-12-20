/**
 * @file main.cpp
 * @brief 3D Model Viewer application entry point.
 *
 * Renders a 3D model loaded from glTF format using Vulkan.
 */

#include "Core/Event.h"
#include "Core/EventDispatcher.h"
#include "Core/Log.h"
#include "Core/Timer.h"
#include "Platform/FileDialog.h"
#include "Platform/Input.h"
#include "Platform/Window.h"
#include "RHI/RHIInstance.h"
#include "RHI/RHIPhysicalDevice.h"
#include "RHI/RHIDevice.h"
#include "RHI/RHISwapchain.h"
#include "RHI/RHIBuffer.h"
#include "RHI/RHIShader.h"
#include "RHI/RHIPipeline.h"
#include "RHI/RHICommandBuffer.h"
#include "RHI/RHIDescriptorSetLayout.h"
#include "RHI/RHIDescriptorPool.h"
#include "RHI/RHIDescriptorSet.h"
#include "RHI/RHITexture.h"
#include "RHI/RHISampler.h"
#include "Renderer/DepthBuffer.h"
#include "Renderer/FrameManager.h"
#include "Renderer/Debug/ImGuiRenderer.h"
#include "Resources/ModelLoader.h"
#include "Resources/Model.h"
#include "Resources/MaterialInstance.h"
#include "Resources/Vertex.h"
#include "Resources/UniformBufferObjects.h"
#include "Scene/Camera.h"

#include <imgui.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cstdlib>
#include <array>
#include <filesystem>

/**
 * @brief Per-mesh draw information for material-aware rendering.
 */
struct MeshDrawInfo {
    uint32_t IndexOffset = 0;
    uint32_t IndexCount = 0;
    int32_t MaterialIndex = -1;
};

/**
 * @brief Create a 1x1 white texture for use as default/fallback.
 */
Core::Ref<RHI::RHITexture> CreateDefaultWhiteTexture(const Core::Ref<RHI::RHIDevice>& device)
{
    RHI::TextureDesc texDesc;
    texDesc.Width = 1;
    texDesc.Height = 1;
    texDesc.Format = VK_FORMAT_R8G8B8A8_UNORM;
    texDesc.MipLevels = 1;
    texDesc.DebugName = "Default White Texture";

    auto texture = RHI::RHITexture::Create(device, texDesc);
    if (texture) {
        uint32_t whitePixel = 0xFFFFFFFF;
        texture->UploadData(device, &whitePixel, sizeof(whitePixel));
    }
    return texture;
}

/**
 * @brief Load textures for a model's materials.
 * @param model The loaded model with MaterialData.
 * @param device RHI device for texture creation.
 * @param modelPath Path to the model file (for resolving relative texture paths).
 * @return Vector of textures per material (5 textures per material: baseColor, normal, metallicRoughness, occlusion, emissive).
 */
std::vector<std::array<Core::Ref<RHI::RHITexture>, 5>> LoadMaterialTextures(
    const Core::Ref<Resources::Model>& model,
    const Core::Ref<RHI::RHIDevice>& device,
    [[maybe_unused]] const std::string& modelPath)
{
    std::vector<std::array<Core::Ref<RHI::RHITexture>, 5>> materialTextures;

    // Note: ModelLoader already stores full texture paths, so we use them directly

    for (size_t i = 0; i < model->GetMaterialDataCount(); ++i) {
        const auto& matData = model->GetMaterialData(i);
        std::array<Core::Ref<RHI::RHITexture>, 5> textures = {nullptr, nullptr, nullptr, nullptr, nullptr};

        // Base color texture (sRGB)
        if (!matData.BaseColorTexturePath.empty()) {
            textures[0] = RHI::RHITexture::LoadFromFile(device, matData.BaseColorTexturePath, true, true);
            if (textures[0]) {
                LOG_DEBUG("Loaded base color texture: {}", matData.BaseColorTexturePath);
            }
        }

        // Normal map (linear)
        if (!matData.NormalTexturePath.empty()) {
            textures[1] = RHI::RHITexture::LoadFromFile(device, matData.NormalTexturePath, false, true);
            if (textures[1]) {
                LOG_DEBUG("Loaded normal texture: {}", matData.NormalTexturePath);
            }
        }

        // Metallic-Roughness map (linear)
        if (!matData.MetallicRoughnessTexturePath.empty()) {
            textures[2] = RHI::RHITexture::LoadFromFile(device, matData.MetallicRoughnessTexturePath, false, true);
            if (textures[2]) {
                LOG_DEBUG("Loaded metallic-roughness texture: {}", matData.MetallicRoughnessTexturePath);
            }
        }

        // Occlusion map (linear)
        if (!matData.OcclusionTexturePath.empty()) {
            textures[3] = RHI::RHITexture::LoadFromFile(device, matData.OcclusionTexturePath, false, true);
            if (textures[3]) {
                LOG_DEBUG("Loaded occlusion texture: {}", matData.OcclusionTexturePath);
            }
        }

        // Emissive map (sRGB)
        if (!matData.EmissiveTexturePath.empty()) {
            textures[4] = RHI::RHITexture::LoadFromFile(device, matData.EmissiveTexturePath, true, true);
            if (textures[4]) {
                LOG_DEBUG("Loaded emissive texture: {}", matData.EmissiveTexturePath);
            }
        }

        materialTextures.push_back(textures);
    }

    return materialTextures;
}

/**
 * @brief Recreate swapchain and related resources on window resize.
 */
bool RecreateSwapchain(
    const Core::Ref<RHI::RHISwapchain>& swapchain,
    const Core::Ref<RHI::RHIDevice>& device,
    Platform::Window& window,
    Core::Ref<Renderer::DepthBuffer>& depthBuffer)
{
    uint32_t width = 0;
    uint32_t height = 0;
    window.GetFramebufferSize(width, height);

    // Wait if window is minimized
    while (width == 0 || height == 0)
    {
        window.GetFramebufferSize(width, height);
        window.WaitEvents();
    }

    if (!swapchain->Recreate(width, height))
    {
        return false;
    }

    // Recreate depth buffer
    return depthBuffer->Resize(device, width, height);
}

/**
 * @brief Transition image layout.
 */
void TransitionImageLayout(
    const Core::Ref<RHI::RHICommandBuffer>& cmdBuffer,
    VkImage image,
    VkImageLayout oldLayout,
    VkImageLayout newLayout,
    VkImageAspectFlags aspectMask,
    VkPipelineStageFlags srcStage,
    VkPipelineStageFlags dstStage,
    VkAccessFlags srcAccess,
    VkAccessFlags dstAccess)
{
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = aspectMask;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = srcAccess;
    barrier.dstAccessMask = dstAccess;

    cmdBuffer->PipelineBarrier(srcStage, dstStage, {}, {}, {barrier});
}

/**
 * @brief Load a new model and recreate all GPU resources including materials.
 * @param filepath Path to the glTF/glb file.
 * @param device RHI device for buffer creation.
 * @param loadedModel Reference to model pointer (will be replaced).
 * @param vertexBuffer Reference to vertex buffer (will be recreated).
 * @param indexBuffer Reference to index buffer (will be recreated).
 * @param meshDrawInfos Reference to mesh draw info list (will be recreated).
 * @param materialTextures Reference to material textures (will be recreated).
 * @param materialInstances Reference to material instances (will be recreated).
 * @param materialDescriptorPool Reference to material descriptor pool (will be recreated).
 * @param materialDescriptorLayout Material descriptor set layout (read-only).
 * @param materialSampler Material sampler (read-only).
 * @param defaultTexture Default white texture (read-only).
 * @param totalIndexCount Reference to total index count (will be updated).
 * @param camera Reference to camera (position will be updated).
 * @return true on success, false on failure.
 */
bool LoadNewModel(
    const std::string& filepath,
    const Core::Ref<RHI::RHIDevice>& device,
    Core::Ref<Resources::Model>& loadedModel,
    Core::Ref<RHI::RHIBuffer>& vertexBuffer,
    Core::Ref<RHI::RHIBuffer>& indexBuffer,
    std::vector<MeshDrawInfo>& meshDrawInfos,
    std::vector<std::array<Core::Ref<RHI::RHITexture>, 5>>& materialTextures,
    std::vector<Core::Ref<Resources::MaterialInstance>>& materialInstances,
    Core::Ref<Resources::MaterialInstance>& fallbackMaterial,
    Core::Ref<RHI::RHIDescriptorPool>& materialDescriptorPool,
    const Core::Ref<RHI::RHIDescriptorSetLayout>& materialDescriptorLayout,
    const Core::Ref<RHI::RHISampler>& materialSampler,
    const Core::Ref<RHI::RHITexture>& defaultTexture,
    uint32_t& totalIndexCount,
    Scene::Camera& camera)
{
    // Wait for GPU to finish using current resources
    device->WaitIdle();

    // Load new model
    Resources::ModelLoadOptions options;
    options.CalculateTangents = true;

    auto newModel = Resources::ModelLoader::LoadGLTF(filepath, options);
    if (!newModel || newModel->GetMeshCount() == 0)
    {
        LOG_ERROR("Failed to load model: {}", filepath);
        return false;
    }

    // Merge all meshes into single buffers and build draw info
    std::vector<Resources::Vertex> mergedVertices;
    std::vector<uint32_t> mergedIndices;
    std::vector<MeshDrawInfo> newMeshDrawInfos;

    for (size_t meshIdx = 0; meshIdx < newModel->GetMeshCount(); ++meshIdx)
    {
        const auto& mesh = newModel->GetMesh(meshIdx);
        uint32_t vertexOffset = static_cast<uint32_t>(mergedVertices.size());

        mergedVertices.insert(mergedVertices.end(), mesh.Vertices.begin(), mesh.Vertices.end());

        MeshDrawInfo drawInfo;
        drawInfo.IndexOffset = static_cast<uint32_t>(mergedIndices.size());

        for (uint32_t index : mesh.Indices)
        {
            mergedIndices.push_back(vertexOffset + index);
        }

        drawInfo.IndexCount = static_cast<uint32_t>(mesh.Indices.size());
        drawInfo.MaterialIndex = mesh.MaterialIndex;
        newMeshDrawInfos.push_back(drawInfo);
    }

    // Create new vertex buffer
    RHI::BufferDesc vertexDesc;
    vertexDesc.Size = sizeof(Resources::Vertex) * mergedVertices.size();
    vertexDesc.Usage = RHI::BufferUsage::Vertex;
    vertexDesc.Memory = RHI::MemoryUsage::CpuToGpu;
    vertexDesc.DebugName = "Model Vertex Buffer";

    auto newVertexBuffer = RHI::RHIBuffer::Create(device, vertexDesc);
    if (!newVertexBuffer)
    {
        LOG_ERROR("Failed to create vertex buffer for new model");
        return false;
    }
    newVertexBuffer->SetData(mergedVertices.data(), vertexDesc.Size);

    // Create new index buffer
    RHI::BufferDesc indexDesc;
    indexDesc.Size = sizeof(uint32_t) * mergedIndices.size();
    indexDesc.Usage = RHI::BufferUsage::Index;
    indexDesc.Memory = RHI::MemoryUsage::CpuToGpu;
    indexDesc.DebugName = "Model Index Buffer";

    auto newIndexBuffer = RHI::RHIBuffer::Create(device, indexDesc);
    if (!newIndexBuffer)
    {
        LOG_ERROR("Failed to create index buffer for new model");
        return false;
    }
    newIndexBuffer->SetData(mergedIndices.data(), indexDesc.Size);

    // Load textures for new model materials
    auto newMaterialTextures = LoadMaterialTextures(newModel, device, filepath);
    LOG_INFO("Loaded textures for {} materials", newMaterialTextures.size());

    // Calculate pool size based on number of materials (+1 for fallback material)
    size_t numMaterials = std::max(newModel->GetMaterialDataCount(), static_cast<size_t>(1));
    size_t poolSize = numMaterials + 1; // +1 for fallback material

    // Create new material descriptor pool
    auto newMaterialDescriptorPool = RHI::RHIDescriptorPool::Create(
        device,
        static_cast<uint32_t>(poolSize),
        {
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, static_cast<uint32_t>(poolSize)},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast<uint32_t>(poolSize * 5)}
        });

    if (!newMaterialDescriptorPool)
    {
        LOG_ERROR("Failed to create material descriptor pool for new model");
        return false;
    }

    // Create new material instances
    // Reserve space to maintain index alignment with material data
    std::vector<Core::Ref<Resources::MaterialInstance>> newMaterialInstances;
    newMaterialInstances.reserve(newModel->GetMaterialDataCount());

    // Create a fallback material first (used when individual material creation fails)
    auto newFallbackMaterial = Resources::MaterialInstance::Create(
        device, newMaterialDescriptorPool, materialDescriptorLayout, materialSampler, defaultTexture);
    if (newFallbackMaterial)
    {
        newFallbackMaterial->SetBaseColor(glm::vec4(0.8f, 0.8f, 0.8f, 1.0f));
        newFallbackMaterial->SetRoughness(0.5f);
    }

    for (size_t i = 0; i < newModel->GetMaterialDataCount(); ++i)
    {
        const auto& matData = newModel->GetMaterialData(i);
        const auto& textures = newMaterialTextures[i];

        auto matInstance = Resources::MaterialInstance::Create(
            device, newMaterialDescriptorPool, materialDescriptorLayout, materialSampler, defaultTexture);

        if (!matInstance)
        {
            LOG_ERROR("Failed to create material instance {}, using fallback", i);
            // Push fallback to maintain index alignment
            newMaterialInstances.push_back(newFallbackMaterial);
            continue;
        }

        // Set material parameters from glTF data
        matInstance->SetBaseColor(glm::vec4(
            matData.BaseColorFactor[0],
            matData.BaseColorFactor[1],
            matData.BaseColorFactor[2],
            matData.BaseColorFactor[3]));
        matInstance->SetMetallic(matData.MetallicFactor);
        matInstance->SetRoughness(matData.RoughnessFactor);
        matInstance->SetEmissiveFactor(glm::vec3(
            matData.EmissiveFactor[0],
            matData.EmissiveFactor[1],
            matData.EmissiveFactor[2]));
        matInstance->SetAlphaCutoff(matData.AlphaCutoff);

        // Bind textures
        if (textures[0]) matInstance->SetTexture(Resources::TextureSlot::BaseColor, textures[0]);
        if (textures[1]) matInstance->SetTexture(Resources::TextureSlot::Normal, textures[1]);
        if (textures[2]) matInstance->SetTexture(Resources::TextureSlot::MetallicRoughness, textures[2]);
        if (textures[3]) matInstance->SetTexture(Resources::TextureSlot::Occlusion, textures[3]);
        if (textures[4]) matInstance->SetTexture(Resources::TextureSlot::Emissive, textures[4]);

        newMaterialInstances.push_back(matInstance);
        LOG_DEBUG("Created material instance: {}", matData.Name);
    }

    // If no materials exist in model, add the fallback as the only material
    if (newMaterialInstances.empty() && newFallbackMaterial)
    {
        newMaterialInstances.push_back(newFallbackMaterial);
    }

    LOG_INFO("Created {} material instances", newMaterialInstances.size());

    // Clear old resources (GPU is idle, safe to destroy)
    // Order matters: clear instances first, then fallback, then textures, then pool
    materialInstances.clear();
    fallbackMaterial.reset();
    for (auto& texArr : materialTextures) {
        for (auto& tex : texArr) tex.reset();
    }
    materialTextures.clear();
    materialDescriptorPool.reset();

    // Assign new resources
    vertexBuffer = std::move(newVertexBuffer);
    indexBuffer = std::move(newIndexBuffer);
    meshDrawInfos = std::move(newMeshDrawInfos);
    materialTextures = std::move(newMaterialTextures);
    materialInstances = std::move(newMaterialInstances);
    fallbackMaterial = std::move(newFallbackMaterial);
    materialDescriptorPool = std::move(newMaterialDescriptorPool);
    totalIndexCount = static_cast<uint32_t>(mergedIndices.size());
    loadedModel = newModel;

    // Update camera position for new model bounds
    const auto& bounds = newModel->GetBounds();
    float centerArr[3], extentArr[3];
    bounds.GetCenter(centerArr);
    bounds.GetExtents(extentArr);
    glm::vec3 center(centerArr[0], centerArr[1], centerArr[2]);
    glm::vec3 extent(extentArr[0], extentArr[1], extentArr[2]);
    float maxExtent = glm::max(extent.x, glm::max(extent.y, extent.z));
    float cameraDistance = maxExtent * 5.0f;

    camera.SetPosition(center + glm::vec3(0.0f, maxExtent * 0.5f, cameraDistance));
    camera.LookAt(center);

    LOG_INFO("Model loaded: {} ({} meshes, {} vertices, {} triangles)",
             loadedModel->GetName(),
             loadedModel->GetMeshCount(),
             loadedModel->GetTotalVertexCount(),
             loadedModel->GetTotalIndexCount() / 3);

    return true;
}

int main()
{
    // Initialize logging system
    Core::Log::Init();

    LOG_INFO("Vulkan Renderer - 3D Model Viewer");

    // =========================================================================
    // Window Creation
    // =========================================================================
    Platform::WindowConfig windowConfig;
    windowConfig.Width = 1280;
    windowConfig.Height = 720;
    windowConfig.Title = "Vulkan Renderer - 3D Model Viewer";
    windowConfig.Resizable = true;

    Platform::Window window(windowConfig);
    Platform::Input::Init(window);

    LOG_INFO("Window created: {}x{}", windowConfig.Width, windowConfig.Height);

    // =========================================================================
    // Vulkan Instance
    // =========================================================================
    RHI::RHIInstanceConfig instanceConfig;
    instanceConfig.ApplicationName = "3D Model Viewer";
    instanceConfig.ApplicationVersion = VK_MAKE_VERSION(1, 0, 0);
    instanceConfig.EngineName = "Vulkan Renderer";
    instanceConfig.EngineVersion = VK_MAKE_VERSION(0, 1, 0);
#ifdef _DEBUG
    instanceConfig.EnableValidation = true;
#else
    instanceConfig.EnableValidation = false;
#endif

    auto instance = RHI::RHIInstance::Create(instanceConfig);
    if (!instance)
    {
        LOG_FATAL("Failed to create Vulkan instance!");
        return EXIT_FAILURE;
    }
    LOG_INFO("Vulkan instance created (validation: {})",
             instance->IsValidationEnabled() ? "enabled" : "disabled");

    // =========================================================================
    // Surface Creation
    // =========================================================================
    VkSurfaceKHR surface = window.CreateSurface(instance->GetHandle());
    if (surface == VK_NULL_HANDLE)
    {
        LOG_FATAL("Failed to create window surface!");
        return EXIT_FAILURE;
    }
    LOG_INFO("Window surface created");

    // =========================================================================
    // Physical Device Selection
    // =========================================================================
    auto physicalDevice = RHI::RHIPhysicalDevice::Select(instance, surface);
    if (!physicalDevice)
    {
        LOG_FATAL("Failed to find a suitable GPU!");
        return EXIT_FAILURE;
    }
    LOG_INFO("Selected GPU: {}", physicalDevice->GetInfo().GetDeviceName());

    // =========================================================================
    // Logical Device Creation
    // =========================================================================
    auto device = RHI::RHIDevice::Create(instance, physicalDevice, surface);
    if (!device)
    {
        LOG_FATAL("Failed to create logical device!");
        return EXIT_FAILURE;
    }
    LOG_INFO("Logical device created");

    // =========================================================================
    // Swapchain Creation
    // =========================================================================
    uint32_t fbWidth = 0;
    uint32_t fbHeight = 0;
    window.GetFramebufferSize(fbWidth, fbHeight);

    auto swapchain = RHI::RHISwapchain::Create(device, surface, fbWidth, fbHeight);
    if (!swapchain)
    {
        LOG_FATAL("Failed to create swapchain!");
        return EXIT_FAILURE;
    }
    LOG_INFO("Swapchain created: {}x{}, {} images",
             swapchain->GetExtent().width,
             swapchain->GetExtent().height,
             swapchain->GetImageCount());

    // =========================================================================
    // Depth Buffer
    // =========================================================================
    Renderer::DepthBufferDesc depthBufferDesc;
    depthBufferDesc.Width = fbWidth;
    depthBufferDesc.Height = fbHeight;
    depthBufferDesc.Format = VK_FORMAT_D32_SFLOAT;

    auto depthBuffer = Renderer::DepthBuffer::Create(device, depthBufferDesc);
    if (!depthBuffer)
    {
        LOG_FATAL("Failed to create depth buffer!");
        return EXIT_FAILURE;
    }
    LOG_INFO("Depth buffer created");

    // =========================================================================
    // Frame Manager
    // =========================================================================
    auto frameManager = Renderer::FrameManager::Create(device);
    if (!frameManager)
    {
        LOG_FATAL("Failed to create frame manager!");
        return EXIT_FAILURE;
    }
    LOG_INFO("Frame manager created ({} frames in flight)", Renderer::MAX_FRAMES_IN_FLIGHT);

    if (!frameManager->CreateRenderFinishedSemaphores(device, swapchain->GetImageCount()))
    {
        LOG_FATAL("Failed to create render finished semaphores!");
        return EXIT_FAILURE;
    }

    // =========================================================================
    // Model Loading
    // =========================================================================
    std::string modelPath = "assets/models/a_contortionist_dancer/scene.gltf";

    Resources::ModelLoadOptions modelOptions;
    modelOptions.CalculateTangents = true;

    auto loadedModel = Resources::ModelLoader::LoadGLTF(modelPath, modelOptions);

    if (!loadedModel || loadedModel->GetMeshCount() == 0)
    {
        LOG_FATAL("Failed to load model!");
        return EXIT_FAILURE;
    }

    LOG_INFO("Model loaded: {} ({} meshes, {} materials, {} vertices, {} triangles)",
             loadedModel->GetName(),
             loadedModel->GetMeshCount(),
             loadedModel->GetMaterialDataCount(),
             loadedModel->GetTotalVertexCount(),
             loadedModel->GetTotalIndexCount() / 3);

    // =========================================================================
    // Model Buffers (merge all meshes but track per-mesh draw info)
    // =========================================================================
    std::vector<Resources::Vertex> mergedVertices;
    std::vector<uint32_t> mergedIndices;
    std::vector<MeshDrawInfo> meshDrawInfos;

    for (size_t meshIdx = 0; meshIdx < loadedModel->GetMeshCount(); ++meshIdx)
    {
        const auto& mesh = loadedModel->GetMesh(meshIdx);
        uint32_t vertexOffset = static_cast<uint32_t>(mergedVertices.size());
        uint32_t indexOffset = static_cast<uint32_t>(mergedIndices.size());

        // Add vertices
        mergedVertices.insert(mergedVertices.end(), mesh.Vertices.begin(), mesh.Vertices.end());

        // Add indices with offset
        for (uint32_t index : mesh.Indices)
        {
            mergedIndices.push_back(vertexOffset + index);
        }

        // Track draw info for this mesh
        MeshDrawInfo drawInfo;
        drawInfo.IndexOffset = indexOffset;
        drawInfo.IndexCount = static_cast<uint32_t>(mesh.Indices.size());
        drawInfo.MaterialIndex = mesh.MaterialIndex;
        meshDrawInfos.push_back(drawInfo);
    }

    // Vertex buffer
    RHI::BufferDesc vertexBufferDesc;
    vertexBufferDesc.Size = sizeof(Resources::Vertex) * mergedVertices.size();
    vertexBufferDesc.Usage = RHI::BufferUsage::Vertex;
    vertexBufferDesc.Memory = RHI::MemoryUsage::CpuToGpu;
    vertexBufferDesc.DebugName = "Model Vertex Buffer";

    auto vertexBuffer = RHI::RHIBuffer::Create(device, vertexBufferDesc);
    if (!vertexBuffer)
    {
        LOG_FATAL("Failed to create vertex buffer!");
        return EXIT_FAILURE;
    }
    vertexBuffer->SetData(mergedVertices.data(), vertexBufferDesc.Size);
    LOG_INFO("Vertex buffer created: {} bytes", vertexBufferDesc.Size);

    // Index buffer
    RHI::BufferDesc indexBufferDesc;
    indexBufferDesc.Size = sizeof(uint32_t) * mergedIndices.size();
    indexBufferDesc.Usage = RHI::BufferUsage::Index;
    indexBufferDesc.Memory = RHI::MemoryUsage::CpuToGpu;
    indexBufferDesc.DebugName = "Model Index Buffer";

    auto indexBuffer = RHI::RHIBuffer::Create(device, indexBufferDesc);
    if (!indexBuffer)
    {
        LOG_FATAL("Failed to create index buffer!");
        return EXIT_FAILURE;
    }
    indexBuffer->SetData(mergedIndices.data(), indexBufferDesc.Size);
    LOG_INFO("Index buffer created: {} bytes", indexBufferDesc.Size);

    // Store total index count for drawing
    uint32_t totalIndexCount = static_cast<uint32_t>(mergedIndices.size());

    // =========================================================================
    // Material System Setup
    // =========================================================================

    // Create default white texture for unbound slots
    auto defaultTexture = CreateDefaultWhiteTexture(device);
    if (!defaultTexture)
    {
        LOG_FATAL("Failed to create default white texture!");
        return EXIT_FAILURE;
    }
    LOG_INFO("Default white texture created");

    // Create sampler for material textures
    auto materialSampler = RHI::RHISampler::CreateLinear(device);
    if (!materialSampler)
    {
        LOG_FATAL("Failed to create material sampler!");
        return EXIT_FAILURE;
    }
    LOG_INFO("Material sampler created");

    // Load textures from model materials
    auto materialTextures = LoadMaterialTextures(loadedModel, device, modelPath);
    LOG_INFO("Loaded textures for {} materials", materialTextures.size());

    // Create material descriptor set layout
    auto materialDescriptorLayout = Resources::MaterialInstance::CreateDescriptorSetLayout(device);
    if (!materialDescriptorLayout)
    {
        LOG_FATAL("Failed to create material descriptor set layout!");
        return EXIT_FAILURE;
    }

    // Calculate pool size based on number of materials (+1 for fallback material)
    size_t numMaterials = std::max(loadedModel->GetMaterialDataCount(), static_cast<size_t>(1));
    size_t poolSize = numMaterials + 1; // +1 for fallback material

    // Create material descriptor pool
    auto materialDescriptorPool = RHI::RHIDescriptorPool::Create(
        device,
        static_cast<uint32_t>(poolSize),
        {
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, static_cast<uint32_t>(poolSize)},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast<uint32_t>(poolSize * 5)}
        });

    if (!materialDescriptorPool)
    {
        LOG_FATAL("Failed to create material descriptor pool!");
        return EXIT_FAILURE;
    }

    // Create material instances
    // Reserve space to maintain index alignment with material data
    std::vector<Core::Ref<Resources::MaterialInstance>> materialInstances;
    materialInstances.reserve(loadedModel->GetMaterialDataCount());

    // Create a fallback material first (used when individual material creation fails)
    auto fallbackMaterial = Resources::MaterialInstance::Create(
        device, materialDescriptorPool, materialDescriptorLayout, materialSampler, defaultTexture);
    if (fallbackMaterial)
    {
        fallbackMaterial->SetBaseColor(glm::vec4(0.7f, 0.7f, 0.7f, 1.0f));
        fallbackMaterial->SetRoughness(0.5f);
    }

    for (size_t i = 0; i < loadedModel->GetMaterialDataCount(); ++i)
    {
        const auto& matData = loadedModel->GetMaterialData(i);
        const auto& textures = materialTextures[i];

        auto matInstance = Resources::MaterialInstance::Create(
            device, materialDescriptorPool, materialDescriptorLayout, materialSampler, defaultTexture);

        if (!matInstance)
        {
            LOG_ERROR("Failed to create material instance {}, using fallback", i);
            // Push fallback to maintain index alignment
            materialInstances.push_back(fallbackMaterial);
            continue;
        }

        // Set material parameters from glTF data
        matInstance->SetBaseColor(glm::vec4(
            matData.BaseColorFactor[0],
            matData.BaseColorFactor[1],
            matData.BaseColorFactor[2],
            matData.BaseColorFactor[3]));
        matInstance->SetMetallic(matData.MetallicFactor);
        matInstance->SetRoughness(matData.RoughnessFactor);
        matInstance->SetEmissiveFactor(glm::vec3(
            matData.EmissiveFactor[0],
            matData.EmissiveFactor[1],
            matData.EmissiveFactor[2]));

        if (matData.Alpha == Resources::MaterialData::AlphaMode::Mask)
        {
            matInstance->SetAlphaCutoff(matData.AlphaCutoff);
        }

        // Bind textures
        if (textures[0]) matInstance->SetTexture(Resources::TextureSlot::BaseColor, textures[0]);
        if (textures[1]) matInstance->SetTexture(Resources::TextureSlot::Normal, textures[1]);
        if (textures[2]) matInstance->SetTexture(Resources::TextureSlot::MetallicRoughness, textures[2]);
        if (textures[3]) matInstance->SetTexture(Resources::TextureSlot::Occlusion, textures[3]);
        if (textures[4]) matInstance->SetTexture(Resources::TextureSlot::Emissive, textures[4]);

        materialInstances.push_back(matInstance);
        LOG_DEBUG("Created material instance: {}", matData.Name);
    }

    // If no materials exist in model, add the fallback as the only material
    if (materialInstances.empty() && fallbackMaterial)
    {
        materialInstances.push_back(fallbackMaterial);
    }

    LOG_INFO("Created {} material instances", materialInstances.size());

    // =========================================================================
    // Camera and Uniform Buffers
    // =========================================================================
    Scene::Camera camera;

    // Position camera based on model bounds
    const auto& bounds = loadedModel->GetBounds();
    float centerArr[3], extentArr[3];
    bounds.GetCenter(centerArr);
    bounds.GetExtents(extentArr);
    glm::vec3 center(centerArr[0], centerArr[1], centerArr[2]);
    glm::vec3 extent(extentArr[0], extentArr[1], extentArr[2]);
    float maxExtent = glm::max(extent.x, glm::max(extent.y, extent.z));
    float cameraDistance = maxExtent * 5.0f;

    camera.SetPosition(center + glm::vec3(0.0f, maxExtent * 0.5f, cameraDistance));
    camera.LookAt(center);
    camera.SetPerspective(45.0f, static_cast<float>(fbWidth) / fbHeight, 0.1f, 1000.0f);

    // Camera uniform buffer (per frame)
    std::array<Core::Ref<RHI::RHIBuffer>, Renderer::MAX_FRAMES_IN_FLIGHT> cameraUBOs;
    for (size_t i = 0; i < Renderer::MAX_FRAMES_IN_FLIGHT; ++i)
    {
        RHI::BufferDesc uboDesc;
        uboDesc.Size = sizeof(Resources::CameraUBO);
        uboDesc.Usage = RHI::BufferUsage::Uniform;
        uboDesc.Memory = RHI::MemoryUsage::CpuToGpu;
        uboDesc.DebugName = "Camera UBO";

        cameraUBOs[i] = RHI::RHIBuffer::Create(device, uboDesc);
        if (!cameraUBOs[i])
        {
            LOG_FATAL("Failed to create camera UBO!");
            return EXIT_FAILURE;
        }
    }

    // Object uniform buffer (per frame)
    std::array<Core::Ref<RHI::RHIBuffer>, Renderer::MAX_FRAMES_IN_FLIGHT> objectUBOs;
    for (size_t i = 0; i < Renderer::MAX_FRAMES_IN_FLIGHT; ++i)
    {
        RHI::BufferDesc uboDesc;
        uboDesc.Size = sizeof(Resources::ObjectUBO);
        uboDesc.Usage = RHI::BufferUsage::Uniform;
        uboDesc.Memory = RHI::MemoryUsage::CpuToGpu;
        uboDesc.DebugName = "Object UBO";

        objectUBOs[i] = RHI::RHIBuffer::Create(device, uboDesc);
        if (!objectUBOs[i])
        {
            LOG_FATAL("Failed to create object UBO!");
            return EXIT_FAILURE;
        }
    }
    LOG_INFO("Uniform buffers created");

    // =========================================================================
    // Descriptor Set Layout
    // =========================================================================
    // Bindings:
    // 0: CameraData (UBO) - vertex + fragment
    // 1: ObjectData (UBO) - vertex
    auto descriptorLayout = RHI::RHIDescriptorSetLayout::Create(device, {
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT},
        {1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT}
    });
    if (!descriptorLayout)
    {
        LOG_FATAL("Failed to create descriptor set layout!");
        return EXIT_FAILURE;
    }

    // =========================================================================
    // Descriptor Pool and Sets
    // =========================================================================
    auto descriptorPool = RHI::RHIDescriptorPool::Create(
        device,
        Renderer::MAX_FRAMES_IN_FLIGHT,
        {
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, Renderer::MAX_FRAMES_IN_FLIGHT * 2}
        });

    if (!descriptorPool)
    {
        LOG_FATAL("Failed to create descriptor pool!");
        return EXIT_FAILURE;
    }

    std::array<Core::Ref<RHI::RHIDescriptorSet>, Renderer::MAX_FRAMES_IN_FLIGHT> descriptorSets;
    for (size_t i = 0; i < Renderer::MAX_FRAMES_IN_FLIGHT; ++i)
    {
        descriptorSets[i] = RHI::RHIDescriptorSet::Create(device, descriptorPool, descriptorLayout);
        if (!descriptorSets[i])
        {
            LOG_FATAL("Failed to create descriptor set!");
            return EXIT_FAILURE;
        }

        // Bind UBOs
        descriptorSets[i]->UpdateBuffer(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, cameraUBOs[i]);
        descriptorSets[i]->UpdateBuffer(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, objectUBOs[i]);
    }
    LOG_INFO("Descriptor sets created");

    // =========================================================================
    // Shaders
    // =========================================================================
    auto vertexShader = RHI::RHIShader::CreateFromHLSL(
        device,
        "shaders/hlsl/vertex/model.hlsl",
        RHI::ShaderStage::Vertex);
    if (!vertexShader)
    {
        LOG_FATAL("Failed to create vertex shader!");
        return EXIT_FAILURE;
    }
    LOG_INFO("Vertex shader loaded");

    auto fragmentShader = RHI::RHIShader::CreateFromHLSL(
        device,
        "shaders/hlsl/pixel/model_pbr.hlsl",
        RHI::ShaderStage::Fragment);
    if (!fragmentShader)
    {
        LOG_FATAL("Failed to create fragment shader!");
        return EXIT_FAILURE;
    }
    LOG_INFO("Fragment shader (PBR) loaded");

    // =========================================================================
    // Graphics Pipeline
    // =========================================================================
    RHI::GraphicsPipelineDesc pipelineDesc;
    pipelineDesc.VertexShader = vertexShader;
    pipelineDesc.FragmentShader = fragmentShader;

    // Vertex input (using Resources::Vertex format)
    auto bindingDescription = Resources::Vertex::GetBindingDescription();
    pipelineDesc.VertexBindings.push_back(bindingDescription);

    auto attributeDescriptions = Resources::Vertex::GetAttributeDescriptions();
    for (const auto& attr : attributeDescriptions)
    {
        pipelineDesc.VertexAttributes.push_back(attr);
    }

    // Rasterization
    pipelineDesc.CullMode = VK_CULL_MODE_BACK_BIT;
    pipelineDesc.FrontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    // Depth testing
    pipelineDesc.DepthTestEnable = true;
    pipelineDesc.DepthWriteEnable = true;
    pipelineDesc.DepthCompareOp = VK_COMPARE_OP_LESS;

    // Color blending
    RHI::ColorBlendAttachment colorBlendAttachment;
    colorBlendAttachment.BlendEnable = false;
    pipelineDesc.ColorBlendAttachments.push_back(colorBlendAttachment);

    // Formats
    pipelineDesc.ColorAttachmentFormats.push_back(swapchain->GetImageFormat());
    pipelineDesc.DepthAttachmentFormat = depthBuffer->GetFormat();

    // Descriptor set layouts: Set 0 = scene data, Set 1 = material data
    pipelineDesc.DescriptorSetLayouts.push_back(descriptorLayout->GetHandle());
    pipelineDesc.DescriptorSetLayouts.push_back(materialDescriptorLayout->GetHandle());

    auto pipeline = RHI::RHIPipeline::CreateGraphics(device, pipelineDesc);
    if (!pipeline)
    {
        LOG_FATAL("Failed to create graphics pipeline!");
        return EXIT_FAILURE;
    }
    LOG_INFO("Graphics pipeline created");

    // =========================================================================
    // ImGui Renderer
    // =========================================================================
    Renderer::ImGuiRendererConfig imguiConfig;
    imguiConfig.Instance = instance->GetHandle();
    imguiConfig.QueueFamily = device->GetQueueFamilies().GraphicsFamily.value();
    imguiConfig.GraphicsQueue = device->GetGraphicsQueue();
    imguiConfig.ColorFormat = swapchain->GetImageFormat();
    imguiConfig.ImageCount = swapchain->GetImageCount();

    auto imguiRenderer = Renderer::ImGuiRenderer::Create(device, window, imguiConfig);
    if (!imguiRenderer)
    {
        LOG_FATAL("Failed to create ImGui renderer!");
        return EXIT_FAILURE;
    }
    LOG_INFO("ImGui renderer created");

    // =========================================================================
    // Main Loop
    // =========================================================================
    LOG_INFO("Entering main loop...");
    LOG_INFO("Controls: WASD to move, Mouse to look, ESC to exit");

    bool framebufferResized = false;

    // Subscribe to window resize events using the event system
    window.GetEventDispatcher().Subscribe<Core::WindowResizeEvent>(
        [&framebufferResized, &camera](Core::WindowResizeEvent& e) {
            LOG_DEBUG("Window resized to: {}x{}", e.GetWidth(), e.GetHeight());
            framebufferResized = true;
            if (e.GetWidth() > 0 && e.GetHeight() > 0)
            {
                camera.SetAspectRatio(static_cast<float>(e.GetWidth()) / e.GetHeight());
            }
        }
    );

    // Frame timing
    Core::Timer& frameTimer = Core::GetGlobalTimer();
    frameTimer.Start();

    // Debug UI state
    bool showStatsWindow = true;
    bool showDemoWindow = false;

    // Camera control
    float cameraSpeed = 2.0f;
    float mouseSensitivity = 0.1f;
    bool mouseCaptured = false;
    float rotationAngle = 0.0f;

    // Debug stats
    Renderer::DebugStats stats{};

    while (!window.ShouldClose())
    {
        // Update frame timing
        frameTimer.Tick();
        float deltaTime = frameTimer.GetDeltaTime();

        // Update input
        Platform::Input::Update();
        window.PollEvents();

        // Global shortcuts
        if (Platform::Input::IsKeyPressed(Platform::KeyCode::Escape))
        {
            if (mouseCaptured)
            {
                mouseCaptured = false;
                Platform::Input::SetCursorMode(Platform::CursorMode::Normal);
            }
            else
            {
                break;
            }
        }
        if (Platform::Input::IsKeyPressed(Platform::KeyCode::F1))
        {
            showStatsWindow = !showStatsWindow;
        }
        if (Platform::Input::IsKeyPressed(Platform::KeyCode::F2))
        {
            showDemoWindow = !showDemoWindow;
        }

        // Mouse capture toggle
        if (Platform::Input::IsMouseButtonPressed(Platform::MouseButton::Right))
        {
            mouseCaptured = !mouseCaptured;
            Platform::Input::SetCursorMode(
                mouseCaptured ? Platform::CursorMode::Disabled : Platform::CursorMode::Normal);
        }

        // Camera movement
        glm::vec3 moveDir(0.0f);
        if (Platform::Input::IsKeyDown(Platform::KeyCode::W))
            moveDir += camera.GetForward();
        if (Platform::Input::IsKeyDown(Platform::KeyCode::S))
            moveDir -= camera.GetForward();
        if (Platform::Input::IsKeyDown(Platform::KeyCode::A))
            moveDir -= camera.GetRight();
        if (Platform::Input::IsKeyDown(Platform::KeyCode::D))
            moveDir += camera.GetRight();
        if (Platform::Input::IsKeyDown(Platform::KeyCode::Q))
            moveDir -= glm::vec3(0.0f, 1.0f, 0.0f);
        if (Platform::Input::IsKeyDown(Platform::KeyCode::E))
            moveDir += glm::vec3(0.0f, 1.0f, 0.0f);

        if (glm::length(moveDir) > 0.0f)
        {
            camera.SetPosition(camera.GetPosition() + glm::normalize(moveDir) * cameraSpeed * deltaTime);
        }

        // Camera rotation (when captured)
        if (mouseCaptured)
        {
            float dx = static_cast<float>(Platform::Input::GetMouseDeltaX());
            float dy = static_cast<float>(Platform::Input::GetMouseDeltaY());
            float pitch = camera.GetPitch() - dy * mouseSensitivity;
            float yaw = camera.GetYaw() + dx * mouseSensitivity;

            pitch = glm::clamp(pitch, -89.0f, 89.0f);
            camera.SetRotation(pitch, yaw);
        }

        // Auto-rotate model
        rotationAngle += deltaTime * 30.0f;

        // Handle window minimization
        window.GetFramebufferSize(fbWidth, fbHeight);
        if (fbWidth == 0 || fbHeight == 0)
        {
            window.WaitEvents();
            continue;
        }

        // Wait for frame
        if (!frameManager->WaitForFrame())
        {
            LOG_ERROR("Failed to wait for frame fence");
            break;
        }

        // Acquire swapchain image
        uint32_t imageIndex = swapchain->AcquireNextImage(
            frameManager->GetImageAvailableSemaphore());

        // Handle swapchain recreation
        if (imageIndex == UINT32_MAX || swapchain->NeedsRecreation() || framebufferResized)
        {
            framebufferResized = false;
            device->WaitIdle();

            if (!frameManager->ResetSyncPrimitives(device))
            {
                LOG_ERROR("Failed to reset sync primitives");
                break;
            }

            if (!RecreateSwapchain(swapchain, device, window, depthBuffer))
            {
                LOG_ERROR("Failed to recreate swapchain");
                break;
            }

            if (frameManager->GetRenderFinishedSemaphoreCount() != swapchain->GetImageCount())
            {
                if (!frameManager->CreateRenderFinishedSemaphores(device, swapchain->GetImageCount()))
                {
                    LOG_ERROR("Failed to recreate render finished semaphores");
                    break;
                }
            }

            frameManager->NextFrame();
            continue;
        }

        // Update uniform buffers
        uint32_t frameIndex = frameManager->GetCurrentFrameIndex();

        Resources::CameraUBO cameraData = camera.BuildCameraUBO();
        cameraUBOs[frameIndex]->SetData(&cameraData, sizeof(cameraData));

        Resources::ObjectUBO objectData;
        // First rotate around X axis by 0 degrees to stand the model upright,
        // then apply Y-axis auto-rotation
        glm::mat4 standUpright = glm::rotate(glm::mat4(1.0f), glm::radians(0.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        glm::mat4 autoRotate = glm::rotate(glm::mat4(1.0f), glm::radians(rotationAngle), glm::vec3(0.0f, 1.0f, 0.0f));
        objectData.Model = autoRotate * standUpright;
        objectData.NormalMatrix = glm::transpose(glm::inverse(objectData.Model));
        objectUBOs[frameIndex]->SetData(&objectData, sizeof(objectData));

        // =====================================================================
        // ImGui Frame
        // =====================================================================
        imguiRenderer->BeginFrame();

        stats.FrameTime = frameTimer.GetFrameTimeMs();
        stats.FPS = frameTimer.GetFPS();
        stats.DrawCalls = static_cast<uint32_t>(meshDrawInfos.size());
        stats.TriangleCount = totalIndexCount / 3;

        imguiRenderer->ShowStatsWindow(stats, &showStatsWindow);

        if (showStatsWindow)
        {
            ImGui::Begin("Model Info", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::Text("Name: %s", loadedModel->GetName().c_str());
            ImGui::Text("Meshes: %zu", loadedModel->GetMeshCount());
            ImGui::Text("Vertices: %zu", loadedModel->GetTotalVertexCount());
            ImGui::Text("Triangles: %zu", loadedModel->GetTotalIndexCount() / 3);
            ImGui::Separator();
            ImGui::Text("Camera Position: (%.1f, %.1f, %.1f)",
                        camera.GetPosition().x, camera.GetPosition().y, camera.GetPosition().z);
            ImGui::Text("Right-click to capture mouse");
            ImGui::Separator();
            if (ImGui::Button("Load Model..."))
            {
                auto filePath = Platform::FileDialog::OpenGLTFFile();
                if (filePath.has_value())
                {
                    if (!LoadNewModel(filePath.value(), device, loadedModel,
                                      vertexBuffer, indexBuffer, meshDrawInfos,
                                      materialTextures, materialInstances, fallbackMaterial,
                                      materialDescriptorPool, materialDescriptorLayout,
                                      materialSampler, defaultTexture,
                                      totalIndexCount, camera))
                    {
                        LOG_ERROR("Failed to load model from file dialog");
                    }
                }
            }
            ImGui::End();
        }

        imguiRenderer->ShowDemoWindow(&showDemoWindow);
        imguiRenderer->EndFrame();

        // =====================================================================
        // Record Commands
        // =====================================================================
        auto& cmdBuffer = frameManager->GetCommandBuffer();
        cmdBuffer->Reset();
        cmdBuffer->Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

        VkExtent2D extent = swapchain->GetExtent();
        VkImageView colorImageView = swapchain->GetImageView(imageIndex);
        VkImage colorImage = swapchain->GetImage(imageIndex);

        // Transition color attachment
        TransitionImageLayout(cmdBuffer, colorImage,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);

        // Transition depth attachment
        TransitionImageLayout(cmdBuffer, depthBuffer->GetImage(),
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            VK_IMAGE_ASPECT_DEPTH_BIT,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            0, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);

        // Begin rendering
        RHI::RenderingConfig renderConfig;
        renderConfig.RenderArea = {{0, 0}, extent};

        RHI::ColorAttachment colorAttachment;
        colorAttachment.ImageView = colorImageView;
        colorAttachment.Layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.LoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.StoreOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.ClearValue = {{0.1f, 0.1f, 0.15f, 1.0f}};
        renderConfig.ColorAttachments.push_back(colorAttachment);

        renderConfig.Depth.ImageView = depthBuffer->GetImageView();
        renderConfig.Depth.Layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        renderConfig.Depth.LoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        renderConfig.Depth.StoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        renderConfig.Depth.ClearValue = {1.0f, 0};

        cmdBuffer->BeginRendering(renderConfig);

        // Set viewport and scissor
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(extent.width);
        viewport.height = static_cast<float>(extent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        cmdBuffer->SetViewport(viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = extent;
        cmdBuffer->SetScissor(scissor);

        // Bind pipeline and scene data
        cmdBuffer->BindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetHandle());
        cmdBuffer->BindDescriptorSets(
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipeline->GetLayout(),
            0,
            {descriptorSets[frameIndex]->GetHandle()});
        cmdBuffer->BindVertexBuffer(vertexBuffer->GetHandle());
        cmdBuffer->BindIndexBuffer(indexBuffer->GetHandle(), VK_INDEX_TYPE_UINT32);

        // Draw each mesh with its material
        for (const auto& drawInfo : meshDrawInfos)
        {
            // Get material for this mesh
            int materialIdx = drawInfo.MaterialIndex;
            if (materialIdx >= 0 && materialIdx < static_cast<int>(materialInstances.size()))
            {
                auto& material = materialInstances[materialIdx];
                material->UploadToGPU();

                // Bind material descriptor set at set 1
                cmdBuffer->BindDescriptorSets(
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    pipeline->GetLayout(),
                    1,
                    {material->GetDescriptorSet()->GetHandle()});
            }
            else if (!materialInstances.empty())
            {
                // Use first material as fallback
                auto& material = materialInstances[0];
                material->UploadToGPU();
                cmdBuffer->BindDescriptorSets(
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    pipeline->GetLayout(),
                    1,
                    {material->GetDescriptorSet()->GetHandle()});
            }

            // Draw this mesh
            cmdBuffer->DrawIndexed(drawInfo.IndexCount, 1, drawInfo.IndexOffset, 0, 0);
        }

        cmdBuffer->EndRendering();

        // Render ImGui
        imguiRenderer->Render(cmdBuffer, colorImageView, extent);

        // Transition to present
        TransitionImageLayout(cmdBuffer, colorImage,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0);

        cmdBuffer->End();

        // Submit
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore waitSemaphores[] = {frameManager->GetImageAvailableSemaphore()};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;

        VkCommandBuffer cmdHandle = cmdBuffer->GetHandle();
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmdHandle;

        VkSemaphore signalSemaphores[] = {frameManager->GetRenderFinishedSemaphore(imageIndex)};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        VkResult result = vkQueueSubmit(
            device->GetGraphicsQueue(),
            1,
            &submitInfo,
            frameManager->GetInFlightFence());

        if (result != VK_SUCCESS)
        {
            LOG_ERROR("Failed to submit command buffer");
            break;
        }

        // Present
        bool presentResult = swapchain->Present(
            device->GetPresentQueue(),
            imageIndex,
            frameManager->GetRenderFinishedSemaphore(imageIndex));

        if (!presentResult || swapchain->NeedsRecreation())
        {
            framebufferResized = true;
        }

        frameManager->NextFrame();
    }

    // =========================================================================
    // Cleanup
    // =========================================================================
    LOG_INFO("Shutting down...");

    device->WaitIdle();

    imguiRenderer.reset();
    pipeline.reset();

    pipelineDesc.VertexShader.reset();
    pipelineDesc.FragmentShader.reset();
    fragmentShader.reset();
    vertexShader.reset();

    for (auto& ds : descriptorSets) ds.reset();
    descriptorPool.reset();
    descriptorLayout.reset();

    // Material system cleanup
    // Order matters: clear instances first, then fallback, then textures, then pool
    materialInstances.clear();
    fallbackMaterial.reset();
    for (auto& texArr : materialTextures) {
        for (auto& tex : texArr) tex.reset();
    }
    materialTextures.clear();
    materialDescriptorPool.reset();
    materialDescriptorLayout.reset();
    materialSampler.reset();
    defaultTexture.reset();

    for (auto& ubo : objectUBOs) ubo.reset();
    for (auto& ubo : cameraUBOs) ubo.reset();

    indexBuffer.reset();
    vertexBuffer.reset();
    loadedModel.reset();

    frameManager.reset();
    depthBuffer.reset();
    swapchain.reset();

    vkDestroySurfaceKHR(instance->GetHandle(), surface, nullptr);

    device.reset();
    physicalDevice.reset();
    instance.reset();

    Platform::Input::Shutdown();

    LOG_INFO("Goodbye!");
    Core::Log::Shutdown();

    return EXIT_SUCCESS;
}
