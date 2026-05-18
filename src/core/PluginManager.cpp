#include "core/PluginManager.h"

bool PluginManager::LoadPlugin(const std::string& dllPath) {
    HMODULE handle = LoadLibraryA(dllPath.c_str());
    if (!handle) return false;

    auto createFunc = (CreatePluginFunc)GetProcAddress(handle, "CreatePlugin");
    if (!createFunc) {
        FreeLibrary(handle);
        return false;
    }

    IPlugin* instance = createFunc();
    if (!instance) {
        FreeLibrary(handle);
        return false;
    }

    plugins_.push_back({handle, instance, dllPath});
    return true;
}

void PluginManager::UnloadAll() {
    for (auto& p : plugins_) {
        auto destroyFunc = (DestroyPluginFunc)GetProcAddress(p.handle, "DestroyPlugin");
        if (destroyFunc) {
            destroyFunc(p.instance);
        }
        FreeLibrary(p.handle);
    }
    plugins_.clear();
}

IPlugin* PluginManager::GetPlugin(const std::string& name) {
    for (auto& p : plugins_) {
        if (p.instance->GetName() == name) return p.instance;
    }
    return nullptr;
}

const std::vector<LoadedPlugin>& PluginManager::GetAll() const {
    return plugins_;
}
