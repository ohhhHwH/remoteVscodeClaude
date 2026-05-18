#include "core/PluginManager.h"
#include "core/EventBus.h"
#include "core/Config.h"
#include <iostream>
#include <thread>
#include <atomic>
#include <filesystem>

static std::atomic<bool> g_running{true};

BOOL WINAPI ConsoleHandler(DWORD signal) {
    if (signal == CTRL_C_EVENT) {
        g_running = false;
        return TRUE;
    }
    return FALSE;
}

int main(int argc, char* argv[]) {
    SetConsoleCtrlHandler(ConsoleHandler, TRUE);

    Config config;
    std::string configPath = (argc > 1) ? argv[1] : "config/config.json";
    if (!config.Load(configPath)) {
        std::cerr << "Failed to load config: " << configPath << std::endl;
        return 1;
    }

    EventBus eventBus;
    PluginManager pluginMgr;

    std::string pluginDir = config.Get().pluginDir;
    if (std::filesystem::exists(pluginDir)) {
        for (auto& entry : std::filesystem::directory_iterator(pluginDir)) {
            if (entry.path().extension() == ".dll") {
                if (pluginMgr.LoadPlugin(entry.path().string())) {
                    std::cout << "Loaded: " << entry.path().filename().string() << std::endl;
                }
            }
        }
    }

    PluginConfig pluginConfig{configPath};
    for (auto& p : pluginMgr.GetAll()) {
        p.instance->Init(pluginConfig);
    }

    std::cout << "RemoteMonitor running. Press Ctrl+C to exit." << std::endl;

    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(config.Get().pollIntervalMs));
    }

    for (auto& p : pluginMgr.GetAll()) {
        p.instance->Shutdown();
    }
    pluginMgr.UnloadAll();

    std::cout << "Shutdown complete." << std::endl;
    return 0;
}
