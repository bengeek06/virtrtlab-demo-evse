#pragma once
#include "circular_buffer.hpp"
#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <thread>

namespace evse {

// Abstract UART channel reader.
// Runs one thread that reads bytes from a source and pushes them into a
// bounded circular buffer. The concrete source is provided by the subclass
// via read_bytes().
class UartReader {
public:
    // channel_id : 0=metering, 1=payment, 2=service
    explicit UartReader(int channel_id, std::size_t buf_capacity = 4096);
    virtual ~UartReader();

    // Start the reader thread.
    void start(std::atomic<bool>& stop_flag);

    // Signal the reader to stop and join its thread.
    void stop();

    CircularBuffer& buffer();
    const CircularBuffer& buffer() const;

    int channel_id() const { return channel_id_; }

protected:
    // Subclasses implement this to read up to max_bytes bytes.
    // Returns number of bytes read, 0 on timeout, -1 on error.
    virtual int read_bytes(uint8_t* dst, std::size_t max_bytes) = 0;

    // Subclasses can override to perform setup before the loop starts.
    virtual void on_start() {}
    // Subclasses can override to perform cleanup after the loop exits.
    virtual void on_stop() {}

private:
    void run(std::atomic<bool>& stop_flag);

    int            channel_id_;
    CircularBuffer buffer_;
    std::thread    thread_;
};

// A UART reader that reads from a POSIX file descriptor (e.g. /dev/ttyUSB0).
class FdUartReader : public UartReader {
public:
    FdUartReader(int channel_id, std::string device_path,
                 std::size_t buf_capacity = 4096);
    ~FdUartReader() override;

protected:
    int read_bytes(uint8_t* dst, std::size_t max_bytes) override;
    void on_start() override;
    void on_stop() override;

private:
    std::string device_path_;
    int         fd_{-1};
};

} // namespace evse
