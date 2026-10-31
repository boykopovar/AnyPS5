#include "prx/libSceSystemLogger/include/Api.hpp"
#include <atomic>
#include <stdexcept>
#include <string>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {
std::atomic<bool> g_initialized{false};
}

extern "C" {

int APS5_VABI sceSystemLoggerInitialize() {
    bool expected = false;
    if (!g_initialized.compare_exchange_strong(expected, true)) throw std::logic_error(std::string(__func__) + ": already initialized");
    return 0;
}

int APS5_VABI sceSystemLoggerSetLogData() {
    if (!g_initialized.load()) throw std::logic_error(std::string(__func__) + ": not initialized");
    return 0;
}

int APS5_VABI sceSystemLoggerTerminate() {
    bool expected = true;
    if (!g_initialized.compare_exchange_strong(expected, false)) throw std::logic_error(std::string(__func__) + ": not initialized");
    return 0;
}

}
