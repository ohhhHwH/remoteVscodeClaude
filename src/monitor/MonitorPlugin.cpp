#define NOMINMAX
#include "monitor/IScreenCapture.h"
#include "monitor/IRegionMonitor.h"
#include "core/IPlugin.h"
#include <opencv2/imgproc.hpp>
#include <unordered_map>

class ScreenCapture : public IScreenCapture {
public:
    cv::Mat CaptureRegion(const Rect& rect) override {
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

    cv::Mat CaptureWindow(HWND hwnd) override {
        RECT rc;
        GetWindowRect(hwnd, &rc);
        Rect rect{rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top};
        return CaptureRegion(rect);
    }
};

struct RegionState {
    Rect rect;
    double threshold;
    cv::Mat lastFrame;
};

class RegionMonitor : public IRegionMonitor {
public:
    RegionMonitor() : capture_(nullptr) {}
    void SetCapture(IScreenCapture* cap) { capture_ = cap; }

    void AddRegion(const std::string& id, const Rect& rect, double threshold) override {
        regions_[id] = {rect, threshold, cv::Mat()};
    }

    void RemoveRegion(const std::string& id) override {
        regions_.erase(id);
    }

    bool CheckChange(const std::string& id) override {
        auto it = regions_.find(id);
        if (it == regions_.end() || !capture_) return false;

        auto& state = it->second;
        cv::Mat current = capture_->CaptureRegion(state.rect);

        if (state.lastFrame.empty()) {
            state.lastFrame = current.clone();
            return false;
        }

        cv::Mat diff;
        cv::absdiff(state.lastFrame, current, diff);
        cv::cvtColor(diff, diff, cv::COLOR_BGR2GRAY);
        double changeRatio = cv::countNonZero(diff > 30) / (double)diff.total();

        bool changed = changeRatio > state.threshold;
        state.lastFrame = current.clone();
        return changed;
    }

    cv::Mat GetSnapshot(const std::string& id) override {
        auto it = regions_.find(id);
        if (it == regions_.end() || !capture_) return cv::Mat();
        return capture_->CaptureRegion(it->second.rect);
    }

private:
    IScreenCapture* capture_;
    std::unordered_map<std::string, RegionState> regions_;
};

class MonitorPlugin : public IPlugin {
public:
    bool Init(const PluginConfig& config) override { return true; }
    void Shutdown() override {}
    std::string GetName() const override { return "MonitorPlugin"; }
    std::string GetVersion() const override { return "0.1.0"; }

    ScreenCapture& GetCapture() { return capture_; }
    RegionMonitor& GetMonitor() { return monitor_; }

private:
    ScreenCapture capture_;
    RegionMonitor monitor_;
};

EXPORT_PLUGIN(MonitorPlugin)
