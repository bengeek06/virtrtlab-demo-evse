#include "event_queue.hpp"

namespace evse {

EventQueue::EventQueue(std::size_t max_size) : max_size_(max_size) {}

void EventQueue::push(Event ev) {
    std::lock_guard<std::mutex> lk(mtx_);
    if (queue_.size() >= max_size_) {
        queue_.pop_front();  // drop oldest
    }
    queue_.push_back(ev);
    cv_.notify_one();
}

std::optional<Event> EventQueue::wait_pop(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lk(mtx_);
    if (cv_.wait_for(lk, timeout, [this] { return !queue_.empty(); })) {
        Event ev = queue_.front();
        queue_.pop_front();
        return ev;
    }
    return std::nullopt;
}

std::optional<Event> EventQueue::try_pop() {
    std::lock_guard<std::mutex> lk(mtx_);
    if (queue_.empty()) {
        return std::nullopt;
    }
    Event ev = queue_.front();
    queue_.pop_front();
    return ev;
}

void EventQueue::notify_all() {
    cv_.notify_all();
}

std::size_t EventQueue::size() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return queue_.size();
}

bool EventQueue::empty() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return queue_.empty();
}

} // namespace evse
