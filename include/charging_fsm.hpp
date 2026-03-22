#pragma once
#include <mutex>
#include <string>

namespace evse {

// Charging session state machine.
// States: BOOT → IDLE ↔ WAIT_AUTH → PRECHARGE_CHECK → CHARGING → STOPPING → FAULT
enum class ChargingState {
    BOOT,
    IDLE,
    WAIT_AUTH,
    PRECHARGE_CHECK,
    CHARGING,
    STOPPING,
    FAULT,
};

enum class ChargingEvent {
    PLUG_IN,
    PLUG_OUT,
    AUTH_OK,
    AUTH_DENY,
    PRECHARGE_OK,
    PRECHARGE_FAIL,
    CHARGING_DONE,
    ESTOP,
    FAULT_RESET,
    COMMS_LOST,
    BOOT_DONE,
};

const char* to_string(ChargingState s);
const char* to_string(ChargingEvent e);

class ChargingFSM {
public:
    ChargingFSM();

    // Apply an event, possibly transitioning to a new state.
    // Returns true if a transition occurred.
    bool process(ChargingEvent ev);

    ChargingState state() const;

    // Fault path is latched; only FAULT_RESET clears it from FAULT.
    bool is_faulted() const;

private:
    mutable std::mutex mtx_;
    ChargingState      state_{ChargingState::BOOT};
};

} // namespace evse
