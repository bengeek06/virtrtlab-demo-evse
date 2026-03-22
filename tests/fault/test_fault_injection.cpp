// Fault-injection tests: validate that degraded-transport scenarios are
// observable and that state machines remain deterministic.

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

using namespace evse;

// ---------------------------------------------------------------------------
// MockUartReader re-used from integration tests.
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
// Test: UART byte-drop simulation – malformed frames increment invalid counter.
// ---------------------------------------------------------------------------
TEST(FaultTest, ByteDropMalformsFrame) {
    // A valid frame is ≥4 bytes before '\n'. Drop bytes to leave only 2.
    CircularBuffer buf(64);
    // Push a malformed frame: "AB\n" (only 2 chars before newline).
    buf.push('A');
    buf.push('B');
    buf.push('\n');

    CommsFSM comms;
    std::vector<uint8_t> acc;
    uint8_t b;
    while (buf.pop(b)) {
        if (b == '\n') {
            CommsEvent ev = acc.size() >= 4 ? CommsEvent::FRAME_OK
                                            : CommsEvent::FRAME_INVALID;
            comms.process(ev);
            acc.clear();
        } else {
            acc.push_back(b);
        }
    }
    EXPECT_EQ(comms.state(), CommsState::COMMS_OK);  // only 1 invalid, need 3
}

// ---------------------------------------------------------------------------
// Test: Repeated invalid frames degrade then lose comms.
// ---------------------------------------------------------------------------
TEST(FaultTest, RepeatedInvalidFramesDegradeComms) {
    CommsFSM comms;
    for (int i = 0; i < 3; ++i) comms.process(CommsEvent::FRAME_INVALID);
    EXPECT_EQ(comms.state(), CommsState::COMMS_DEGRADED);
    for (int i = 0; i < 3; ++i) comms.process(CommsEvent::FRAME_INVALID);
    EXPECT_EQ(comms.state(), CommsState::COMMS_LOST);
    EXPECT_TRUE(comms.is_lost());
}

// ---------------------------------------------------------------------------
// Test: UART silence (timeout events) drives comms to COMMS_LOST.
// ---------------------------------------------------------------------------
TEST(FaultTest, UartSilenceDrivesCommsLost) {
    CommsFSM comms;
    for (int i = 0; i < 6; ++i) comms.process(CommsEvent::TIMEOUT);
    EXPECT_EQ(comms.state(), CommsState::COMMS_LOST);
}

// ---------------------------------------------------------------------------
// Test: COMMS_LOST while charging forces FAULT state.
// ---------------------------------------------------------------------------
TEST(FaultTest, CommsLostWhileChargingCausesFault) {
    ChargingFSM charging;
    CommsFSM    comms;

    charging.process(ChargingEvent::BOOT_DONE);
    charging.process(ChargingEvent::PLUG_IN);
    charging.process(ChargingEvent::AUTH_OK);
    charging.process(ChargingEvent::PRECHARGE_OK);
    ASSERT_EQ(charging.state(), ChargingState::CHARGING);

    for (int i = 0; i < 6; ++i) comms.process(CommsEvent::FRAME_INVALID);
    ASSERT_TRUE(comms.is_lost());

    charging.process(ChargingEvent::COMMS_LOST);
    EXPECT_EQ(charging.state(), ChargingState::FAULT);
}

// ---------------------------------------------------------------------------
// Test: GPIO glitch (rapid ESTOP assert then release) latches safely.
// ---------------------------------------------------------------------------
TEST(FaultTest, GpioGlitchEstopLatches) {
    SafetyFSM safety;
    // Two rapid ESTOP events – should still only reach TRIPPED once.
    safety.process(SafetyEvent::ESTOP);
    safety.process(SafetyEvent::ESTOP);  // ignored
    EXPECT_EQ(safety.state(), SafetyState::TRIPPED);
    EXPECT_TRUE(safety.is_unsafe());
}

// ---------------------------------------------------------------------------
// Test: Combined fault – comms degradation AND ESTOP together.
// ---------------------------------------------------------------------------
TEST(FaultTest, CombinedTransportDegradationAndEstop) {
    ChargingFSM charging;
    CommsFSM    comms;
    SafetyFSM   safety;
    EventQueue  q(64);

    charging.process(ChargingEvent::BOOT_DONE);
    charging.process(ChargingEvent::PLUG_IN);
    charging.process(ChargingEvent::AUTH_OK);
    charging.process(ChargingEvent::PRECHARGE_OK);

    // Simulate degraded transport.
    q.push(Event{EventType::UART_FRAME_INVALID, 0});
    q.push(Event{EventType::UART_FRAME_INVALID, 0});
    q.push(Event{EventType::UART_FRAME_INVALID, 0});
    // Simultaneous ESTOP.
    q.push(Event{EventType::GPIO_ESTOP, 3});

    std::atomic<bool> stop{false};
    auto opt = q.wait_pop(std::chrono::milliseconds(10));
    while (opt) {
        switch (opt->type) {
            case EventType::UART_FRAME_INVALID:
                comms.process(CommsEvent::FRAME_INVALID);
                if (comms.is_lost()) charging.process(ChargingEvent::COMMS_LOST);
                break;
            case EventType::GPIO_ESTOP:
                safety.process(SafetyEvent::ESTOP);
                charging.process(ChargingEvent::ESTOP);
                break;
            default: break;
        }
        opt = q.try_pop();
    }

    EXPECT_EQ(charging.state(), ChargingState::FAULT);
    EXPECT_TRUE(safety.is_unsafe());
}

// ---------------------------------------------------------------------------
// Test: Producer burst fills buffer beyond capacity; overrun count asserted.
// ---------------------------------------------------------------------------
TEST(FaultTest, ProducerBurstOverrunAsserted) {
    CircularBuffer buf(32);
    constexpr int  kBurst = 128;
    for (int i = 0; i < kBurst; ++i) buf.push(static_cast<uint8_t>(i & 0xFF));
    EXPECT_EQ(buf.stats().overrun_count, static_cast<uint64_t>(kBurst - 32));
    EXPECT_EQ(buf.size(), 32u);
}

// ---------------------------------------------------------------------------
// Test: Recovery after comms loss restores charging capability.
// ---------------------------------------------------------------------------
TEST(FaultTest, CommsRecoveryAllowsRetryAfterFaultReset) {
    ChargingFSM charging;
    CommsFSM    comms;

    charging.process(ChargingEvent::BOOT_DONE);
    charging.process(ChargingEvent::PLUG_IN);
    charging.process(ChargingEvent::AUTH_OK);
    charging.process(ChargingEvent::PRECHARGE_OK);

    // Force COMMS_LOST and propagate to charging FSM.
    for (int i = 0; i < 6; ++i) comms.process(CommsEvent::FRAME_INVALID);
    charging.process(ChargingEvent::COMMS_LOST);
    ASSERT_EQ(charging.state(), ChargingState::FAULT);

    // Operator resets fault; comms recover.
    comms.process(CommsEvent::RECONNECTED);
    charging.process(ChargingEvent::FAULT_RESET);
    EXPECT_EQ(charging.state(), ChargingState::IDLE);
    EXPECT_EQ(comms.state(), CommsState::COMMS_OK);
}

// ---------------------------------------------------------------------------
// Test: Underrun during long idle period is observable.
// ---------------------------------------------------------------------------
TEST(FaultTest, LongIdleUnderrunObservable) {
    CircularBuffer buf(64);
    // Buffer is empty; consumer pops repeatedly.
    uint8_t b;
    for (int i = 0; i < 10; ++i) buf.pop(b);
    EXPECT_EQ(buf.stats().underrun_count, 10u);
}
