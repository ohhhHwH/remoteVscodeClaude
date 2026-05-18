#pragma once
#include <string>
#include <opencv2/core.hpp>
#include "IScreenCapture.h"

class IRegionMonitor {
public:
    virtual ~IRegionMonitor() = default;
    virtual void AddRegion(const std::string& id, const Rect& rect, double threshold) = 0;
    virtual void RemoveRegion(const std::string& id) = 0;
    virtual bool CheckChange(const std::string& id) = 0;
    virtual cv::Mat GetSnapshot(const std::string& id) = 0;
};
