#include "prx/libSceAgcDriver/Execution/include/DrawInputsInPlace.hpp"
#include <cstdio>

namespace {

struct Case {
    VkDriverId driver;
    bool windows;
    bool inPlace;
};

constexpr Case Cases[]{
    {VK_DRIVER_ID_NVIDIA_PROPRIETARY, true, false},
    {VK_DRIVER_ID_NVIDIA_PROPRIETARY, false, true},
    {VK_DRIVER_ID_AMD_PROPRIETARY, true, true},
    {VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS, true, true},
    {VK_DRIVER_ID_MESA_RADV, false, true},
    {VK_DRIVER_ID_MESA_LLVMPIPE, false, true},
};

}

int main() {
    int failures = 0;
    for (const auto& item : Cases) {
        if (AgcDriver::DrawInputsInPlaceUsable(item.driver, item.windows) != item.inPlace) {
            std::fprintf(stderr, "driver %d on %s: expected draw inputs %s\n", static_cast<int>(item.driver), item.windows ? "Windows" : "another host", item.inPlace ? "in place" : "copied");
            ++failures;
        }
    }
    return failures == 0 ? 0 : 1;
}
