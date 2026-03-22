#include "comms_fsm.hpp"
#include <gtest/gtest.h>

using namespace evse;

TEST(CommsFSMTest, InitialStateIsOk) {
    CommsFSM fsm;
    EXPECT_EQ(fsm.state(), CommsState::COMMS_OK);
    EXPECT_FALSE(fsm.is_lost());
}

TEST(CommsFSMTest, StaysOkOnGoodFrames) {
    CommsFSM fsm;
    for (int i = 0; i < 10; ++i) fsm.process(CommsEvent::FRAME_OK);
    EXPECT_EQ(fsm.state(), CommsState::COMMS_OK);
}

TEST(CommsFSMTest, DegradedAfterThreeFailures) {
    CommsFSM fsm;
    fsm.process(CommsEvent::FRAME_INVALID);
    fsm.process(CommsEvent::FRAME_INVALID);
    EXPECT_EQ(fsm.state(), CommsState::COMMS_OK);
    fsm.process(CommsEvent::FRAME_INVALID);
    EXPECT_EQ(fsm.state(), CommsState::COMMS_DEGRADED);
}

TEST(CommsFSMTest, LostAfterSixFailures) {
    CommsFSM fsm;
    for (int i = 0; i < 6; ++i) fsm.process(CommsEvent::FRAME_INVALID);
    EXPECT_EQ(fsm.state(), CommsState::COMMS_LOST);
    EXPECT_TRUE(fsm.is_lost());
}

TEST(CommsFSMTest, RecoveryOnGoodFrame) {
    CommsFSM fsm;
    for (int i = 0; i < 6; ++i) fsm.process(CommsEvent::FRAME_INVALID);
    EXPECT_EQ(fsm.state(), CommsState::COMMS_LOST);
    EXPECT_TRUE(fsm.process(CommsEvent::FRAME_OK));
    EXPECT_EQ(fsm.state(), CommsState::COMMS_OK);
    EXPECT_FALSE(fsm.is_lost());
}

TEST(CommsFSMTest, ReconnectedResetsState) {
    CommsFSM fsm;
    for (int i = 0; i < 4; ++i) fsm.process(CommsEvent::FRAME_INVALID);
    EXPECT_TRUE(fsm.process(CommsEvent::RECONNECTED));
    EXPECT_EQ(fsm.state(), CommsState::COMMS_OK);
}

TEST(CommsFSMTest, TimeoutCountsAsFailure) {
    CommsFSM fsm;
    for (int i = 0; i < 3; ++i) fsm.process(CommsEvent::TIMEOUT);
    EXPECT_EQ(fsm.state(), CommsState::COMMS_DEGRADED);
}

TEST(CommsFSMTest, ToStringNotNull) {
    EXPECT_NE(to_string(CommsState::COMMS_OK), nullptr);
    EXPECT_NE(to_string(CommsEvent::FRAME_OK), nullptr);
}
