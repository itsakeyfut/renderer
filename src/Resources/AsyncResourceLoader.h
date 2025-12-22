/**
 * @file AsyncResourceLoader.h
 * @brief Asynchronous resource loading system.
 *
 * Provides background thread loading for textures and models to prevent
 * main thread blocking during resource loading. Supports callbacks,
 * progress tracking, and load prioritization.
 */

#pragma once

#include "Core/ThreadPool.h"
#include "Core/Types.h"
#include "Resources/ResourceHandle.h"
#include "Resources/ResourceManager.h"

#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <unordered_map>

namespace RHI {
    class RHIDevice;
} // namespace RHI

namespace Resources {

// Forward declarations
class Model;
struct Texture;
struct TextureData;

/**
 * @brief Load request status.
 */
enum class LoadStatus {
    Pending,    ///< Waiting in queue
    Loading,    ///< Currently being loaded
    Completed,  ///< Successfully loaded
    Failed      ///< Load failed
};

/**
 * @brief Load priority levels.
 */
enum class LoadPriority {
    Low = 0,       ///< Background loading, lowest priority
    Normal = 1,    ///< Standard priority
    High = 2,      ///< Important resources, higher priority
    Immediate = 3  ///< Critical resources, highest priority
};

/**
 * @brief Callback type for texture load completion.
 * @param handle Handle to the loaded texture (invalid if failed).
 * @param success true if load succeeded.
 */
using TextureLoadCallback = std::function<void(TextureHandle handle, bool success)>;

/**
 * @brief Callback type for model load completion.
 * @param handle Handle to the loaded model (invalid if failed).
 * @param success true if load succeeded.
 */
using ModelLoadCallback = std::function<void(ModelHandle handle, bool success)>;

/**
 * @brief Progress callback for tracking load progress.
 * @param path Resource path.
 * @param progress Progress value (0.0 to 1.0).
 */
using ProgressCallback = std::function<void(const std::string& path, float progress)>;

/**
 * @brief Information about a pending load request.
 */
struct LoadRequestInfo {
    std::string Path;
    LoadStatus Status = LoadStatus::Pending;
    LoadPriority Priority = LoadPriority::Normal;
    float Progress = 0.0f;
};

/**
 * @brief Asynchronous resource loader.
 *
 * Loads textures and models on background threads to prevent main thread
 * blocking. Resources are loaded asynchronously and then transferred to
 * the GPU on the main thread via ProcessCompletedLoads().
 *
 * Thread Safety:
 * - LoadTextureAsync/LoadModelAsync: Thread-safe
 * - ProcessCompletedLoads: Must be called from main thread
 * - Cancel/GetStatus: Thread-safe
 *
 * Usage:
 * @code
 * auto loader = AsyncResourceLoader::Create(device);
 *
 * // Async load with callback
 * loader->LoadTextureAsync("textures/diffuse.png", [](TextureHandle handle, bool success) {
 *     if (success) {
 *         // Use handle
 *     }
 * });
 *
 * // Async load with future
 * auto future = loader->LoadTextureAsync("textures/normal.png");
 *
 * // In render loop - process completed loads
 * loader->ProcessCompletedLoads();
 *
 * // Later, get the handle
 * TextureHandle handle = future.get();
 * @endcode
 */
class AsyncResourceLoader {
public:
    /**
     * @brief Factory method to create an async loader.
     * @param device RHI device for GPU resource creation.
     * @param numWorkerThreads Number of loader threads (0 = auto).
     * @return Shared pointer to the created loader.
     */
    static Core::Ref<AsyncResourceLoader> Create(
        const Core::Ref<RHI::RHIDevice>& device,
        size_t numWorkerThreads = 0);

    /**
     * @brief Destructor - cancels pending loads and waits for completion.
     */
    ~AsyncResourceLoader();

    // Non-copyable, non-movable
    AsyncResourceLoader(const AsyncResourceLoader&) = delete;
    AsyncResourceLoader& operator=(const AsyncResourceLoader&) = delete;
    AsyncResourceLoader(AsyncResourceLoader&&) = delete;
    AsyncResourceLoader& operator=(AsyncResourceLoader&&) = delete;

    // =========================================================================
    // Texture Loading
    // =========================================================================

    /**
     * @brief Load a texture asynchronously with callback.
     *
     * @param path Path to the texture file.
     * @param callback Callback invoked when loading completes.
     * @param priority Load priority.
     * @param desc Texture loading parameters.
     */
    void LoadTextureAsync(
        const std::string& path,
        TextureLoadCallback callback,
        LoadPriority priority = LoadPriority::Normal,
        const TextureDesc& desc = {});

    /**
     * @brief Load a texture asynchronously with future.
     *
     * @param path Path to the texture file.
     * @param priority Load priority.
     * @param desc Texture loading parameters.
     * @return Future that will contain the texture handle.
     */
    std::future<TextureHandle> LoadTextureAsync(
        const std::string& path,
        LoadPriority priority = LoadPriority::Normal,
        const TextureDesc& desc = {});

    // =========================================================================
    // Model Loading
    // =========================================================================

    /**
     * @brief Load a model asynchronously with callback.
     *
     * @param path Path to the model file.
     * @param callback Callback invoked when loading completes.
     * @param priority Load priority.
     * @param desc Model loading parameters.
     */
    void LoadModelAsync(
        const std::string& path,
        ModelLoadCallback callback,
        LoadPriority priority = LoadPriority::Normal,
        const ModelDesc& desc = {});

    /**
     * @brief Load a model asynchronously with future.
     *
     * @param path Path to the model file.
     * @param priority Load priority.
     * @param desc Model loading parameters.
     * @return Future that will contain the model handle.
     */
    std::future<ModelHandle> LoadModelAsync(
        const std::string& path,
        LoadPriority priority = LoadPriority::Normal,
        const ModelDesc& desc = {});

    // =========================================================================
    // Main Thread Processing
    // =========================================================================

    /**
     * @brief Process completed loads on the main thread.
     *
     * Must be called from the main/render thread. Invokes callbacks for
     * completed loads and performs GPU uploads if needed.
     *
     * @param maxProcessCount Maximum number of loads to process per call.
     *                        0 = process all pending.
     * @return Number of loads processed.
     */
    size_t ProcessCompletedLoads(size_t maxProcessCount = 0);

    // =========================================================================
    // Status and Control
    // =========================================================================

    /**
     * @brief Check if a resource is currently being loaded.
     * @param path Resource path.
     * @return true if the resource is in the load queue or being loaded.
     */
    bool IsLoading(const std::string& path) const;

    /**
     * @brief Get the load status of a resource.
     * @param path Resource path.
     * @return Load status, or nullopt if not found.
     */
    std::optional<LoadRequestInfo> GetLoadStatus(const std::string& path) const;

    /**
     * @brief Get the load progress of a resource.
     * @param path Resource path.
     * @return Progress (0.0 to 1.0), or -1.0 if not found.
     */
    float GetProgress(const std::string& path) const;

    /**
     * @brief Cancel a pending load request.
     *
     * Only cancels loads that are still in the Pending status. Loads that
     * have already been submitted to the thread pool and are in Loading status
     * cannot be cancelled - the worker thread will still execute the load,
     * but callbacks will not be invoked for cancelled requests.
     *
     * @note This method removes the request from tracking but does not stop
     *       already-submitted tasks from executing. The loaded resource will
     *       simply be discarded when it completes.
     *
     * @param path Resource path to cancel.
     * @return true if the request was cancelled (was in Pending status).
     */
    bool CancelLoad(const std::string& path);

    /**
     * @brief Cancel all pending load requests.
     *
     * Does not cancel loads already in progress.
     */
    void CancelAllPending();

    /**
     * @brief Wait for all pending loads to complete.
     *
     * Blocks until all loads finish. Does not process callbacks.
     */
    void WaitForAll();

    /**
     * @brief Get the number of pending loads.
     * @return Number of loads waiting or in progress.
     */
    size_t GetPendingCount() const;

    /**
     * @brief Get the number of completed loads waiting for processing.
     * @return Number of completed loads.
     */
    size_t GetCompletedCount() const;

    /**
     * @brief Set a global progress callback.
     * @param callback Callback invoked when any load progresses.
     */
    void SetProgressCallback(ProgressCallback callback);

    /**
     * @brief Shut down the loader and cancel all pending requests.
     */
    void Shutdown();

private:
    /**
     * @brief Private constructor - use Create() factory method.
     */
    AsyncResourceLoader(
        const Core::Ref<RHI::RHIDevice>& device,
        size_t numWorkerThreads);

    /**
     * @brief Internal texture load task.
     */
    struct TextureLoadTask {
        std::string Path;
        TextureDesc Desc;
        TextureLoadCallback Callback;
        Core::Ref<std::promise<TextureHandle>> Promise;
        LoadPriority Priority;
    };

    /**
     * @brief Internal model load task.
     */
    struct ModelLoadTask {
        std::string Path;
        ModelDesc Desc;
        ModelLoadCallback Callback;
        Core::Ref<std::promise<ModelHandle>> Promise;
        LoadPriority Priority;
    };

    /**
     * @brief Completed load result for texture.
     *
     * Contains CPU-loaded texture data that will be uploaded to GPU
     * on the main thread during ProcessCompletedLoads().
     */
    struct TextureLoadResult {
        std::string Path;
        Core::Ref<TextureData> Data;  ///< CPU-loaded pixel data (GPU upload happens on main thread)
        TextureLoadCallback Callback;
        Core::Ref<std::promise<TextureHandle>> Promise;
        bool Success;
    };

    /**
     * @brief Completed load result for model.
     */
    struct ModelLoadResult {
        std::string Path;
        Core::Ref<Model> Model;
        ModelLoadCallback Callback;
        Core::Ref<std::promise<ModelHandle>> Promise;
        bool Success;
    };

    /**
     * @brief Execute texture loading on worker thread.
     */
    void ExecuteTextureLoad(TextureLoadTask task);

    /**
     * @brief Execute model loading on worker thread.
     */
    void ExecuteModelLoad(ModelLoadTask task);

    /**
     * @brief Update progress for a load request.
     */
    void UpdateProgress(const std::string& path, float progress);

    /**
     * @brief Mark a request as started.
     */
    void MarkAsLoading(const std::string& path);

    /**
     * @brief Remove a request from tracking.
     */
    void RemoveRequest(const std::string& path);

    // Device reference for GPU operations
    Core::Ref<RHI::RHIDevice> m_Device;

    // Thread pool for async loading
    Core::Ref<Core::ThreadPool> m_ThreadPool;

    // Completed texture loads waiting for main thread processing
    std::queue<TextureLoadResult> m_CompletedTextures;
    mutable std::mutex m_CompletedTexturesMutex;

    // Completed model loads waiting for main thread processing
    std::queue<ModelLoadResult> m_CompletedModels;
    mutable std::mutex m_CompletedModelsMutex;

    // Active load requests tracking
    std::unordered_map<std::string, LoadRequestInfo> m_ActiveRequests;
    mutable std::mutex m_RequestsMutex;

    // Pending callbacks for duplicate load requests (texture)
    // When multiple callers request the same texture, all callbacks are stored here
    std::unordered_map<std::string, std::vector<TextureLoadCallback>> m_PendingTextureCallbacks;
    std::unordered_map<std::string, std::vector<Core::Ref<std::promise<TextureHandle>>>> m_PendingTexturePromises;

    // Pending callbacks for duplicate load requests (model)
    std::unordered_map<std::string, std::vector<ModelLoadCallback>> m_PendingModelCallbacks;
    std::unordered_map<std::string, std::vector<Core::Ref<std::promise<ModelHandle>>>> m_PendingModelPromises;

    // Progress callback
    ProgressCallback m_ProgressCallback;
    mutable std::mutex m_CallbackMutex;

    // Shutdown flag
    std::atomic<bool> m_ShuttingDown{false};
};

} // namespace Resources
