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
enable_testing()
find_package(GTest REQUIRED)

add_subdirectory(tests)
