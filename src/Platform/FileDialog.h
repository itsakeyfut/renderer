/**
 * @file FileDialog.h
 * @brief Native file dialog interface for file selection.
 *
 * Provides file dialog functionality using native OS dialogs.
 * Currently supports Windows (IFileOpenDialog).
 */

#pragma once

#include <optional>
#include <string>
#include <vector>

namespace Platform
{
    /**
     * @brief File filter specification for dialog
     */
    struct FileFilter
    {
        /**
         * @brief Display name for the filter (e.g., "glTF Files")
         */
        std::string Name;

        /**
         * @brief File extension pattern (e.g., "*.gltf;*.glb")
         */
        std::string Extensions;
    };

    /**
     * @brief Configuration for file open dialog
     */
    struct FileDialogConfig
    {
        /**
         * @brief Dialog window title
         */
        std::string Title = "Open File";

        /**
         * @brief Initial directory path (empty = current directory)
         */
        std::string DefaultPath;

        /**
         * @brief File type filters
         */
        std::vector<FileFilter> Filters;
    };

    /**
     * @brief Native file dialog wrapper
     *
     * Provides static methods to show native OS file dialogs.
     * On Windows, uses IFileOpenDialog (COM) for modern dialog appearance.
     *
     * Usage:
     * @code
     * auto filePath = Platform::FileDialog::OpenGLTFFile();
     * if (filePath.has_value()) {
     *     // Load the file at filePath.value()
     * }
     * @endcode
     */
    class FileDialog
    {
    public:
        FileDialog() = delete;
        ~FileDialog() = delete;

        /**
         * @brief Show a file open dialog
         * @param config Dialog configuration
         * @return Selected file path, or std::nullopt if cancelled
         */
        static std::optional<std::string> OpenFile(const FileDialogConfig& config);

        /**
         * @brief Show a file open dialog for glTF/glb files
         * @return Selected file path, or std::nullopt if cancelled
         *
         * Convenience method with pre-configured glTF filter.
         */
        static std::optional<std::string> OpenGLTFFile();

        /**
         * @brief Show a file open dialog for HDR environment maps
         * @return Selected file path, or std::nullopt if cancelled
         *
         * Convenience method with pre-configured HDR filter.
         */
        static std::optional<std::string> OpenHDRFile();
    };

} // namespace Platform
