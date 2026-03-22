#pragma once
#include <mutex>

namespace evse {

// Communication health state machine.
// COMMS_OK → COMMS_DEGRADED → COMMS_LOST
enum class CommsState {
    COMMS_OK,
    COMMS_DEGRADED,
    COMMS_LOST,
};

enum class CommsEvent {
    FRAME_OK,
    FRAME_INVALID,
    TIMEOUT,
    RECONNECTED,
};

const char* to_string(CommsState s);
const char* to_string(CommsEvent e);

class CommsFSM {
public:
    CommsFSM();

    bool process(CommsEvent ev);

    CommsState state() const;

    // Returns true when communication has been lost.
    bool is_lost() const;

private:
    mutable std::mutex mtx_;
    CommsState         state_{CommsState::COMMS_OK};
    int                consecutive_failures_{0};

    static constexpr int kDegradedThreshold = 3;
    static constexpr int kLostThreshold     = 6;
};

} // namespace evse
