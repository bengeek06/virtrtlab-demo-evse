#include "event_queue.hpp"
#include <gtest/gtest.h>
#include <thread>

using namespace evse;

TEST(EventQueueTest, PushAndWaitPop) {
    EventQueue q;
    q.push(Event{EventType::GPIO_PLUG_IN, 0});
    auto opt = q.wait_pop(std::chrono::milliseconds(50));
    ASSERT_TRUE(opt.has_value());
    EXPECT_EQ(opt->type, EventType::GPIO_PLUG_IN);
}

TEST(EventQueueTest, WaitPopTimesOut) {
    EventQueue q;
    auto opt = q.wait_pop(std::chrono::milliseconds(20));
    EXPECT_FALSE(opt.has_value());
}

TEST(EventQueueTest, TryPopEmpty) {
    EventQueue q;
    EXPECT_FALSE(q.try_pop().has_value());
}

TEST(EventQueueTest, DropOldestWhenFull) {
    EventQueue q(3);
    q.push(Event{EventType::GPIO_PLUG_IN,  0});
    q.push(Event{EventType::GPIO_PLUG_OUT, 0});
    q.push(Event{EventType::GPIO_ESTOP,    0});
    // 4th push drops the oldest.
    q.push(Event{EventType::GPIO_USER_AUTH, 0});
    EXPECT_EQ(q.size(), 3u);

    auto first = q.try_pop();
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(first->type, EventType::GPIO_PLUG_OUT);  // GPIO_PLUG_IN was dropped
}

TEST(EventQueueTest, NotifyAllUnblocksWaiters) {
    EventQueue q;
    bool unblocked = false;
    std::thread t([&] {
        q.wait_pop(std::chrono::seconds(5));
        unblocked = true;
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    q.notify_all();
    t.join();
    EXPECT_TRUE(unblocked);
}

TEST(EventQueueTest, ConcurrentPushPop) {
    EventQueue q(512);
    constexpr int kN = 200;

    std::thread producer([&] {
        for (int i = 0; i < kN; ++i) {
            q.push(Event{EventType::UART_FRAME_VALID, i % 3});
        }
    });

    int count = 0;
    std::thread consumer([&] {
        while (count < kN) {
            auto opt = q.wait_pop(std::chrono::milliseconds(100));
            if (opt) ++count;
        }
    });

    producer.join();
    consumer.join();
    EXPECT_EQ(count, kN);
}
