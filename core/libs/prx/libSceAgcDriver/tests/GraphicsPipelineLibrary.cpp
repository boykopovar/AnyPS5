#include "prx/libSceAgcDriver/Execution/include/GraphicsPipelineLibrary.hpp"
#include <cstdio>

namespace {

struct Case {
    VkDriverId driver;
    std::uint32_t deviceId;
    std::uint32_t driverVersion;
    bool usable;
};

constexpr Case Cases[]{
    {VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS, 0xa78bu, 1659865u, false},
    {VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS, 0xa78bu, 1660911u, true},
    {VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS, 0xa78bu, 0u, true},
    {VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS, 0xa78au, 1659865u, true},
    {VK_DRIVER_ID_INTEL_OPEN_SOURCE_MESA, 0xa78bu, 1659865u, true},
    {VK_DRIVER_ID_MESA_RADV, 0xa78bu, 1659865u, true},
    {VK_DRIVER_ID_AMD_PROPRIETARY, 0xa78bu, 1659865u, true},
    {VK_DRIVER_ID_NVIDIA_PROPRIETARY, 0xa78bu, 1659865u, true},
    {VK_DRIVER_ID_MAX_ENUM, 0xa78bu, 1659865u, true},
};

}

int main() {
    int failures = 0;
    for (const auto& item : Cases) {
        if (AgcDriver::GraphicsPipelineLibraryUsable(item.driver, item.deviceId, item.driverVersion) != item.usable) {
            std::fprintf(stderr, "driver %d device 0x%x version %u: expected graphics pipeline libraries %s\n", static_cast<int>(item.driver), item.deviceId, item.driverVersion, item.usable ? "usable" : "unusable");
            ++failures;
        }
    }
    return failures == 0 ? 0 : 1;
}
