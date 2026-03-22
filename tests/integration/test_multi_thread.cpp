// Integration tests: multi-threaded runtime behavior using mock UART/GPIO sources.
//
// These tests use a MockUartReader that plays back byte sequences without
// requiring real hardware, and exercise the Controller's thread coordination.

#include "charging_fsm.hpp"
#include "circular_buffer.hpp"
#include "comms_fsm.hpp"
#include "event_queue.hpp"
#include "safety_fsm.hpp"
#include "uart_reader.hpp"
#include <atomic>
#include <cstring>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

using namespace evse;

// ---------------------------------------------------------------------------
// MockUartReader: replays a pre-loaded byte sequence from memory.
// ---------------------------------------------------------------------------
class MockUartReader : public UartReader {
public:
    MockUartReader(int channel_id, std::vector<uint8_t> data,
                   std::size_t buf_cap = 512)
        : UartReader(channel_id, buf_cap),
          data_(std::move(data)),
          pos_(0) {}

protected:
    int read_bytes(uint8_t* dst, std::size_t max_bytes) override {
        if (pos_ >= data_.size()) {
            // Slow idle to avoid spinning.
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            return 0;
        }
        std::size_t n = std::min(max_bytes, data_.size() - pos_);
        std::memcpy(dst, data_.data() + pos_, n);
        pos_ += n;
        return static_cast<int>(n);
    }

private:
    std::vector<uint8_t> data_;
    std::size_t          pos_;
};

// ---------------------------------------------------------------------------
// Helper: build a newline-terminated frame.
// ---------------------------------------------------------------------------
static std::vector<uint8_t> make_frame(const std::string& payload) {
    std::vector<uint8_t> v(payload.begin(), payload.end());
    v.push_back('\n');
    return v;
}

// ---------------------------------------------------------------------------
// Test: UART reader pushes bytes into a circular buffer concurrently.
// ---------------------------------------------------------------------------
TEST(IntegrationTest, UartReaderFillsBuffer) {
    // Build a sequence of 10 valid frames.
    std::vector<uint8_t> data;
    for (int i = 0; i < 10; ++i) {
        auto f = make_frame("METER:0123456789");  // 16-char payload + '\n'
        data.insert(data.end(), f.begin(), f.end());
    }

    std::atomic<bool> stop{false};
    MockUartReader reader(0, data, 2048);
    reader.start(stop);

    // Wait for all data to be consumed by the reader thread.
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    stop.store(true);
    reader.stop();

    auto stats = reader.buffer().stats();
    EXPECT_EQ(stats.push_count, static_cast<uint64_t>(data.size()));
    EXPECT_EQ(stats.overrun_count, 0u);
}

// ---------------------------------------------------------------------------
// Test: Producer burst faster than consumer → observable overruns.
// ---------------------------------------------------------------------------
TEST(IntegrationTest, BufferOverrunObservable) {
    // 8-byte buffer with 64 bytes of data → 56 overruns.
    std::vector<uint8_t> data(64, 0xAA);
    std::atomic<bool> stop{false};
    MockUartReader reader(1, data, 8);
    reader.start(stop);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    stop.store(true);
    reader.stop();

    EXPECT_GT(reader.buffer().stats().overrun_count, 0u);
}

// ---------------------------------------------------------------------------
// Test: EventQueue producer/consumer across state machines.
// ---------------------------------------------------------------------------
TEST(IntegrationTest, EventQueueFSMDispatch) {
    EventQueue q(64);
    ChargingFSM charging;
    CommsFSM    comms;
    SafetyFSM   safety;

    charging.process(ChargingEvent::BOOT_DONE);  // IDLE

    std::atomic<bool> stop{false};

    // Producer: inject a scripted sequence of events.
    std::thread producer([&] {
        q.push(Event{EventType::GPIO_PLUG_IN, 0});
        q.push(Event{EventType::GPIO_USER_AUTH, 1});
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        q.push(Event{EventType::UART_FRAME_VALID, 0});
        q.push(Event{EventType::GPIO_ESTOP, 3});
        q.push(Event{EventType::SHUTDOWN, 0});
    });

    // Consumer: dispatch events to FSMs.
    std::thread consumer([&] {
        while (!stop.load()) {
            auto opt = q.wait_pop(std::chrono::milliseconds(50));
            if (!opt) continue;
            const Event& ev = *opt;
            switch (ev.type) {
                case EventType::GPIO_PLUG_IN:
                    charging.process(ChargingEvent::PLUG_IN); break;
                case EventType::GPIO_USER_AUTH:
                    charging.process(ChargingEvent::AUTH_OK); break;
                case EventType::UART_FRAME_VALID:
                    comms.process(CommsEvent::FRAME_OK); break;
                case EventType::GPIO_ESTOP:
                    safety.process(SafetyEvent::ESTOP);
                    charging.process(ChargingEvent::ESTOP); break;
                case EventType::SHUTDOWN:
                    stop.store(true); break;
                default: break;
            }
        }
    });

    producer.join();
    consumer.join();

    EXPECT_EQ(charging.state(), ChargingState::FAULT);
    EXPECT_EQ(safety.state(), SafetyState::TRIPPED);
    EXPECT_EQ(comms.state(), CommsState::COMMS_OK);
}

// ---------------------------------------------------------------------------
// Test: Emergency stop forces safe transition without deadlock.
// ---------------------------------------------------------------------------
TEST(IntegrationTest, EmergencyStopNeverDeadlocks) {
    ChargingFSM charging;
    SafetyFSM   safety;
    EventQueue  q(32);

    charging.process(ChargingEvent::BOOT_DONE);
    charging.process(ChargingEvent::PLUG_IN);
    charging.process(ChargingEvent::AUTH_OK);
    charging.process(ChargingEvent::PRECHARGE_OK);
    ASSERT_EQ(charging.state(), ChargingState::CHARGING);

    std::atomic<bool> done{false};
    std::thread t([&] {
        // Simulate ESTOP from another thread.
        q.push(Event{EventType::GPIO_ESTOP, 3});
        auto opt = q.wait_pop(std::chrono::milliseconds(100));
        if (opt && opt->type == EventType::GPIO_ESTOP) {
            safety.process(SafetyEvent::ESTOP);
            charging.process(ChargingEvent::ESTOP);
        }
        done.store(true);
    });

    t.join();

    EXPECT_TRUE(done.load());
    EXPECT_EQ(charging.state(), ChargingState::FAULT);
    EXPECT_TRUE(safety.is_unsafe());
}

// ---------------------------------------------------------------------------
// Test: Fragmented frames across buffer pops.
// ---------------------------------------------------------------------------
TEST(IntegrationTest, FragmentedFrameAccumulation) {
    // Push a frame byte-by-byte and decode manually.
    CircularBuffer buf(128);
    std::string    frame = "METER:volt=230\n";
    for (char c : frame) buf.push(static_cast<uint8_t>(c));

    std::vector<uint8_t> acc;
    int                  frames_decoded = 0;
    uint8_t              b;
    while (buf.pop(b)) {
        if (b == '\n') {
            if (acc.size() >= 4) ++frames_decoded;
            acc.clear();
        } else {
            acc.push_back(b);
        }
    }
    EXPECT_EQ(frames_decoded, 1);
}

// ---------------------------------------------------------------------------
// Test: Long idle followed by burst re-fills buffer correctly.
// ---------------------------------------------------------------------------
TEST(IntegrationTest, LongIdleThenBurst) {
    // Build a large burst of frames sent after a pause.
    std::vector<uint8_t> burst;
    for (int i = 0; i < 20; ++i) {
        auto f = make_frame("DATA:v=230;a=16;w=3680");
        burst.insert(burst.end(), f.begin(), f.end());
    }

    std::atomic<bool> stop{false};
    MockUartReader reader(2, burst, 4096);
    reader.start(stop);

    // Simulate idle by waiting a bit before any consumer activity.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Drain the buffer.
    uint8_t b;
    int     bytes_consumed = 0;
    while (reader.buffer().pop(b)) ++bytes_consumed;

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    stop.store(true);
    reader.stop();

    EXPECT_GT(bytes_consumed, 0);
    EXPECT_EQ(reader.buffer().stats().overrun_count, 0u);
}
