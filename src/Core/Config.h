/**
 * @file Config.h
 * @brief Configuration file system for the renderer.
 *
 * Provides a JSON-based configuration system that supports:
 * - Loading settings from JSON files
 * - Default value fallback for missing keys
 * - Hierarchical access using dot notation (e.g., "window.width")
 * - Optional saving of modified settings
 *
 * Usage:
 * @code
 * // Load configuration
 * Config::Load("settings.json");
 *
 * // Get values with defaults
 * uint32_t width = Config::Get<uint32_t>("window.width", 1280);
 * bool vsync = Config::Get<bool>("graphics.vsync", true);
 * float fov = Config::Get<float>("camera.fov", 45.0f);
 *
 * // Set values
 * Config::Set("window.width", 1920);
 *
 * // Save changes
 * Config::Save("settings.json");
 * @endcode
 */

#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <optional>

namespace Core {

/**
 * @class Config
 * @brief Static configuration class for loading and managing application settings.
 *
 * The configuration system supports hierarchical keys using dot notation.
 * For example, "window.width" accesses the "width" field inside the "window" object.
 */
class Config {
public:
    /**
     * @brief Loads configuration from a JSON file.
     *
     * Parses the specified JSON file and stores the configuration data.
     * If the file does not exist or contains invalid JSON, the function
     * returns false and the configuration remains empty (or unchanged).
     *
     * @param filepath Path to the JSON configuration file.
     * @return true if the file was loaded successfully, false otherwise.
     */
    static bool Load(const std::string& filepath);

    /**
     * @brief Saves the current configuration to a JSON file.
     *
     * Writes the current configuration state to the specified file.
     * Creates the file if it does not exist, overwrites if it does.
     *
     * @param filepath Path to save the JSON configuration file.
     * @return true if the file was saved successfully, false otherwise.
     */
    static bool Save(const std::string& filepath);

    /**
     * @brief Resets the configuration to an empty state.
     *
     * Clears all loaded configuration data.
     */
    static void Clear();

    /**
     * @brief Checks if configuration data has been loaded.
     * @return true if configuration is loaded, false otherwise.
     */
    [[nodiscard]] static bool IsLoaded();

    /**
     * @brief Checks if a key exists in the configuration.
     *
     * @param key The key to check, using dot notation for nested values.
     * @return true if the key exists, false otherwise.
     */
    [[nodiscard]] static bool HasKey(const std::string& key);

    /**
     * @brief Gets a configuration value with a default fallback.
     *
     * Retrieves the value at the specified key path. If the key does not exist
     * or the value cannot be converted to the requested type, returns the default.
     *
     * @tparam T The type of value to retrieve.
     * @param key The key path using dot notation (e.g., "window.width").
     * @param defaultValue The value to return if the key is not found.
     * @return The configuration value or the default value.
     */
    template<typename T>
    [[nodiscard]] static T Get(const std::string& key, const T& defaultValue);

    /**
     * @brief Gets a configuration value as an optional.
     *
     * Retrieves the value at the specified key path. If the key does not exist
     * or the value cannot be converted, returns std::nullopt.
     *
     * @tparam T The type of value to retrieve.
     * @param key The key path using dot notation.
     * @return The configuration value or std::nullopt.
     */
    template<typename T>
    [[nodiscard]] static std::optional<T> GetOptional(const std::string& key);

    /**
     * @brief Sets a configuration value.
     *
     * Sets the value at the specified key path. Creates intermediate objects
     * as needed for nested keys.
     *
     * @tparam T The type of value to set.
     * @param key The key path using dot notation.
     * @param value The value to set.
     */
    template<typename T>
    static void Set(const std::string& key, const T& value);

    /**
     * @brief Gets the raw JSON object for advanced access.
     *
     * Use this for complex operations that require direct JSON manipulation.
     *
     * @return Reference to the internal JSON object.
     */
    [[nodiscard]] static const nlohmann::json& GetRawJson();

private:
    /**
     * @brief Navigates to a nested JSON value using dot notation.
     *
     * @param key The key path using dot notation.
     * @return Pointer to the JSON value, or nullptr if not found.
     */
    [[nodiscard]] static const nlohmann::json* NavigateToKey(const std::string& key);

    /**
     * @brief Navigates to or creates a nested JSON value using dot notation.
     *
     * @param key The key path using dot notation.
     * @return Reference to the JSON value (created if necessary).
     */
    static nlohmann::json& NavigateOrCreateKey(const std::string& key);

    /**
     * @brief Splits a dot-separated key into parts.
     *
     * @param key The key path using dot notation.
     * @return Vector of key parts.
     */
    [[nodiscard]] static std::vector<std::string> SplitKey(const std::string& key);

    static nlohmann::json s_Config;
    static bool s_IsLoaded;
};

// =============================================================================
// Template Implementations
// =============================================================================

template<typename T>
T Config::Get(const std::string& key, const T& defaultValue) {
    const nlohmann::json* value = NavigateToKey(key);
    if (value == nullptr) {
        return defaultValue;
    }

    try {
        return value->get<T>();
    } catch (const nlohmann::json::exception&) {
        return defaultValue;
    }
}

template<typename T>
std::optional<T> Config::GetOptional(const std::string& key) {
    const nlohmann::json* value = NavigateToKey(key);
    if (value == nullptr) {
        return std::nullopt;
    }

    try {
        return value->get<T>();
    } catch (const nlohmann::json::exception&) {
        return std::nullopt;
    }
}

template<typename T>
void Config::Set(const std::string& key, const T& value) {
    nlohmann::json& target = NavigateOrCreateKey(key);
    target = value;
    s_IsLoaded = true;
}

} // namespace Core
