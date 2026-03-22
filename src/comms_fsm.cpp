#include "comms_fsm.hpp"

namespace evse {

const char* to_string(CommsState s) {
    switch (s) {
        case CommsState::COMMS_OK:       return "COMMS_OK";
        case CommsState::COMMS_DEGRADED: return "COMMS_DEGRADED";
        case CommsState::COMMS_LOST:     return "COMMS_LOST";
        default:                         return "UNKNOWN";
    }
}

const char* to_string(CommsEvent e) {
    switch (e) {
        case CommsEvent::FRAME_OK:      return "FRAME_OK";
        case CommsEvent::FRAME_INVALID: return "FRAME_INVALID";
        case CommsEvent::TIMEOUT:       return "TIMEOUT";
        case CommsEvent::RECONNECTED:   return "RECONNECTED";
        default:                        return "UNKNOWN";
    }
}

CommsFSM::CommsFSM() = default;

bool CommsFSM::process(CommsEvent ev) {
    std::lock_guard<std::mutex> lk(mtx_);
    CommsState next = state_;

    if (ev == CommsEvent::FRAME_OK || ev == CommsEvent::RECONNECTED) {
        consecutive_failures_ = 0;
        next = CommsState::COMMS_OK;
    } else {
        ++consecutive_failures_;
        if (consecutive_failures_ >= kLostThreshold) {
            next = CommsState::COMMS_LOST;
        } else if (consecutive_failures_ >= kDegradedThreshold) {
            next = CommsState::COMMS_DEGRADED;
        }
    }

    if (next != state_) {
        state_ = next;
        return true;
    }
    return false;
}

CommsState CommsFSM::state() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return state_;
}

bool CommsFSM::is_lost() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return state_ == CommsState::COMMS_LOST;
}

} // namespace evse
