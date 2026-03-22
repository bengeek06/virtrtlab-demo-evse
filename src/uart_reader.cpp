#include "uart_reader.hpp"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <stdexcept>
#include <termios.h>
#include <unistd.h>

namespace evse {

// ---------------------------------------------------------------------------
// UartReader (abstract base)
// ---------------------------------------------------------------------------

UartReader::UartReader(int channel_id, std::size_t buf_capacity)
    : channel_id_(channel_id), buffer_(buf_capacity) {}

UartReader::~UartReader() { stop(); }

void UartReader::start(std::atomic<bool>& stop_flag) {
    on_start();
    thread_ = std::thread([this, &stop_flag] { run(stop_flag); });
}

void UartReader::stop() {
    if (thread_.joinable()) {
        thread_.join();
    }
    on_stop();
}

CircularBuffer& UartReader::buffer() { return buffer_; }
const CircularBuffer& UartReader::buffer() const { return buffer_; }

void UartReader::run(std::atomic<bool>& stop_flag) {
    constexpr std::size_t kChunkSize = 64;
    uint8_t               chunk[kChunkSize];

    while (!stop_flag.load(std::memory_order_relaxed)) {
        int n = read_bytes(chunk, kChunkSize);
        if (n > 0) {
            for (int i = 0; i < n; ++i) {
                buffer_.push(chunk[i]);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// FdUartReader
// ---------------------------------------------------------------------------

FdUartReader::FdUartReader(int channel_id, std::string device_path,
                           std::size_t buf_capacity)
    : UartReader(channel_id, buf_capacity),
      device_path_(std::move(device_path)) {}

FdUartReader::~FdUartReader() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

void FdUartReader::on_start() {
    fd_ = ::open(device_path_.c_str(), O_RDONLY | O_NOCTTY | O_NONBLOCK);
    if (fd_ < 0) {
        std::cerr << "[uart" << channel_id() << "] open(" << device_path_
                  << "): " << std::strerror(errno) << "\n";
    }
}

void FdUartReader::on_stop() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

int FdUartReader::read_bytes(uint8_t* dst, std::size_t max_bytes) {
    if (fd_ < 0) {
        struct timespec ts {0, 10'000'000};  // 10 ms
        nanosleep(&ts, nullptr);
        return 0;
    }

    // Use select for a non-blocking read with a short timeout.
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(fd_, &rfds);
    struct timeval tv {0, 10'000};  // 10 ms
    int ret = ::select(fd_ + 1, &rfds, nullptr, nullptr, &tv);
    if (ret <= 0) {
        return 0;
    }

    ssize_t n = ::read(fd_, dst, max_bytes);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
        std::cerr << "[uart" << channel_id() << "] read error: "
                  << std::strerror(errno) << "\n";
        return -1;
    }
    return static_cast<int>(n);
}

} // namespace evse
