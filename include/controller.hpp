#pragma once
#include "charging_fsm.hpp"
#include "comms_fsm.hpp"
#include "event_queue.hpp"
#include "gpio_watcher.hpp"
#include "safety_fsm.hpp"
#include "uart_reader.hpp"
#include <atomic>
#include <memory>
#include <thread>
#include <vector>

namespace evse {

struct ControllerConfig {
    // UART device paths; must have exactly 3 entries (uart0, uart1, uart2).
    std::vector<std::string> uart_devices;
    // GPIO sysfs value-file paths; must have exactly 4 entries (gpio0..gpio3).
    std::vector<std::string> gpio_paths;
    // Buffer capacity per UART channel (bytes).
    std::size_t uart_buf_capacity{4096};
    // Event queue capacity (events).
    std::size_t event_queue_capacity{256};
    // GPIO poll interval.
    std::chrono::milliseconds gpio_poll_ms{10};
    // Watchdog / timeout check interval.
    std::chrono::milliseconds watchdog_interval_ms{500};
    // UART inactivity timeout before UART_TIMEOUT event.
    std::chrono::milliseconds uart_timeout_ms{2000};
};

// Main EVSE controller.
// Owns all sub-systems: UART readers, GPIO watcher, state machines, and the
// coordinator/watchdog threads.
class Controller {
public:
    explicit Controller(ControllerConfig cfg);
    ~Controller();

    // Start all threads. Does not block.
    void start();

    // Signal all threads to stop and join them. Safe to call multiple times.
    void stop();

    // Accessors for test inspection.
    ChargingState charging_state() const;
    CommsState    comms_state() const;
    SafetyState   safety_state() const;

    // Per-channel buffer stats.
    BufferStats uart_stats(int channel) const;

    EventQueue& event_queue() { return event_queue_; }

private:
    // Coordinator thread: dispatches events from the queue to state machines.
    void coordinator_loop();

    // Watchdog thread: injects UART_TIMEOUT events when channels go silent.
    void watchdog_loop();

    void dispatch_event(const Event& ev);

    ControllerConfig cfg_;
    std::atomic<bool> stop_flag_{false};

    EventQueue event_queue_;

    std::vector<std::unique_ptr<UartReader>> uart_readers_;
    std::unique_ptr<GpioWatcher>             gpio_watcher_;

    ChargingFSM charging_fsm_;
    CommsFSM    comms_fsm_;
    SafetyFSM   safety_fsm_;

    std::thread coordinator_thread_;
    std::thread watchdog_thread_;
};

} // namespace evse
