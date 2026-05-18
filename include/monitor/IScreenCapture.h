#pragma once
#define NOMINMAX
#include <opencv2/core.hpp>
#include <windows.h>

struct Rect {
    int x, y, width, height;
};

class IScreenCapture {
public:
    virtual ~IScreenCapture() = default;
    virtual cv::Mat CaptureRegion(const Rect& rect) = 0;
    virtual cv::Mat CaptureWindow(HWND hwnd) = 0;
};
