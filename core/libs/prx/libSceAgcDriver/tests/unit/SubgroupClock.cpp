#include <Testing/Test.hpp>
#include "prx/libSceAgcDriver/Execution/include/SubgroupClock.hpp"

#include <cstdio>
#include <initializer_list>
#include <string>
#include <string_view>

namespace {

using Testing::Case;
using Testing::RequireEqual;

struct Device {
    VkDriverId driver;
    std::uint32_t deviceId;
    std::string_view name;
    bool narrow;
};

void RequireClocks(std::initializer_list<Device> devices) {
    for (const auto& item : devices) {
        char deviceId[16];
        std::snprintf(deviceId, sizeof(deviceId), "0x%x", item.deviceId);
        RequireEqual(AgcDriver::NarrowSubgroupClock(item.driver, item.deviceId, item.name), item.narrow,
                     "driver " + std::to_string(static_cast<int>(item.driver)) + " device " + deviceId + " \"" + std::string(item.name) + "\" narrow subgroup clock");
    }
}

const Case radvRdna2And3{"NarrowSubgroupClock_RadvRdna2OrRdna3_IsNarrow", [] {
    RequireClocks({
        {VK_DRIVER_ID_MESA_RADV, 0, "AMD Radeon RX 6750 XT (RADV NAVI22)", true},
        {VK_DRIVER_ID_MESA_RADV, 0, "AMD Ryzen 5 7600X 6-Core Processor (RADV RAPHAEL_MENDOCINO)", true},
        {VK_DRIVER_ID_MESA_RADV, 0, "AMD Radeon RX 7900 XTX (RADV NAVI31)", true},
        {VK_DRIVER_ID_MESA_RADV, 0, "AMD Radeon 890M (RADV STRIX1)", true},
    });
}};

const Case radvOtherGenerations{"NarrowSubgroupClock_RadvRdna1Rdna4OrMalformedName_IsFull", [] {
    RequireClocks({
        {VK_DRIVER_ID_MESA_RADV, 0, "AMD Radeon RX 9070 XT (RADV GFX1201)", false},
        {VK_DRIVER_ID_MESA_RADV, 0, "AMD Radeon RX 5700 XT (RADV NAVI10)", false},
        {VK_DRIVER_ID_MESA_RADV, 0, "AMD BC-250 (RADV GFX1013)", false},
        {VK_DRIVER_ID_MESA_RADV, 0, "AMD Radeon RX 6750 XT (RADV NAVI22", false},
        {VK_DRIVER_ID_MESA_RADV, 0, "AMD Radeon RX 6750 XT (RADV NAVI2)", false},
        {VK_DRIVER_ID_MESA_RADV, 0, "", false},
    });
}};

const Case amdProprietary{"NarrowSubgroupClock_AmdProprietary_DependsOnDeviceId", [] {
    RequireClocks({
        {VK_DRIVER_ID_AMD_PROPRIETARY, 0x1114, "AMD Radeon 860M", true},
        {VK_DRIVER_ID_AMD_PROPRIETARY, 0x1115, "AMD Radeon 860M", false},
        {VK_DRIVER_ID_AMD_PROPRIETARY, 0x744c, "AMD Radeon RX 7900 XT", true},
        {VK_DRIVER_ID_AMD_PROPRIETARY, 0x744d, "AMD Radeon RX 7900 XT", false},
        {VK_DRIVER_ID_AMD_PROPRIETARY, 0x747e, "AMD Radeon RX 7800 XT", true},
        {VK_DRIVER_ID_AMD_PROPRIETARY, 0x747f, "AMD Radeon RX 7800 XT", false},
        {VK_DRIVER_ID_AMD_PROPRIETARY, 0, "AMD Radeon RX 6750 XT (RADV NAVI22)", false},
    });
}};

const Case otherDrivers{"NarrowSubgroupClock_AmdOpenSourceOrNvidia_IsFull", [] {
    RequireClocks({
        {VK_DRIVER_ID_AMD_OPEN_SOURCE, 0x1114, "AMD Radeon 860M", false},
        {VK_DRIVER_ID_NVIDIA_PROPRIETARY, 0, "NVIDIA GeForce RTX 5070 Ti", false},
    });
}};

} // namespace
