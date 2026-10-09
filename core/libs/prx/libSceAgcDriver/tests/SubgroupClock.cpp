#include "prx/libSceAgcDriver/Execution/include/SubgroupClock.hpp"
#include <cstdio>
#include <string_view>

namespace {

struct Case {
    VkDriverId driver;
    std::string_view name;
    bool narrow;
};

constexpr Case Cases[]{
    {VK_DRIVER_ID_MESA_RADV, "AMD Radeon RX 6750 XT (RADV NAVI22)", true},
    {VK_DRIVER_ID_MESA_RADV, "AMD Ryzen 5 7600X 6-Core Processor (RADV RAPHAEL_MENDOCINO)", true},
    {VK_DRIVER_ID_MESA_RADV, "AMD Radeon RX 7900 XTX (RADV NAVI31)", true},
    {VK_DRIVER_ID_MESA_RADV, "AMD Radeon 890M (RADV STRIX1)", true},
    {VK_DRIVER_ID_MESA_RADV, "AMD Radeon RX 9070 XT (RADV GFX1201)", false},
    {VK_DRIVER_ID_MESA_RADV, "AMD Radeon RX 5700 XT (RADV NAVI10)", false},
    {VK_DRIVER_ID_MESA_RADV, "AMD BC-250 (RADV GFX1013)", false},
    {VK_DRIVER_ID_MESA_RADV, "AMD Radeon RX 6750 XT (RADV NAVI22", false},
    {VK_DRIVER_ID_MESA_RADV, "AMD Radeon RX 6750 XT (RADV NAVI2)", false},
    {VK_DRIVER_ID_MESA_RADV, "", false},
    {VK_DRIVER_ID_AMD_PROPRIETARY, "AMD Radeon RX 6750 XT (RADV NAVI22)", false},
    {VK_DRIVER_ID_AMD_OPEN_SOURCE, "AMD Radeon RX 6750 XT", false},
    {VK_DRIVER_ID_NVIDIA_PROPRIETARY, "NVIDIA GeForce RTX 5070 Ti", false},
};

}

int main() {
    int failures = 0;
    for (const auto& item : Cases) {
        if (AgcDriver::NarrowSubgroupClock(item.driver, item.name) != item.narrow) {
            std::fprintf(stderr, "driver %d \"%.*s\": expected %s subgroup clock\n", static_cast<int>(item.driver), static_cast<int>(item.name.size()), item.name.data(), item.narrow ? "a narrow" : "a full");
            ++failures;
        }
    }
    return failures == 0 ? 0 : 1;
}
