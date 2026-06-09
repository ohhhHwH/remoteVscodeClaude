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

        // Parse strategies (optional section)
        config_.strategies.clear();
        try {
            auto& sarr = root["strategies"].as_array();
            for (auto& sitem : sarr) {
                StrategyConfig sc;
                sc.name = sitem["name"].as_string();
                sc.idleTimeoutSec = sitem["idleTimeoutSec"].as_int();
                try {
                    sc.enabled = std::get<bool>(sitem["enabled"].data);
                } catch (...) { sc.enabled = true; }
                for (auto& aitem : sitem["actions"].as_array()) {
                    ActionStepConfig asc;
                    asc.type = aitem["type"].as_string();
                    asc.params = aitem["params"].as_string();
                    try { asc.delayMs = aitem["delayMs"].as_int(); } catch (...) { asc.delayMs = 500; }
                    sc.actions.push_back(asc);
                }
                config_.strategies.push_back(sc);
            }
        } catch (...) {
            // strategies section is optional — keep empty
        }

        // Parse yesDetect (optional section)
        try {
            auto& yd = root["yesDetect"];
            try { config_.yesDetect.enabled = std::get<bool>(yd["enabled"].data); } catch (...) {}
            try { config_.yesDetect.matchThreshold = (float)yd["matchThreshold"].as_number(); } catch (...) {}
            try { config_.yesDetect.templateImage = yd["templateImage"].as_string(); } catch (...) {}
            try { config_.yesDetect.clickOffsetX = yd["clickOffsetX"].as_int(); } catch (...) {}
            try { config_.yesDetect.clickOffsetY = yd["clickOffsetY"].as_int(); } catch (...) {}
        } catch (...) {
            // yesDetect section is optional — keep defaults
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
    f << "    ],\n";
    f << "    \"strategies\": [\n";
    for (size_t i = 0; i < config_.strategies.size(); i++) {
        auto& s = config_.strategies[i];
        f << "        {\"name\":\"" << s.name << "\",\"idleTimeoutSec\":" << s.idleTimeoutSec
          << ",\"enabled\":" << (s.enabled ? "true" : "false") << ",\"actions\":[";
        for (size_t j = 0; j < s.actions.size(); j++) {
            auto& a = s.actions[j];
            f << "{\"type\":\"" << a.type << "\",\"params\":\"" << a.params << "\",\"delayMs\":" << a.delayMs << "}";
            if (j + 1 < s.actions.size()) f << ",";
        }
        f << "]}";
        if (i + 1 < config_.strategies.size()) f << ",";
        f << "\n";
    }
    f << "    ],\n";
    f << "    \"yesDetect\": {\n";
    f << "        \"enabled\":" << (config_.yesDetect.enabled ? "true" : "false") << ",\n";
    f << "        \"matchThreshold\":" << config_.yesDetect.matchThreshold << ",\n";
    f << "        \"templateImage\":\"" << config_.yesDetect.templateImage << "\",\n";
    f << "        \"clickOffsetX\":" << config_.yesDetect.clickOffsetX << ",\n";
    f << "        \"clickOffsetY\":" << config_.yesDetect.clickOffsetY << "\n";
    f << "    }\n";
    f << "}\n";
    return true;
}

const AppConfig& Config::Get() const {
    return config_;
}

AppConfig& Config::GetMutable() {
    return config_;
}
