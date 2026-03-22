#pragma once
#include <mutex>

namespace evse {

// Safety latch state machine.
// SAFE → TRIPPED → WAIT_RESET → SAFE
enum class SafetyState {
    SAFE,
    TRIPPED,
    WAIT_RESET,
};

enum class SafetyEvent {
    ESTOP,
    FAULT,
    RESET_ACK,
    CLEAR,
};

const char* to_string(SafetyState s);
const char* to_string(SafetyEvent e);

class SafetyFSM {
public:
    SafetyFSM();

    bool process(SafetyEvent ev);

    SafetyState state() const;

    // True when the latch is tripped or awaiting operator reset.
    bool is_unsafe() const;

private:
    mutable std::mutex mtx_;
    SafetyState        state_{SafetyState::SAFE};
};

} // namespace evse
