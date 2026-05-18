#pragma once
#include <string>
#include <opencv2/core.hpp>

class IOCR {
public:
    virtual ~IOCR() = default;
    virtual std::string Recognize(const cv::Mat& image) = 0;
};
