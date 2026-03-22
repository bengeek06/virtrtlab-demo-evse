#include "gpio_watcher.hpp"
#include <cerrno>
#include <cstring>
#include <fstream>
#include <iostream>
#include <thread>

namespace evse {

GpioWatcher::GpioWatcher(EventQueue& event_queue) : event_queue_(event_queue) {}

GpioWatcher::~GpioWatcher() { stop(); }

void GpioWatcher::add_line(GpioLine line) {
    lines_.push_back(std::move(line));
}

void GpioWatcher::start(std::atomic<bool>& stop_flag,
                        std::chrono::milliseconds poll_interval) {
    thread_ = std::thread(
        [this, &stop_flag, poll_interval] { run(stop_flag, poll_interval); });
}

void GpioWatcher::stop() {
    if (thread_.joinable()) {
        thread_.join();
    }
}

void GpioWatcher::run(std::atomic<bool>& stop_flag,
                      std::chrono::milliseconds poll_interval) {
    // Remember previous values to detect edges.
    std::vector<int> prev(lines_.size(), -1);

    while (!stop_flag.load(std::memory_order_relaxed)) {
        for (std::size_t i = 0; i < lines_.size(); ++i) {
            int val = read_gpio_value(lines_[i].path);
            if (val < 0) continue;  // file not readable yet

            if (prev[i] != -1 && val != prev[i]) {
                EventType et = gpio_to_event(lines_[i].line_id, prev[i], val);
                event_queue_.push(Event{et, lines_[i].line_id});
            }
            prev[i] = val;
        }
        std::this_thread::sleep_for(poll_interval);
    }
}

int GpioWatcher::read_gpio_value(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return -1;
    int v = -1;
    f >> v;
    return v;
}

EventType GpioWatcher::gpio_to_event(int line_id, int /*old_val*/, int new_val) {
    // Rising edge (0→1) meanings per line:
    //   0: plug present   → PLUG_IN / PLUG_OUT
    //   1: user authorize → GPIO_USER_AUTH
    //   2: fault reset    → GPIO_FAULT_RESET
    //   3: emergency stop → GPIO_ESTOP
    switch (line_id) {
        case 0: return new_val ? EventType::GPIO_PLUG_IN : EventType::GPIO_PLUG_OUT;
        case 1: return EventType::GPIO_USER_AUTH;
        case 2: return EventType::GPIO_FAULT_RESET;
        case 3: return EventType::GPIO_ESTOP;
        default: return EventType::TIMEOUT;
    }
}

} // namespace evse
