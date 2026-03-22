add_executable(unit_tests
    test_circular_buffer.cpp
    test_charging_fsm.cpp
    test_comms_fsm.cpp
    test_safety_fsm.cpp
    test_event_queue.cpp
)

target_link_libraries(unit_tests PRIVATE evse_core GTest::GTest GTest::Main)

include(GoogleTest)
gtest_discover_tests(unit_tests
    TEST_PREFIX "unit/"
    PROPERTIES LABELS "unit"
)
