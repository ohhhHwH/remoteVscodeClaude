#pragma once
#define NOMINMAX
#include "monitor/IScreenCapture.h"
#include "monitor/IRegionMonitor.h"
#include "comm/IOCR.h"
#include "action/IActionExecutor.h"
#include <opencv2/core.hpp>
#include <d3d11.h>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>

struct AppState {
    Rect monitorRegion{0, 0, 0, 0};
    Rect commInputRegion{0, 0, 0, 0};
    Rect commChatRegion{0, 0, 0, 0};
    POINT commClickPoint{0, 0}; // absolute screen position to double-click for copy
    float threshold = 0.05f;
    int pollIntervalMs = 1000;
    int noChangeTimeoutSec = 10;
    bool monitorRunning = false;
    bool commRunning = false;
};

// Command format: "CMD:action:params"
// e.g. "CMD:click:100,200" "CMD:type:hello" "CMD:enter" "CMD:stop"
struct Command {
    std::string action;
    std::string params;
};

class App {
public:
    App(ID3D11Device* device);
    ~App();

    void Render();
    void SelectRegion(Rect& outRect, HWND parentHwnd);

private:
    void RenderMonitorPanel();
    void RenderCommPanel();
    void RenderLogPanel();

    void StartMonitor();
    void StopMonitor();
    void StartComm();
    void StopComm();

    void MonitorThread();
    void CommThread();
    void SendToComm(const std::string& text);
    void SendImageToComm(const cv::Mat& image);

    std::string ReadChatByClipboard();
    std::string ReadChatAtPosition(int absX, int absY);
    Command ParseCommand(const std::string& text);
    std::vector<Command> ParseCommands(const std::string& text);
    void ExecuteCommand(const Command& cmd);
    void ExecuteCommands(const std::string& text);

    void UpdateTexture(ID3D11ShaderResourceView** srv, const cv::Mat& img);
    void FlushPendingFrames();

    ID3D11Device* device_ = nullptr;
    AppState state_;

    // Textures for preview
    ID3D11ShaderResourceView* monitorSrv_ = nullptr;
    ID3D11ShaderResourceView* commInputSrv_ = nullptr;
    ID3D11ShaderResourceView* commChatSrv_ = nullptr;
    int monitorTexW_ = 0, monitorTexH_ = 0;
    int commInputTexW_ = 0, commInputTexH_ = 0;
    int commChatTexW_ = 0, commChatTexH_ = 0;

    // Pending frames from background threads (for thread-safe texture update)
    std::mutex frameMutex_;
    cv::Mat pendingMonitorFrame_;
    cv::Mat pendingChatFrame_;

    // Threads
    std::thread monitorThread_;
    std::thread commThread_;
    std::atomic<bool> monitorStop_{false};
    std::atomic<bool> commStop_{false};

    // Pending message to send
    std::mutex sendMutex_;
    std::string pendingSend_;
    bool suppressNextRead_ = false; // set after we send so CommThread skips one cycle

    // Logs
    std::mutex logMutex_;
    std::vector<std::string> monitorLog_;
    std::vector<std::string> commLog_;
    std::vector<std::string> actionLog_;
};
