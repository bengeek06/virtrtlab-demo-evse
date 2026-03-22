#include "controller.hpp"
#include <iostream>

namespace evse {

// ---------------------------------------------------------------------------
// Concrete UartReader that also pushes UART_FRAME_VALID / UART_FRAME_INVALID
// events into the shared EventQueue when it detects complete frames.
// For this reference AUT we use a simple framing scheme: frames end with 0x0A
// ('\n'). The decoder runs inline in the coordinator thread which pops bytes
// from the buffer.
// ---------------------------------------------------------------------------

Controller::Controller(ControllerConfig cfg)
    : cfg_(std::move(cfg)),
      event_queue_(cfg_.event_queue_capacity) {
    // Create UART readers.
    for (int i = 0; i < static_cast<int>(cfg_.uart_devices.size()); ++i) {
        uart_readers_.push_back(std::make_unique<FdUartReader>(
            i, cfg_.uart_devices[i], cfg_.uart_buf_capacity));
    }

    // Create GPIO watcher.
    gpio_watcher_ = std::make_unique<GpioWatcher>(event_queue_);
    for (int i = 0; i < static_cast<int>(cfg_.gpio_paths.size()); ++i) {
        gpio_watcher_->add_line(GpioLine{i, cfg_.gpio_paths[i]});
    }
}

Controller::~Controller() { stop(); }

void Controller::start() {
    stop_flag_.store(false, std::memory_order_relaxed);

    for (auto& r : uart_readers_) {
        r->start(stop_flag_);
    }
    gpio_watcher_->start(stop_flag_, cfg_.gpio_poll_ms);

    // Boot the charging FSM.
    charging_fsm_.process(ChargingEvent::BOOT_DONE);

    coordinator_thread_ =
        std::thread([this] { coordinator_loop(); });
    watchdog_thread_ =
        std::thread([this] { watchdog_loop(); });
}

void Controller::stop() {
    stop_flag_.store(true, std::memory_order_relaxed);
    event_queue_.notify_all();

    if (coordinator_thread_.joinable()) coordinator_thread_.join();
    if (watchdog_thread_.joinable()) watchdog_thread_.join();

    gpio_watcher_->stop();
    for (auto& r : uart_readers_) {
        r->stop();
    }
}

ChargingState Controller::charging_state() const {
    return charging_fsm_.state();
}

CommsState Controller::comms_state() const {
    return comms_fsm_.state();
}

SafetyState Controller::safety_state() const {
    return safety_fsm_.state();
}

BufferStats Controller::uart_stats(int channel) const {
    return uart_readers_.at(static_cast<std::size_t>(channel))->buffer().stats();
}

// ---------------------------------------------------------------------------
// Coordinator thread
// ---------------------------------------------------------------------------

void Controller::coordinator_loop() {
    // Simple inline UART decoder: per-channel frame accumulator.
    std::vector<std::vector<uint8_t>> frame_bufs(uart_readers_.size());

    while (!stop_flag_.load(std::memory_order_relaxed)) {
        // Drain UART buffers and detect frames (newline-terminated).
        for (std::size_t i = 0; i < uart_readers_.size(); ++i) {
            uint8_t byte;
            while (uart_readers_[i]->buffer().pop(byte)) {
                if (byte == '\n') {
                    // Frame complete – basic validity: at least 4 bytes.
                    EventType et = frame_bufs[i].size() >= 4
                                       ? EventType::UART_FRAME_VALID
                                       : EventType::UART_FRAME_INVALID;
                    event_queue_.push(Event{et, static_cast<int>(i)});
                    frame_bufs[i].clear();
                } else {
                    frame_bufs[i].push_back(byte);
                    // Guard against runaway frame accumulation.
                    if (frame_bufs[i].size() > 512) {
                        frame_bufs[i].clear();
                        event_queue_.push(
                            Event{EventType::UART_FRAME_INVALID, static_cast<int>(i)});
                    }
                }
            }
        }

        // Process one event from the queue (10 ms wait).
        auto opt = event_queue_.wait_pop(std::chrono::milliseconds(10));
        if (!opt) continue;

        dispatch_event(*opt);
    }
}

// ---------------------------------------------------------------------------
// Watchdog thread
// ---------------------------------------------------------------------------

void Controller::watchdog_loop() {
    // Track the last time we received a valid frame per UART channel.
    std::vector<std::chrono::steady_clock::time_point> last_frame(
        uart_readers_.size(), std::chrono::steady_clock::now());

    while (!stop_flag_.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_for(cfg_.watchdog_interval_ms);

        auto now = std::chrono::steady_clock::now();
        for (std::size_t i = 0; i < uart_readers_.size(); ++i) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - last_frame[i]);
            if (elapsed >= cfg_.uart_timeout_ms) {
                event_queue_.push(
                    Event{EventType::UART_TIMEOUT, static_cast<int>(i)});
                last_frame[i] = now;  // reset so we don't spam
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Event dispatcher
// ---------------------------------------------------------------------------

void Controller::dispatch_event(const Event& ev) {
    switch (ev.type) {
        case EventType::GPIO_PLUG_IN:
            std::cout << "[ctrl] GPIO_PLUG_IN\n";
            charging_fsm_.process(ChargingEvent::PLUG_IN);
            break;

        case EventType::GPIO_PLUG_OUT:
            std::cout << "[ctrl] GPIO_PLUG_OUT\n";
            charging_fsm_.process(ChargingEvent::PLUG_OUT);
            break;

        case EventType::GPIO_USER_AUTH:
            std::cout << "[ctrl] GPIO_USER_AUTH\n";
            charging_fsm_.process(ChargingEvent::AUTH_OK);
            break;

        case EventType::GPIO_FAULT_RESET:
            std::cout << "[ctrl] GPIO_FAULT_RESET\n";
            charging_fsm_.process(ChargingEvent::FAULT_RESET);
            safety_fsm_.process(SafetyEvent::RESET_ACK);
            break;

        case EventType::GPIO_ESTOP:
            std::cout << "[ctrl] GPIO_ESTOP\n";
            safety_fsm_.process(SafetyEvent::ESTOP);
            charging_fsm_.process(ChargingEvent::ESTOP);
            break;

        case EventType::UART_FRAME_VALID:
            comms_fsm_.process(CommsEvent::FRAME_OK);
            break;

        case EventType::UART_FRAME_INVALID:
            comms_fsm_.process(CommsEvent::FRAME_INVALID);
            if (comms_fsm_.is_lost()) {
                charging_fsm_.process(ChargingEvent::COMMS_LOST);
            }
            break;

        case EventType::UART_TIMEOUT:
            std::cout << "[ctrl] UART_TIMEOUT ch=" << ev.source_id << "\n";
            comms_fsm_.process(CommsEvent::TIMEOUT);
            if (comms_fsm_.is_lost()) {
                charging_fsm_.process(ChargingEvent::COMMS_LOST);
            }
            break;

        case EventType::SHUTDOWN:
            stop_flag_.store(true, std::memory_order_relaxed);
            break;

        default:
            break;
    }

    std::cout << "[ctrl] charging=" << to_string(charging_fsm_.state())
              << " comms=" << to_string(comms_fsm_.state())
              << " safety=" << to_string(safety_fsm_.state()) << "\n";
}

} // namespace evse
