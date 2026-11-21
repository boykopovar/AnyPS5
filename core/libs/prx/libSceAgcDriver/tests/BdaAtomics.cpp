#include "prx/libSceAgcDriver/Execution/include/BdaAtomics.hpp"
#include <cstdio>

namespace {

struct Case {
    VkDriverId driver;
    bool usable;
};

constexpr Case Cases[]{
    {VK_DRIVER_ID_AMD_PROPRIETARY, true},
    {VK_DRIVER_ID_AMD_OPEN_SOURCE, true},
    {VK_DRIVER_ID_MESA_RADV, true},
    {VK_DRIVER_ID_NVIDIA_PROPRIETARY, true},
    {VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS, true},
    {VK_DRIVER_ID_INTEL_OPEN_SOURCE_MESA, true},
    {VK_DRIVER_ID_MESA_LLVMPIPE, true},
    {VK_DRIVER_ID_MOLTENVK, false},
};

}

int main() {
    int failures = 0;
    for (const auto& item : Cases) {
        if (AgcDriver::BdaAtomicsUsable(item.driver) != item.usable) {
            std::fprintf(stderr, "driver %d: expected atomics on buffer device addresses to be %s\n", static_cast<int>(item.driver), item.usable ? "usable" : "unusable");
            ++failures;
        }
    }
    return failures == 0 ? 0 : 1;
}
