#include "prx/libSceAgcDriver/Execution/include/SubgroupClock.hpp"
#include <algorithm>
#include <array>

namespace AgcDriver {

namespace {

constexpr std::array<std::string_view, 19> NarrowClockFamilies{
    "NAVI21", "NAVI22", "VANGOGH", "NAVI23", "NAVI24", "REMBRANDT", "RAPHAEL_MENDOCINO",
    "NAVI31", "NAVI32", "NAVI33", "PHOENIX", "PHOENIX2",
    "STRIX1", "STRIX_HALO", "KRACKAN1", "GFX1153", "GFX1156",
    "GFX1170", "GFX1171",
};

}

bool NarrowSubgroupClock(VkDriverId driver, std::string_view deviceName) {
    if (driver != VK_DRIVER_ID_MESA_RADV) return false;
    constexpr std::string_view prefix = "(RADV ";
    const auto start = deviceName.rfind(prefix);
    if (start == std::string_view::npos || deviceName.empty() || deviceName.back() != ')') return false;
    const auto family = deviceName.substr(start + prefix.size(), deviceName.size() - start - prefix.size() - 1u);
    return std::find(NarrowClockFamilies.begin(), NarrowClockFamilies.end(), family) != NarrowClockFamilies.end();
}

}
