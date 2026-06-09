#pragma once
#include <string>
#include <vector>
#include <unordered_map>

struct RegionConfig {
    std::string id;
    int x, y, width, height;
    double threshold;
};

struct ActionStepConfig {
    std::string type;
    std::string params;
    int delayMs = 500;
};

struct StrategyConfig {
    std::string name;
    int idleTimeoutSec = 30;                // 0 = run on every check
    std::vector<ActionStepConfig> actions;
    bool enabled = true;
};

struct YesDetectConfig {
    bool enabled = false;
    float matchThreshold = 0.7f;
    std::string templateImage;
    int clickOffsetX = 0;
    int clickOffsetY = 0;
};

struct AppConfig {
    std::vector<RegionConfig> monitorRegions;
    std::string commTarget;
    int pollIntervalMs;
    std::string pluginDir;
    std::vector<StrategyConfig> strategies;
    YesDetectConfig yesDetect;
};

class Config {
public:
    bool Load(const std::string& path);
    bool Save(const std::string& path);
    const AppConfig& Get() const;
    AppConfig& GetMutable();

private:
    AppConfig config_;
};
