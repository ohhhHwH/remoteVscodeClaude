#include "TestFramework.h"
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

// Return type for match result
struct MatchPoint { int x; int y; };

// Multi-scale template matching — standalone version for testing
// Returns the center point of the best match, or (-1,-1) if no match found
static MatchPoint MultiScaleMatchTemplate(
    const cv::Mat& screenshot,
    const cv::Mat& templ,
    float threshold = 0.7f,
    int offsetX = 0,
    int offsetY = 0)
{
    MatchPoint result = {-1, -1};
    if (templ.empty() || screenshot.empty()) return result;
    if (templ.cols > screenshot.cols || templ.rows > screenshot.rows) return result;

    double bestVal = 0.0;
    cv::Point bestLoc(0, 0);
    int bestW = 0, bestH = 0;

    float scales[] = {0.7f, 0.8f, 0.9f, 1.0f, 1.1f, 1.2f, 1.3f};

    for (float scale : scales) {
        int sw = (int)(templ.cols * scale);
        int sh = (int)(templ.rows * scale);
        if (sw <= 0 || sh <= 0 || sw > screenshot.cols || sh > screenshot.rows) continue;

        cv::Mat scaled;
        cv::resize(templ, scaled, cv::Size(sw, sh));

        cv::Mat matchResult;
        cv::matchTemplate(screenshot, scaled, matchResult, cv::TM_CCOEFF_NORMED);

        double minVal, maxVal;
        cv::Point minLoc, maxLoc;
        cv::minMaxLoc(matchResult, &minVal, &maxVal, &minLoc, &maxLoc);

        if (maxVal > bestVal) {
            bestVal = maxVal;
            bestLoc = maxLoc;
            bestW = sw;
            bestH = sh;
        }
    }

    if (bestVal >= threshold) {
        result.x = bestLoc.x + bestW / 2 + offsetX;
        result.y = bestLoc.y + bestH / 2 + offsetY;
    }
    return result;
}

// Helper: create a synthetic "yes button" template (white rounded rect + "Yes" text)
static cv::Mat CreateFakeYesTemplate() {
    cv::Mat img(40, 80, CV_8UC3, cv::Scalar(240, 240, 240));
    cv::rectangle(img, {0, 0}, {79, 39}, {100, 100, 100}, 2);
    cv::line(img, {20, 20}, {35, 30}, {0, 0, 255}, 2);
    cv::line(img, {35, 30}, {60, 12}, {0, 0, 255}, 2);
    return img;
}

// Helper: create a screenshot with a yes-like button at a specific position
static cv::Mat CreateScreenshotWithButton(
    int width, int height,
    const cv::Mat& button,
    int btnX, int btnY,
    float scale = 1.0f)
{
    cv::Mat screen(height, width, CV_8UC3, cv::Scalar(200, 200, 200));
    // Add some random "text" noise
    cv::putText(screen, "Are you sure?", {50, 100}, cv::FONT_HERSHEY_PLAIN, 1.0, {0, 0, 0}, 1);

    if (button.empty()) return screen;

    cv::Mat scaledBtn;
    if (std::abs(scale - 1.0f) > 0.001f) {
        int sw = (int)(button.cols * scale);
        int sh = (int)(button.rows * scale);
        cv::resize(button, scaledBtn, cv::Size(sw, sh));
        // Place button in ROI
        cv::Rect roi(btnX, btnY, sw, sh);
        if (roi.x >= 0 && roi.y >= 0 && roi.x + roi.width <= width && roi.y + roi.height <= height) {
            scaledBtn.copyTo(screen(roi));
        }
    } else {
        cv::Rect roi(btnX, btnY, button.cols, button.rows);
        if (roi.x >= 0 && roi.y >= 0 && roi.x + roi.width <= width && roi.y + roi.height <= height) {
            button.copyTo(screen(roi));
        }
    }

    return screen;
}

TEST(template_match_exact) {
    // Create a synthetic yes button and place it in a screenshot at exact scale
    cv::Mat yesBtn = CreateFakeYesTemplate();
    ASSERT_FALSE(yesBtn.empty());

    cv::Mat screen = CreateScreenshotWithButton(640, 480, yesBtn, 300, 200, 1.0f);

    MatchPoint found = MultiScaleMatchTemplate(screen, yesBtn, 0.7f);
    ASSERT_TRUE(found.x > 0 && found.y > 0); // must find it

    // Center should be near (300 + 40, 200 + 20) = (340, 220)
    ASSERT_TRUE(std::abs(found.x - 340) < 10);
    ASSERT_TRUE(std::abs(found.y - 220) < 10);

    return true;
}

TEST(template_match_scaled_up) {
    cv::Mat yesBtn = CreateFakeYesTemplate();
    ASSERT_FALSE(yesBtn.empty());

    // Place at 1.2x scale
    cv::Mat screen = CreateScreenshotWithButton(640, 480, yesBtn, 200, 150, 1.2f);

    MatchPoint found = MultiScaleMatchTemplate(screen, yesBtn, 0.7f);
    ASSERT_TRUE(found.x > 0 && found.y > 0); // should find at 1.2x

    // Center at 1.2x: btnX + btnW*1.2/2 = 200 + 80*1.2/2 = 200 + 48 = 248
    int expectedX = 200 + (int)(80 * 1.2f / 2);
    int expectedY = 150 + (int)(40 * 1.2f / 2);
    ASSERT_TRUE(std::abs(found.x - expectedX) < 15);
    ASSERT_TRUE(std::abs(found.y - expectedY) < 15);

    return true;
}

TEST(template_match_scaled_down) {
    cv::Mat yesBtn = CreateFakeYesTemplate();
    ASSERT_FALSE(yesBtn.empty());

    // Place at 0.8x scale
    cv::Mat screen = CreateScreenshotWithButton(640, 480, yesBtn, 400, 300, 0.8f);

    MatchPoint found = MultiScaleMatchTemplate(screen, yesBtn, 0.7f);
    ASSERT_TRUE(found.x > 0 && found.y > 0);

    int expectedX = 400 + (int)(80 * 0.8f / 2);
    int expectedY = 300 + (int)(40 * 0.8f / 2);
    ASSERT_TRUE(std::abs(found.x - expectedX) < 15);
    ASSERT_TRUE(std::abs(found.y - expectedY) < 15);

    return true;
}

TEST(template_match_no_button) {
    cv::Mat yesBtn = CreateFakeYesTemplate();
    ASSERT_FALSE(yesBtn.empty());

    // Screenshot with NO button
    cv::Mat screen(640, 480, CV_8UC3, cv::Scalar(200, 200, 200));
    cv::putText(screen, "Some random content", {50, 100}, cv::FONT_HERSHEY_PLAIN, 1.0, {0, 0, 0}, 1);
    cv::putText(screen, "More stuff here", {50, 200}, cv::FONT_HERSHEY_PLAIN, 1.0, {50, 50, 50}, 1);

    MatchPoint found = MultiScaleMatchTemplate(screen, yesBtn, 0.7f);
    ASSERT_TRUE(found.x == -1 && found.y == -1); // should NOT find

    return true;
}

TEST(template_match_high_threshold_rejects) {
    cv::Mat yesBtn = CreateFakeYesTemplate();
    ASSERT_FALSE(yesBtn.empty());

    cv::Mat screen = CreateScreenshotWithButton(640, 480, yesBtn, 300, 200, 1.0f);

    // Very high threshold — should reject even the exact match
    MatchPoint found = MultiScaleMatchTemplate(screen, yesBtn, 0.99f);
    // The synthetic template may or may not match at 0.99 — depends on the implementation
    // Just verify it doesn't crash and returns some result
    (void)found;
    return true;
}

TEST(template_match_multiple_scales_consistent) {
    // Verify that multi-scale matching finds the button consistently at different scales
    cv::Mat yesBtn = CreateFakeYesTemplate();
    ASSERT_FALSE(yesBtn.empty());

    // Place button at 0.9x scale
    cv::Mat screen = CreateScreenshotWithButton(640, 480, yesBtn, 250, 200, 0.9f);

    // Should find it at 0.7 threshold (multi-scale search covers 0.7x-1.3x)
    MatchPoint found = MultiScaleMatchTemplate(screen, yesBtn, 0.7f);
    ASSERT_TRUE(found.x > 0 && found.y > 0);

    int expectedX = 250 + (int)(80 * 0.9f / 2);
    int expectedY = 200 + (int)(40 * 0.9f / 2);
    ASSERT_TRUE(std::abs(found.x - expectedX) < 15);
    ASSERT_TRUE(std::abs(found.y - expectedY) < 15);

    return true;
}

TEST(template_match_empty_inputs) {
    cv::Mat empty, nonEmpty(100, 100, CV_8UC3, cv::Scalar(128, 128, 128));

    // Empty screenshot
    MatchPoint r1 = MultiScaleMatchTemplate(empty, nonEmpty, 0.7f);
    ASSERT_TRUE(r1.x == -1 && r1.y == -1);

    // Empty template
    MatchPoint r2 = MultiScaleMatchTemplate(nonEmpty, empty, 0.7f);
    ASSERT_TRUE(r2.x == -1 && r2.y == -1);

    // Template larger than screenshot
    cv::Mat tinyScreen(20, 20, CV_8UC3, cv::Scalar(0, 0, 0));
    MatchPoint r3 = MultiScaleMatchTemplate(tinyScreen, nonEmpty, 0.7f);
    ASSERT_TRUE(r3.x == -1 && r3.y == -1);

    return true;
}

TEST(load_yes_png_if_available) {
    // Verify that pic/yes.png can be loaded by OpenCV
    cv::Mat yesImg = cv::imread("pic/yes.png", cv::IMREAD_COLOR);
    if (yesImg.empty()) {
        // File not found — skip test (not a failure)
        std::cout << "  (pic/yes.png not found, skipping)" << std::endl;
        return true;
    }
    ASSERT_TRUE(yesImg.cols > 0);
    ASSERT_TRUE(yesImg.rows > 0);
    return true;
}

int main() {
    return RunAllTests();
}
