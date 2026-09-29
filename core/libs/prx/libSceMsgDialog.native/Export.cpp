#include <atomic>
#include <cstdint>
#include <stdexcept>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {
// Common dialog status: 0 none, 1 initialized, 2 running, 3 finished. Nothing is ever opened here, so it never runs.
std::atomic<int> g_status{0};
}

extern "C" {

int SceMsgDialogNativeModuleLoaded_nid_no_patch = 1;

int APS5_VABI sceMsgDialogGetResult(void* result) {
 (void)result;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceMsgDialogInitialize(void) {
    int expected = 0;
    if (!g_status.compare_exchange_strong(expected, 1)) throw std::logic_error("sceMsgDialogInitialize: already initialized");
    return 0;
}

int APS5_VABI sceMsgDialogOpen(const void* param) {
 (void)param;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceMsgDialogTerminate(void) {
    int expected = 1;
    if (!g_status.compare_exchange_strong(expected, 0)) throw std::logic_error("sceMsgDialogTerminate: not initialized or still running");
    return 0;
}

int APS5_VABI sceMsgDialogUpdateStatus(void) {
    return g_status.load();
}

}
