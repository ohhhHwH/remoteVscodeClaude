#include "TestFramework.h"
#include "core/EventBus.h"
#include <string>
#include <any>

TEST(EventBus_SubscribeAndPublish) {
    EventBus bus;
    int callCount = 0;
    std::string receivedSource;

    bus.Subscribe(EventType::REGION_CHANGED, [&](const Event& e) {
        callCount++;
        receivedSource = e.source;
    });

    Event evt{EventType::REGION_CHANGED, "monitor1", std::any{}};
    bus.Publish(evt);

    ASSERT_EQ(callCount, 1);
    ASSERT_EQ(receivedSource, "monitor1");
    return true;
}

TEST(EventBus_MultipleSubscribers) {
    EventBus bus;
    int count1 = 0, count2 = 0;

    bus.Subscribe(EventType::MESSAGE_RECEIVED, [&](const Event&) { count1++; });
    bus.Subscribe(EventType::MESSAGE_RECEIVED, [&](const Event&) { count2++; });

    Event evt{EventType::MESSAGE_RECEIVED, "comm", std::any{}};
    bus.Publish(evt);

    ASSERT_EQ(count1, 1);
    ASSERT_EQ(count2, 1);
    return true;
}

TEST(EventBus_DifferentEventTypes) {
    EventBus bus;
    int regionCount = 0, msgCount = 0;

    bus.Subscribe(EventType::REGION_CHANGED, [&](const Event&) { regionCount++; });
    bus.Subscribe(EventType::MESSAGE_RECEIVED, [&](const Event&) { msgCount++; });

    bus.Publish({EventType::REGION_CHANGED, "src", {}});

    ASSERT_EQ(regionCount, 1);
    ASSERT_EQ(msgCount, 0);
    return true;
}

TEST(EventBus_EventData) {
    EventBus bus;
    std::string received;

    bus.Subscribe(EventType::COMMAND_RECEIVED, [&](const Event& e) {
        received = std::any_cast<std::string>(e.data);
    });

    bus.Publish({EventType::COMMAND_RECEIVED, "user", std::string("click 100 200")});

    ASSERT_EQ(received, "click 100 200");
    return true;
}

TEST(EventBus_NoSubscribers) {
    EventBus bus;
    // 无订阅者时发布不应崩溃
    bus.Publish({EventType::SCREENSHOT_READY, "test", {}});
    ASSERT_TRUE(true);
    return true;
}

int main() {
    return RunAllTests();
}
