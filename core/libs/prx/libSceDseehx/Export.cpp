#include "prx/libSceDseehx/include/Api.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

std::atomic<bool> g_initialized{false};

}

extern "C" {

int APS5_VABI sceDseehxConvert() {
    if (!g_initialized.load()) {
        throw std::logic_error(std::string(__func__) + ": not initialized");
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDseehxExtInit() {
    bool expected = false;
    if (!g_initialized.compare_exchange_strong(expected, true)) {
        throw std::logic_error(std::string(__func__) + ": already initialized");
    }
    return 0;
}

int APS5_VABI sceDseehxFlush() {
    if (!g_initialized.load()) {
        throw std::logic_error(std::string(__func__) + ": not initialized");
    }
    return 0;
}

int APS5_VABI sceDseehxInit() {
    bool expected = false;
    if (!g_initialized.compare_exchange_strong(expected, true)) {
        throw std::logic_error(std::string(__func__) + ": already initialized");
    }
    return 0;
}

}
