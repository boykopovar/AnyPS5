#include "prx/libSceAgcDriver/Execution/include/ReadTrapsOverHostPointerImports.hpp"
#include <cstdio>

namespace {

struct Case {
    VkDriverId driver;
    bool usable;
};

constexpr Case Cases[]{
    {VK_DRIVER_ID_NVIDIA_PROPRIETARY, true},
    {VK_DRIVER_ID_MESA_RADV, false},
    {VK_DRIVER_ID_AMD_PROPRIETARY, false},
    {VK_DRIVER_ID_INTEL_OPEN_SOURCE_MESA, false},
    {VK_DRIVER_ID_MESA_NVK, false},
    {VK_DRIVER_ID_MESA_LLVMPIPE, false},
    {VK_DRIVER_ID_MOLTENVK, false},
};

}

int main() {
    int failures = 0;
    for (const auto& item : Cases) {
        if (AgcDriver::ReadTrapsOverHostPointerImportsUsable(item.driver) != item.usable) {
            std::fprintf(stderr, "driver %d: expected read traps %s host pointer imports\n", static_cast<int>(item.driver), item.usable ? "over" : "kept off");
            ++failures;
        }
    }
    return failures == 0 ? 0 : 1;
}
