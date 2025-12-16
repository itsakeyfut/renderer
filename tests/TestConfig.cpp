/**
 * @file TestConfig.cpp
 * @brief Test file for Config.h configuration system using GoogleTest.
 *
 * This file tests that the configuration system loads, saves,
 * and manages JSON configuration data correctly.
 */

#include <gtest/gtest.h>
#include "Core/Config.h"
#include "Core/Log.h"
#include <fstream>
#include <filesystem>

// =============================================================================
// Test Fixture for Config Tests
// =============================================================================

class ConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize logging for tests
        if (!Core::Log::IsInitialized()) {
            Core::Log::Init();
        }
        // Clear config before each test
        Core::Config::Clear();
    }

    void TearDown() override {
        // Clean up config after each test
        Core::Config::Clear();
        // Remove any test files created
        CleanupTestFiles();
    }

    void CleanupTestFiles() {
        std::filesystem::remove("test_config.json");
        std::filesystem::remove("test_output.json");
        std::filesystem::remove("invalid_config.json");
    }

    void CreateTestConfigFile(const std::string& filepath, const std::string& content) {
        std::ofstream file(filepath);
        file << content;
        file.close();
    }
};

// =============================================================================
// Initial State Tests
// =============================================================================

TEST_F(ConfigTest, InitiallyNotLoaded) {
    EXPECT_FALSE(Core::Config::IsLoaded());
}

TEST_F(ConfigTest, InitiallyNoKeys) {
    EXPECT_FALSE(Core::Config::HasKey("anything"));
}

// =============================================================================
// Load Tests
// =============================================================================

TEST_F(ConfigTest, LoadValidJsonFile) {
    const std::string testJson = R"({
        "window": {
            "width": 1920,
            "height": 1080
        }
    })";
    CreateTestConfigFile("test_config.json", testJson);

    EXPECT_TRUE(Core::Config::Load("test_config.json"));
    EXPECT_TRUE(Core::Config::IsLoaded());
}

TEST_F(ConfigTest, LoadNonExistentFile) {
    EXPECT_FALSE(Core::Config::Load("non_existent_file.json"));
    EXPECT_FALSE(Core::Config::IsLoaded());
}

TEST_F(ConfigTest, LoadInvalidJson) {
    CreateTestConfigFile("invalid_config.json", "{ invalid json }");

    EXPECT_FALSE(Core::Config::Load("invalid_config.json"));
    EXPECT_FALSE(Core::Config::IsLoaded());
}

TEST_F(ConfigTest, LoadEmptyJson) {
    CreateTestConfigFile("test_config.json", "{}");

    EXPECT_TRUE(Core::Config::Load("test_config.json"));
    EXPECT_TRUE(Core::Config::IsLoaded());
}

// =============================================================================
// Get Tests
// =============================================================================

TEST_F(ConfigTest, GetReturnsDefaultWhenNotLoaded) {
    int value = Core::Config::Get<int>("window.width", 1280);
    EXPECT_EQ(value, 1280);
}

TEST_F(ConfigTest, GetReturnsDefaultWhenKeyMissing) {
    CreateTestConfigFile("test_config.json", R"({"window": {"width": 1920}})");
    Core::Config::Load("test_config.json");

    int value = Core::Config::Get<int>("window.height", 720);
    EXPECT_EQ(value, 720);
}

TEST_F(ConfigTest, GetReturnsActualValue) {
    CreateTestConfigFile("test_config.json", R"({"window": {"width": 1920}})");
    Core::Config::Load("test_config.json");

    int value = Core::Config::Get<int>("window.width", 1280);
    EXPECT_EQ(value, 1920);
}

TEST_F(ConfigTest, GetNestedValues) {
    const std::string testJson = R"({
        "graphics": {
            "vsync": true,
            "msaaSamples": 4
        },
        "camera": {
            "fov": 45.0,
            "nearPlane": 0.1,
            "farPlane": 1000.0
        }
    })";
    CreateTestConfigFile("test_config.json", testJson);
    Core::Config::Load("test_config.json");

    EXPECT_TRUE(Core::Config::Get<bool>("graphics.vsync", false));
    EXPECT_EQ(Core::Config::Get<int>("graphics.msaaSamples", 1), 4);
    EXPECT_FLOAT_EQ(Core::Config::Get<float>("camera.fov", 60.0f), 45.0f);
    EXPECT_FLOAT_EQ(Core::Config::Get<float>("camera.nearPlane", 1.0f), 0.1f);
    EXPECT_FLOAT_EQ(Core::Config::Get<float>("camera.farPlane", 100.0f), 1000.0f);
}

TEST_F(ConfigTest, GetStringValue) {
    CreateTestConfigFile("test_config.json", R"({"window": {"title": "Vulkan Renderer"}})");
    Core::Config::Load("test_config.json");

    std::string title = Core::Config::Get<std::string>("window.title", "Default Title");
    EXPECT_EQ(title, "Vulkan Renderer");
}

TEST_F(ConfigTest, GetReturnsDefaultOnTypeMismatch) {
    CreateTestConfigFile("test_config.json", R"({"value": "not a number"})");
    Core::Config::Load("test_config.json");

    int value = Core::Config::Get<int>("value", 42);
    EXPECT_EQ(value, 42);
}

// =============================================================================
// GetOptional Tests
// =============================================================================

TEST_F(ConfigTest, GetOptionalReturnsNulloptWhenMissing) {
    CreateTestConfigFile("test_config.json", R"({})");
    Core::Config::Load("test_config.json");

    auto value = Core::Config::GetOptional<int>("missing.key");
    EXPECT_FALSE(value.has_value());
}

TEST_F(ConfigTest, GetOptionalReturnsValueWhenPresent) {
    CreateTestConfigFile("test_config.json", R"({"setting": 42})");
    Core::Config::Load("test_config.json");

    auto value = Core::Config::GetOptional<int>("setting");
    EXPECT_TRUE(value.has_value());
    EXPECT_EQ(*value, 42);
}

// =============================================================================
// HasKey Tests
// =============================================================================

TEST_F(ConfigTest, HasKeyReturnsTrueForExistingKey) {
    CreateTestConfigFile("test_config.json", R"({"window": {"width": 1920}})");
    Core::Config::Load("test_config.json");

    EXPECT_TRUE(Core::Config::HasKey("window"));
    EXPECT_TRUE(Core::Config::HasKey("window.width"));
}

TEST_F(ConfigTest, HasKeyReturnsFalseForMissingKey) {
    CreateTestConfigFile("test_config.json", R"({"window": {"width": 1920}})");
    Core::Config::Load("test_config.json");

    EXPECT_FALSE(Core::Config::HasKey("missing"));
    EXPECT_FALSE(Core::Config::HasKey("window.height"));
    EXPECT_FALSE(Core::Config::HasKey("window.width.nested"));
}

// =============================================================================
// Set Tests
// =============================================================================

TEST_F(ConfigTest, SetCreatesNewKey) {
    Core::Config::Set("window.width", 1920);

    EXPECT_TRUE(Core::Config::IsLoaded());
    EXPECT_TRUE(Core::Config::HasKey("window.width"));
    EXPECT_EQ(Core::Config::Get<int>("window.width", 0), 1920);
}

TEST_F(ConfigTest, SetOverwritesExistingValue) {
    CreateTestConfigFile("test_config.json", R"({"window": {"width": 1920}})");
    Core::Config::Load("test_config.json");

    Core::Config::Set("window.width", 2560);
    EXPECT_EQ(Core::Config::Get<int>("window.width", 0), 2560);
}

TEST_F(ConfigTest, SetCreatesNestedStructure) {
    Core::Config::Set("deeply.nested.value", 42);

    EXPECT_TRUE(Core::Config::HasKey("deeply"));
    EXPECT_TRUE(Core::Config::HasKey("deeply.nested"));
    EXPECT_TRUE(Core::Config::HasKey("deeply.nested.value"));
    EXPECT_EQ(Core::Config::Get<int>("deeply.nested.value", 0), 42);
}

TEST_F(ConfigTest, SetDifferentTypes) {
    Core::Config::Set("int_value", 42);
    Core::Config::Set("float_value", 3.14f);
    Core::Config::Set("bool_value", true);
    Core::Config::Set("string_value", std::string("hello"));

    EXPECT_EQ(Core::Config::Get<int>("int_value", 0), 42);
    EXPECT_FLOAT_EQ(Core::Config::Get<float>("float_value", 0.0f), 3.14f);
    EXPECT_TRUE(Core::Config::Get<bool>("bool_value", false));
    EXPECT_EQ(Core::Config::Get<std::string>("string_value", ""), "hello");
}

// =============================================================================
// Save Tests
// =============================================================================

TEST_F(ConfigTest, SaveCreatesFile) {
    Core::Config::Set("window.width", 1920);
    Core::Config::Set("window.height", 1080);

    EXPECT_TRUE(Core::Config::Save("test_output.json"));
    EXPECT_TRUE(std::filesystem::exists("test_output.json"));
}

TEST_F(ConfigTest, SavedFileCanBeLoaded) {
    Core::Config::Set("window.width", 1920);
    Core::Config::Set("graphics.vsync", true);
    Core::Config::Save("test_output.json");

    Core::Config::Clear();
    EXPECT_TRUE(Core::Config::Load("test_output.json"));
    EXPECT_EQ(Core::Config::Get<int>("window.width", 0), 1920);
    EXPECT_TRUE(Core::Config::Get<bool>("graphics.vsync", false));
}

// =============================================================================
// Clear Tests
// =============================================================================

TEST_F(ConfigTest, ClearRemovesAllData) {
    Core::Config::Set("key", "value");
    EXPECT_TRUE(Core::Config::IsLoaded());

    Core::Config::Clear();

    EXPECT_FALSE(Core::Config::IsLoaded());
    EXPECT_FALSE(Core::Config::HasKey("key"));
}

// =============================================================================
// Edge Case Tests
// =============================================================================

TEST_F(ConfigTest, EmptyKeyReturnsRootAccess) {
    CreateTestConfigFile("test_config.json", R"({"key": "value"})");
    Core::Config::Load("test_config.json");

    // Empty key should still allow operations
    EXPECT_TRUE(Core::Config::HasKey(""));
}

TEST_F(ConfigTest, TopLevelValue) {
    CreateTestConfigFile("test_config.json", R"({"version": "1.0.0"})");
    Core::Config::Load("test_config.json");

    std::string version = Core::Config::Get<std::string>("version", "0.0.0");
    EXPECT_EQ(version, "1.0.0");
}

TEST_F(ConfigTest, ArrayAccess) {
    CreateTestConfigFile("test_config.json", R"({"items": [1, 2, 3]})");
    Core::Config::Load("test_config.json");

    // HasKey should return true for the array key
    EXPECT_TRUE(Core::Config::HasKey("items"));

    // Getting the array should work via GetRawJson
    const auto& json = Core::Config::GetRawJson();
    EXPECT_TRUE(json.contains("items"));
    EXPECT_TRUE(json["items"].is_array());
    EXPECT_EQ(json["items"].size(), 3);
}

TEST_F(ConfigTest, FullSettingsExample) {
    const std::string testJson = R"({
        "window": {
            "width": 1920,
            "height": 1080,
            "title": "Vulkan Renderer",
            "fullscreen": false
        },
        "graphics": {
            "vsync": true,
            "validationLayers": true,
            "msaaSamples": 4
        },
        "camera": {
            "fov": 45.0,
            "nearPlane": 0.1,
            "farPlane": 1000.0,
            "moveSpeed": 5.0,
            "mouseSensitivity": 0.1
        }
    })";
    CreateTestConfigFile("test_config.json", testJson);
    Core::Config::Load("test_config.json");

    // Window settings
    EXPECT_EQ(Core::Config::Get<int>("window.width", 0), 1920);
    EXPECT_EQ(Core::Config::Get<int>("window.height", 0), 1080);
    EXPECT_EQ(Core::Config::Get<std::string>("window.title", ""), "Vulkan Renderer");
    EXPECT_FALSE(Core::Config::Get<bool>("window.fullscreen", true));

    // Graphics settings
    EXPECT_TRUE(Core::Config::Get<bool>("graphics.vsync", false));
    EXPECT_TRUE(Core::Config::Get<bool>("graphics.validationLayers", false));
    EXPECT_EQ(Core::Config::Get<int>("graphics.msaaSamples", 0), 4);

    // Camera settings
    EXPECT_FLOAT_EQ(Core::Config::Get<float>("camera.fov", 0.0f), 45.0f);
    EXPECT_FLOAT_EQ(Core::Config::Get<float>("camera.nearPlane", 0.0f), 0.1f);
    EXPECT_FLOAT_EQ(Core::Config::Get<float>("camera.farPlane", 0.0f), 1000.0f);
    EXPECT_FLOAT_EQ(Core::Config::Get<float>("camera.moveSpeed", 0.0f), 5.0f);
    EXPECT_FLOAT_EQ(Core::Config::Get<float>("camera.mouseSensitivity", 0.0f), 0.1f);
}
