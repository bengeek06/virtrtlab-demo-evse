#pragma once
#include "event_queue.hpp"
#include <atomic>
#include <string>
#include <thread>
#include <vector>

namespace evse {

// GPIO line descriptor: path to the sysfs value file (or a mock).
struct GpioLine {
    int         line_id;   // 0=plug, 1=auth, 2=fault_reset, 3=estop
    std::string path;      // e.g. "/sys/class/gpio/gpio17/value"
};

// Monitors GPIO lines by polling their sysfs value files.
// Edge changes are converted to EventType values and pushed to an EventQueue.
class GpioWatcher {
public:
    explicit GpioWatcher(EventQueue& event_queue);
    ~GpioWatcher();

    // Add a GPIO line to watch before calling start().
    void add_line(GpioLine line);

    // Start the watcher thread. poll_interval_ms controls how often to check.
    void start(std::atomic<bool>& stop_flag,
               std::chrono::milliseconds poll_interval =
                   std::chrono::milliseconds(10));

    void stop();

private:
    void run(std::atomic<bool>& stop_flag,
             std::chrono::milliseconds poll_interval);

    // Read an integer value from a sysfs file. Returns -1 on failure.
    static int read_gpio_value(const std::string& path);

    // Map GPIO line ID + value change → EventType.
    static EventType gpio_to_event(int line_id, int old_val, int new_val);

    EventQueue&           event_queue_;
    std::vector<GpioLine> lines_;
    std::thread           thread_;
};

} // namespace evse
