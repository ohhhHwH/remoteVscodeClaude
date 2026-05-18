#include "TestFramework.h"
#include "comm/ICommunicator.h"
#include "comm/IOCR.h"
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

// Mock OCR用于测试
class MockOCR : public IOCR {
public:
    std::string textToReturn;
    std::string Recognize(const cv::Mat& image) override {
        return textToReturn;
    }
};

// Mock Communicator用于测试接口
class MockCommunicator : public ICommunicator {
public:
    std::vector<std::string> sentTexts;
    std::vector<std::string> sentImages;
    std::vector<Message> messagesToReturn;

    bool SendImage(const std::string& imagePath) override {
        sentImages.push_back(imagePath);
        return true;
    }
    bool SendText(const std::string& text) override {
        sentTexts.push_back(text);
        return true;
    }
    std::vector<Message> ReadMessages() override {
        return messagesToReturn;
    }
};

TEST(Communicator_SendText) {
    MockCommunicator comm;
    ASSERT_TRUE(comm.SendText("hello"));
    ASSERT_EQ(comm.sentTexts.size(), 1u);
    ASSERT_EQ(comm.sentTexts[0], "hello");
    return true;
}

TEST(Communicator_SendImage) {
    MockCommunicator comm;
    ASSERT_TRUE(comm.SendImage("C:/test.png"));
    ASSERT_EQ(comm.sentImages.size(), 1u);
    ASSERT_EQ(comm.sentImages[0], "C:/test.png");
    return true;
}

TEST(Communicator_ReadMessages) {
    MockCommunicator comm;
    comm.messagesToReturn = {{"user1", "cmd1", 1000}, {"user2", "cmd2", 2000}};
    auto msgs = comm.ReadMessages();
    ASSERT_EQ(msgs.size(), 2u);
    ASSERT_EQ(msgs[0].sender, "user1");
    ASSERT_EQ(msgs[0].content, "cmd1");
    ASSERT_EQ(msgs[1].content, "cmd2");
    return true;
}

TEST(OCR_MockRecognize) {
    MockOCR ocr;
    ocr.textToReturn = "detected text";
    cv::Mat img = cv::Mat::zeros(50, 200, CV_8UC3);
    ASSERT_EQ(ocr.Recognize(img), "detected text");
    return true;
}

TEST(OCR_EmptyImage) {
    MockOCR ocr;
    ocr.textToReturn = "";
    cv::Mat img;
    ASSERT_EQ(ocr.Recognize(img), "");
    return true;
}

int main() {
    return RunAllTests();
}
