add_executable(fault_tests
    test_fault_injection.cpp
)

target_link_libraries(fault_tests PRIVATE evse_core GTest::GTest GTest::Main)

include(GoogleTest)
gtest_discover_tests(fault_tests
    TEST_PREFIX "fault/"
    PROPERTIES LABELS "fault"
)
