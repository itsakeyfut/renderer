/**
 * @file AsyncResourceLoader.cpp
 * @brief Implementation of the asynchronous resource loading system.
 */

#include "Resources/AsyncResourceLoader.h"
#include "Resources/ModelLoader.h"
#include "Resources/TextureLoader.h"
#include "Resources/Model.h"
#include "Core/Log.h"

// Note: TextureLoader.h defines struct Texture, not Texture.h
// The two are incompatible - TextureLoader's version is used here

namespace Resources {

Core::Ref<AsyncResourceLoader> AsyncResourceLoader::Create(
    const Core::Ref<RHI::RHIDevice>& device,
    size_t numWorkerThreads)
{
    return Core::Ref<AsyncResourceLoader>(new AsyncResourceLoader(device, numWorkerThreads));
}

AsyncResourceLoader::AsyncResourceLoader(
    const Core::Ref<RHI::RHIDevice>& device,
    size_t numWorkerThreads)
    : m_Device(device)
{
    // Use 2 threads by default for resource loading
    // More threads don't help much since loading is usually I/O bound
    if (numWorkerThreads == 0) {
        numWorkerThreads = 2;
    }

    m_ThreadPool = Core::ThreadPool::Create(numWorkerThreads);
    LOG_INFO("AsyncResourceLoader created with {} worker threads", numWorkerThreads);
}

AsyncResourceLoader::~AsyncResourceLoader()
{
    Shutdown();
}

void AsyncResourceLoader::Shutdown()
{
    if (m_ShuttingDown.exchange(true)) {
        return;  // Already shutting down
    }

    LOG_DEBUG("AsyncResourceLoader shutting down");

    // Cancel all pending requests
    CancelAllPending();

    // Shutdown the thread pool
    if (m_ThreadPool) {
        m_ThreadPool->Shutdown();
    }

    // Process any remaining completed loads
    ProcessCompletedLoads(0);

    LOG_DEBUG("AsyncResourceLoader shutdown complete");
}

// =============================================================================
// Texture Loading
// =============================================================================

void AsyncResourceLoader::LoadTextureAsync(
    const std::string& path,
    TextureLoadCallback callback,
    LoadPriority priority,
    const TextureDesc& desc)
{
    if (m_ShuttingDown) {
        LOG_WARN("AsyncResourceLoader: Cannot load texture - shutting down");
        if (callback) {
            callback(TextureHandle(), false);
        }
        return;
    }

    // Check if already loading
    {
        std::lock_guard<std::mutex> lock(m_RequestsMutex);
        if (m_ActiveRequests.find(path) != m_ActiveRequests.end()) {
            LOG_DEBUG("Texture already loading: {}", path);
            // Don't start a new load, but don't call the callback either
            // The existing load will complete and the callback won't be called
            // This is a simple deduplication strategy
            return;
        }

        // Register the request
        m_ActiveRequests[path] = LoadRequestInfo{
            path,
            LoadStatus::Pending,
            priority,
            0.0f
        };
    }

    LOG_DEBUG("Queuing async texture load: {} (priority={})",
              path, static_cast<int>(priority));

    TextureLoadTask task{
        path,
        desc,
        std::move(callback),
        nullptr,
        priority
    };

    // Submit to thread pool
    m_ThreadPool->SubmitWithPriority(
        static_cast<Core::ThreadPool::Priority>(priority),
        [this, task = std::move(task)]() mutable {
            ExecuteTextureLoad(std::move(task));
        }
    );
}

std::future<TextureHandle> AsyncResourceLoader::LoadTextureAsync(
    const std::string& path,
    LoadPriority priority,
    const TextureDesc& desc)
{
    auto promise = std::make_shared<std::promise<TextureHandle>>();
    auto future = promise->get_future();

    if (m_ShuttingDown) {
        LOG_WARN("AsyncResourceLoader: Cannot load texture - shutting down");
        promise->set_value(TextureHandle());
        return future;
    }

    // Check if already loading
    {
        std::lock_guard<std::mutex> lock(m_RequestsMutex);
        if (m_ActiveRequests.find(path) != m_ActiveRequests.end()) {
            LOG_DEBUG("Texture already loading: {}", path);
            // Return a future that will resolve to an invalid handle
            // A more sophisticated implementation might share the future
            promise->set_value(TextureHandle());
            return future;
        }

        // Register the request
        m_ActiveRequests[path] = LoadRequestInfo{
            path,
            LoadStatus::Pending,
            priority,
            0.0f
        };
    }

    LOG_DEBUG("Queuing async texture load (future): {} (priority={})",
              path, static_cast<int>(priority));

    TextureLoadTask task{
        path,
        desc,
        nullptr,
        promise,
        priority
    };

    // Submit to thread pool
    m_ThreadPool->SubmitWithPriority(
        static_cast<Core::ThreadPool::Priority>(priority),
        [this, task = std::move(task)]() mutable {
            ExecuteTextureLoad(std::move(task));
        }
    );

    return future;
}

void AsyncResourceLoader::ExecuteTextureLoad(TextureLoadTask task)
{
    if (m_ShuttingDown) {
        // Notify failure
        TextureLoadResult result{
            task.Path,
            nullptr,
            std::move(task.Callback),
            task.Promise,
            false
        };

        std::lock_guard<std::mutex> lock(m_CompletedTexturesMutex);
        m_CompletedTextures.push(std::move(result));
        return;
    }

    MarkAsLoading(task.Path);
    UpdateProgress(task.Path, 0.1f);

    LOG_DEBUG("Loading texture: {}", task.Path);

    // Load the texture on this worker thread
    TextureLoadOptions options;
    options.GenerateMipmaps = task.Desc.GenerateMipmaps;
    options.SRGB = task.Desc.sRGB;

    UpdateProgress(task.Path, 0.3f);

    Core::Ref<Texture> texture = TextureLoader::Load(m_Device, task.Path, options);

    UpdateProgress(task.Path, 0.9f);

    bool success = (texture != nullptr);

    if (success) {
        LOG_DEBUG("Texture loaded successfully: {}", task.Path);
    }
    else {
        LOG_WARN("Failed to load texture: {}", task.Path);
    }

    // Queue the result for main thread processing
    TextureLoadResult result{
        task.Path,
        texture,
        std::move(task.Callback),
        task.Promise,
        success
    };

    {
        std::lock_guard<std::mutex> lock(m_CompletedTexturesMutex);
        m_CompletedTextures.push(std::move(result));
    }

    UpdateProgress(task.Path, 1.0f);
}

// =============================================================================
// Model Loading
// =============================================================================

void AsyncResourceLoader::LoadModelAsync(
    const std::string& path,
    ModelLoadCallback callback,
    LoadPriority priority,
    const ModelDesc& desc)
{
    if (m_ShuttingDown) {
        LOG_WARN("AsyncResourceLoader: Cannot load model - shutting down");
        if (callback) {
            callback(ModelHandle(), false);
        }
        return;
    }

    // Check if already loading
    {
        std::lock_guard<std::mutex> lock(m_RequestsMutex);
        if (m_ActiveRequests.find(path) != m_ActiveRequests.end()) {
            LOG_DEBUG("Model already loading: {}", path);
            return;
        }

        // Register the request
        m_ActiveRequests[path] = LoadRequestInfo{
            path,
            LoadStatus::Pending,
            priority,
            0.0f
        };
    }

    LOG_DEBUG("Queuing async model load: {} (priority={})",
              path, static_cast<int>(priority));

    ModelLoadTask task{
        path,
        desc,
        std::move(callback),
        nullptr,
        priority
    };

    // Submit to thread pool
    m_ThreadPool->SubmitWithPriority(
        static_cast<Core::ThreadPool::Priority>(priority),
        [this, task = std::move(task)]() mutable {
            ExecuteModelLoad(std::move(task));
        }
    );
}

std::future<ModelHandle> AsyncResourceLoader::LoadModelAsync(
    const std::string& path,
    LoadPriority priority,
    const ModelDesc& desc)
{
    auto promise = std::make_shared<std::promise<ModelHandle>>();
    auto future = promise->get_future();

    if (m_ShuttingDown) {
        LOG_WARN("AsyncResourceLoader: Cannot load model - shutting down");
        promise->set_value(ModelHandle());
        return future;
    }

    // Check if already loading
    {
        std::lock_guard<std::mutex> lock(m_RequestsMutex);
        if (m_ActiveRequests.find(path) != m_ActiveRequests.end()) {
            LOG_DEBUG("Model already loading: {}", path);
            promise->set_value(ModelHandle());
            return future;
        }

        // Register the request
        m_ActiveRequests[path] = LoadRequestInfo{
            path,
            LoadStatus::Pending,
            priority,
            0.0f
        };
    }

    LOG_DEBUG("Queuing async model load (future): {} (priority={})",
              path, static_cast<int>(priority));

    ModelLoadTask task{
        path,
        desc,
        nullptr,
        promise,
        priority
    };

    // Submit to thread pool
    m_ThreadPool->SubmitWithPriority(
        static_cast<Core::ThreadPool::Priority>(priority),
        [this, task = std::move(task)]() mutable {
            ExecuteModelLoad(std::move(task));
        }
    );

    return future;
}

void AsyncResourceLoader::ExecuteModelLoad(ModelLoadTask task)
{
    if (m_ShuttingDown) {
        // Notify failure
        ModelLoadResult result{
            task.Path,
            nullptr,
            std::move(task.Callback),
            task.Promise,
            false
        };

        std::lock_guard<std::mutex> lock(m_CompletedModelsMutex);
        m_CompletedModels.push(std::move(result));
        return;
    }

    MarkAsLoading(task.Path);
    UpdateProgress(task.Path, 0.1f);

    LOG_DEBUG("Loading model: {}", task.Path);

    // Load the model on this worker thread
    ModelLoadOptions options;
    options.CalculateNormals = task.Desc.CalculateNormals;
    options.CalculateTangents = task.Desc.CalculateTangents;

    UpdateProgress(task.Path, 0.3f);

    Core::Ref<Model> model = ModelLoader::LoadGLTF(task.Path, options);

    UpdateProgress(task.Path, 0.9f);

    bool success = (model != nullptr);

    if (success) {
        LOG_DEBUG("Model loaded successfully: {}", task.Path);
    }
    else {
        LOG_WARN("Failed to load model: {}", task.Path);
    }

    // Queue the result for main thread processing
    ModelLoadResult result{
        task.Path,
        model,
        std::move(task.Callback),
        task.Promise,
        success
    };

    {
        std::lock_guard<std::mutex> lock(m_CompletedModelsMutex);
        m_CompletedModels.push(std::move(result));
    }

    UpdateProgress(task.Path, 1.0f);
}

// =============================================================================
// Main Thread Processing
// =============================================================================

size_t AsyncResourceLoader::ProcessCompletedLoads(size_t maxProcessCount)
{
    size_t processed = 0;

    // Process completed texture loads
    while (true) {
        if (maxProcessCount > 0 && processed >= maxProcessCount) {
            break;
        }

        TextureLoadResult textureResult;
        {
            std::lock_guard<std::mutex> lock(m_CompletedTexturesMutex);
            if (m_CompletedTextures.empty()) {
                break;
            }
            textureResult = std::move(m_CompletedTextures.front());
            m_CompletedTextures.pop();
        }

        // Remove from active requests
        RemoveRequest(textureResult.Path);

        TextureHandle handle;
        if (textureResult.Success && textureResult.Texture) {
            // The texture is already loaded. Use ResourceManager to cache it.
            // Note: In a full implementation, ResourceManager would have an AddTexture()
            // method to add pre-loaded resources directly. For now, we use LoadTexture
            // which will load it again (but quickly from disk cache).
            auto& rm = ResourceManager::Instance();
            handle = rm.LoadTexture(textureResult.Path);
        }

        // Invoke callback
        if (textureResult.Callback) {
            textureResult.Callback(handle, textureResult.Success);
        }

        // Fulfill promise
        if (textureResult.Promise) {
            textureResult.Promise->set_value(handle);
        }

        processed++;
    }

    // Process completed model loads
    while (true) {
        if (maxProcessCount > 0 && processed >= maxProcessCount) {
            break;
        }

        ModelLoadResult modelResult;
        {
            std::lock_guard<std::mutex> lock(m_CompletedModelsMutex);
            if (m_CompletedModels.empty()) {
                break;
            }
            modelResult = std::move(m_CompletedModels.front());
            m_CompletedModels.pop();
        }

        // Remove from active requests
        RemoveRequest(modelResult.Path);

        ModelHandle handle;
        if (modelResult.Success && modelResult.Model) {
            // Add to ResourceManager
            auto& rm = ResourceManager::Instance();

            // For now, use synchronous loading to add to cache
            handle = rm.LoadModel(modelResult.Path);
        }

        // Invoke callback
        if (modelResult.Callback) {
            modelResult.Callback(handle, modelResult.Success);
        }

        // Fulfill promise
        if (modelResult.Promise) {
            modelResult.Promise->set_value(handle);
        }

        processed++;
    }

    return processed;
}

// =============================================================================
// Status and Control
// =============================================================================

bool AsyncResourceLoader::IsLoading(const std::string& path) const
{
    std::lock_guard<std::mutex> lock(m_RequestsMutex);
    auto it = m_ActiveRequests.find(path);
    if (it == m_ActiveRequests.end()) {
        return false;
    }
    return it->second.Status == LoadStatus::Pending ||
           it->second.Status == LoadStatus::Loading;
}

std::optional<LoadRequestInfo> AsyncResourceLoader::GetLoadStatus(const std::string& path) const
{
    std::lock_guard<std::mutex> lock(m_RequestsMutex);
    auto it = m_ActiveRequests.find(path);
    if (it == m_ActiveRequests.end()) {
        return std::nullopt;
    }
    return it->second;
}

float AsyncResourceLoader::GetProgress(const std::string& path) const
{
    std::lock_guard<std::mutex> lock(m_RequestsMutex);
    auto it = m_ActiveRequests.find(path);
    if (it == m_ActiveRequests.end()) {
        return -1.0f;
    }
    return it->second.Progress;
}

bool AsyncResourceLoader::CancelLoad(const std::string& path)
{
    std::lock_guard<std::mutex> lock(m_RequestsMutex);
    auto it = m_ActiveRequests.find(path);
    if (it == m_ActiveRequests.end()) {
        return false;
    }

    // Can only cancel pending loads, not ones in progress
    if (it->second.Status != LoadStatus::Pending) {
        return false;
    }

    m_ActiveRequests.erase(it);
    LOG_DEBUG("Cancelled load request: {}", path);
    return true;
}

void AsyncResourceLoader::CancelAllPending()
{
    std::lock_guard<std::mutex> lock(m_RequestsMutex);

    auto it = m_ActiveRequests.begin();
    while (it != m_ActiveRequests.end()) {
        if (it->second.Status == LoadStatus::Pending) {
            it = m_ActiveRequests.erase(it);
        }
        else {
            ++it;
        }
    }

    LOG_DEBUG("Cancelled all pending load requests");
}

void AsyncResourceLoader::WaitForAll()
{
    if (m_ThreadPool) {
        m_ThreadPool->WaitForAll();
    }
}

size_t AsyncResourceLoader::GetPendingCount() const
{
    std::lock_guard<std::mutex> lock(m_RequestsMutex);

    size_t count = 0;
    for (const auto& [_, info] : m_ActiveRequests) {
        if (info.Status == LoadStatus::Pending ||
            info.Status == LoadStatus::Loading) {
            count++;
        }
    }
    return count;
}

size_t AsyncResourceLoader::GetCompletedCount() const
{
    size_t count = 0;

    {
        std::lock_guard<std::mutex> lock(m_CompletedTexturesMutex);
        count += m_CompletedTextures.size();
    }

    {
        std::lock_guard<std::mutex> lock(m_CompletedModelsMutex);
        count += m_CompletedModels.size();
    }

    return count;
}

void AsyncResourceLoader::SetProgressCallback(ProgressCallback callback)
{
    std::lock_guard<std::mutex> lock(m_CallbackMutex);
    m_ProgressCallback = std::move(callback);
}

// =============================================================================
// Internal Helpers
// =============================================================================

void AsyncResourceLoader::UpdateProgress(const std::string& path, float progress)
{
    {
        std::lock_guard<std::mutex> lock(m_RequestsMutex);
        auto it = m_ActiveRequests.find(path);
        if (it != m_ActiveRequests.end()) {
            it->second.Progress = progress;
        }
    }

    // Notify progress callback
    {
        std::lock_guard<std::mutex> lock(m_CallbackMutex);
        if (m_ProgressCallback) {
            m_ProgressCallback(path, progress);
        }
    }
}

void AsyncResourceLoader::MarkAsLoading(const std::string& path)
{
    std::lock_guard<std::mutex> lock(m_RequestsMutex);
    auto it = m_ActiveRequests.find(path);
    if (it != m_ActiveRequests.end()) {
        it->second.Status = LoadStatus::Loading;
    }
}

void AsyncResourceLoader::RemoveRequest(const std::string& path)
{
    std::lock_guard<std::mutex> lock(m_RequestsMutex);
    m_ActiveRequests.erase(path);
}

} // namespace Resources
