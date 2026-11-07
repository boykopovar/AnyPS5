#include "prx/libSceLoginService/include/Api.hpp"
#include <atomic>
#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int SCE_LOGIN_SERVICE_ERROR_NOT_INITIALIZED = static_cast<int>(0x813a0001u);
constexpr int SCE_LOGIN_SERVICE_ERROR_INVALID_ARGUMENT = static_cast<int>(0x813a0003u);

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
    if (param == nullptr) {
        return SCE_LOGIN_SERVICE_ERROR_INVALID_ARGUMENT;
    }
    if (!g_initialized.load()) {
        return SCE_LOGIN_SERVICE_ERROR_NOT_INITIALIZED;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
