#include "TestFramework.h"
#include "core/Config.h"
#include <fstream>
#include <cstdio>

// Helper: write a temporary JSON config file
static bool WriteTempConfig(const std::string& path, const std::string& json) {
    std::ofstream f(path);
    if (!f) return false;
    f << json;
    f.close();
    return true;
}

TEST(parse_strategies_basic) {
    std::string json = R"({
        "pollIntervalMs": 1000,
        "pluginDir": "./plugins",
        "commTarget": "",
        "monitorRegions": [
            {"id": "r1", "x": 0, "y": 0, "width": 100, "height": 100, "threshold": 0.05}
        ],
        "strategies": [
            {
                "name": "test_strategy",
                "idleTimeoutSec": 30,
                "enabled": true,
                "actions": [
                    {"type": "click", "params": "100,200", "delayMs": 500},
                    {"type": "key", "params": "enter", "delayMs": 300}
                ]
            }
        ]
    })";

    std::string tmpPath = "test_config_strat.json";
    ASSERT_TRUE(WriteTempConfig(tmpPath, json));

    Config config;
    ASSERT_TRUE(config.Load(tmpPath));

    auto& strategies = config.Get().strategies;
    ASSERT_EQ((int)strategies.size(), 1);
    ASSERT_TRUE(strategies[0].name == "test_strategy");
    ASSERT_EQ(strategies[0].idleTimeoutSec, 30);
    ASSERT_TRUE(strategies[0].enabled);

    auto& actions = strategies[0].actions;
    ASSERT_EQ((int)actions.size(), 2);
    ASSERT_TRUE(actions[0].type == "click");
    ASSERT_TRUE(actions[0].params == "100,200");
    ASSERT_EQ(actions[0].delayMs, 500);
    ASSERT_TRUE(actions[1].type == "key");
    ASSERT_TRUE(actions[1].params == "enter");
    ASSERT_EQ(actions[1].delayMs, 300);

    std::remove(tmpPath.c_str());
    return true;
}

TEST(parse_strategies_empty) {
    std::string json = R"({
        "pollIntervalMs": 500,
        "pluginDir": ".",
        "commTarget": "test",
        "monitorRegions": [],
        "strategies": []
    })";

    std::string tmpPath = "test_config_empty.json";
    ASSERT_TRUE(WriteTempConfig(tmpPath, json));

    Config config;
    ASSERT_TRUE(config.Load(tmpPath));
    ASSERT_EQ((int)config.Get().strategies.size(), 0);

    std::remove(tmpPath.c_str());
    return true;
}

TEST(parse_strategies_no_section) {
    // Config without strategies section at all — should load with empty defaults
    std::string json = R"({
        "pollIntervalMs": 500,
        "pluginDir": ".",
        "commTarget": "test",
        "monitorRegions": []
    })";

    std::string tmpPath = "test_config_nostrat.json";
    ASSERT_TRUE(WriteTempConfig(tmpPath, json));

    Config config;
    ASSERT_TRUE(config.Load(tmpPath));
    ASSERT_EQ((int)config.Get().strategies.size(), 0);

    std::remove(tmpPath.c_str());
    return true;
}

TEST(parse_strategies_multiple) {
    std::string json = R"({
        "pollIntervalMs": 1000,
        "pluginDir": "./plugins",
        "commTarget": "",
        "monitorRegions": [
            {"id": "r1", "x": 0, "y": 0, "width": 100, "height": 100, "threshold": 0.05}
        ],
        "strategies": [
            {
                "name": "s1",
                "idleTimeoutSec": 10,
                "enabled": true,
                "actions": [{"type": "click", "params": "50,50", "delayMs": 100}]
            },
            {
                "name": "s2",
                "idleTimeoutSec": 0,
                "enabled": false,
                "actions": [
                    {"type": "type", "params": "hello", "delayMs": 500},
                    {"type": "key", "params": "enter", "delayMs": 200},
                    {"type": "wait", "params": "1000", "delayMs": 0}
                ]
            }
        ]
    })";

    std::string tmpPath = "test_config_multi.json";
    ASSERT_TRUE(WriteTempConfig(tmpPath, json));

    Config config;
    ASSERT_TRUE(config.Load(tmpPath));

    auto& strategies = config.Get().strategies;
    ASSERT_EQ((int)strategies.size(), 2);
    ASSERT_TRUE(strategies[0].name == "s1");
    ASSERT_TRUE(strategies[0].enabled);
    ASSERT_TRUE(strategies[1].name == "s2");
    ASSERT_FALSE(strategies[1].enabled);
    ASSERT_EQ((int)strategies[1].actions.size(), 3);

    std::remove(tmpPath.c_str());
    return true;
}

TEST(parse_yes_detect) {
    std::string json = R"({
        "pollIntervalMs": 1000,
        "pluginDir": "./plugins",
        "commTarget": "",
        "monitorRegions": [
            {"id": "r1", "x": 0, "y": 0, "width": 100, "height": 100, "threshold": 0.05}
        ],
        "yesDetect": {
            "enabled": true,
            "matchThreshold": 0.85,
            "templateImage": "pic/yes.png",
            "clickOffsetX": 5,
            "clickOffsetY": -3
        }
    })";

    std::string tmpPath = "test_config_yes.json";
    ASSERT_TRUE(WriteTempConfig(tmpPath, json));

    Config config;
    ASSERT_TRUE(config.Load(tmpPath));

    auto& yd = config.Get().yesDetect;
    ASSERT_TRUE(yd.enabled);
    ASSERT_EQ(yd.matchThreshold, 0.85f);
    ASSERT_TRUE(yd.templateImage == "pic/yes.png");
    ASSERT_EQ(yd.clickOffsetX, 5);
    ASSERT_EQ(yd.clickOffsetY, -3);

    std::remove(tmpPath.c_str());
    return true;
}

TEST(parse_yes_detect_defaults) {
    // Config without yesDetect section — should use defaults
    std::string json = R"({
        "pollIntervalMs": 500,
        "pluginDir": ".",
        "commTarget": "",
        "monitorRegions": []
    })";

    std::string tmpPath = "test_config_nodefaults.json";
    ASSERT_TRUE(WriteTempConfig(tmpPath, json));

    Config config;
    ASSERT_TRUE(config.Load(tmpPath));

    auto& yd = config.Get().yesDetect;
    ASSERT_FALSE(yd.enabled);
    ASSERT_EQ(yd.matchThreshold, 0.7f);    // default

    std::remove(tmpPath.c_str());
    return true;
}

TEST(parse_action_types) {
    // Verify all action types parse correctly
    std::string json = R"({
        "pollIntervalMs": 1000,
        "pluginDir": ".",
        "commTarget": "",
        "monitorRegions": [
            {"id": "r1", "x": 0, "y": 0, "width": 100, "height": 100, "threshold": 0.05}
        ],
        "strategies": [{
            "name": "all_types",
            "idleTimeoutSec": 5,
            "enabled": true,
            "actions": [
                {"type": "click", "params": "10,20", "delayMs": 100},
                {"type": "dclick", "params": "30,40", "delayMs": 200},
                {"type": "rclick", "params": "50,60", "delayMs": 300},
                {"type": "move", "params": "70,80", "delayMs": 50},
                {"type": "type", "params": "test text", "delayMs": 400},
                {"type": "key", "params": "enter", "delayMs": 100},
                {"type": "hotkey", "params": "ctrl+c", "delayMs": 500},
                {"type": "wait", "params": "2000", "delayMs": 0},
                {"type": "scroll", "params": "100,200,3", "delayMs": 100}
            ]
        }]
    })";

    std::string tmpPath = "test_config_alltypes.json";
    ASSERT_TRUE(WriteTempConfig(tmpPath, json));

    Config config;
    ASSERT_TRUE(config.Load(tmpPath));

    auto& actions = config.Get().strategies[0].actions;
    ASSERT_EQ((int)actions.size(), 9);
    ASSERT_TRUE(actions[0].type == "click");
    ASSERT_TRUE(actions[0].params == "10,20");
    ASSERT_TRUE(actions[1].type == "dclick");
    ASSERT_TRUE(actions[2].type == "rclick");
    ASSERT_TRUE(actions[3].type == "move");
    ASSERT_TRUE(actions[4].type == "type");
    ASSERT_TRUE(actions[4].params == "test text");
    ASSERT_TRUE(actions[5].type == "key");
    ASSERT_TRUE(actions[6].type == "hotkey");
    ASSERT_TRUE(actions[7].type == "wait");
    ASSERT_TRUE(actions[7].params == "2000");
    ASSERT_TRUE(actions[8].type == "scroll");

    std::remove(tmpPath.c_str());
    return true;
}

int main() {
    return RunAllTests();
}
