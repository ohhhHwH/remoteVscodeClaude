#pragma once
#include <string>
#include <vector>
#include <unordered_map>

struct RegionConfig {
    std::string id;
    int x, y, width, height;
    double threshold;
};

struct AppConfig {
    std::vector<RegionConfig> monitorRegions;
    std::string commTarget;       // QQ/微信窗口标题
    int pollIntervalMs;           // 监控轮询间隔
    std::string pluginDir;        // 插件目录
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
