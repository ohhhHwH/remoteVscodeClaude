#define NOMINMAX
#include "comm/ICommunicator.h"
#include "comm/IOCR.h"
#include "monitor/IScreenCapture.h"
#include "core/IPlugin.h"
#include <windows.h>
#include <shlobj.h>
#include <opencv2/imgcodecs.hpp>
#include <sstream>
#include <chrono>

class WeChatComm : public ICommunicator, public IPlugin {
public:
    void SetTargetWindow(const std::string& windowTitle) { targetTitle_ = windowTitle; }
    void SetOCR(IOCR* ocr) { ocr_ = ocr; }
    void SetCapture(IScreenCapture* capture) { capture_ = capture; }
    void SetChatRegion(const Rect& rect) { chatRegion_ = rect; }

    bool Init(const PluginConfig& config) override { return true; }
    void Shutdown() override {}
    std::string GetName() const override { return "WeChatComm"; }
    std::string GetVersion() const override { return "0.1.0"; }

    bool SendImage(const std::string& imagePath) override {
        HWND hwnd = FindWindowA(nullptr, targetTitle_.c_str());
        if (!hwnd) return false;

        SetForegroundWindow(hwnd);
        Sleep(200);

        if (OpenClipboard(nullptr)) {
            EmptyClipboard();
            size_t pathLen = imagePath.size() + 1;
            size_t totalSize = sizeof(DROPFILES) + pathLen + 1;
            HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, totalSize);
            if (hGlobal) {
                auto* df = (DROPFILES*)GlobalLock(hGlobal);
                df->pFiles = sizeof(DROPFILES);
                df->fWide = FALSE;
                memcpy((char*)df + sizeof(DROPFILES), imagePath.c_str(), pathLen);
                ((char*)df)[totalSize - 1] = '\0';
                GlobalUnlock(hGlobal);
                SetClipboardData(CF_HDROP, hGlobal);
            }
            CloseClipboard();
        }

        keybd_event(VK_CONTROL, 0, 0, 0);
        keybd_event('V', 0, 0, 0);
        keybd_event('V', 0, KEYEVENTF_KEYUP, 0);
        keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
        Sleep(500);
        keybd_event(VK_RETURN, 0, 0, 0);
        keybd_event(VK_RETURN, 0, KEYEVENTF_KEYUP, 0);
        return true;
    }

    bool SendText(const std::string& text) override {
        HWND hwnd = FindWindowA(nullptr, targetTitle_.c_str());
        if (!hwnd) return false;

        SetForegroundWindow(hwnd);
        Sleep(200);

        if (OpenClipboard(nullptr)) {
            EmptyClipboard();
            HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
            if (hGlobal) {
                memcpy(GlobalLock(hGlobal), text.c_str(), text.size() + 1);
                GlobalUnlock(hGlobal);
                SetClipboardData(CF_TEXT, hGlobal);
            }
            CloseClipboard();
        }

        keybd_event(VK_CONTROL, 0, 0, 0);
        keybd_event('V', 0, 0, 0);
        keybd_event('V', 0, KEYEVENTF_KEYUP, 0);
        keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
        Sleep(200);
        keybd_event(VK_RETURN, 0, 0, 0);
        keybd_event(VK_RETURN, 0, KEYEVENTF_KEYUP, 0);
        return true;
    }

    std::vector<Message> ReadMessages() override {
        std::vector<Message> messages;
        if (!ocr_ || !capture_) return messages;

        cv::Mat chatImage = capture_->CaptureRegion(chatRegion_);
        if (chatImage.empty()) return messages;

        std::string text = ocr_->Recognize(chatImage);
        if (text.empty()) return messages;

        std::istringstream iss(text);
        std::string line;
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        while (std::getline(iss, line)) {
            if (line.empty()) continue;
            messages.push_back({"remote", line, (uint64_t)now});
        }

        if (messages.size() > lastMessageCount_) {
            std::vector<Message> newMsgs(messages.begin() + lastMessageCount_, messages.end());
            lastMessageCount_ = messages.size();
            return newMsgs;
        }
        lastMessageCount_ = messages.size();
        return {};
    }

private:
    std::string targetTitle_;
    IOCR* ocr_ = nullptr;
    IScreenCapture* capture_ = nullptr;
    Rect chatRegion_{0, 0, 0, 0};
    size_t lastMessageCount_ = 0;
};

EXPORT_PLUGIN(WeChatComm)
