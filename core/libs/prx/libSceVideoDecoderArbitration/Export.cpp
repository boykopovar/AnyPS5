#include "prx/libSceVideoDecoderArbitration/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <atomic>
#include <stdexcept>
#include <string>

namespace {

std::atomic<bool> g_initialized{false};

}

extern "C" {

int APS5_VABI sceVideoDecoderArbitrationAcceptEvent() {
    if (!g_initialized.load()) {
        throw std::logic_error(std::string(__func__) + ": not initialized");
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoDecoderArbitrationEnable() {
    if (!g_initialized.load()) {
        throw std::logic_error(std::string(__func__) + ": not initialized");
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoDecoderArbitrationEnableSuspendMode() {
    if (!g_initialized.load()) {
        throw std::logic_error(std::string(__func__) + ": not initialized");
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoDecoderArbitrationInitialize() {
    bool expected = false;
    if (!g_initialized.compare_exchange_strong(expected, true)) {
        throw std::logic_error(std::string(__func__) + ": already initialized");
    }
    return 0;
}

}
