#include "safety_fsm.hpp"
#include <gtest/gtest.h>

using namespace evse;

TEST(SafetyFSMTest, InitialStateIsSafe) {
    SafetyFSM fsm;
    EXPECT_EQ(fsm.state(), SafetyState::SAFE);
    EXPECT_FALSE(fsm.is_unsafe());
}

TEST(SafetyFSMTest, EstopTripsLatch) {
    SafetyFSM fsm;
    EXPECT_TRUE(fsm.process(SafetyEvent::ESTOP));
    EXPECT_EQ(fsm.state(), SafetyState::TRIPPED);
    EXPECT_TRUE(fsm.is_unsafe());
}

TEST(SafetyFSMTest, FaultTripsLatch) {
    SafetyFSM fsm;
    EXPECT_TRUE(fsm.process(SafetyEvent::FAULT));
    EXPECT_EQ(fsm.state(), SafetyState::TRIPPED);
    EXPECT_TRUE(fsm.is_unsafe());
}

TEST(SafetyFSMTest, ResetAckMovesToWaitReset) {
    SafetyFSM fsm;
    fsm.process(SafetyEvent::ESTOP);
    EXPECT_TRUE(fsm.process(SafetyEvent::RESET_ACK));
    EXPECT_EQ(fsm.state(), SafetyState::WAIT_RESET);
    EXPECT_TRUE(fsm.is_unsafe());
}

TEST(SafetyFSMTest, ClearFromWaitResetRestoresSafe) {
    SafetyFSM fsm;
    fsm.process(SafetyEvent::ESTOP);
    fsm.process(SafetyEvent::RESET_ACK);
    EXPECT_TRUE(fsm.process(SafetyEvent::CLEAR));
    EXPECT_EQ(fsm.state(), SafetyState::SAFE);
    EXPECT_FALSE(fsm.is_unsafe());
}

TEST(SafetyFSMTest, ClearIgnoredWhenTripped) {
    SafetyFSM fsm;
    fsm.process(SafetyEvent::ESTOP);
    // CLEAR does not apply from TRIPPED (must go through WAIT_RESET first).
    EXPECT_FALSE(fsm.process(SafetyEvent::CLEAR));
    EXPECT_EQ(fsm.state(), SafetyState::TRIPPED);
}

TEST(SafetyFSMTest, MultipleEstopDoesNotChange) {
    SafetyFSM fsm;
    fsm.process(SafetyEvent::ESTOP);
    EXPECT_FALSE(fsm.process(SafetyEvent::ESTOP));  // already tripped
    EXPECT_EQ(fsm.state(), SafetyState::TRIPPED);
}

TEST(SafetyFSMTest, ToStringNotNull) {
    EXPECT_NE(to_string(SafetyState::SAFE), nullptr);
    EXPECT_NE(to_string(SafetyEvent::ESTOP), nullptr);
}
