#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <array>
#include <cstdint>
#include <stdexcept>

extern "C" {
std::int32_t APS5_VABI sceDeviceServiceInitialize(std::int32_t mode, std::int32_t flags);
std::int32_t APS5_VABI sceDeviceServiceTerminate();
std::int32_t APS5_VABI sceDeviceServiceGetEventState(std::int32_t deviceType);
std::int32_t APS5_VABI sceDeviceServiceQueryDeviceInfo_(std::int32_t deviceClass, std::int32_t arg1, std::int32_t arg2, void* infos, std::int32_t maxInfos, std::int32_t* count, std::int32_t* reserved, std::uint64_t infoSize);
}

namespace {

using Testing::Case;
using Testing::RequireEqual;
using Testing::RequireThrows;

constexpr std::int32_t deviceClass = 0x7001;

class DeviceService {
public:
    DeviceService() {
        RequireEqual(sceDeviceServiceInitialize(3, 0), 0, "initialization");
    }

    ~DeviceService() {
        sceDeviceServiceTerminate();
    }

    DeviceService(const DeviceService&) = delete;
    DeviceService& operator=(const DeviceService&) = delete;
};

const Case initializeAndTerminate{"Initialize_ThenTerminate_BothSucceed", [] {
    RequireEqual(sceDeviceServiceInitialize(3, 0), 0, "initialization");
    RequireEqual(sceDeviceServiceTerminate(), 0, "termination");
}};

const Case eventState{"GetEventState_NoDevices_ReportsNoEvent", [] {
    const DeviceService service;
    RequireEqual(sceDeviceServiceGetEventState(1), 0, "device event state");
}};

const Case queryDevices{"QueryDeviceInfo_NoDevices_ListsNothing", [] {
    const DeviceService service;
    std::array<std::uint8_t, 0x70> info{};
    std::int32_t count = 7;
    std::int32_t reserved = 0;
    RequireEqual(sceDeviceServiceQueryDeviceInfo_(deviceClass, 0, 0, info.data(), 1, &count, &reserved, info.size()), 0, "query result");
    RequireEqual(count, 0, "listed device count");
}};

const Case queryNullCount{"QueryDeviceInfo_NullCount_ThrowsInvalidArgument", [] {
    const DeviceService service;
    std::array<std::uint8_t, 0x70> info{};
    std::int32_t reserved = 0;
    RequireThrows<std::invalid_argument>([&] {
        sceDeviceServiceQueryDeviceInfo_(deviceClass, 0, 0, info.data(), 1, nullptr, &reserved, info.size());
    }, "query with a null count");
}};

} // namespace
