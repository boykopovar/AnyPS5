#include "prx/libSceAgcDriver/Execution/include/FragmentBarycentric.hpp"
#include <cstdio>

namespace {

struct Case {
    VkDriverId driver;
    bool reported;
    bool usable;
};

constexpr Case Cases[]{
    {VK_DRIVER_ID_MOLTENVK, true, false},
    {VK_DRIVER_ID_MOLTENVK, false, false},
    {VK_DRIVER_ID_MESA_RADV, true, true},
    {VK_DRIVER_ID_MESA_RADV, false, false},
    {VK_DRIVER_ID_NVIDIA_PROPRIETARY, true, true},
    {VK_DRIVER_ID_AMD_PROPRIETARY, true, true},
    {VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS, false, false},
};

}

int main() {
    int failures = 0;
    for (const auto& item : Cases) {
        if (AgcDriver::FragmentShaderBarycentricUsable(item.driver, item.reported) != item.usable) {
            std::fprintf(stderr, "driver %d reported %d: expected %s fragment barycentric\n", static_cast<int>(item.driver), item.reported ? 1 : 0, item.usable ? "usable" : "unusable");
            ++failures;
        }
    }
    return failures == 0 ? 0 : 1;
}
