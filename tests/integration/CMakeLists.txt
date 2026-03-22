add_executable(integration_tests
    test_multi_thread.cpp
)

target_link_libraries(integration_tests PRIVATE evse_core GTest::GTest GTest::Main)

include(GoogleTest)
gtest_discover_tests(integration_tests
    TEST_PREFIX "integration/"
    PROPERTIES LABELS "integration"
)
