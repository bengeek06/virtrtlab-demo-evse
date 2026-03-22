#pragma once
#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <chrono>

namespace evse {

enum class EventType {
    // GPIO-driven
    GPIO_PLUG_IN,
    GPIO_PLUG_OUT,
    GPIO_USER_AUTH,
    GPIO_FAULT_RESET,
    GPIO_ESTOP,
    // UART-driven
    UART_FRAME_VALID,
    UART_FRAME_INVALID,
    UART_TIMEOUT,
    // Internal
    TIMEOUT,
    SHUTDOWN,
};

struct Event {
    EventType type;
    int       source_id{0};  // UART channel index or GPIO line index
};

// Thread-safe bounded event queue with condition-variable wake-up.
// Producers push(); consumers wait_pop() with an optional timeout.
class EventQueue {
public:
    explicit EventQueue(std::size_t max_size = 256);

    // Push an event. Drops the oldest if full.
    void push(Event ev);

    // Block until an event is available or the timeout expires.
    // Returns std::nullopt on timeout.
    std::optional<Event> wait_pop(std::chrono::milliseconds timeout =
                                      std::chrono::milliseconds(100));

    // Non-blocking pop; returns std::nullopt if empty.
    std::optional<Event> try_pop();

    // Unblock all waiters (call before stopping consumer threads).
    void notify_all();

    std::size_t size() const;
    bool        empty() const;

private:
    mutable std::mutex      mtx_;
    std::condition_variable cv_;
    std::deque<Event>       queue_;
    std::size_t             max_size_;
};

} // namespace evse
