#include "charging_fsm.hpp"
#include <stdexcept>

namespace evse {

const char* to_string(ChargingState s) {
    switch (s) {
        case ChargingState::BOOT:            return "BOOT";
        case ChargingState::IDLE:            return "IDLE";
        case ChargingState::WAIT_AUTH:       return "WAIT_AUTH";
        case ChargingState::PRECHARGE_CHECK: return "PRECHARGE_CHECK";
        case ChargingState::CHARGING:        return "CHARGING";
        case ChargingState::STOPPING:        return "STOPPING";
        case ChargingState::FAULT:           return "FAULT";
        default:                             return "UNKNOWN";
    }
}

const char* to_string(ChargingEvent e) {
    switch (e) {
        case ChargingEvent::PLUG_IN:        return "PLUG_IN";
        case ChargingEvent::PLUG_OUT:       return "PLUG_OUT";
        case ChargingEvent::AUTH_OK:        return "AUTH_OK";
        case ChargingEvent::AUTH_DENY:      return "AUTH_DENY";
        case ChargingEvent::PRECHARGE_OK:   return "PRECHARGE_OK";
        case ChargingEvent::PRECHARGE_FAIL: return "PRECHARGE_FAIL";
        case ChargingEvent::CHARGING_DONE:  return "CHARGING_DONE";
        case ChargingEvent::ESTOP:          return "ESTOP";
        case ChargingEvent::FAULT_RESET:    return "FAULT_RESET";
        case ChargingEvent::COMMS_LOST:     return "COMMS_LOST";
        case ChargingEvent::BOOT_DONE:      return "BOOT_DONE";
        default:                            return "UNKNOWN";
    }
}

ChargingFSM::ChargingFSM() = default;

bool ChargingFSM::process(ChargingEvent ev) {
    std::lock_guard<std::mutex> lk(mtx_);
    ChargingState next = state_;

    switch (state_) {
        case ChargingState::BOOT:
            if (ev == ChargingEvent::BOOT_DONE) next = ChargingState::IDLE;
            break;

        case ChargingState::IDLE:
            if (ev == ChargingEvent::PLUG_IN)   next = ChargingState::WAIT_AUTH;
            if (ev == ChargingEvent::ESTOP)      next = ChargingState::FAULT;
            break;

        case ChargingState::WAIT_AUTH:
            if (ev == ChargingEvent::AUTH_OK)    next = ChargingState::PRECHARGE_CHECK;
            if (ev == ChargingEvent::AUTH_DENY)  next = ChargingState::IDLE;
            if (ev == ChargingEvent::PLUG_OUT)   next = ChargingState::IDLE;
            if (ev == ChargingEvent::ESTOP)      next = ChargingState::FAULT;
            break;

        case ChargingState::PRECHARGE_CHECK:
            if (ev == ChargingEvent::PRECHARGE_OK)   next = ChargingState::CHARGING;
            if (ev == ChargingEvent::PRECHARGE_FAIL) next = ChargingState::FAULT;
            if (ev == ChargingEvent::ESTOP)           next = ChargingState::FAULT;
            if (ev == ChargingEvent::PLUG_OUT)        next = ChargingState::IDLE;
            break;

        case ChargingState::CHARGING:
            if (ev == ChargingEvent::CHARGING_DONE) next = ChargingState::STOPPING;
            if (ev == ChargingEvent::PLUG_OUT)      next = ChargingState::STOPPING;
            if (ev == ChargingEvent::ESTOP)         next = ChargingState::FAULT;
            if (ev == ChargingEvent::COMMS_LOST)    next = ChargingState::FAULT;
            break;

        case ChargingState::STOPPING:
            if (ev == ChargingEvent::PLUG_OUT)  next = ChargingState::IDLE;
            if (ev == ChargingEvent::ESTOP)     next = ChargingState::FAULT;
            break;

        case ChargingState::FAULT:
            if (ev == ChargingEvent::FAULT_RESET) next = ChargingState::IDLE;
            break;
    }

    if (next != state_) {
        state_ = next;
        return true;
    }
    return false;
}

ChargingState ChargingFSM::state() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return state_;
}

bool ChargingFSM::is_faulted() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return state_ == ChargingState::FAULT;
}

} // namespace evse
