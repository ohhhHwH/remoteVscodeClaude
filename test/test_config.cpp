#include "TestFramework.h"
#include "core/Config.h"
#include <fstream>
#include <filesystem>

static std::string CreateTempConfig() {
    std::string path = std::filesystem::temp_directory_path().string() + "\\test_config.json";
    std::ofstream f(path);
    f << "{\n"
      << "    \"pollIntervalMs\": 500,\n"
      << "    \"pluginDir\": \"./test_plugins\",\n"
      << "    \"commTarget\": \"WeChat\",\n"
      << "    \"monitorRegions\": [\n"
      << "        {\"id\": \"region1\", \"x\": 10, \"y\": 20, \"width\": 300, \"height\": 200, \"threshold\": 0.1},\n"
      << "        {\"id\": \"region2\", \"x\": 50, \"y\": 60, \"width\": 640, \"height\": 480, \"threshold\": 0.05}\n"
      << "    ]\n"
      << "}\n";
    f.close();
    return path;
}

TEST(Config_LoadValid) {
    std::string path = CreateTempConfig();
    Config config;
    ASSERT_TRUE(config.Load(path));
    ASSERT_EQ(config.Get().pollIntervalMs, 500);
    ASSERT_EQ(config.Get().pluginDir, "./test_plugins");
    ASSERT_EQ(config.Get().commTarget, "WeChat");
    std::filesystem::remove(path);
    return true;
}

TEST(Config_LoadRegions) {
    std::string path = CreateTempConfig();
    Config config;
    config.Load(path);

    ASSERT_EQ((int)config.Get().monitorRegions.size(), 2);
    ASSERT_EQ(config.Get().monitorRegions[0].id, "region1");
    ASSERT_EQ(config.Get().monitorRegions[0].x, 10);
    ASSERT_EQ(config.Get().monitorRegions[0].width, 300);
    ASSERT_EQ(config.Get().monitorRegions[1].id, "region2");

    std::filesystem::remove(path);
    return true;
}

TEST(Config_LoadInvalidPath) {
    Config config;
    ASSERT_FALSE(config.Load("nonexistent_file_xyz.json"));
    return true;
}

TEST(Config_SaveAndReload) {
    std::string path = std::filesystem::temp_directory_path().string() + "\\test_save.json";
    Config config;
    config.GetMutable().pollIntervalMs = 2000;
    config.GetMutable().pluginDir = "./my_plugins";
    config.GetMutable().commTarget = "QQ";
    config.GetMutable().monitorRegions.push_back({"r1", 0, 0, 100, 100, 0.02});

    ASSERT_TRUE(config.Save(path));

    Config loaded;
    ASSERT_TRUE(loaded.Load(path));
    ASSERT_EQ(loaded.Get().pollIntervalMs, 2000);
    ASSERT_EQ(loaded.Get().pluginDir, "./my_plugins");
    ASSERT_EQ(loaded.Get().commTarget, "QQ");
    ASSERT_EQ((int)loaded.Get().monitorRegions.size(), 1);
    ASSERT_EQ(loaded.Get().monitorRegions[0].id, "r1");

    std::filesystem::remove(path);
    return true;
}

TEST(Config_Mutable) {
    Config config;
    config.GetMutable().pollIntervalMs = 3000;
    ASSERT_EQ(config.Get().pollIntervalMs, 3000);
    return true;
}

int main() {
    return RunAllTests();
}
