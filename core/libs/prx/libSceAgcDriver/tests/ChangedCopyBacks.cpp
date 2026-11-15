#include "prx/libSceAgcDriver/Execution/include/ChangedCopyBacks.hpp"
#include <cstdio>

namespace {

struct Case {
    VkDriverId driver;
    bool windows;
    bool changed;
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
        if (AgcDriver::ChangedCopyBacksUsable(item.driver, item.windows) != item.changed) {
            std::fprintf(stderr, "driver %d on %s: expected copy-backs of %s\n", static_cast<int>(item.driver), item.windows ? "Windows" : "another host", item.changed ? "the changed bytes" : "the whole range");
            ++failures;
        }
    }
    return failures == 0 ? 0 : 1;
}
