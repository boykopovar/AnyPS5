#include "prx/libSceAgcDriver/Execution/include/PipelineLinkOptimization.hpp"
#include <cstdio>

namespace {

struct Case {
    VkDriverId driver;
    bool usable;
};

constexpr Case Cases[]{
    {VK_DRIVER_ID_NVIDIA_PROPRIETARY, false},
    {VK_DRIVER_ID_MESA_RADV, true},
    {VK_DRIVER_ID_AMD_PROPRIETARY, true},
    {VK_DRIVER_ID_AMD_OPEN_SOURCE, true},
    {VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS, true},
    {VK_DRIVER_ID_MESA_LLVMPIPE, true},
    {VK_DRIVER_ID_MOLTENVK, true},
};

}

int main() {
    int failures = 0;
    for (const auto& item : Cases) {
        if (AgcDriver::PipelineLinkTimeOptimizationUsable(item.driver) != item.usable) {
            std::fprintf(stderr, "driver %d: expected %s background link-time optimization\n", static_cast<int>(item.driver), item.usable ? "usable" : "unusable");
            ++failures;
        }
    }
    return failures == 0 ? 0 : 1;
}
