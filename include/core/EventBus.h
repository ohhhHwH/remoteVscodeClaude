#pragma once
#include <string>
#include <functional>
#include <vector>
#include <any>
#include <mutex>
#include <unordered_map>

enum class EventType {
    REGION_CHANGED,
    MESSAGE_RECEIVED,
    COMMAND_RECEIVED,
    SCREENSHOT_READY
};

struct Event {
    EventType type;
    std::string source;
    std::any data;
};

using EventCallback = std::function<void(const Event&)>;

struct EventTypeHash {
    size_t operator()(EventType t) const { return static_cast<size_t>(t); }
};

class EventBus {
public:
    void Subscribe(EventType type, EventCallback callback);
    void Publish(const Event& event);

private:
    std::mutex mutex_;
    std::unordered_map<EventType, std::vector<EventCallback>, EventTypeHash> subscribers_;
};
