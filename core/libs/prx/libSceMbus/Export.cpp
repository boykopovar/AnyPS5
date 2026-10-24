#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

#include <string>

struct DeviceServiceThreadParam {
    std::uint64_t affinityMask;
    std::int32_t priority;
    std::uint32_t reserved;
};

struct DeviceServiceInitParam {
    std::uint32_t size;
    std::uint32_t reserved;
    const DeviceServiceThreadParam* threadParam;
};

struct DeviceServiceUsageFilter {
    std::uint16_t usagePage;
    std::uint16_t minimumUsage;
    std::uint16_t maximumUsage;
};

static_assert(sizeof(DeviceServiceThreadParam) == 16);
static_assert(offsetof(DeviceServiceThreadParam, priority) == 8);
static_assert(sizeof(DeviceServiceInitParam) == 16);
static_assert(offsetof(DeviceServiceInitParam, threadParam) == 8);
static_assert(sizeof(DeviceServiceUsageFilter) == 6);

extern "C" {

std::int32_t APS5_VABI sceDeviceServiceInitialize(std::int32_t mode, const DeviceServiceInitParam* param) {
    (void)mode;
    if (param != nullptr) {
        if (param->size != sizeof(DeviceServiceInitParam) || param->reserved != 0) return static_cast<std::int32_t>(0x809B0080u);
        if (param->threadParam != nullptr) {
            if (param->threadParam->reserved != 0) return static_cast<std::int32_t>(0x809B0080u);
            throw std::runtime_error("sceDeviceServiceInitialize: custom worker scheduling is not implemented");
        }
    }
    return 0;
}

std::int32_t APS5_VABI sceDeviceServiceTerminate() {
    return 0;
}

std::int32_t APS5_VABI sceDeviceServiceGetEventState(std::int32_t clear) {
    (void)clear;
    return 0;
}

std::int32_t APS5_VABI sceDeviceServiceQueryDeviceInfo_(std::int32_t deviceClass, const DeviceServiceUsageFilter* filter, std::int32_t arg2, void* infos, std::int32_t maxInfos, std::int32_t* count, std::int32_t* reserved, std::uint32_t infoSize) {
    (void)arg2;
    (void)reserved;
    constexpr auto invalidArgument = static_cast<std::int32_t>(0x809B0080u);
    switch (deviceClass) {
    case 0x1001:
    case 0x2001:
    case 0x3001:
    case 0x4001:
    case 0x4002:
    case 0x5001:
    case 0x6001:
    case 0x7001:
    case 0x9001:
        if (filter != nullptr) return invalidArgument;
        break;
    case 0x8001:
        if (filter == nullptr || filter->minimumUsage > filter->maximumUsage) return invalidArgument;
        break;
    default:
        return invalidArgument;
    }
    if (infoSize != 112 || maxInfos < 0 || (maxInfos != 0 && infos == nullptr)) return invalidArgument;
    if (count != nullptr) *count = 0;
    return 0;
}

std::int32_t APS5_VABI sceDeviceServiceGetGeneration(void) {
    return 0;
}

}
