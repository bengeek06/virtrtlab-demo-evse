#pragma once
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

namespace evse {

struct BufferStats {
    uint64_t push_count{0};
    uint64_t pop_count{0};
    uint64_t overrun_count{0};
    uint64_t underrun_count{0};
    std::size_t high_watermark{0};
};

// Bounded circular byte buffer.
// On full: drops the oldest byte (overrun).
// On empty pop: returns false (underrun counted).
// All operations are mutex-protected and safe from multiple threads.
class CircularBuffer {
public:
    explicit CircularBuffer(std::size_t capacity);

    // Push one byte. If full, drops oldest byte and increments overrun_count.
    void push(uint8_t byte);

    // Pop one byte into out. Returns false (and increments underrun_count) if empty.
    bool pop(uint8_t& out);

    // Peek at the number of bytes currently available.
    std::size_t size() const;

    bool empty() const;
    bool full() const;

    BufferStats stats() const;

    void reset_stats();

private:
    mutable std::mutex mtx_;
    std::vector<uint8_t> buf_;
    std::size_t capacity_;
    std::size_t head_{0};  // next write position
    std::size_t tail_{0};  // next read position
    std::size_t count_{0};
    BufferStats stats_;
};

} // namespace evse
