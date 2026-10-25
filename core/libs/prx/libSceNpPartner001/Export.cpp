#include "prx/libSceNpPartner001/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <atomic>
#include <stdexcept>
#include <string>

namespace {
std::atomic<bool> g_initialized{false};
}

extern "C" {

int APS5_VABI sceNpEAAccessInitialize(void) {
    bool expected = false;
    if (!g_initialized.compare_exchange_strong(expected, true)) throw std::logic_error(std::string(__func__) + ": already initialized");
    return 0;
}

int APS5_VABI sceNpEAAccessTerminate(void) {
    bool expected = true;
    if (!g_initialized.compare_exchange_strong(expected, false)) throw std::logic_error(std::string(__func__) + ": not initialized");
    return 0;
}

int APS5_VABI sceNpHasEAAccessSubscription(int userId, bool* result) {
    (void)userId;
    if (!g_initialized.load()) throw std::logic_error(std::string(__func__) + ": not initialized");
    if (result == nullptr) throw std::logic_error(std::string(__func__) + ": null result");
    *result = false;
    return 0;
}

}
