#include "prx/libc/include/General.hpp"
#include <atomic>

static std::atomic<bool> g_initialized{false};

extern "C" {

int APS5_VABI sceNpEAAccessInitialize(void) {
    g_initialized.store(true, std::memory_order_relaxed);
    return 0;
}

int APS5_VABI sceNpEAAccessTerminate(void) {
    if (!g_initialized.exchange(false, std::memory_order_relaxed)) {
        throw std::invalid_argument("sceNpEAAccessTerminate: not initialized");
    }
    return 0;
}

}
