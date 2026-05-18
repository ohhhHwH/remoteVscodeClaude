#include "core/Config.h"
#include "core/Json.h"
#include <fstream>

bool Config::Load(const std::string& path) {
    try {
        json::Parser parser;
        auto root = parser.ParseFile(path);

        config_.pollIntervalMs = root["pollIntervalMs"].as_int();
        config_.pluginDir = root["pluginDir"].as_string();
        config_.commTarget = root["commTarget"].as_string();

        config_.monitorRegions.clear();
        for (auto& item : root["monitorRegions"].as_array()) {
            RegionConfig rc;
            rc.id = item["id"].as_string();
            rc.x = item["x"].as_int();
            rc.y = item["y"].as_int();
            rc.width = item["width"].as_int();
            rc.height = item["height"].as_int();
            rc.threshold = item["threshold"].as_number();
            config_.monitorRegions.push_back(rc);
        }
        return true;
    } catch (...) {
        config_.pollIntervalMs = 1000;
        config_.pluginDir = "./plugins";
        config_.commTarget = "";
        return false;
    }
}

bool Config::Save(const std::string& path) {
    std::ofstream f(path);
    if (!f.is_open()) return false;

    f << "{\n";
    f << "    \"pollIntervalMs\": " << config_.pollIntervalMs << ",\n";
    f << "    \"pluginDir\": \"" << config_.pluginDir << "\",\n";
    f << "    \"commTarget\": \"" << config_.commTarget << "\",\n";
    f << "    \"monitorRegions\": [\n";
    for (size_t i = 0; i < config_.monitorRegions.size(); i++) {
        auto& r = config_.monitorRegions[i];
        f << "        {\"id\":\"" << r.id << "\",\"x\":" << r.x << ",\"y\":" << r.y
          << ",\"width\":" << r.width << ",\"height\":" << r.height
          << ",\"threshold\":" << r.threshold << "}";
        if (i + 1 < config_.monitorRegions.size()) f << ",";
        f << "\n";
    }
    f << "    ]\n}\n";
    return true;
}

const AppConfig& Config::Get() const {
    return config_;
}

AppConfig& Config::GetMutable() {
    return config_;
}
