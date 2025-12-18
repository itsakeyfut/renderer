/**
 * @file main.cpp
 * @brief Hello Triangle application entry point.
 *
 * This is the Phase 1 milestone implementation that displays a colored triangle
 * on screen using Vulkan dynamic rendering (Vulkan 1.3).
 */

#include "Core/Log.h"
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
#include "Renderer/FrameManager.h"

#include <glm/glm.hpp>

#include <cstdlib>
#include <array>

/**
 * @brief Vertex structure matching the HLSL shader input.
 */
struct Vertex
{
    glm::vec3 Position;
    glm::vec3 Color;

    static VkVertexInputBindingDescription GetBindingDescription()
    {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindingDescription;
    }

    static std::array<VkVertexInputAttributeDescription, 2> GetAttributeDescriptions()
    {
        std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};

        // Position attribute
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, Position);

        // Color attribute
        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, Color);

        return attributeDescriptions;
    }
};

/**
 * @brief Triangle vertex data with RGB colors.
 */
constexpr std::array<Vertex, 3> TRIANGLE_VERTICES = {{
    // Position (x, y, z),  Color (r, g, b)
    {{0.0f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},   // Top vertex - Red
    {{0.5f, 0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},    // Bottom right - Green
    {{-0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}}    // Bottom left - Blue
}};

/**
 * @brief Recreate swapchain and related resources on window resize.
 */
bool RecreateSwapchain(
    const Core::Ref<RHI::RHISwapchain>& swapchain,
    Platform::Window& window)
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

    return swapchain->Recreate(width, height);
}

/**
 * @brief Record rendering commands for a frame.
 */
void RecordCommands(
    const Core::Ref<RHI::RHICommandBuffer>& cmdBuffer,
    const Core::Ref<RHI::RHISwapchain>& swapchain,
    const Core::Ref<RHI::RHIPipeline>& pipeline,
    const Core::Ref<RHI::RHIBuffer>& vertexBuffer,
    uint32_t imageIndex)
{
    cmdBuffer->Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VkExtent2D extent = swapchain->GetExtent();
    VkImageView imageView = swapchain->GetImageView(imageIndex);
    VkImage image = swapchain->GetImage(imageIndex);

    // Transition image to color attachment optimal
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    cmdBuffer->PipelineBarrier(
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        {},
        {},
        {barrier});

    // Begin dynamic rendering
    RHI::RenderingConfig renderConfig;
    renderConfig.RenderArea = {{0, 0}, extent};

    RHI::ColorAttachment colorAttachment;
    colorAttachment.ImageView = imageView;
    colorAttachment.Layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.LoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.StoreOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.ClearValue = {{0.1f, 0.1f, 0.15f, 1.0f}};  // Dark blue-gray background
    renderConfig.ColorAttachments.push_back(colorAttachment);

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
    cmdBuffer->BindVertexBuffer(vertexBuffer->GetHandle());
    cmdBuffer->Draw(3);  // 3 vertices for triangle

    cmdBuffer->EndRendering();

    // Transition image to present layout
    barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = 0;

    cmdBuffer->PipelineBarrier(
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        {},
        {},
        {barrier});

    cmdBuffer->End();
}

int main()
{
    // Initialize logging system
    Core::Log::Init();

    LOG_INFO("Vulkan Renderer - Hello Triangle");
    LOG_INFO("Phase 1 Milestone");

    // =========================================================================
    // Window Creation
    // =========================================================================
    Platform::WindowConfig windowConfig;
    windowConfig.Width = 1280;
    windowConfig.Height = 720;
    windowConfig.Title = "Vulkan Renderer - Hello Triangle";
    windowConfig.Resizable = true;

    Platform::Window window(windowConfig);
    Platform::Input::Init(window);

    LOG_INFO("Window created: {}x{}", windowConfig.Width, windowConfig.Height);

    // =========================================================================
    // Vulkan Instance
    // =========================================================================
    RHI::RHIInstanceConfig instanceConfig;
    instanceConfig.ApplicationName = "Hello Triangle";
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
    // Frame Manager
    // =========================================================================
    auto frameManager = Renderer::FrameManager::Create(device);
    if (!frameManager)
    {
        LOG_FATAL("Failed to create frame manager!");
        return EXIT_FAILURE;
    }
    LOG_INFO("Frame manager created ({} frames in flight)", Renderer::MAX_FRAMES_IN_FLIGHT);

    // =========================================================================
    // Vertex Buffer
    // =========================================================================
    RHI::BufferDesc vertexBufferDesc;
    vertexBufferDesc.Size = sizeof(Vertex) * TRIANGLE_VERTICES.size();
    vertexBufferDesc.Usage = RHI::BufferUsage::Vertex;
    vertexBufferDesc.Memory = RHI::MemoryUsage::CpuToGpu;  // Host-visible for simplicity
    vertexBufferDesc.DebugName = "Triangle Vertex Buffer";

    auto vertexBuffer = RHI::RHIBuffer::Create(device, vertexBufferDesc);
    if (!vertexBuffer)
    {
        LOG_FATAL("Failed to create vertex buffer!");
        return EXIT_FAILURE;
    }
    vertexBuffer->SetData(TRIANGLE_VERTICES.data(), vertexBufferDesc.Size);
    LOG_INFO("Vertex buffer created: {} bytes", vertexBufferDesc.Size);

    // =========================================================================
    // Shaders
    // =========================================================================
    auto vertexShader = RHI::RHIShader::CreateFromHLSL(
        device,
        "shaders/hlsl/vertex/triangle.hlsl",
        RHI::ShaderStage::Vertex);
    if (!vertexShader)
    {
        LOG_FATAL("Failed to create vertex shader!");
        return EXIT_FAILURE;
    }
    LOG_INFO("Vertex shader loaded");

    auto fragmentShader = RHI::RHIShader::CreateFromHLSL(
        device,
        "shaders/hlsl/pixel/triangle.hlsl",
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

    // Vertex input
    auto bindingDescription = Vertex::GetBindingDescription();
    pipelineDesc.VertexBindings.push_back(bindingDescription);

    auto attributeDescriptions = Vertex::GetAttributeDescriptions();
    for (const auto& attr : attributeDescriptions)
    {
        pipelineDesc.VertexAttributes.push_back(attr);
    }

    // Rasterization - no culling for simple triangle
    pipelineDesc.CullMode = VK_CULL_MODE_NONE;
    pipelineDesc.FrontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    // Disable depth testing for 2D triangle
    pipelineDesc.DepthTestEnable = false;
    pipelineDesc.DepthWriteEnable = false;

    // Color blending (no blending for opaque triangle)
    RHI::ColorBlendAttachment colorBlendAttachment;
    colorBlendAttachment.BlendEnable = false;
    pipelineDesc.ColorBlendAttachments.push_back(colorBlendAttachment);

    // Dynamic rendering format
    pipelineDesc.ColorAttachmentFormats.push_back(swapchain->GetImageFormat());

    auto pipeline = RHI::RHIPipeline::CreateGraphics(device, pipelineDesc);
    if (!pipeline)
    {
        LOG_FATAL("Failed to create graphics pipeline!");
        return EXIT_FAILURE;
    }
    LOG_INFO("Graphics pipeline created");

    // =========================================================================
    // Main Loop
    // =========================================================================
    LOG_INFO("Entering main loop...");
    LOG_INFO("Press ESC to exit");

    bool framebufferResized = false;
    window.SetResizeCallback([&framebufferResized]([[maybe_unused]] uint32_t width,
                                                    [[maybe_unused]] uint32_t height) {
        LOG_DEBUG("Window resized to: {}x{}", width, height);
        framebufferResized = true;
    });

    while (!window.ShouldClose())
    {
        window.PollEvents();
        Platform::Input::Update();

        // Handle ESC key
        if (Platform::Input::IsKeyPressed(Platform::KeyCode::Escape))
        {
            break;
        }

        // Handle window minimization
        window.GetFramebufferSize(fbWidth, fbHeight);
        if (fbWidth == 0 || fbHeight == 0)
        {
            window.WaitEvents();
            continue;
        }

        // Wait for the current frame's fence
        if (!frameManager->WaitForFrame())
        {
            LOG_ERROR("Failed to wait for frame fence");
            break;
        }

        // Acquire next swapchain image
        uint32_t imageIndex = swapchain->AcquireNextImage(
            frameManager->GetImageAvailableSemaphore());

        // Check if swapchain needs recreation
        if (imageIndex == UINT32_MAX || swapchain->NeedsRecreation() || framebufferResized)
        {
            framebufferResized = false;
            device->WaitIdle();

            if (!RecreateSwapchain(swapchain, window))
            {
                LOG_ERROR("Failed to recreate swapchain");
                break;
            }
            LOG_DEBUG("Swapchain recreated: {}x{}",
                     swapchain->GetExtent().width,
                     swapchain->GetExtent().height);

            // Advance to next frame to avoid reusing the signaled semaphore.
            // When AcquireNextImage succeeds with VK_SUBOPTIMAL_KHR, the semaphore
            // is signaled but not consumed. Advancing the frame ensures the next
            // iteration uses a different semaphore.
            frameManager->NextFrame();
            continue;
        }

        // Get current frame's command buffer
        auto& cmdBuffer = frameManager->GetCommandBuffer();

        // Reset command buffer for new recording
        cmdBuffer->Reset();

        // Record rendering commands
        RecordCommands(cmdBuffer, swapchain, pipeline, vertexBuffer, imageIndex);

        // Submit command buffer
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

        VkSemaphore signalSemaphores[] = {frameManager->GetRenderFinishedSemaphore()};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        VkResult result = vkQueueSubmit(
            device->GetGraphicsQueue(),
            1,
            &submitInfo,
            frameManager->GetInFlightFence());

        if (result != VK_SUCCESS)
        {
            LOG_ERROR("Failed to submit command buffer: {}", static_cast<int>(result));
            break;
        }

        // Present the frame
        bool presentResult = swapchain->Present(
            device->GetPresentQueue(),
            imageIndex,
            frameManager->GetRenderFinishedSemaphore());

        if (!presentResult || swapchain->NeedsRecreation())
        {
            framebufferResized = true;
        }

        // Advance to next frame
        frameManager->NextFrame();
    }

    // =========================================================================
    // Cleanup
    // =========================================================================
    LOG_INFO("Shutting down...");

    // Wait for GPU to finish
    device->WaitIdle();

    // Explicit cleanup (RAII will handle most of this)
    pipeline.reset();
    fragmentShader.reset();
    vertexShader.reset();
    vertexBuffer.reset();
    frameManager.reset();
    swapchain.reset();

    // Destroy surface
    vkDestroySurfaceKHR(instance->GetHandle(), surface, nullptr);

    device.reset();
    physicalDevice.reset();
    instance.reset();

    // Shutdown input system
    Platform::Input::Shutdown();

    // Shutdown logging
    LOG_INFO("Goodbye!");
    Core::Log::Shutdown();

    return EXIT_SUCCESS;
}
