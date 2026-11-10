#include "prx/libSceS3da/include/Api.hpp"
#include <atomic>
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int ERROR_INVALID_ARGUMENT = static_cast<int>(0x81600002u);
constexpr int ERROR_ALREADY_OPEN = static_cast<int>(0x81600006u);
constexpr std::uint32_t MAX_SIZE = 0xA000;

std::atomic<bool> g_slots[2]{false, false};

}

extern "C" {

int APS5_VABI sceS3daClose(std::uint32_t port) {
    if (port > 1) return ERROR_INVALID_ARGUMENT;
    bool expected = true;
    if (!g_slots[port].compare_exchange_strong(expected, false)) {
        return ERROR_INVALID_ARGUMENT;
    }
    return 0;
}

int APS5_VABI sceS3daGetInfo(std::uint32_t port, S3daInfo* info) {
    if (port > 1) return ERROR_INVALID_ARGUMENT;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceS3daOpen(std::uint32_t port) {
    if (port > 1) return ERROR_INVALID_ARGUMENT;
    bool expected = false;
    if (!g_slots[port].compare_exchange_strong(expected, true)) {
        return ERROR_ALREADY_OPEN;
    }
    return static_cast<int>(port);
}

int APS5_VABI sceS3daSendData(std::uint32_t port, const void* data, std::uint32_t size) {
    if (port > 1 || data == nullptr || size == 0 || size > MAX_SIZE) return ERROR_INVALID_ARGUMENT;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
