#include "prx/libSceLoginService/include/Api.hpp"
#include <atomic>
#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

std::atomic<bool> g_initialized{false};

}

extern "C" {

int APS5_VABI sceLoginServiceInitialize(void) {
    g_initialized.store(true);
    return 0;
}

int APS5_VABI sceLoginServiceTerminate(void) {
    g_initialized.store(false);
    return 0;
}

int APS5_VABI sceLoginServiceRequestDevices(const LoginServiceDeviceRequest* param) {
    if (!param) return static_cast<int>(0x813a0003);
    if (!g_initialized.load()) return static_cast<int>(0x813a0001);
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
