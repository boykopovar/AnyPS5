#include <atomic>
#include "prx/libc/include/General.hpp"

namespace {
std::atomic<bool> g_initialized{false};
}

extern "C" {

int APS5_VABI sceNpEAAccessInitialize(void) {
    g_initialized.store(true);
    return 0;
}

int APS5_VABI sceNpEAAccessTerminate(void) {
    g_initialized.store(false);
    return 0;
}

}
