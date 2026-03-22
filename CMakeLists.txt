cmake_minimum_required(VERSION 3.16)
project(virtrtlab_demo_evse CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Compiler warnings
# ---------------------------------------------------------------------------
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ---------------------------------------------------------------------------
# Core library (everything except main.cpp)
# ---------------------------------------------------------------------------
add_library(evse_core
    src/circular_buffer.cpp
    src/event_queue.cpp
    src/charging_fsm.cpp
    src/comms_fsm.cpp
    src/safety_fsm.cpp
    src/uart_reader.cpp
    src/gpio_watcher.cpp
    src/controller.cpp
)

target_include_directories(evse_core PUBLIC include)
target_link_libraries(evse_core PUBLIC pthread)

# ---------------------------------------------------------------------------
# Main executable
# ---------------------------------------------------------------------------
add_executable(evse src/main.cpp)
target_link_libraries(evse PRIVATE evse_core)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
include(CTest)

if(BUILD_TESTING)
    find_package(GTest QUIET CONFIG)

    if(NOT GTest_FOUND)
        find_package(GTest QUIET)
    endif()

    if(NOT GTest_FOUND)
        include(FetchContent)

        FetchContent_Declare(
            googletest
            URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        )
        FetchContent_MakeAvailable(googletest)
    endif()

    if(TARGET GTest::gtest AND NOT TARGET GTest::GTest)
        add_library(GTest::GTest INTERFACE IMPORTED)
        target_link_libraries(GTest::GTest INTERFACE GTest::gtest)
    endif()

    if(TARGET GTest::gtest_main AND NOT TARGET GTest::Main)
        add_library(GTest::Main INTERFACE IMPORTED)
        target_link_libraries(GTest::Main INTERFACE GTest::gtest_main)
    endif()

    add_subdirectory(tests)
endif()
