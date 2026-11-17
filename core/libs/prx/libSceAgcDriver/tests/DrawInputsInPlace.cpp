#include "prx/libSceAgcDriver/Execution/include/DrawInputsInPlace.hpp"
#include <cstddef>
#include <cstdio>

namespace {

struct Case {
    VkDriverId driver;
    bool windows;
    VkPhysicalDeviceType type;
    bool inPlace;
};

constexpr Case Cases[]{
    {VK_DRIVER_ID_NVIDIA_PROPRIETARY, true, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, false},
    {VK_DRIVER_ID_NVIDIA_PROPRIETARY, false, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, true},
    {VK_DRIVER_ID_AMD_PROPRIETARY, true, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, false},
    {VK_DRIVER_ID_AMD_PROPRIETARY, true, VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU, true},
    {VK_DRIVER_ID_AMD_PROPRIETARY, false, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, true},
    {VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS, true, VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU, true},
    {VK_DRIVER_ID_MESA_RADV, false, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, true},
    {VK_DRIVER_ID_MESA_LLVMPIPE, false, VK_PHYSICAL_DEVICE_TYPE_CPU, true},
};

}

int main() {
    int failures = 0;
    for (const auto& item : Cases) {
        if (AgcDriver::DrawInputsInPlaceUsable(item.driver, item.windows, item.type) != item.inPlace) {
            std::fprintf(stderr, "driver %d (device type %d) on %s: expected draw inputs %s\n", static_cast<int>(item.driver), static_cast<int>(item.type), item.windows ? "Windows" : "another host", item.inPlace ? "in place" : "copied");
            ++failures;
        }
        const std::size_t below = item.inPlace ? 0 : 1024;
        if (AgcDriver::DrawInputsInPlaceBelow(item.driver, item.windows, item.type) != below) {
            std::fprintf(stderr, "driver %d on %s: expected draw inputs fetching up to %zu bytes in place where they are copied\n", static_cast<int>(item.driver), item.windows ? "Windows" : "another host", below);
            ++failures;
        }
    }
    return failures == 0 ? 0 : 1;
}
