#include "prx/libSceAppChecker/include/Api.hpp"
#include <atomic>
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

std::atomic<bool> g_aborted{false};

}

extern "C" {

int APS5_VABI sceAppCheckerAbort(void) {
    g_aborted.store(true);
    return 0;
}

int APS5_VABI sceAppCheckerExecute(void) {
    g_aborted.store(false);
    return 0;
}

int APS5_VABI sceAppCheckerExecuteEx(void) {
    g_aborted.store(false);
    return 0;
}

}
