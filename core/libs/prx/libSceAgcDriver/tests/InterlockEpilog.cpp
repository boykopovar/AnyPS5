#include "prx/libSceAgcDriver/Execution/include/InterlockEpilog.hpp"
#include <cstdint>
#include <cstdio>

namespace {

struct Case {
    VkDriverId driver;
    std::uint32_t version;
    bool ordered;
};

constexpr Case Cases[]{
    {VK_DRIVER_ID_MESA_RADV, VK_MAKE_API_VERSION(0, 26, 2, 7), false},
    {VK_DRIVER_ID_MESA_RADV, VK_MAKE_API_VERSION(0, 26, 2, 99), false},
    {VK_DRIVER_ID_MESA_RADV, VK_MAKE_API_VERSION(0, 26, 3, 0), false},
    {VK_DRIVER_ID_MESA_RADV, VK_MAKE_API_VERSION(0, 26, 3, 1), true},
    {VK_DRIVER_ID_MESA_RADV, VK_MAKE_API_VERSION(0, 26, 4, 0), true},
    {VK_DRIVER_ID_MESA_RADV, VK_MAKE_API_VERSION(0, 27, 0, 0), true},
    {VK_DRIVER_ID_MESA_RADV, 0u, false},
    {VK_DRIVER_ID_AMD_PROPRIETARY, VK_MAKE_API_VERSION(0, 26, 3, 1), false},
    {VK_DRIVER_ID_AMD_OPEN_SOURCE, VK_MAKE_API_VERSION(0, 26, 3, 1), false},
    {VK_DRIVER_ID_NVIDIA_PROPRIETARY, VK_MAKE_API_VERSION(0, 621, 1, 0), false},
    {VK_DRIVER_ID_INTEL_OPEN_SOURCE_MESA, VK_MAKE_API_VERSION(0, 26, 3, 1), false},
    {VK_DRIVER_ID_MESA_LLVMPIPE, VK_MAKE_API_VERSION(0, 26, 3, 1), false},
    {VK_DRIVER_ID_MOLTENVK, VK_MAKE_API_VERSION(0, 1, 4, 0), false},
    {VK_DRIVER_ID_MAX_ENUM, VK_MAKE_API_VERSION(0, 26, 3, 1), false},
};

}

int main() {
    int failures = 0;
    for (const auto& item : Cases) {
        if (AgcDriver::OrderedInterlockEpilogs(item.driver, item.version) != item.ordered) {
            std::fprintf(stderr, "driver %d version %u.%u.%u: expected %s interlock epilogs\n", static_cast<int>(item.driver), VK_API_VERSION_MAJOR(item.version), VK_API_VERSION_MINOR(item.version), VK_API_VERSION_PATCH(item.version), item.ordered ? "ordered" : "unordered");
            ++failures;
        }
    }
    return failures == 0 ? 0 : 1;
}
