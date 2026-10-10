#include "prx/libc/include/general/VabiMacros.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

extern "C" {
std::int32_t APS5_VABI sceDeviceServiceInitialize(std::int32_t mode, std::int32_t flags);
std::int32_t APS5_VABI sceDeviceServiceTerminate();
std::int32_t APS5_VABI sceDeviceServiceGetEventState(std::int32_t deviceType);
std::int32_t APS5_VABI sceDeviceServiceQueryDeviceInfo_(std::int32_t deviceClass, std::int32_t arg1, std::int32_t arg2, void* infos, std::int32_t maxInfos, std::int32_t* count, std::int32_t* reserved, std::uint64_t infoSize);
}

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "DeviceService: %s\n", message);
        std::abort();
    }
}

}

int main() {
    Require(sceDeviceServiceInitialize(3, 0) == 0, "initialization failed");
    Require(sceDeviceServiceGetEventState(1) == 0, "a device event was reported");
    std::array<std::uint8_t, 0x70> info{};
    std::int32_t count = 7;
    std::int32_t reserved = 0;
    Require(sceDeviceServiceQueryDeviceInfo_(0x7001, 0, 0, info.data(), 1, &count, &reserved, info.size()) == 0 && count == 0, "a device was listed");
    bool threw = false;
    try {
        sceDeviceServiceQueryDeviceInfo_(0x7001, 0, 0, info.data(), 1, nullptr, &reserved, info.size());
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw, "a null count was accepted");
    Require(sceDeviceServiceTerminate() == 0, "termination failed");
    std::puts("DeviceService tests passed");
    return 0;
}
