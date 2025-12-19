/**
 * @file main.cpp
 * @brief 3D Model Viewer application entry point.
 *
 * Renders a 3D model loaded from glTF format using Vulkan.
 */

#include "Core/Log.h"
#include "Core/Timer.h"
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
#include "Renderer/DepthBuffer.h"
#include "Renderer/FrameManager.h"
#include "Renderer/Debug/ImGuiRenderer.h"
#include "Resources/ModelLoader.h"
#include "Resources/Model.h"
#include "Resources/Vertex.h"
#include "Resources/UniformBufferObjects.h"
#include "Scene/Camera.h"

#include <imgui.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cstdlib>
#include <array>

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
    Resources::ModelLoadOptions modelOptions;
    modelOptions.CalculateTangents = true;

    auto loadedModel = Resources::ModelLoader::LoadGLTF(
        "assets/models/a_contortionist_dancer/scene.gltf",
        modelOptions);

    if (!loadedModel || loadedModel->GetMeshCount() == 0)
    {
        LOG_FATAL("Failed to load model!");
        return EXIT_FAILURE;
    }

    LOG_INFO("Model loaded: {} ({} meshes, {} vertices, {} triangles)",
             loadedModel->GetName(),
             loadedModel->GetMeshCount(),
             loadedModel->GetTotalVertexCount(),
             loadedModel->GetTotalIndexCount() / 3);

    // =========================================================================
    // Model Buffers
    // =========================================================================
    const auto& mesh = loadedModel->GetMesh(0);

    // Vertex buffer
    RHI::BufferDesc vertexBufferDesc;
    vertexBufferDesc.Size = sizeof(Resources::Vertex) * mesh.Vertices.size();
    vertexBufferDesc.Usage = RHI::BufferUsage::Vertex;
    vertexBufferDesc.Memory = RHI::MemoryUsage::CpuToGpu;
    vertexBufferDesc.DebugName = "Model Vertex Buffer";

    auto vertexBuffer = RHI::RHIBuffer::Create(device, vertexBufferDesc);
    if (!vertexBuffer)
    {
        LOG_FATAL("Failed to create vertex buffer!");
        return EXIT_FAILURE;
    }
    vertexBuffer->SetData(mesh.Vertices.data(), vertexBufferDesc.Size);
    LOG_INFO("Vertex buffer created: {} bytes", vertexBufferDesc.Size);

    // Index buffer
    RHI::BufferDesc indexBufferDesc;
    indexBufferDesc.Size = sizeof(uint32_t) * mesh.Indices.size();
    indexBufferDesc.Usage = RHI::BufferUsage::Index;
    indexBufferDesc.Memory = RHI::MemoryUsage::CpuToGpu;
    indexBufferDesc.DebugName = "Model Index Buffer";

    auto indexBuffer = RHI::RHIBuffer::Create(device, indexBufferDesc);
    if (!indexBuffer)
    {
        LOG_FATAL("Failed to create index buffer!");
        return EXIT_FAILURE;
    }
    indexBuffer->SetData(mesh.Indices.data(), indexBufferDesc.Size);
    LOG_INFO("Index buffer created: {} bytes", indexBufferDesc.Size);

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
        "shaders/hlsl/pixel/model.hlsl",
        RHI::ShaderStage::Fragment);
    if (!fragmentShader)
    {
        LOG_FATAL("Failed to create fragment shader!");
        return EXIT_FAILURE;
    }
    LOG_INFO("Fragment shader loaded");

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

    // Descriptor set layout
    pipelineDesc.DescriptorSetLayouts.push_back(descriptorLayout->GetHandle());

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
    window.SetResizeCallback([&framebufferResized, &camera](uint32_t width, uint32_t height) {
        LOG_DEBUG("Window resized to: {}x{}", width, height);
        framebufferResized = true;
        if (width > 0 && height > 0)
        {
            camera.SetAspectRatio(static_cast<float>(width) / height);
        }
    });

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
        // First rotate around X axis by 90 degrees to stand the model upright,
        // then apply Y-axis auto-rotation
        glm::mat4 standUpright = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
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
        stats.DrawCalls = 1;
        stats.TriangleCount = static_cast<uint32_t>(mesh.Indices.size() / 3);

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

        // Bind pipeline and draw
        cmdBuffer->BindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetHandle());
        cmdBuffer->BindDescriptorSets(
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipeline->GetLayout(),
            0,
            {descriptorSets[frameIndex]->GetHandle()});
        cmdBuffer->BindVertexBuffer(vertexBuffer->GetHandle());
        cmdBuffer->BindIndexBuffer(indexBuffer->GetHandle(), VK_INDEX_TYPE_UINT32);
        cmdBuffer->DrawIndexed(static_cast<uint32_t>(mesh.Indices.size()));

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
