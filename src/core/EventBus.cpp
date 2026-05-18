#include "core/EventBus.h"

void EventBus::Subscribe(EventType type, EventCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    subscribers_[type].push_back(std::move(callback));
}

void EventBus::Publish(const Event& event) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = subscribers_.find(event.type);
    if (it != subscribers_.end()) {
        for (auto& cb : it->second) {
            cb(event);
        }
    }
}
