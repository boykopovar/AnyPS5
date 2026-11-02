#include "prx/libScePsmUtil/include/Api.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <stdexcept>
#include <string>

namespace {
std::atomic<bool> g_initialized{false};
}

extern "C" {

int APS5_VABI scePsmUtilFinalize() {
    bool expected = true;
    if (!g_initialized.compare_exchange_strong(expected, false)) {
        throw std::logic_error(std::string(__func__) + ": not initialized");
    }
    return 0;
}

int APS5_VABI scePsmUtilGetPsmIniFile() {
    if (!g_initialized.load()) {
        throw std::logic_error(std::string(__func__) + ": not initialized");
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePsmUtilInitialize() {
    bool expected = false;
    if (!g_initialized.compare_exchange_strong(expected, true)) {
        throw std::logic_error(std::string(__func__) + ": already initialized");
    }
    return 0;
}

int APS5_VABI scePsmUtilRegisterCallbacks() {
    if (!g_initialized.load()) {
        throw std::logic_error(std::string(__func__) + ": not initialized");
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
