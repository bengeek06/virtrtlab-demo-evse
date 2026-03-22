#include "safety_fsm.hpp"

namespace evse {

const char* to_string(SafetyState s) {
    switch (s) {
        case SafetyState::SAFE:       return "SAFE";
        case SafetyState::TRIPPED:    return "TRIPPED";
        case SafetyState::WAIT_RESET: return "WAIT_RESET";
        default:                      return "UNKNOWN";
    }
}

const char* to_string(SafetyEvent e) {
    switch (e) {
        case SafetyEvent::ESTOP:     return "ESTOP";
        case SafetyEvent::FAULT:     return "FAULT";
        case SafetyEvent::RESET_ACK: return "RESET_ACK";
        case SafetyEvent::CLEAR:     return "CLEAR";
        default:                     return "UNKNOWN";
    }
}

SafetyFSM::SafetyFSM() = default;

bool SafetyFSM::process(SafetyEvent ev) {
    std::lock_guard<std::mutex> lk(mtx_);
    SafetyState next = state_;

    switch (state_) {
        case SafetyState::SAFE:
            if (ev == SafetyEvent::ESTOP || ev == SafetyEvent::FAULT)
                next = SafetyState::TRIPPED;
            break;

        case SafetyState::TRIPPED:
            if (ev == SafetyEvent::RESET_ACK)
                next = SafetyState::WAIT_RESET;
            break;

        case SafetyState::WAIT_RESET:
            if (ev == SafetyEvent::CLEAR)
                next = SafetyState::SAFE;
            break;
    }

    if (next != state_) {
        state_ = next;
        return true;
    }
    return false;
}

SafetyState SafetyFSM::state() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return state_;
}

bool SafetyFSM::is_unsafe() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return state_ != SafetyState::SAFE;
}

} // namespace evse
