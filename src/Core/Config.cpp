/**
 * @file Config.cpp
 * @brief Implementation of the configuration file system.
 */

#include "Config.h"
#include "Log.h"

#include <fstream>
#include <sstream>

namespace Core {

nlohmann::json Config::s_Config = nlohmann::json::object();
bool Config::s_IsLoaded = false;

bool Config::Load(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        LOG_WARN("Config: Failed to open file: {}", filepath);
        return false;
    }

    try {
        s_Config = nlohmann::json::parse(file);
        s_IsLoaded = true;
        LOG_INFO("Config: Loaded configuration from {}", filepath);
        return true;
    } catch (const nlohmann::json::parse_error& e) {
        LOG_ERROR("Config: JSON parse error in {}: {}", filepath, e.what());
        return false;
    }
}

bool Config::Save(const std::string& filepath) {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        LOG_ERROR("Config: Failed to open file for writing: {}", filepath);
        return false;
    }

    try {
        file << s_Config.dump(4);
        LOG_INFO("Config: Saved configuration to {}", filepath);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Config: Failed to save configuration: {}", e.what());
        return false;
    }
}

void Config::Clear() {
    s_Config = nlohmann::json::object();
    s_IsLoaded = false;
}

bool Config::IsLoaded() {
    return s_IsLoaded;
}

bool Config::HasKey(const std::string& key) {
    return NavigateToKey(key) != nullptr;
}

const nlohmann::json& Config::GetRawJson() {
    return s_Config;
}

const nlohmann::json* Config::NavigateToKey(const std::string& key) {
    if (key.empty()) {
        return &s_Config;
    }

    std::vector<std::string> parts = SplitKey(key);
    const nlohmann::json* current = &s_Config;

    for (const auto& part : parts) {
        if (!current->is_object() || !current->contains(part)) {
            return nullptr;
        }
        current = &(*current)[part];
    }

    return current;
}

nlohmann::json& Config::NavigateOrCreateKey(const std::string& key) {
    if (key.empty()) {
        return s_Config;
    }

    std::vector<std::string> parts = SplitKey(key);
    nlohmann::json* current = &s_Config;

    for (const auto& part : parts) {
        if (!current->is_object()) {
            *current = nlohmann::json::object();
        }
        current = &(*current)[part];
    }

    return *current;
}

std::vector<std::string> Config::SplitKey(const std::string& key) {
    std::vector<std::string> parts;
    std::istringstream stream(key);
    std::string part;

    while (std::getline(stream, part, '.')) {
        if (!part.empty()) {
            parts.push_back(part);
        }
    }

    return parts;
}

} // namespace Core
