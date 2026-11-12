#include "prx/libSceS3da/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <atomic>
#include <cstdint>
#include <cstddef>
#include <mutex>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int ERROR_INVALID_ARGUMENT = static_cast<int>(0x81600002u);
constexpr int ERROR_ALREADY_OPEN = static_cast<int>(0x81600006u);
constexpr std::uint32_t MAX_SIZE = 0xA000;
constexpr std::uint32_t SendDataCommand = 0x8018a601;
constexpr std::uint32_t GetInfoCommand = 0x8010a602;

std::atomic<bool> g_slots[2]{false, false};
std::once_flag g_opened;
int g_descriptor = -1;

struct SendDataRequest {
    std::uint32_t port;
    std::uint32_t reserved0;
    const void* data;
    std::uint32_t size;
    std::uint32_t reserved1;
};
static_assert(sizeof(SendDataRequest) == ((SendDataCommand >> 16) & 0x1fff));

struct GetInfoRequest {
    std::uint32_t port;
    std::uint32_t reserved;
    S3daInfo* info;
};
static_assert(sizeof(GetInfoRequest) == ((GetInfoCommand >> 16) & 0x1fff));

int Descriptor() {
    std::call_once(g_opened, [] { g_descriptor = open_nid_postfix("/dev/s3da", 2, 0); });
    return g_descriptor;
}

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
    GetInfoRequest request{port, 0, info};
    const int result = ioctl_nid_postfix(Descriptor(), GetInfoCommand, &request);
    return result < 0 ? result : 0;
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
    SendDataRequest request{port, 0, data, size, 0};
    const int result = ioctl_nid_postfix(Descriptor(), SendDataCommand, &request);
    return result < 0 ? result : 0;
}

}
