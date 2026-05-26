#include "App.h"
#include "imgui.h"
#include <opencv2/imgproc.hpp>
#include <chrono>
#include <ctime>

#include <algorithm>

// Inline implementations of the modules (avoid linking plugin DLLs)
static cv::Mat CaptureScreen(const Rect& rect) {
    HDC hScreen = GetDC(nullptr);
    HDC hDC = CreateCompatibleDC(hScreen);
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreen, rect.width, rect.height);
    SelectObject(hDC, hBitmap);
    BitBlt(hDC, 0, 0, rect.width, rect.height, hScreen, rect.x, rect.y, SRCCOPY);

    cv::Mat mat(rect.height, rect.width, CV_8UC4);
    BITMAPINFOHEADER bi = {sizeof(BITMAPINFOHEADER), rect.width, -rect.height, 1, 32, BI_RGB};
    GetDIBits(hDC, hBitmap, 0, rect.height, mat.data, (BITMAPINFO*)&bi, DIB_RGB_COLORS);

    DeleteObject(hBitmap);
    DeleteDC(hDC);
    ReleaseDC(nullptr, hScreen);

    cv::Mat bgr;
    cv::cvtColor(mat, bgr, cv::COLOR_BGRA2BGR);
    return bgr;
}

// Convert wide string (UTF-16) to UTF-8
static std::string WideToUtf8(const wchar_t* wstr) {
    if (!wstr) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0) return "";
    std::string result(len - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, &result[0], len, nullptr, nullptr);
    return result;
}

static std::string TimeNow() {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    char buf[32];
    struct tm tm_buf;
    localtime_s(&tm_buf, &t);
    strftime(buf, sizeof(buf), "%H:%M:%S", &tm_buf);
    return buf;
}

// Region selection overlay window
static LRESULT CALLBACK OverlayProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    static POINT startPt{}, endPt{};
    static bool dragging = false;
    static Rect* outRect = nullptr;

    switch (msg) {
    case WM_CREATE:
        outRect = (Rect*)((CREATESTRUCT*)lp)->lpCreateParams;
        return 0;
    case WM_LBUTTONDOWN:
        GetCursorPos(&startPt);
        dragging = true;
        SetCapture(hwnd);
        return 0;
    case WM_MOUSEMOVE:
        if (dragging) {
            GetCursorPos(&endPt);
            InvalidateRect(hwnd, nullptr, TRUE);
        }
        return 0;
    case WM_LBUTTONUP:
        if (dragging) {
            dragging = false;
            ReleaseCapture();
            GetCursorPos(&endPt);
            if (outRect) {
                int x1 = (int)std::min(startPt.x, endPt.x);
                int y1 = (int)std::min(startPt.y, endPt.y);
                int x2 = (int)std::max(startPt.x, endPt.x);
                int y2 = (int)std::max(startPt.y, endPt.y);
                *outRect = {x1, y1, x2 - x1, y2 - y1};
            }
            DestroyWindow(hwnd);
        }
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        if (dragging) {
            RECT wr;
            GetWindowRect(hwnd, &wr);
            int dx = wr.left, dy = wr.top;
            HPEN pen = CreatePen(PS_SOLID, 2, RGB(0, 255, 0));
            HBRUSH brush = (HBRUSH)GetStockObject(NULL_BRUSH);
            SelectObject(hdc, pen);
            SelectObject(hdc, brush);
            Rectangle(hdc, startPt.x - dx, startPt.y - dy, endPt.x - dx, endPt.y - dy);
            DeleteObject(pen);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

App::App(ID3D11Device* device) : device_(device) {}

App::~App() {
    StopMonitor();
    StopComm();
    if (monitorSrv_) monitorSrv_->Release();
    if (commInputSrv_) commInputSrv_->Release();
    if (commChatSrv_) commChatSrv_->Release();
}

void App::SelectRegion(Rect& outRect, HWND parentHwnd) {
    ShowWindow(parentHwnd, SW_MINIMIZE);
    Sleep(300);

    WNDCLASSA wc = {};
    wc.lpfnWndProc = OverlayProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = "OverlaySelect";
    wc.hCursor = LoadCursor(nullptr, IDC_CROSS);
    RegisterClassA(&wc);

    int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int sw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int sh = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    HWND overlay = CreateWindowExA(
        WS_EX_TOPMOST | WS_EX_LAYERED,
        "OverlaySelect", "",
        WS_POPUP | WS_VISIBLE,
        vx, vy, sw, sh,
        nullptr, nullptr, wc.hInstance, &outRect);

    SetLayeredWindowAttributes(overlay, 0, 80, LWA_ALPHA);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    UnregisterClassA("OverlaySelect", wc.hInstance);
    ShowWindow(parentHwnd, SW_RESTORE);
    SetForegroundWindow(parentHwnd);
}

void App::UpdateTexture(ID3D11ShaderResourceView** srv, const cv::Mat& img) {
    if (img.empty()) return;

    cv::Mat rgba;
    cv::cvtColor(img, rgba, cv::COLOR_BGR2RGBA);

    if (*srv) { (*srv)->Release(); *srv = nullptr; }

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = rgba.cols;
    desc.Height = rgba.rows;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA sub = {};
    sub.pSysMem = rgba.data;
    sub.SysMemPitch = rgba.cols * 4;

    ID3D11Texture2D* tex = nullptr;
    device_->CreateTexture2D(&desc, &sub, &tex);
    if (tex) {
        device_->CreateShaderResourceView(tex, nullptr, srv);
        tex->Release();
    }
}

void App::Render() {
    FlushPendingFrames();

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::Begin("RemoteMonitor", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

    float w = ImGui::GetContentRegionAvail().x;
    ImGui::BeginChild("Left", ImVec2(w * 0.5f, -ImGui::GetFrameHeightWithSpacing() * 8), true);
    RenderMonitorPanel();
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("Right", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 8), true);
    RenderCommPanel();
    ImGui::EndChild();

    ImGui::BeginChild("Bottom", ImVec2(0, 0), true);
    RenderLogPanel();
    ImGui::EndChild();

    ImGui::End();
}

void App::FlushPendingFrames() {
    std::lock_guard<std::mutex> lk(frameMutex_);
    if (!pendingMonitorFrame_.empty()) {
        monitorTexW_ = pendingMonitorFrame_.cols;
        monitorTexH_ = pendingMonitorFrame_.rows;
        UpdateTexture(&monitorSrv_, pendingMonitorFrame_);
        pendingMonitorFrame_ = cv::Mat();
    }
    if (!pendingChatFrame_.empty()) {
        commChatTexW_ = pendingChatFrame_.cols;
        commChatTexH_ = pendingChatFrame_.rows;
        UpdateTexture(&commChatSrv_, pendingChatFrame_);
        pendingChatFrame_ = cv::Mat();
    }
}

void App::RenderMonitorPanel() {
    ImGui::Text("Monitor Zone");
    ImGui::Separator();

    if (monitorSrv_ && monitorTexW_ > 0 && monitorTexH_ > 0) {
        float maxW = 360.0f, maxH = 760.0f;
        float scaleW = maxW / (float)monitorTexW_;
        float scaleH = maxH / (float)monitorTexH_;
        float scale = std::min(scaleW, scaleH);
        float dispW = monitorTexW_ * scale;
        float dispH = monitorTexH_ * scale;
        ImGui::Image((ImTextureID)monitorSrv_, ImVec2(dispW, dispH));
    }

    if (ImGui::Button("Select Region##mon")) {
        HWND hwnd = GetForegroundWindow();
        SelectRegion(state_.monitorRegion, hwnd);
        if (state_.monitorRegion.width > 0) {
            cv::Mat img = CaptureScreen(state_.monitorRegion);
            monitorTexW_ = img.cols; monitorTexH_ = img.rows;
            UpdateTexture(&monitorSrv_, img);
        }
    }

    ImGui::Text("Region: %d, %d, %d x %d",
        state_.monitorRegion.x, state_.monitorRegion.y,
        state_.monitorRegion.width, state_.monitorRegion.height);

    ImGui::SliderFloat("Threshold", &state_.threshold, 0.01f, 0.5f);
    ImGui::SliderInt("Poll (ms)", &state_.pollIntervalMs, 100, 5000);
    ImGui::SliderInt("Idle Timeout (s)", &state_.noChangeTimeoutSec, 10, 300);

    if (!state_.monitorRunning) {
        if (ImGui::Button("Start Monitor") && state_.monitorRegion.width > 0)
            StartMonitor();
    } else {
        if (ImGui::Button("Stop Monitor"))
            StopMonitor();
    }

    ImGui::Text("Status: %s", state_.monitorRunning ? "Monitoring" : "Idle");
}

void App::RenderCommPanel() {
    ImGui::Text("Communication Zone");
    ImGui::Separator();

    ImGui::Text("Input Region (paste target):");
    if (commInputSrv_ && commInputTexW_ > 0) {
        float maxW = 360.0f, maxH = 200.0f;
        float s = std::min(maxW / commInputTexW_, maxH / commInputTexH_);
        ImGui::Image((ImTextureID)commInputSrv_, ImVec2(commInputTexW_ * s, commInputTexH_ * s));
    }
    if (ImGui::Button("Select Input Region")) {
        HWND hwnd = GetForegroundWindow();
        SelectRegion(state_.commInputRegion, hwnd);
        if (state_.commInputRegion.width > 0) {
            cv::Mat img = CaptureScreen(state_.commInputRegion);
            commInputTexW_ = img.cols; commInputTexH_ = img.rows;
            UpdateTexture(&commInputSrv_, img);
        }
    }
    ImGui::Text("  %d, %d, %d x %d",
        state_.commInputRegion.x, state_.commInputRegion.y,
        state_.commInputRegion.width, state_.commInputRegion.height);

    ImGui::Spacing();
    ImGui::Text("Chat Region (read replies):");
    if (commChatSrv_ && commChatTexW_ > 0) {
        float maxW = 360.0f, maxH = 400.0f;
        float s = std::min(maxW / (float)commChatTexW_, maxH / (float)commChatTexH_);
        ImGui::Image((ImTextureID)commChatSrv_, ImVec2(commChatTexW_ * s, commChatTexH_ * s));
    }
    if (ImGui::Button("Select Chat Region")) {
        HWND hwnd = GetForegroundWindow();
        SelectRegion(state_.commChatRegion, hwnd);
        if (state_.commChatRegion.width > 0) {
            cv::Mat img = CaptureScreen(state_.commChatRegion);
            commChatTexW_ = img.cols; commChatTexH_ = img.rows;
            UpdateTexture(&commChatSrv_, img);
        }
    }
    ImGui::Text("  %d, %d, %d x %d",
        state_.commChatRegion.x, state_.commChatRegion.y,
        state_.commChatRegion.width, state_.commChatRegion.height);

    ImGui::Spacing();
    if (ImGui::Button("Pick Click Point")) {
        HWND hwnd = GetForegroundWindow();
        ShowWindow(hwnd, SW_MINIMIZE);
        Sleep(300);
        // Wait for user to click a point
        while (!(GetAsyncKeyState(VK_LBUTTON) & 0x8000)) Sleep(10);
        GetCursorPos(&state_.commClickPoint);
        while (GetAsyncKeyState(VK_LBUTTON) & 0x8000) Sleep(10);
        ShowWindow(hwnd, SW_RESTORE);
        SetForegroundWindow(hwnd);
    }
    ImGui::SameLine();
    ImGui::Text("Click Point: (%ld, %ld)", state_.commClickPoint.x, state_.commClickPoint.y);

    ImGui::Spacing();
    if (!state_.commRunning) {
        if (ImGui::Button("Start Listen") && state_.commChatRegion.width > 0)
            StartComm();
    } else {
        if (ImGui::Button("Stop Listen"))
            StopComm();
    }

    ImGui::Text("Status: %s", state_.commRunning ? "Listening" : "Idle");

    ImGui::Separator();
    ImGui::Text("Received:");
    std::lock_guard<std::mutex> lk(logMutex_);
    for (int i = (int)commLog_.size() - 1; i >= 0 && i >= (int)commLog_.size() - 10; i--)
        ImGui::TextWrapped("%s", commLog_[i].c_str());
}

void App::RenderLogPanel() {
    ImGui::Text("Action Log");
    ImGui::Separator();
    std::lock_guard<std::mutex> lk(logMutex_);
    for (int i = (int)actionLog_.size() - 1; i >= 0 && i >= (int)actionLog_.size() - 8; i--)
        ImGui::TextWrapped("%s", actionLog_[i].c_str());
}

void App::StartMonitor() {
    monitorStop_ = false;
    state_.monitorRunning = true;
    monitorThread_ = std::thread(&App::MonitorThread, this);
}

void App::StopMonitor() {
    monitorStop_ = true;
    state_.monitorRunning = false;
    if (monitorThread_.joinable()) monitorThread_.join();
}

void App::StartComm() {
    if (commThread_.joinable()) {
        commStop_ = true;
        commThread_.join();
    }
    commStop_ = false;
    state_.commRunning = true;
    commThread_ = std::thread(&App::CommThread, this);
}

void App::StopComm() {
    commStop_ = true;
    state_.commRunning = false;
    if (commThread_.joinable()) commThread_.join();
}

void App::MonitorThread() {
    cv::Mat lastFrame;
    auto lastChangeTime = std::chrono::steady_clock::now();
    bool alerted = false;

    while (!monitorStop_) {
        cv::Mat current = CaptureScreen(state_.monitorRegion);
        if (current.empty()) { Sleep(state_.pollIntervalMs); continue; }

        { std::lock_guard<std::mutex> lk(frameMutex_); pendingMonitorFrame_ = current.clone(); }

        if (!lastFrame.empty()) {
            cv::Mat diff;
            cv::absdiff(lastFrame, current, diff);
            cv::Mat gray;
            cv::cvtColor(diff, gray, cv::COLOR_BGR2GRAY);
            double ratio = cv::countNonZero(gray > 30) / (double)gray.total();

            if (ratio > state_.threshold) {
                lastChangeTime = std::chrono::steady_clock::now();
                alerted = false;
                std::lock_guard<std::mutex> lk(logMutex_);
                monitorLog_.push_back("[" + TimeNow() + "] Change: " + std::to_string((int)(ratio * 100)) + "%");
            } else if (!alerted) {
                auto elapsed = std::chrono::steady_clock::now() - lastChangeTime;
                int elapsedSec = (int)std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
                if (elapsedSec >= state_.noChangeTimeoutSec) {
                    alerted = true;
                    {
                        std::lock_guard<std::mutex> lk(logMutex_);
                        monitorLog_.push_back("[" + TimeNow() + "] NO CHANGE " + std::to_string(elapsedSec) + "s - alert!");
                        actionLog_.push_back("[" + TimeNow() + "] ALERT: AI idle, sending screenshot + notification");
                    }
                    SendImageToComm(current);
                    Sleep(500);
                    SendToComm("AI stopped working - no change for " + std::to_string(elapsedSec) + "s");
                }
            }
        }
        lastFrame = current.clone();
        Sleep(state_.pollIntervalMs);
    }
}

void App::SendImageToComm(const cv::Mat& image) {
    if (state_.commInputRegion.width <= 0 || image.empty()) return;

    // Copy image to clipboard as CF_DIB
    cv::Mat bgra;
    cv::cvtColor(image, bgra, cv::COLOR_BGR2BGRA);
    // Flip vertically for DIB format
    cv::flip(bgra, bgra, 0);

    BITMAPINFOHEADER bi = {};
    bi.biSize = sizeof(bi);
    bi.biWidth = bgra.cols;
    bi.biHeight = bgra.rows;
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;
    bi.biSizeImage = bgra.total() * 4;

    if (OpenClipboard(nullptr)) {
        EmptyClipboard();
        HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, sizeof(bi) + bi.biSizeImage);
        if (hGlobal) {
            char* ptr = (char*)GlobalLock(hGlobal);
            memcpy(ptr, &bi, sizeof(bi));
            memcpy(ptr + sizeof(bi), bgra.data, bi.biSizeImage);
            GlobalUnlock(hGlobal);
            SetClipboardData(CF_DIB, hGlobal);
        }
        CloseClipboard();
    }

    // Click input region, paste image, send
    int cx = state_.commInputRegion.x + state_.commInputRegion.width / 2;
    int cy = state_.commInputRegion.y + state_.commInputRegion.height / 2;
    SetCursorPos(cx, cy);
    Sleep(100);
    mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
    mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
    Sleep(200);

    keybd_event(VK_CONTROL, 0, 0, 0);
    keybd_event('V', 0, 0, 0);
    keybd_event('V', 0, KEYEVENTF_KEYUP, 0);
    keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
    Sleep(500);

    keybd_event(VK_RETURN, 0, 0, 0);
    keybd_event(VK_RETURN, 0, KEYEVENTF_KEYUP, 0);

    {
        std::lock_guard<std::mutex> lk(logMutex_);
        actionLog_.push_back("[" + TimeNow() + "] Sent screenshot to comm");
    }
}

void App::SendToComm(const std::string& text) {
    if (state_.commInputRegion.width <= 0) return;

    // Store pending for log
    {
        std::lock_guard<std::mutex> lk(sendMutex_);
        pendingSend_ = text;
    }

    // Copy text to clipboard
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

    // Click center of input region to focus it
    int cx = state_.commInputRegion.x + state_.commInputRegion.width / 2;
    int cy = state_.commInputRegion.y + state_.commInputRegion.height / 2;
    {
        std::lock_guard<std::mutex> lk(logMutex_);
        actionLog_.push_back("[" + TimeNow() + "] Click input at (" + std::to_string(cx) + "," + std::to_string(cy) + ")");
    }
    SetCursorPos(cx, cy);
    Sleep(100);
    mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
    mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
    Sleep(200);

    // Ctrl+V paste
    keybd_event(VK_CONTROL, 0, 0, 0);
    keybd_event('V', 0, 0, 0);
    keybd_event('V', 0, KEYEVENTF_KEYUP, 0);
    keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
    Sleep(300);

    // Enter to send
    keybd_event(VK_RETURN, 0, 0, 0);
    keybd_event(VK_RETURN, 0, KEYEVENTF_KEYUP, 0);

    {
        std::lock_guard<std::mutex> lk(logMutex_);
        actionLog_.push_back("[" + TimeNow() + "] Sent: \"" + text + "\"");
    }

    // Suppress the next CommThread read cycle — the chat change was caused by us
    {
        std::lock_guard<std::mutex> lk(sendMutex_);
        suppressNextRead_ = true;
    }

    // Auto-start listening for reply
    if (!state_.commRunning && state_.commChatRegion.width > 0) {
        StartComm();
        std::lock_guard<std::mutex> lk(logMutex_);
        actionLog_.push_back("[" + TimeNow() + "] Started listening for reply");
    }
}

void App::CommThread() {
    cv::Mat lastFrame;
    std::string lastReadText;

    while (!commStop_) {
        if (state_.commChatRegion.width <= 0) { Sleep(state_.pollIntervalMs); continue; }

        {
            std::lock_guard<std::mutex> lk(sendMutex_);
            if (suppressNextRead_) {
                suppressNextRead_ = false;
                lastFrame = cv::Mat();
                Sleep(state_.pollIntervalMs);
                continue;
            }
        }

        cv::Mat current = CaptureScreen(state_.commChatRegion);
        if (current.empty()) { Sleep(state_.pollIntervalMs); continue; }

        bool changed = false;
        if (!lastFrame.empty()) {
            cv::Mat diff;
            cv::absdiff(lastFrame, current, diff);
            cv::Mat gray;
            cv::cvtColor(diff, gray, cv::COLOR_BGR2GRAY);
            double ratio = cv::countNonZero(gray > 30) / (double)gray.total();
            changed = ratio > 0.02;
        }
        lastFrame = current.clone();

        if (changed && state_.commClickPoint.x != 0 && state_.commClickPoint.y != 0) {
            { std::lock_guard<std::mutex> lk(frameMutex_); pendingChatFrame_ = current.clone(); }

            Sleep(500);
            std::string text = ReadChatAtPosition(state_.commClickPoint.x, state_.commClickPoint.y);
            if (!text.empty() && text != lastReadText) {
                lastReadText = text;
                {
                    std::lock_guard<std::mutex> lk(logMutex_);
                    commLog_.push_back("[" + TimeNow() + "] " + text);
                    actionLog_.push_back("[" + TimeNow() + "] Comm: copied text, stopping listen");
                }
                // TODO: parse/process message here in the future
                commStop_ = true;
                state_.commRunning = false;
                return;
            }
        }
        Sleep(state_.pollIntervalMs);
    }
}

std::string App::ReadChatByClipboard() {
    return ReadChatAtPosition(
        state_.commChatRegion.x + state_.commChatRegion.width / 2,
        state_.commChatRegion.y + state_.commChatRegion.height / 2);
}

std::string App::ReadChatAtPosition(int absX, int absY) {
    // Clear clipboard
    if (OpenClipboard(nullptr)) {
        EmptyClipboard();
        CloseClipboard();
    }
    Sleep(50);

    // Double-click at the specified position
    SetCursorPos(absX, absY);
    Sleep(50);
    mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
    mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
    Sleep(50);
    mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
    mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
    Sleep(100);

    // Ctrl+C copy
    keybd_event(VK_CONTROL, 0, 0, 0);
    keybd_event('C', 0, 0, 0);
    keybd_event('C', 0, KEYEVENTF_KEYUP, 0);
    keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
    Sleep(200);

    // Read clipboard
    std::string result;
    if (OpenClipboard(nullptr)) {
        HANDLE hData = GetClipboardData(CF_UNICODETEXT);
        if (hData) {
            wchar_t* wtext = (wchar_t*)GlobalLock(hData);
            if (wtext) result = WideToUtf8(wtext);
            GlobalUnlock(hData);
        }
        CloseClipboard();
    }
    return result;
}

// Command format: "CMD:action:params"
// CMD:click:x,y    - click at position (relative to monitor region)
// CMD:type:text    - type text
// CMD:enter        - press enter
// CMD:stop         - stop monitoring
Command App::ParseCommand(const std::string& text) {
    Command cmd;
    // Find last CMD: in the text (latest message)
    size_t pos = text.rfind("CMD:");
    if (pos == std::string::npos) return cmd;

    std::string line = text.substr(pos + 4);
    // Trim to end of line
    size_t nl = line.find_first_of("\r\n");
    if (nl != std::string::npos) line = line.substr(0, nl);

    size_t colon = line.find(':');
    if (colon != std::string::npos) {
        cmd.action = line.substr(0, colon);
        cmd.params = line.substr(colon + 1);
    } else {
        cmd.action = line;
    }
    return cmd;
}

void App::ExecuteCommand(const Command& cmd) {
    {
        std::lock_guard<std::mutex> lk(logMutex_);
        actionLog_.push_back("[" + TimeNow() + "] Exec: " + cmd.action + ":" + cmd.params);
    }

    if (cmd.action == "click") {
        int x = 0, y = 0;
        if (sscanf(cmd.params.c_str(), "%d,%d", &x, &y) == 2) {
            int absX = state_.monitorRegion.x + x;
            int absY = state_.monitorRegion.y + y;
            SetCursorPos(absX, absY);
            Sleep(50);
            mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
            mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
        }
    } else if (cmd.action == "type") {
        for (char c : cmd.params) {
            SHORT vk = VkKeyScanA(c);
            if (vk == -1) continue;
            BYTE key = LOBYTE(vk);
            bool shift = (HIBYTE(vk) & 1) != 0;
            if (shift) keybd_event(VK_SHIFT, 0, 0, 0);
            keybd_event(key, 0, 0, 0);
            keybd_event(key, 0, KEYEVENTF_KEYUP, 0);
            if (shift) keybd_event(VK_SHIFT, 0, KEYEVENTF_KEYUP, 0);
            Sleep(20);
        }
    } else if (cmd.action == "enter") {
        keybd_event(VK_RETURN, 0, 0, 0);
        keybd_event(VK_RETURN, 0, KEYEVENTF_KEYUP, 0);
    } else if (cmd.action == "stop") {
        StopMonitor();
    }
}
