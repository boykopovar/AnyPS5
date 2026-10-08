#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

#include <string>

extern "C" {

std::int32_t APS5_VABI sceDeviceServiceInitialize(std::int32_t mode, std::int32_t flags) {
    (void)mode;
    (void)flags;
    return 0;
}

std::int32_t APS5_VABI sceDeviceServiceTerminate() {
    return 0;
}

std::int32_t APS5_VABI sceDeviceServiceGetEventState(std::int32_t deviceType) {
    (void)deviceType;
    return 0;
}

std::int32_t APS5_VABI sceDeviceServiceQueryDeviceInfo_(std::int32_t deviceClass, std::int32_t arg1, std::int32_t arg2, void* infos, std::int32_t maxInfos, std::int32_t* count, std::int32_t* reserved, std::uint64_t infoSize) {
    (void)deviceClass;
    (void)arg1;
    (void)arg2;
    (void)infos;
    (void)reserved;
    (void)infoSize;
    if (count == nullptr || maxInfos < 0) throw std::invalid_argument(std::string(__func__) + ": invalid argument");
    *count = 0;
    return 0;
}

}
