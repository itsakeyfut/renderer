/**
 * @file ImGuiRenderer.cpp
 * @brief ImGui renderer implementation for Vulkan.
 */

#include "Renderer/Debug/ImGuiRenderer.h"
#include "Core/Assert.h"
#include "Core/Log.h"
#include "Platform/Window.h"
#include "RHI/RHIDevice.h"
#include "RHI/RHICommandBuffer.h"
#include "RHI/RHICommandPool.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <GLFW/glfw3.h>

namespace Renderer
{
    Core::Ref<ImGuiRenderer> ImGuiRenderer::Create(
        const Core::Ref<RHI::RHIDevice>& device,
        Platform::Window& window,
        const ImGuiRendererConfig& config)
    {
        auto renderer = Core::Ref<ImGuiRenderer>(new ImGuiRenderer());
        if (!renderer->Initialize(device, window, config))
        {
            return nullptr;
        }
        return renderer;
    }

    ImGuiRenderer::~ImGuiRenderer()
    {
        if (m_Initialized)
        {
            // Wait for GPU to finish before destroying resources
            if (m_Device != VK_NULL_HANDLE)
            {
                vkDeviceWaitIdle(m_Device);
            }

            ImGui_ImplVulkan_Shutdown();
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext();

            if (m_DescriptorPool != VK_NULL_HANDLE && m_Device != VK_NULL_HANDLE)
            {
                vkDestroyDescriptorPool(m_Device, m_DescriptorPool, nullptr);
            }

            LOG_DEBUG("ImGui renderer destroyed");
        }
    }

    bool ImGuiRenderer::Initialize(
        const Core::Ref<RHI::RHIDevice>& device,
        Platform::Window& window,
        const ImGuiRendererConfig& config)
    {
        ASSERT(device != nullptr);
        ASSERT(config.Instance != VK_NULL_HANDLE);
        ASSERT(config.GraphicsQueue != VK_NULL_HANDLE);

        m_Device = device->GetHandle();

        // Create ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable keyboard navigation
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable gamepad navigation

        // Setup Dear ImGui style
        ImGui::StyleColorsDark();

        // Customize style for better visibility
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 4.0f;
        style.FrameRounding = 2.0f;
        style.ScrollbarRounding = 2.0f;
        style.GrabRounding = 2.0f;
        style.Alpha = 0.95f;

        // Create descriptor pool for ImGui
        if (!CreateDescriptorPool(m_Device))
        {
            LOG_ERROR("Failed to create ImGui descriptor pool");
            return false;
        }

        // Initialize Platform/Renderer backends
        GLFWwindow* glfwWindow = window.GetHandle();
        if (!ImGui_ImplGlfw_InitForVulkan(glfwWindow, true))
        {
            LOG_ERROR("Failed to initialize ImGui GLFW backend");
            return false;
        }

        // Setup Vulkan init info for dynamic rendering (Vulkan 1.3)
        ImGui_ImplVulkan_InitInfo initInfo{};
        initInfo.Instance = config.Instance;
        initInfo.PhysicalDevice = device->GetPhysicalDevice();
        initInfo.Device = m_Device;
        initInfo.QueueFamily = config.QueueFamily;
        initInfo.Queue = config.GraphicsQueue;
        initInfo.PipelineCache = VK_NULL_HANDLE;
        initInfo.DescriptorPool = m_DescriptorPool;
        initInfo.RenderPass = VK_NULL_HANDLE;  // Not using render pass, using dynamic rendering
        initInfo.Subpass = 0;
        initInfo.MinImageCount = config.ImageCount;
        initInfo.ImageCount = config.ImageCount;
        initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        initInfo.Allocator = nullptr;
        initInfo.CheckVkResultFn = [](VkResult result) {
            if (result != VK_SUCCESS)
            {
                LOG_ERROR("ImGui Vulkan error: {}", static_cast<int>(result));
            }
        };

        // Use dynamic rendering (no render pass) - requires Vulkan 1.3
        initInfo.UseDynamicRendering = true;
        initInfo.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
        initInfo.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
        initInfo.PipelineRenderingCreateInfo.pColorAttachmentFormats = &config.ColorFormat;

        if (!ImGui_ImplVulkan_Init(&initInfo))
        {
            LOG_ERROR("Failed to initialize ImGui Vulkan backend");
            ImGui_ImplGlfw_Shutdown();
            return false;
        }

        // Upload fonts
        if (!UploadFonts(device))
        {
            LOG_ERROR("Failed to upload ImGui fonts");
            ImGui_ImplVulkan_Shutdown();
            ImGui_ImplGlfw_Shutdown();
            return false;
        }

        m_Initialized = true;
        LOG_INFO("ImGui renderer initialized");

        return true;
    }

    bool ImGuiRenderer::CreateDescriptorPool(VkDevice device)
    {
        // Create a descriptor pool with enough resources for ImGui
        VkDescriptorPoolSize poolSizes[] = {
            {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
            {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
            {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
            {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
            {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}
        };

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolInfo.maxSets = 1000;
        poolInfo.poolSizeCount = static_cast<uint32_t>(std::size(poolSizes));
        poolInfo.pPoolSizes = poolSizes;

        VkResult result = vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_DescriptorPool);
        if (result != VK_SUCCESS)
        {
            LOG_ERROR("Failed to create ImGui descriptor pool: {}", static_cast<int>(result));
            return false;
        }

        return true;
    }

    bool ImGuiRenderer::UploadFonts([[maybe_unused]] const Core::Ref<RHI::RHIDevice>& device)
    {
        // Create fonts texture (ImGui handles command buffer internally in v1.91+)
        if (!ImGui_ImplVulkan_CreateFontsTexture())
        {
            LOG_ERROR("Failed to create ImGui fonts texture");
            return false;
        }

        return true;
    }

    void ImGuiRenderer::BeginFrame()
    {
        ASSERT(m_Initialized);

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void ImGuiRenderer::EndFrame()
    {
        ASSERT(m_Initialized);

        ImGui::Render();
    }

    void ImGuiRenderer::Render(
        const Core::Ref<RHI::RHICommandBuffer>& cmdBuffer,
        VkImageView imageView,
        VkExtent2D extent)
    {
        ASSERT(m_Initialized);
        ASSERT(cmdBuffer != nullptr);

        // Setup rendering config for ImGui
        RHI::RenderingConfig renderConfig;
        renderConfig.RenderArea = {{0, 0}, extent};

        RHI::ColorAttachment colorAttachment;
        colorAttachment.ImageView = imageView;
        colorAttachment.Layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.LoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;  // Preserve existing content
        colorAttachment.StoreOp = VK_ATTACHMENT_STORE_OP_STORE;
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

        // Record ImGui draw commands
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdBuffer->GetHandle());

        cmdBuffer->EndRendering();
    }

    void ImGuiRenderer::ShowStatsWindow(const DebugStats& stats, bool* show)
    {
        if (show && !*show)
        {
            return;
        }

        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoResize |
                                        ImGuiWindowFlags_AlwaysAutoResize |
                                        ImGuiWindowFlags_NoSavedSettings;

        if (ImGui::Begin("Performance Stats", show, windowFlags))
        {
            ImGui::Text("Frame Time: %.3f ms", stats.FrameTime);
            ImGui::Text("FPS: %.1f", stats.FPS);
            ImGui::Separator();
            ImGui::Text("Draw Calls: %u", stats.DrawCalls);
            ImGui::Text("Triangles: %u", stats.TriangleCount);
            ImGui::Separator();

            // Display memory in MB
            float usedMB = static_cast<float>(stats.GPUMemoryUsed) / (1024.0f * 1024.0f);
            float totalMB = static_cast<float>(stats.GPUMemoryTotal) / (1024.0f * 1024.0f);
            ImGui::Text("GPU Memory: %.1f / %.1f MB", usedMB, totalMB);

            if (stats.GPUMemoryTotal > 0)
            {
                float usagePercent = static_cast<float>(stats.GPUMemoryUsed) /
                                     static_cast<float>(stats.GPUMemoryTotal);
                ImGui::ProgressBar(usagePercent, ImVec2(-1, 0));
            }
        }
        ImGui::End();
    }

    void ImGuiRenderer::ShowDemoWindow(bool* show)
    {
        if (show && !*show)
        {
            return;
        }

        ImGui::ShowDemoWindow(show);
    }

    bool ImGuiRenderer::WantCaptureMouse() const
    {
        return ImGui::GetIO().WantCaptureMouse;
    }

    bool ImGuiRenderer::WantCaptureKeyboard() const
    {
        return ImGui::GetIO().WantCaptureKeyboard;
    }

} // namespace Renderer
