#include "prx/libSceAgcDriver/Execution/include/InputTwins.hpp"
#include <cstdio>

namespace {

struct Case {
    VkDriverId driver;
    bool windows;
    bool twins;
};

constexpr Case Cases[]{
    {VK_DRIVER_ID_NVIDIA_PROPRIETARY, true, true},
    {VK_DRIVER_ID_NVIDIA_PROPRIETARY, false, false},
    {VK_DRIVER_ID_AMD_PROPRIETARY, true, false},
    {VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS, true, false},
    {VK_DRIVER_ID_MESA_RADV, false, false},
    {VK_DRIVER_ID_MESA_LLVMPIPE, false, false},
};

}

int main() {
    int failures = 0;
    for (const auto& item : Cases) {
        if (AgcDriver::InputTwinsUsable(item.driver, item.windows) != item.twins) {
            std::fprintf(stderr, "driver %d on %s: expected read-only dispatch inputs %s\n", static_cast<int>(item.driver), item.windows ? "Windows" : "another host", item.twins ? "served from device-local twins" : "bound in place");
            ++failures;
        }
    }
    return failures == 0 ? 0 : 1;
}
