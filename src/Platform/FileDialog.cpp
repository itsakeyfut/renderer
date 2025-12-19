/**
 * @file FileDialog.cpp
 * @brief Native file dialog implementation.
 *
 * Windows implementation uses IFileOpenDialog (COM) for modern dialog.
 */

#include "Platform/FileDialog.h"
#include "Core/Log.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <shobjidl.h>
#include <combaseapi.h>
#endif

namespace Platform
{

#ifdef _WIN32

    /**
     * @brief Convert UTF-8 string to wide string (UTF-16)
     */
    static std::wstring Utf8ToWide(const std::string& utf8)
    {
        if (utf8.empty())
        {
            return std::wstring();
        }

        int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(),
                                             static_cast<int>(utf8.size()), nullptr, 0);
        if (sizeNeeded <= 0)
        {
            return std::wstring();
        }

        std::wstring result(sizeNeeded, 0);
        MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(),
                            static_cast<int>(utf8.size()), &result[0], sizeNeeded);
        return result;
    }

    /**
     * @brief Convert wide string (UTF-16) to UTF-8 string
     */
    static std::string WideToUtf8(const std::wstring& wide)
    {
        if (wide.empty())
        {
            return std::string();
        }

        int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(),
                                              static_cast<int>(wide.size()),
                                              nullptr, 0, nullptr, nullptr);
        if (sizeNeeded <= 0)
        {
            return std::string();
        }

        std::string result(sizeNeeded, 0);
        WideCharToMultiByte(CP_UTF8, 0, wide.c_str(),
                            static_cast<int>(wide.size()),
                            &result[0], sizeNeeded, nullptr, nullptr);
        return result;
    }

    std::optional<std::string> FileDialog::OpenFile(const FileDialogConfig& config)
    {
        // Initialize COM
        HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        bool comInitialized = SUCCEEDED(hr);

        // If COM was already initialized, that's okay
        if (hr == RPC_E_CHANGED_MODE)
        {
            comInitialized = false; // Don't uninitialize if we didn't initialize
        }
        else if (FAILED(hr))
        {
            LOG_ERROR("Failed to initialize COM for file dialog: 0x{:08X}", static_cast<unsigned int>(hr));
            return std::nullopt;
        }

        std::optional<std::string> result;

        // Create the File Open Dialog object
        IFileOpenDialog* pFileOpen = nullptr;
        hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL,
                              IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));

        if (SUCCEEDED(hr))
        {
            // Set dialog title
            if (!config.Title.empty())
            {
                std::wstring wideTitle = Utf8ToWide(config.Title);
                pFileOpen->SetTitle(wideTitle.c_str());
            }

            // Set file type filters
            if (!config.Filters.empty())
            {
                std::vector<COMDLG_FILTERSPEC> filterSpecs;
                std::vector<std::wstring> wideNames;
                std::vector<std::wstring> widePatterns;

                // Keep wide strings alive until dialog is shown
                wideNames.reserve(config.Filters.size());
                widePatterns.reserve(config.Filters.size());

                for (const auto& filter : config.Filters)
                {
                    wideNames.push_back(Utf8ToWide(filter.Name));
                    widePatterns.push_back(Utf8ToWide(filter.Extensions));

                    COMDLG_FILTERSPEC spec;
                    spec.pszName = wideNames.back().c_str();
                    spec.pszSpec = widePatterns.back().c_str();
                    filterSpecs.push_back(spec);
                }

                pFileOpen->SetFileTypes(static_cast<UINT>(filterSpecs.size()), filterSpecs.data());
                pFileOpen->SetFileTypeIndex(1); // Select first filter by default
            }

            // Set default folder if specified
            if (!config.DefaultPath.empty())
            {
                std::wstring widePath = Utf8ToWide(config.DefaultPath);
                IShellItem* pFolder = nullptr;
                hr = SHCreateItemFromParsingName(widePath.c_str(), nullptr,
                                                  IID_IShellItem,
                                                  reinterpret_cast<void**>(&pFolder));
                if (SUCCEEDED(hr))
                {
                    pFileOpen->SetFolder(pFolder);
                    pFolder->Release();
                }
            }

            // Show the dialog
            hr = pFileOpen->Show(nullptr);

            if (SUCCEEDED(hr))
            {
                // Get the result
                IShellItem* pItem = nullptr;
                hr = pFileOpen->GetResult(&pItem);

                if (SUCCEEDED(hr))
                {
                    PWSTR pszFilePath = nullptr;
                    hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);

                    if (SUCCEEDED(hr))
                    {
                        result = WideToUtf8(pszFilePath);
                        CoTaskMemFree(pszFilePath);
                        LOG_DEBUG("File selected: {}", result.value());
                    }

                    pItem->Release();
                }
            }
            else if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED))
            {
                // User cancelled - not an error
                LOG_DEBUG("File dialog cancelled by user");
            }
            else
            {
                LOG_ERROR("File dialog failed: 0x{:08X}", static_cast<unsigned int>(hr));
            }

            pFileOpen->Release();
        }
        else
        {
            LOG_ERROR("Failed to create file dialog: 0x{:08X}", static_cast<unsigned int>(hr));
        }

        // Uninitialize COM if we initialized it
        if (comInitialized)
        {
            CoUninitialize();
        }

        return result;
    }

#else

    std::optional<std::string> FileDialog::OpenFile(const FileDialogConfig& config)
    {
        (void)config;
        LOG_WARN("File dialog not implemented for this platform");
        return std::nullopt;
    }

#endif

    std::optional<std::string> FileDialog::OpenGLTFFile()
    {
        FileDialogConfig config;
        config.Title = "Load 3D Model";
        config.Filters = {
            {"glTF Files", "*.gltf;*.glb"},
            {"All Files", "*.*"}
        };
        return OpenFile(config);
    }

} // namespace Platform
