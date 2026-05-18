#pragma once
#include <string>

struct PluginConfig {
    std::string configPath;
};

class IPlugin {
public:
    virtual ~IPlugin() = default;
    virtual bool Init(const PluginConfig& config) = 0;
    virtual void Shutdown() = 0;
    virtual std::string GetName() const = 0;
    virtual std::string GetVersion() const = 0;
};

// DLL导出函数签名
extern "C" {
    typedef IPlugin* (*CreatePluginFunc)();
    typedef void (*DestroyPluginFunc)(IPlugin*);
}

#define EXPORT_PLUGIN(ClassName) \
    extern "C" __declspec(dllexport) IPlugin* CreatePlugin() { return new ClassName(); } \
    extern "C" __declspec(dllexport) void DestroyPlugin(IPlugin* p) { delete p; }
