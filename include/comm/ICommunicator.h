#pragma once
#include <string>
#include <vector>

struct Message {
    std::string sender;
    std::string content;
    uint64_t timestamp;
};

class ICommunicator {
public:
    virtual ~ICommunicator() = default;
    virtual bool SendImage(const std::string& imagePath) = 0;
    virtual bool SendText(const std::string& text) = 0;
    virtual std::vector<Message> ReadMessages() = 0;
};
