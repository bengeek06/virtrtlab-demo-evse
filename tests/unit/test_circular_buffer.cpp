#include "circular_buffer.hpp"
#include <gtest/gtest.h>
#include <thread>
#include <vector>

using namespace evse;

TEST(CircularBufferTest, BasicPushPop) {
    CircularBuffer buf(8);
    buf.push(0xAB);
    buf.push(0xCD);

    uint8_t b;
    EXPECT_TRUE(buf.pop(b));
    EXPECT_EQ(b, 0xAB);
    EXPECT_TRUE(buf.pop(b));
    EXPECT_EQ(b, 0xCD);
    EXPECT_FALSE(buf.pop(b));
}

TEST(CircularBufferTest, EmptyUnderrun) {
    CircularBuffer buf(4);
    uint8_t b;
    EXPECT_FALSE(buf.pop(b));
    EXPECT_EQ(buf.stats().underrun_count, 1u);
}

TEST(CircularBufferTest, FullOverrunDropsOldest) {
    CircularBuffer buf(3);
    buf.push(1);
    buf.push(2);
    buf.push(3);
    EXPECT_TRUE(buf.full());

    // Push into a full buffer should drop '1'.
    buf.push(4);
    EXPECT_EQ(buf.stats().overrun_count, 1u);

    uint8_t b;
    EXPECT_TRUE(buf.pop(b));
    EXPECT_EQ(b, 2);  // '1' was dropped
    EXPECT_TRUE(buf.pop(b));
    EXPECT_EQ(b, 3);
    EXPECT_TRUE(buf.pop(b));
    EXPECT_EQ(b, 4);
    EXPECT_TRUE(buf.empty());
}

TEST(CircularBufferTest, HighWatermark) {
    CircularBuffer buf(16);
    for (int i = 0; i < 10; ++i) buf.push(static_cast<uint8_t>(i));
    EXPECT_EQ(buf.stats().high_watermark, 10u);

    uint8_t b;
    buf.pop(b);
    // Watermark should not decrease.
    EXPECT_EQ(buf.stats().high_watermark, 10u);
}

TEST(CircularBufferTest, PushPopCounts) {
    CircularBuffer buf(8);
    buf.push(1);
    buf.push(2);
    uint8_t b;
    buf.pop(b);
    EXPECT_EQ(buf.stats().push_count, 2u);
    EXPECT_EQ(buf.stats().pop_count, 1u);
}

TEST(CircularBufferTest, WrapAround) {
    CircularBuffer buf(4);
    buf.push(10);
    buf.push(20);
    uint8_t b;
    buf.pop(b);
    buf.pop(b);
    buf.push(30);
    buf.push(40);
    buf.push(50);
    EXPECT_EQ(buf.size(), 3u);

    buf.pop(b); EXPECT_EQ(b, 30);
    buf.pop(b); EXPECT_EQ(b, 40);
    buf.pop(b); EXPECT_EQ(b, 50);
}

TEST(CircularBufferTest, ResetStats) {
    CircularBuffer buf(4);
    buf.push(1); buf.push(2); buf.push(3); buf.push(4); buf.push(5);
    EXPECT_GT(buf.stats().overrun_count, 0u);
    buf.reset_stats();
    EXPECT_EQ(buf.stats().overrun_count, 0u);
    EXPECT_EQ(buf.stats().push_count, 0u);
}

TEST(CircularBufferTest, ConcurrentProducerConsumer) {
    // Use a buffer large enough that no overruns occur so the consumer
    // can pop exactly kItems bytes.
    const int kItems = 1000;
    CircularBuffer buf(static_cast<std::size_t>(kItems) * 2);

    std::atomic<bool> producer_done{false};
    std::thread producer([&] {
        for (int i = 0; i < kItems; ++i) {
            buf.push(static_cast<uint8_t>(i & 0xFF));
        }
        producer_done.store(true, std::memory_order_release);
    });

    int popped = 0;
    std::thread consumer([&] {
        uint8_t b;
        while (popped < kItems) {
            if (buf.pop(b)) {
                ++popped;
            } else if (producer_done.load(std::memory_order_acquire) &&
                       buf.empty()) {
                break;  // producer finished and buffer empty – stop spinning
            }
        }
    });

    producer.join();
    consumer.join();

    EXPECT_EQ(popped, kItems);
    EXPECT_EQ(buf.stats().push_count, static_cast<uint64_t>(kItems));
    EXPECT_EQ(buf.stats().overrun_count, 0u);
}

TEST(CircularBufferTest, BurstFasterThanConsumer) {
    CircularBuffer buf(16);
    // Producer burst of 64 bytes into a 16-byte buffer: expect overruns.
    for (int i = 0; i < 64; ++i) buf.push(static_cast<uint8_t>(i));
    EXPECT_EQ(buf.stats().overrun_count, 48u);  // 64 - 16 drops
    EXPECT_EQ(buf.size(), 16u);
}
