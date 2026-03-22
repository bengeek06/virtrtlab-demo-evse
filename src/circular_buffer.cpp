#include "circular_buffer.hpp"
#include <stdexcept>

namespace evse {

CircularBuffer::CircularBuffer(std::size_t capacity)
    : buf_(capacity), capacity_(capacity) {
    if (capacity == 0) {
        throw std::invalid_argument("CircularBuffer: capacity must be > 0");
    }
}

void CircularBuffer::push(uint8_t byte) {
    std::lock_guard<std::mutex> lk(mtx_);
    if (count_ == capacity_) {
        // Drop oldest (advance tail).
        tail_ = (tail_ + 1) % capacity_;
        --count_;
        ++stats_.overrun_count;
    }
    buf_[head_] = byte;
    head_ = (head_ + 1) % capacity_;
    ++count_;
    ++stats_.push_count;
    if (count_ > stats_.high_watermark) {
        stats_.high_watermark = count_;
    }
}

bool CircularBuffer::pop(uint8_t& out) {
    std::lock_guard<std::mutex> lk(mtx_);
    if (count_ == 0) {
        ++stats_.underrun_count;
        return false;
    }
    out = buf_[tail_];
    tail_ = (tail_ + 1) % capacity_;
    --count_;
    ++stats_.pop_count;
    return true;
}

std::size_t CircularBuffer::size() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return count_;
}

bool CircularBuffer::empty() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return count_ == 0;
}

bool CircularBuffer::full() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return count_ == capacity_;
}

BufferStats CircularBuffer::stats() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return stats_;
}

void CircularBuffer::reset_stats() {
    std::lock_guard<std::mutex> lk(mtx_);
    stats_ = BufferStats{};
}

} // namespace evse
