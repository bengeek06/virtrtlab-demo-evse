#include "controller.hpp"
#include <csignal>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

static volatile std::sig_atomic_t g_signal = 0;

static void signal_handler(int sig) { g_signal = sig; }

static void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog
              << " <uart0> <uart1> <uart2>"
              << " <gpio0> <gpio1> <gpio2> <gpio3>\n"
              << "\n"
              << "  uart0  Metering module device (e.g. /dev/ttyUSB0)\n"
              << "  uart1  Payment module device  (e.g. /dev/ttyUSB1)\n"
              << "  uart2  Service console device (e.g. /dev/ttyUSB2)\n"
              << "  gpio0  Plug-present sysfs value file\n"
              << "  gpio1  User-authorize sysfs value file\n"
              << "  gpio2  Fault-reset sysfs value file\n"
              << "  gpio3  Emergency-stop sysfs value file\n";
}

int main(int argc, char* argv[]) {
    if (argc != 8) {
        print_usage(argv[0]);
        return 1;
    }

    std::vector<std::string> uarts = {argv[1], argv[2], argv[3]};
    std::vector<std::string> gpios = {argv[4], argv[5], argv[6], argv[7]};

    std::signal(SIGINT,  signal_handler);
    std::signal(SIGTERM, signal_handler);

    evse::ControllerConfig cfg;
    cfg.uart_devices = uarts;
    cfg.gpio_paths   = gpios;

    evse::Controller ctrl(cfg);
    ctrl.start();

    std::cout << "[main] EVSE controller running. Press Ctrl+C to stop.\n";

    while (g_signal == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "[main] Signal " << g_signal << " received, stopping…\n";
    ctrl.stop();
    std::cout << "[main] Stopped.\n";
    return 0;
}
