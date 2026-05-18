#include "action/IActionExecutor.h"
#include "core/IPlugin.h"

class Win32Action : public IActionExecutor, public IPlugin {
public:
    // IPlugin
    bool Init(const PluginConfig& config) override { return true; }
    void Shutdown() override {}
    std::string GetName() const override { return "Win32Action"; }
    std::string GetVersion() const override { return "0.1.0"; }

    // IActionExecutor
    void Click(int x, int y) override {
        SetCursorPos(x, y);
        mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
        mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
    }

    void TypeText(const std::string& text) override {
        for (char c : text) {
            SHORT vk = VkKeyScanA(c);
            BYTE key = LOBYTE(vk);
            BYTE shift = HIBYTE(vk);
            if (shift & 1) keybd_event(VK_SHIFT, 0, 0, 0);
            keybd_event(key, 0, 0, 0);
            keybd_event(key, 0, KEYEVENTF_KEYUP, 0);
            if (shift & 1) keybd_event(VK_SHIFT, 0, KEYEVENTF_KEYUP, 0);
        }
    }

    void KeyPress(WORD vkCode) override {
        keybd_event((BYTE)vkCode, 0, 0, 0);
        keybd_event((BYTE)vkCode, 0, KEYEVENTF_KEYUP, 0);
    }

    void MoveMouse(int x, int y) override {
        SetCursorPos(x, y);
    }
};

EXPORT_PLUGIN(Win32Action)
