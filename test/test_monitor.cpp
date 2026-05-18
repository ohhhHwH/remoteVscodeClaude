#define NOMINMAX
#include "TestFramework.h"
#include "monitor/IScreenCapture.h"
#include "monitor/IRegionMonitor.h"
#include <opencv2/imgproc.hpp>
#include <unordered_map>

class MockScreenCapture : public IScreenCapture {
public:
    cv::Mat frameToReturn;
    cv::Mat CaptureRegion(const Rect& rect) override { return frameToReturn.clone(); }
    cv::Mat CaptureWindow(HWND hwnd) override { return frameToReturn.clone(); }
};

class TestableRegionMonitor : public IRegionMonitor {
public:
    TestableRegionMonitor(IScreenCapture* capture) : capture_(capture) {}

    void AddRegion(const std::string& id, const Rect& rect, double threshold) override {
        regions_[id] = {rect, threshold, cv::Mat()};
    }
    void RemoveRegion(const std::string& id) override { regions_.erase(id); }

    bool CheckChange(const std::string& id) override {
        auto it = regions_.find(id);
        if (it == regions_.end()) return false;
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
        if (it == regions_.end()) return cv::Mat();
        return capture_->CaptureRegion(it->second.rect);
    }

private:
    struct State { Rect rect; double threshold; cv::Mat lastFrame; };
    IScreenCapture* capture_;
    std::unordered_map<std::string, State> regions_;
};

TEST(RegionMonitor_AddRemove) {
    MockScreenCapture mock;
    mock.frameToReturn = cv::Mat::zeros(100, 100, CV_8UC3);
    TestableRegionMonitor monitor(&mock);

    monitor.AddRegion("r1", {0, 0, 100, 100}, 0.05);
    auto snap = monitor.GetSnapshot("r1");
    ASSERT_FALSE(snap.empty());

    monitor.RemoveRegion("r1");
    snap = monitor.GetSnapshot("r1");
    ASSERT_TRUE(snap.empty());
    return true;
}

TEST(RegionMonitor_FirstCheckNoChange) {
    MockScreenCapture mock;
    mock.frameToReturn = cv::Mat::zeros(100, 100, CV_8UC3);
    TestableRegionMonitor monitor(&mock);

    monitor.AddRegion("r1", {0, 0, 100, 100}, 0.05);
    ASSERT_FALSE(monitor.CheckChange("r1"));
    return true;
}

TEST(RegionMonitor_DetectChange) {
    MockScreenCapture mock;
    mock.frameToReturn = cv::Mat::zeros(100, 100, CV_8UC3);
    TestableRegionMonitor monitor(&mock);

    monitor.AddRegion("r1", {0, 0, 100, 100}, 0.05);
    monitor.CheckChange("r1");

    mock.frameToReturn = cv::Mat(100, 100, CV_8UC3, cv::Scalar(255, 255, 255));
    ASSERT_TRUE(monitor.CheckChange("r1"));
    return true;
}

TEST(RegionMonitor_NoChangeWhenSame) {
    MockScreenCapture mock;
    mock.frameToReturn = cv::Mat(100, 100, CV_8UC3, cv::Scalar(50, 50, 50));
    TestableRegionMonitor monitor(&mock);

    monitor.AddRegion("r1", {0, 0, 100, 100}, 0.05);
    monitor.CheckChange("r1");
    ASSERT_FALSE(monitor.CheckChange("r1"));
    return true;
}

TEST(RegionMonitor_ThresholdControl) {
    MockScreenCapture mock;
    mock.frameToReturn = cv::Mat::zeros(100, 100, CV_8UC3);
    TestableRegionMonitor monitor(&mock);

    monitor.AddRegion("r1", {0, 0, 100, 100}, 0.9);
    monitor.CheckChange("r1");

    mock.frameToReturn = cv::Mat::zeros(100, 100, CV_8UC3);
    cv::rectangle(mock.frameToReturn, cv::Point(0, 0), cv::Point(10, 10), cv::Scalar(255, 255, 255), -1);
    ASSERT_FALSE(monitor.CheckChange("r1"));
    return true;
}

TEST(RegionMonitor_NonexistentRegion) {
    MockScreenCapture mock;
    mock.frameToReturn = cv::Mat::zeros(100, 100, CV_8UC3);
    TestableRegionMonitor monitor(&mock);

    ASSERT_FALSE(monitor.CheckChange("nonexistent"));
    ASSERT_TRUE(monitor.GetSnapshot("nonexistent").empty());
    return true;
}

int main() {
    return RunAllTests();
}
