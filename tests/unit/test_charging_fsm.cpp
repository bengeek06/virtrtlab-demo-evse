#include "charging_fsm.hpp"
#include <gtest/gtest.h>

using namespace evse;

TEST(ChargingFSMTest, InitialStateIsBoot) {
    ChargingFSM fsm;
    EXPECT_EQ(fsm.state(), ChargingState::BOOT);
}

TEST(ChargingFSMTest, BootToIdle) {
    ChargingFSM fsm;
    EXPECT_TRUE(fsm.process(ChargingEvent::BOOT_DONE));
    EXPECT_EQ(fsm.state(), ChargingState::IDLE);
}

TEST(ChargingFSMTest, NominalHappyPath) {
    ChargingFSM fsm;
    fsm.process(ChargingEvent::BOOT_DONE);
    EXPECT_EQ(fsm.state(), ChargingState::IDLE);

    EXPECT_TRUE(fsm.process(ChargingEvent::PLUG_IN));
    EXPECT_EQ(fsm.state(), ChargingState::WAIT_AUTH);

    EXPECT_TRUE(fsm.process(ChargingEvent::AUTH_OK));
    EXPECT_EQ(fsm.state(), ChargingState::PRECHARGE_CHECK);

    EXPECT_TRUE(fsm.process(ChargingEvent::PRECHARGE_OK));
    EXPECT_EQ(fsm.state(), ChargingState::CHARGING);

    EXPECT_TRUE(fsm.process(ChargingEvent::CHARGING_DONE));
    EXPECT_EQ(fsm.state(), ChargingState::STOPPING);

    EXPECT_TRUE(fsm.process(ChargingEvent::PLUG_OUT));
    EXPECT_EQ(fsm.state(), ChargingState::IDLE);
}

TEST(ChargingFSMTest, AuthDenyReturnsToIdle) {
    ChargingFSM fsm;
    fsm.process(ChargingEvent::BOOT_DONE);
    fsm.process(ChargingEvent::PLUG_IN);
    EXPECT_TRUE(fsm.process(ChargingEvent::AUTH_DENY));
    EXPECT_EQ(fsm.state(), ChargingState::IDLE);
}

TEST(ChargingFSMTest, PrechargeFailGoesToFault) {
    ChargingFSM fsm;
    fsm.process(ChargingEvent::BOOT_DONE);
    fsm.process(ChargingEvent::PLUG_IN);
    fsm.process(ChargingEvent::AUTH_OK);
    EXPECT_TRUE(fsm.process(ChargingEvent::PRECHARGE_FAIL));
    EXPECT_EQ(fsm.state(), ChargingState::FAULT);
    EXPECT_TRUE(fsm.is_faulted());
}

TEST(ChargingFSMTest, EstopFromChargingGoesToFault) {
    ChargingFSM fsm;
    fsm.process(ChargingEvent::BOOT_DONE);
    fsm.process(ChargingEvent::PLUG_IN);
    fsm.process(ChargingEvent::AUTH_OK);
    fsm.process(ChargingEvent::PRECHARGE_OK);
    EXPECT_TRUE(fsm.process(ChargingEvent::ESTOP));
    EXPECT_EQ(fsm.state(), ChargingState::FAULT);
}

TEST(ChargingFSMTest, EstopFromIdleGoesToFault) {
    ChargingFSM fsm;
    fsm.process(ChargingEvent::BOOT_DONE);
    EXPECT_TRUE(fsm.process(ChargingEvent::ESTOP));
    EXPECT_EQ(fsm.state(), ChargingState::FAULT);
}

TEST(ChargingFSMTest, FaultResetReturnsToIdle) {
    ChargingFSM fsm;
    fsm.process(ChargingEvent::BOOT_DONE);
    fsm.process(ChargingEvent::ESTOP);
    EXPECT_EQ(fsm.state(), ChargingState::FAULT);
    EXPECT_TRUE(fsm.process(ChargingEvent::FAULT_RESET));
    EXPECT_EQ(fsm.state(), ChargingState::IDLE);
    EXPECT_FALSE(fsm.is_faulted());
}

TEST(ChargingFSMTest, CommsLostFromChargingGoesToFault) {
    ChargingFSM fsm;
    fsm.process(ChargingEvent::BOOT_DONE);
    fsm.process(ChargingEvent::PLUG_IN);
    fsm.process(ChargingEvent::AUTH_OK);
    fsm.process(ChargingEvent::PRECHARGE_OK);
    EXPECT_TRUE(fsm.process(ChargingEvent::COMMS_LOST));
    EXPECT_EQ(fsm.state(), ChargingState::FAULT);
}

TEST(ChargingFSMTest, IgnoredEventDoesNotTransition) {
    ChargingFSM fsm;
    fsm.process(ChargingEvent::BOOT_DONE);
    // AUTH_OK from IDLE has no effect.
    EXPECT_FALSE(fsm.process(ChargingEvent::AUTH_OK));
    EXPECT_EQ(fsm.state(), ChargingState::IDLE);
}

TEST(ChargingFSMTest, PlugOutFromWaitAuth) {
    ChargingFSM fsm;
    fsm.process(ChargingEvent::BOOT_DONE);
    fsm.process(ChargingEvent::PLUG_IN);
    EXPECT_TRUE(fsm.process(ChargingEvent::PLUG_OUT));
    EXPECT_EQ(fsm.state(), ChargingState::IDLE);
}

TEST(ChargingFSMTest, ToStringNotNull) {
    EXPECT_NE(to_string(ChargingState::CHARGING), nullptr);
    EXPECT_NE(to_string(ChargingEvent::ESTOP), nullptr);
}
