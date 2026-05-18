#pragma once
#define NOMINMAX
#include "core/IPlugin.h"
#include <string>
#include <vector>
#include <windows.h>

struct LoadedPlugin {
    HMODULE handle;
    IPlugin* instance;
    std::string path;
};

class PluginManager {
public:
    bool LoadPlugin(const std::string& dllPath);
    void UnloadAll();
    IPlugin* GetPlugin(const std::string& name);
    const std::vector<LoadedPlugin>& GetAll() const;

private:
    std::vector<LoadedPlugin> plugins_;
};
